/*
 * gas_srv rewrite for Android 10 (LineageOS 17.1)
 *
 * The original MTK gas_srv links libgas.so (which is absent from this ROM)
 * to construct android::GpuAppSpectatorService. This shim provides an
 * equivalent implementation of that Binder service:
 *   - registers "GpuAppSpectatorService" with the ServiceManager
 *   - serves the IGpuAppSpectatorService transaction set
 *     (1: setSamplingInterval, 2: setGpuUtilizationThreshold,
 *      3: sendAppInfo, 4: changeClassifier)
 *   - monitors GPU loading through libged.so and boosts/releases the GPU
 *     via ged_dvfs_probe when utilization crosses the threshold
 */

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <android/log.h>
#include <binder/Binder.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <utils/String8.h>

using namespace android;

#define TAG "gas_srv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* ---- libged.so function types ---- */
typedef void *ged_handle_t;
typedef ged_handle_t (*ged_create_fn)(void);
typedef void (*ged_destroy_fn)(ged_handle_t);
typedef int (*ged_dvfs_probe_fn)(ged_handle_t, int);
typedef int (*ged_query_info_fn)(ged_handle_t, int, int, void *);

#define GED_LOADING 0

static ged_create_fn p_create = NULL;
static ged_destroy_fn p_destroy = NULL;
static ged_dvfs_probe_fn p_probe = NULL;
static ged_query_info_fn p_query_info = NULL;
static ged_handle_t g_ged = NULL;

/* ---- monitor state ---- */
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_state_cond = PTHREAD_COND_INITIALIZER;
static int g_sampling_interval = 1000;   /* ms */
static int g_gpu_threshold = 90;         /* % */
static int g_loading = -1;
static int g_stop = 0;
static pthread_t g_monitor_tid = 0;

/* ================================================================
 * GpuAppSpectatorService
 * ================================================================ */

class GpuAppSpectatorService : public BBinder {
public:
    static const String16 descriptor;

    GpuAppSpectatorService() {}
    ~GpuAppSpectatorService() override {}

    const String16 &getInterfaceDescriptor() const override {
        return descriptor;
    }

    void setSamplingInterval(int ms) {
        if (ms <= 0)
            ms = 1000;
        pthread_mutex_lock(&g_state_lock);
        g_sampling_interval = ms;
        pthread_cond_signal(&g_state_cond);
        pthread_mutex_unlock(&g_state_lock);
    }

    void setGpuUtilizationThreshold(int threshold) {
        if (threshold <= 0 || threshold > 100)
            threshold = 90;
        pthread_mutex_lock(&g_state_lock);
        g_gpu_threshold = threshold;
        pthread_mutex_unlock(&g_state_lock);
    }

    void changeClassifier(int cls) {
        (void)cls;
    }

    void sendAppInfo(int appGid, int appClass, int tmo) {
        (void)appGid;
        (void)appClass;
        (void)tmo;
    }

    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
                        uint32_t flags) override {
        (void)flags;

        switch (code) {
        case 1:  /* setSamplingInterval */
            setSamplingInterval(data.readInt32());
            return NO_ERROR;
        case 2:  /* setGpuUtilizationThreshold */
            setGpuUtilizationThreshold(data.readInt32());
            return NO_ERROR;
        case 3:  /* sendAppInfo */
            sendAppInfo(data.readInt32(), data.readInt32(), data.readInt32());
            return NO_ERROR;
        case 4:  /* changeClassifier */
            changeClassifier(data.readInt32());
            return NO_ERROR;
        default:
            return BBinder::onTransact(code, data, reply, flags);
        }
    }

    status_t dump(int fd, const Vector<String16> &args) override {
        (void)args;
        String8 result;
        result.appendFormat("GpuAppSpectatorService: pid=%d interval=%dms "
                            "threshold=%d%% loading=%d%%\n",
                            getpid(), g_sampling_interval, g_gpu_threshold,
                            g_loading);
        write(fd, result.string(), result.size());
        return NO_ERROR;
    }
};

const String16 GpuAppSpectatorService::descriptor("GpuAppSpectatorService");

/* ================================================================
 * GPU monitor thread
 * ================================================================ */

static void *monitor_loop(void *arg) {
    (void)arg;

    while (true) {
        pthread_mutex_lock(&g_state_lock);
        if (g_stop) {
            pthread_mutex_unlock(&g_state_lock);
            break;
        }
        /* sleep until the sampling interval elapses or it is changed */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += g_sampling_interval / 1000;
        ts.tv_nsec += (g_sampling_interval % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_cond_timedwait(&g_state_cond, &g_state_lock, &ts);
        int threshold = g_gpu_threshold;
        int stop = g_stop;
        pthread_mutex_unlock(&g_state_lock);

        if (stop)
            break;

        int loading = -1;
        if (g_ged && p_query_info)
            p_query_info(g_ged, GED_LOADING, 4, &loading);

        pthread_mutex_lock(&g_state_lock);
        g_loading = loading;
        pthread_mutex_unlock(&g_state_lock);

        if (loading < 0)
            continue;

        if (g_ged && p_probe) {
            if (loading >= threshold)
                p_probe(g_ged, 1);   /* busy -> boost GPU */
            else
                p_probe(g_ged, -1);  /* idle -> release */
        }
    }

    return NULL;
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    __android_log_print(ANDROID_LOG_ERROR, TAG, "gas_srv: pid=%d", getpid());

    void *libged = dlopen("libged.so", RTLD_NOW);
    if (!libged) {
        LOGE("failed to load libged.so: %s", dlerror());
        return 1;
    }

    p_create = (ged_create_fn)dlsym(libged, "ged_create");
    p_destroy = (ged_destroy_fn)dlsym(libged, "ged_destroy");
    p_probe = (ged_dvfs_probe_fn)dlsym(libged, "ged_dvfs_probe");
    p_query_info = (ged_query_info_fn)dlsym(libged, "ged_query_info");

    if (!p_create || !p_destroy || !p_probe || !p_query_info) {
        LOGE("failed to resolve ged symbols");
        dlclose(libged);
        return 1;
    }

    g_ged = p_create();
    if (!g_ged) {
        LOGE("ged_create failed");
        dlclose(libged);
        return 1;
    }

    pthread_create(&g_monitor_tid, NULL, monitor_loop, NULL);

    sp<IServiceManager> sm = defaultServiceManager();
    if (sm != NULL) {
        sp<GpuAppSpectatorService> service = new GpuAppSpectatorService();
        status_t err = sm->addService(GpuAppSpectatorService::descriptor,
                                      service, false);
        if (err != NO_ERROR)
            LOGE("addService GpuAppSpectatorService failed: %d", err);
        else
            LOGI("GpuAppSpectatorService registered");
    } else {
        LOGE("failed to get ServiceManager");
    }

    /* serve the registered service */
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool(true);

    /* joinThreadPool returned -> shut down the monitor */
    g_stop = 1;
    pthread_mutex_lock(&g_state_lock);
    pthread_cond_signal(&g_state_cond);
    pthread_mutex_unlock(&g_state_lock);
    pthread_join(g_monitor_tid, NULL);

    if (p_destroy && g_ged)
        p_destroy(g_ged);

    dlclose(libged);
    return 0;
}
