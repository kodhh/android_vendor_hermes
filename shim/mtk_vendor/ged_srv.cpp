/*
 * ged_srv rewrite for Android 10 (LineageOS 17.1)
 *
 * Faithful reconstruction of the original MTK ged_srv (disassembled from the
 * lineage-16.0 binary). Drives kernel GED DVFS via libged.so and keeps
 * SurfaceFlinger informed of the vsync offset / FPS bound the way MTK does.
 *
 * Restored behavior:
 *   - SIG44 (vsync)   -> wake the vsync_offset worker
 *   - SIG45 (fps)     -> read /d/ged/hal/fps_upper_bound and push it to SF
 *   - SIG46 (suicide) -> graceful shutdown
 *   - worker thread queries GED event status (0xD) + debug vector (0xE),
 *     toggles the kernel vsync offset (0 / 0xff85ee00) and notifies
 *     SurfaceFlinger (transact 0x2712), driving ged_dvfs_probe(+1/-3).
 *   - setFPS() sends transact 0x2711 to SurfaceFlinger and persists the
 *     bound in persist.mtk.sf.fps.upper_bound.
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cutils/properties.h>
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
typedef void (*ged_log_tpt_print_fn)(void *, const char *, ...);
typedef void (*ged_log_disconnect_fn)(void *);

static ged_create_fn p_create = NULL;
static ged_destroy_fn p_destroy = NULL;
static ged_dvfs_probe_fn p_probe = NULL;
static ged_dvfs_set_vsync_offset_fn p_set_vsync = NULL;
static ged_query_info_fn p_query_info = NULL;
static ged_log_connect_fn p_log_connect = NULL;
static ged_log_tpt_print_fn p_log_tpt_print = NULL;
static ged_log_disconnect_fn p_log_disconnect = NULL;

static ged_handle_t g_ged = NULL;
static void *g_log = NULL;

/* vsync-offset worker state */
static int g_last_ged = 0;
static int g_vsync_run = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static volatile sig_atomic_t g_exit = 1;
static pthread_t g_worker_tid = 0;

/* kernel GED signal numbers */
#define SIG_GED_VSYNC   44
#define SIG_GED_FPS     45
#define SIG_GED_SUICIDE 46

/* Binder transaction codes (MTK SurfaceFlinger) */
#define TRANSACT_FPS_SET       0x2711  /* 10001 */
#define TRANSACT_VSYNC_OFFSET  0x2712  /* 10002 */
#define INTERFACE_TRANSACTION  0x5f4e5446

#define MAGIC              ((int)0xdea0ddad)
#define NOCAL_VSYNC_OFFSET ((int)0xff85ee00)

/* GED query info types (kernel ged_type.h) */
#define GED_EVENT_STATUS 0xD
#define GED_DEBUG_STATUS 0xE

/* ================================================================
 * SurfaceFlinger client (matches original controller object)
 * ================================================================ */

class SFClient {
public:
    SFClient() : mSM(NULL), mSF(NULL), mMagic(0) {
        pthread_mutex_init(&mMutex, NULL);
        mSM = defaultServiceManager();
    }

    ~SFClient() {
        pthread_mutex_destroy(&mMutex);
    }

    /* fetch SurfaceFlinger + its interface descriptor */
    void init() {
        mSF = (mSM != NULL) ? mSM->getService(String16("SurfaceFlinger")) : NULL;
        if (mSF == NULL) {
            mMagic = MAGIC;
            return;
        }
        sf_query_descriptor();
        mMagic = 0;
    }

    /* re-acquire SurfaceFlinger after a failed transaction */
    void reconnect() {
        if (mSM == NULL)
            mSM = defaultServiceManager();
        mSF = (mSM != NULL) ? mSM->getService(String16("SurfaceFlinger")) : NULL;
        if (mSF == NULL) {
            mMagic = MAGIC;
            return;
        }
        sf_query_descriptor();
        mMagic = 0;
    }

    /* send {interface token, value} to SurfaceFlinger */
    status_t transact(uint32_t code, int value) {
        Parcel data, reply;
        data.writeInterfaceToken(mDescriptor);
        data.writeInt32(value);
        pthread_mutex_lock(&mMutex);
        status_t st;
        if (mSF == NULL) {
            mMagic = MAGIC;
            st = MAGIC;
        } else {
            st = mSF->transact(code, data, &reply, 0);
        }
        pthread_mutex_unlock(&mMutex);
        return st;
    }

