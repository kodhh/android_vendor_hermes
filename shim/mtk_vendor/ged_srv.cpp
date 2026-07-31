/*
 * ged_srv rewrite for Android 10 (LineageOS 17.1)
 *
 * Uses libged.so for kernel /proc/ged communication (pure C, no Binder ABI).
 * Implements "GED DVFS Service" Binder interface for HWC queries.
 *
 * Replaces original MTK ged_srv that crashed due to Binder vtable incompatibility.
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <android/log.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

using namespace android;

#define TAG "ged_srv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

/* ---- libged.so function types ---- */
typedef void *ged_handle_t;
typedef ged_handle_t (*ged_create_fn)(void);
typedef void (*ged_destroy_fn)(ged_handle_t);
typedef int (*ged_dvfs_probe_fn)(ged_handle_t, int);
typedef int (*ged_dvfs_set_vsync_offset_fn)(int);
typedef int (*ged_query_info_fn)(ged_handle_t, int, int, void *);
typedef void *(*ged_log_connect_fn)(const char *);
typedef void (*ged_log_tpt_print_fn)(void *, void *);
typedef void (*ged_log_disconnect_fn)(void *);

/* ---- globals ---- */
static volatile sig_atomic_t g_running = 1;
static ged_handle_t g_ged = NULL;
static void *g_log = NULL;
static pthread_t g_worker_tid;

static ged_create_fn p_create = NULL;
static ged_destroy_fn p_destroy = NULL;
static ged_dvfs_probe_fn p_probe = NULL;
static ged_dvfs_set_vsync_offset_fn p_set_vsync = NULL;
static ged_query_info_fn p_query_info = NULL;
static ged_log_connect_fn p_log_connect = NULL;
static ged_log_tpt_print_fn p_log_tpt_print = NULL;
static ged_log_disconnect_fn p_log_disconnect = NULL;

/* worker sync */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static int g_dvfs_active = 0;

/* kernel GED signal numbers */
#define SIG_GED_VSYNC   44
#define SIG_GED_FPS     45
#define SIG_GED_SUICIDE 46

/* Binder transaction codes (from reverse engineering) */
#define TRANSACT_GPU_INFO     0x2711  /* 10001 - query GPU freq info */
#define TRANSACT_VSYNC_OFFSET 0x2712  /* 10002 - set vsync offset */

/* default vsync offset (0xff85ee00 from disassembly) */
#define DEFAULT_VSYNC_OFFSET  ((int)0xff85ee00)

/* ---- signal handler ---- */
static void sighandler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ================================================================
 * Binder service: "GED DVFS Service"
 *
 * HWC (hwcomposer.mt6795.so) calls this service to:
 *   - Query GPU frequency info (transact 0x2711)
 *   - Set vsync offset (transact 0x2712)
 *
 * This replaces the original MTK Binder implementation that crashed
 * due to Android 9->10 Binder ABI incompatibility.
 * ================================================================ */

class GEDService : public BBinder {
public:
    static const String16 descriptor;

    GEDService() : BBinder() {
    }

    ~GEDService() override {}

    status_t onTransact(uint32_t code, const Parcel &data, Parcel *reply,
                        uint32_t flags) override {
        (void)flags;

        switch (code) {
        case TRANSACT_GPU_INFO: {
            /* HWC queries GPU frequency info */
            int param = data.readInt32();
            if (reply != nullptr) {
                reply->writeInt32(0);       /* status: OK */
                reply->writeInt32(param);   /* echo back value */
            }
            LOGD("GPU_INFO query: param=%d", param);
            return NO_ERROR;
        }

        case TRANSACT_VSYNC_OFFSET: {
            /* HWC sets vsync offset */
            int offset = data.readInt32();
            if (p_set_vsync)
                p_set_vsync(offset);
            if (reply != nullptr)
                reply->writeInt32(0);
            LOGD("VSYNC_OFFSET set: 0x%x", offset);
            return NO_ERROR;
        }

        default:
            return BBinder::onTransact(code, data, reply, flags);
        }
    }

    status_t dump(int fd, const Vector<String16> &args) override {
        (void)fd;
        (void)args;
        String8 result;
        result.appendFormat("GED DVFS Service: pid=%d active=%d\n",
                            getpid(), g_dvfs_active);
        write(fd, result.string(), result.size());
        return NO_ERROR;
    }
};

const String16 GEDService::descriptor("GED DVFS Service");

/* ---- FPS upper bound from debugfs ---- */

static int read_fps_upper_bound(void) {
    int fd = open("/sys/kernel/debug/ged/hal/fps_upper_bound", O_RDONLY);
    if (fd < 0) {
        LOGE("cannot open fps_upper_bound: %s", strerror(errno));
        return -1;
    }
    char buf[8] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    int fps = (int)strtoul(buf, NULL, 10);
    LOGI("fps_upper_bound = %d", fps);
    return fps;
}

/* ---- property handling ---- */

