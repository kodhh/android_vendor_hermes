#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#define TAG "gas_srv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

/* ---- libged.so function types ---- */
typedef void *ged_handle_t;
typedef ged_handle_t (*ged_create_fn)(void);
typedef void (*ged_destroy_fn)(ged_handle_t);
typedef int (*ged_query_info_fn)(ged_handle_t, int, int, void *);
typedef void *(*ged_log_connect_fn)(const char *);
typedef void (*ged_log_tpt_print_fn)(void *, void *);
typedef void (*ged_log_disconnect_fn)(void *);

/* ---- globals ---- */
static volatile sig_atomic_t g_running = 1;
static ged_handle_t g_ged = NULL;
static void *g_log = NULL;

static ged_create_fn p_create = NULL;
static ged_destroy_fn p_destroy = NULL;
static ged_query_info_fn p_query_info = NULL;
static ged_log_connect_fn p_log_connect = NULL;
static ged_log_tpt_print_fn p_log_tpt_print = NULL;
static ged_log_disconnect_fn p_log_disconnect = NULL;

/* GED query info types (from kernel ged_type.h) */
#define GED_LOADING     0
#define GED_IDLE        1
#define GED_BLOCKING    2
#define GED_PRE_FREQ    3
#define GED_PRE_FREQ_IDX 4
#define GED_CUR_FREQ    5
#define GED_CUR_FREQ_IDX 6
#define GED_MAX_FREQ_IDX 7
#define GED_MIN_FREQ_IDX 9
#define GED_EVENT_STATUS 13

/* GAS categories (from kernel ged_type.h) */
#define GAS_CATEGORY_GAME   0
#define GAS_CATEGORY_OTHERS 1

static void sighandler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ---- GPU monitoring ---- */

static void gas_check_gpu_utilization(void) {
    if (!p_query_info || !g_ged)
        return;

    int loading = 0, idle = 0, blocking = 0;
    p_query_info(g_ged, GED_LOADING, 4, &loading);
    p_query_info(g_ged, GED_IDLE, 4, &idle);
    p_query_info(g_ged, GED_BLOCKING, 4, &blocking);

    int cur_freq = 0;
    p_query_info(g_ged, GED_CUR_FREQ, 4, &cur_freq);

    int event_status = 0;
    p_query_info(g_ged, GED_EVENT_STATUS, 4, &event_status);

    LOGD("GPU: loading=%d%% idle=%d%% blocking=%d%% freq=%d event=0x%x",
         loading, idle, blocking, cur_freq, event_status);
}

/* ---- main ---- */

int main(void) {
    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);

    /* load libged.so */
    void *libged = dlopen("libged.so", RTLD_NOW);
    if (!libged) {
        LOGE("failed to load libged.so: %s", dlerror());
        return 1;
    }

    p_create = (ged_create_fn)dlsym(libged, "ged_create");
    p_destroy = (ged_destroy_fn)dlsym(libged, "ged_destroy");
    p_query_info = (ged_query_info_fn)dlsym(libged, "ged_query_info");
    p_log_connect = (ged_log_connect_fn)dlsym(libged, "ged_log_connect");
    p_log_tpt_print = (ged_log_tpt_print_fn)dlsym(libged, "ged_log_tpt_print");
    p_log_disconnect = (ged_log_disconnect_fn)dlsym(libged, "ged_log_disconnect");

    if (!p_create || !p_destroy || !p_query_info) {
        LOGE("failed to resolve ged symbols");
        dlclose(libged);
        return 1;
    }

    /* connect to ged log */
    if (p_log_connect)
        g_log = p_log_connect("gas_srv_Log");

    /* create ged instance */
    g_ged = p_create();
    if (!g_ged) {
        LOGE("ged_create failed");
        if (g_log && p_log_disconnect) p_log_disconnect(g_log);
        dlclose(libged);
        return 1;
    }

    LOGI("GpuAppSpectatorService started, pid=%d", getpid());

    /* monitoring loop */
    while (g_running) {
        gas_check_gpu_utilization();
        usleep(500000); /* 500ms */
    }

    /* cleanup */
    if (p_destroy)
        p_destroy(g_ged);
    if (g_log) {
        if (p_log_tpt_print) p_log_tpt_print(g_log, NULL);
        if (p_log_disconnect) p_log_disconnect(g_log);
    }

    LOGI("gas_srv exiting");
    dlclose(libged);
    return 0;
}