    bool sf_ok() const { return mMagic != MAGIC; }

private:
    void sf_query_descriptor() {
        Parcel data, reply;
        if (mSF == NULL) {
            mDescriptor = String16();
            return;
        }
        if (mSF->transact(INTERFACE_TRANSACTION, data, &reply, 0) != NO_ERROR) {
            mDescriptor = String16();
            return;
        }
        mDescriptor = reply.readString16();
    }

    sp<IServiceManager> mSM;
    sp<IBinder> mSF;
    String16 mDescriptor;
    pthread_mutex_t mMutex;
    int mMagic;
};

static SFClient *g_sf = NULL;

/* ================================================================
 * FPS upper bound handling
 * ================================================================ */

static int clamp_fps(int fps) {
    if (fps <= 0 || fps > 60 || (60 % fps) != 0)
        return 60;
    return fps;
}

static void setFPS(int fps) {
    fps = clamp_fps(fps);

    status_t st = g_sf->transact(TRANSACT_FPS_SET, fps);
    if (st == NO_ERROR) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", fps);
        property_set("persist.mtk.sf.fps.upper_bound", buf);
        LOGI("FPS is set to %d", fps);
        return;
    }

    if (g_log && p_log_tpt_print)
        p_log_tpt_print(g_log, "SurfaceFlinger binder operation failed: %x", st);
    LOGI("Worker Try to reconnect SF");
    g_sf->reconnect();
    if (!g_sf->sf_ok()) {
        if (g_log && p_log_tpt_print)
            p_log_tpt_print(g_log, "Error on request FPS change");
        LOGI("Try to reconnect SF");
        return;
    }
    g_sf->transact(TRANSACT_FPS_SET, fps);
}

static void readFPSUpperBoundFile(void) {
    int fd = open("/d/ged/hal/fps_upper_bound", O_RDONLY);
    if (fd < 0) {
        LOGE("Error on opening FPS upper bound file! err=%s", strerror(errno));
        return;
    }
    char buf[8] = {0};
    ssize_t n = read(fd, buf, 2);
    close(fd);
    if (n < 0) {
        LOGE("Error on reading FPS upper bound file! err=%s", strerror(errno));
        return;
    }
    int fps = (int)strtoul(buf, NULL, 10);
    LOGI("buff=%s, fps=%d", buf, fps);
    setFPS(fps);
}

static void readFPSProperty(void) {
    char value[PROP_VALUE_MAX] = {0};
    if (property_get("persist.mtk.sf.fps.upper_bound", value, NULL) <= 0)
        return;
    int fps = (int)strtoul(value, NULL, 10);
    setFPS(fps);
}

/* ================================================================
 * signal handlers
 * ================================================================ */