static void ged_handle_property(void) {
    char value[PROP_VALUE_MAX] = {0};
    __system_property_get("debug.ged.tpt", value);
    if (value[0] == '\0')
        return;
    int tpt = (int)strtoul(value, NULL, 10);
    if (tpt <= 0)
        return;
    if (tpt < 10) tpt = 10;
    if (tpt > 60) tpt = 60;
    LOGI("debug.ged.tpt = %d", tpt);
    p_probe(g_ged, tpt);
}

/* ---- worker thread ---- */

static void *ged_worker_thread(void *arg) {
    (void)arg;

    /* read FPS upper bound from debugfs */
    int fps = read_fps_upper_bound();
    if (fps > 0)
        p_probe(g_ged, fps);

    /* register "GED DVFS Service" with ServiceManager */
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm != nullptr) {
        sp<GEDService> svc = new GEDService();
        status_t err = sm->addService(String16("GED DVFS Service"), svc);
        if (err != OK)
            LOGE("addService failed: %d", err);
        else
            LOGI("GED DVFS Service registered");
    } else {
        LOGE("failed to get ServiceManager");
    }

    /* start Binder thread pool */
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool(true);

    /* event processing loop */
    pthread_mutex_lock(&g_lock);
    g_dvfs_active = 1;

    while (g_running) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&g_cond, &g_lock, &ts);

        if (!g_running)
            break;

        /* query event status from ged */
        int event_status = 0;
        if (p_query_info)
            p_query_info(g_ged, 0xD, 4, &event_status);

        /* handle touch down: set vsync offset */
        if (event_status & 0x1) {
            if (p_set_vsync)
                p_set_vsync(DEFAULT_VSYNC_OFFSET);
        }

        /* handle GAS event */
        if (event_status & 0x4) {
            LOGD("GAS event active");
        }
    }

    g_dvfs_active = 0;
    pthread_mutex_unlock(&g_lock);
    return NULL;
}

/* ---- main ---- */

int main(void) {
    /* register signal handlers (kernel GED sends these) */
    signal(SIG_GED_VSYNC, sighandler);
    signal(SIG_GED_FPS, sighandler);
    signal(SIG_GED_SUICIDE, sighandler);

    /* load libged.so (pure C library, no Binder) */
    void *libged = dlopen("libged.so", RTLD_NOW);
    if (!libged) {
        LOGE("failed to load libged.so: %s", dlerror());
        return 1;
    }

    p_create = (ged_create_fn)dlsym(libged, "ged_create");
    p_destroy = (ged_destroy_fn)dlsym(libged, "ged_destroy");
    p_probe = (ged_dvfs_probe_fn)dlsym(libged, "ged_dvfs_probe");
    p_set_vsync = (ged_dvfs_set_vsync_offset_fn)dlsym(libged, "ged_dvfs_set_vsync_offset");
    p_query_info = (ged_query_info_fn)dlsym(libged, "ged_query_info");
    p_log_connect = (ged_log_connect_fn)dlsym(libged, "ged_log_connect");
    p_log_tpt_print = (ged_log_tpt_print_fn)dlsym(libged, "ged_log_tpt_print");
    p_log_disconnect = (ged_log_disconnect_fn)dlsym(libged, "ged_log_disconnect");

    if (!p_create || !p_destroy || !p_probe || !p_set_vsync || !p_query_info) {
        LOGE("failed to resolve ged symbols");
        dlclose(libged);
        return 1;
    }

    /* connect to ged log */
    if (p_log_connect)
        g_log = p_log_connect("ged_srv_Log");

    /* create ged instance */
    g_ged = p_create();
    if (!g_ged) {
        LOGE("ged_create failed");
        if (g_log && p_log_disconnect) p_log_disconnect(g_log);
        dlclose(libged);
        return 1;
    }

    /* set vsync offset and register PID with kernel */
    p_set_vsync(DEFAULT_VSYNC_OFFSET);
    p_probe(g_ged, (int)getpid());

    LOGI("ged DVFS active, pid=%d", getpid());

    /* read initial property */
    ged_handle_property();

    /* start worker thread (Binder + event loop) */
    pthread_create(&g_worker_tid, NULL, ged_worker_thread, NULL);

    /* main loop: wait for kernel signals */
    while (g_running) {
        pause();
        if (!g_running)
            break;
        /* log TPT on signal */
        if (g_log && p_log_tpt_print)
            p_log_tpt_print(g_log, NULL);
        /* deregister from ged */
        p_probe(g_ged, -1);
    }

    /* wake worker thread */
    pthread_mutex_lock(&g_lock);
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
    pthread_join(g_worker_tid, NULL);

    /* cleanup */
    if (p_destroy)
        p_destroy(g_ged);

    if (g_log) {
        if (p_log_tpt_print) p_log_tpt_print(g_log, NULL);
        if (p_log_disconnect) p_log_disconnect(g_log);
    }

    LOGI("ged_srv exiting");
    dlclose(libged);
    return 0;
}