static void sig_vsync(int sig) {          /* SIG 44: kick the worker */
    (void)sig;
    pthread_mutex_lock(&g_lock);
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

static void sig_fps(int sig) {            /* SIG 45: update FPS bound */
    (void)sig;
    readFPSUpperBoundFile();
}

static void sig_exit(int sig) {           /* SIG 46: graceful shutdown */
    (void)sig;
    g_exit = 0;
}

/* ================================================================
 * vsync offset event loop (matches original eventfunc)
 * ================================================================ */

static int eventfunc(int arg = 0) {
    if (arg != 0)
        g_last_ged = 0;

    int evt = 0, status = 0;
    if (p_query_info) {
        p_query_info(g_ged, GED_EVENT_STATUS, 4, &evt);
        p_query_info(g_ged, GED_DEBUG_STATUS, 4, &status);
    }

    if (g_log && p_log_tpt_print) {
        p_log_tpt_print(g_log, "Vsync-Offset Event Vector", evt);
        p_log_tpt_print(g_log, "Vsync-Offset Debug Vector", status);
    }

    int new_offset;
    if (status & 0x1)
        new_offset = NOCAL_VSYNC_OFFSET;
    else if (status & 0x2)
        new_offset = 0;
    else
        new_offset = g_last_ged;

    int changed = (new_offset != g_last_ged);

    if (changed || (status & 0x4)) {
        if (g_log && p_log_tpt_print)
            p_log_tpt_print(g_log, "Vsync-Offset Debug Vector changed: 0x%x", new_offset);
        if (p_set_vsync)
            p_set_vsync(new_offset);

        status_t st = g_sf->transact(TRANSACT_VSYNC_OFFSET, new_offset);
        if (st != NO_ERROR) {
            if (g_log && p_log_tpt_print)
                p_log_tpt_print(g_log, "SurfaceFlinger binder operation failed: %x", st);
            LOGI("Worker Try to reconnect SF");
            g_sf->reconnect();
            if (!g_sf->sf_ok()) {
                if (g_log && p_log_tpt_print)
                    p_log_tpt_print(g_log, "Error on request FPS change");
                LOGI("Try to reconnect SF");
                g_last_ged = new_offset;
                return 0;   /* SF unreachable -> boost GPU */
            }
            g_sf->transact(TRANSACT_VSYNC_OFFSET, new_offset);
        }
    }

    if (changed && new_offset == 0) {
        usleep(3000000);
        long v = 0;
        if (p_query_info)
            p_query_info(g_ged, 0xC, 8, &v);
        new_offset = (int)v;
        if (new_offset == 0) {
            status_t st = g_sf->transact(TRANSACT_VSYNC_OFFSET, 0);
            if (st != NO_ERROR) {
                if (g_log && p_log_tpt_print)
                    p_log_tpt_print(g_log, "SurfaceFlinger binder operation failed: %x", st);
                LOGI("Worker Try to reconnect SF");
                g_sf->reconnect();
            }
        }
    }

    g_last_ged = new_offset;
    return 1;
}

/* ================================================================
 * vsync offset worker thread
 * ================================================================ */

static void *vsync_offset_worker(void *arg) {
    (void)arg;

    if (g_log && p_log_tpt_print)
        p_log_tpt_print(g_log, "void* vsync_offset_worker(void*): tid=%d",
                        (int)syscall(SYS_gettid));

    eventfunc(1);

    while (g_exit) {
        pthread_mutex_lock(&g_lock);
        if (g_vsync_run) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&g_cond, &g_lock, &ts);
        } else {
            pthread_cond_wait(&g_cond, &g_lock);
        }
        pthread_mutex_unlock(&g_lock);

        if (!g_exit)
            break;

        if (eventfunc() == 0) {
            g_vsync_run = 1;
            if (p_probe)
                p_probe(g_ged, 1);
        } else {
            g_vsync_run = 0;
            if (p_probe)
                p_probe(g_ged, -3);
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

    signal(SIG_GED_VSYNC, sig_vsync);
    signal(SIG_GED_FPS, sig_fps);
    signal(SIG_GED_SUICIDE, sig_exit);

    g_sf = new SFClient();
    g_sf->init();

    readFPSProperty();

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

    if (p_log_connect)
        g_log = p_log_connect("ged_srv_Log");

    g_ged = p_create();
    if (!g_ged) {
        LOGE("ged_create failed");
        if (g_log && p_log_disconnect) p_log_disconnect(g_log);
        dlclose(libged);
        return 1;
    }

    pthread_create(&g_worker_tid, NULL, vsync_offset_worker, NULL);

    /* register with kernel GED DVFS */
    p_set_vsync(NOCAL_VSYNC_OFFSET);
    p_probe(g_ged, (int)getpid());

    LOGI("ged DVFS active, pid=%d", getpid());

    /* main loop: blocked until SIG_GED_SUICIDE arrives */
    while (g_exit)
        pause();

    __android_log_print(ANDROID_LOG_ERROR, TAG, "ged_srv getting out");
    if (g_log && p_log_tpt_print)
        p_log_tpt_print(g_log, "ged_srv getting out");
    if (p_probe)
        p_probe(g_ged, -1);

    /* wake worker and join */
    pthread_mutex_lock(&g_lock);
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
    pthread_join(g_worker_tid, NULL);

    if (p_destroy)
        p_destroy(g_ged);

    delete g_sf;

    __android_log_print(ANDROID_LOG_ERROR, TAG, "ged_srv DIE");
    if (g_log) {
        if (p_log_tpt_print)
            p_log_tpt_print(g_log, "ged_srv DIE");
        if (p_log_disconnect)
            p_log_disconnect(g_log);
    }

    dlclose(libged);
    return 0;
}
