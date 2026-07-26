#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

#define TAG "ged_srv"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

typedef void *ged_handle_t;
typedef ged_handle_t (*ged_create_fn)(void);
typedef void (*ged_destroy_fn)(ged_handle_t);
typedef int (*ged_dvfs_probe_fn)(ged_handle_t, int);
typedef int (*ged_dvfs_set_vsync_offset_fn)(int);
typedef void *(*ged_log_connect_fn)(const char *);
typedef void (*ged_log_tpt_print_fn)(void *, void *);

static volatile sig_atomic_t g_running = 1;
static ged_handle_t g_ged;
static void *g_log;
static ged_dvfs_probe_fn g_probe;
static ged_log_tpt_print_fn g_tpt_print;

static void sighandler(int sig __attribute__((unused))) {
    g_running = 0;
}

int main(void) {
    void *libged = dlopen("libged.so", RTLD_NOW);
    if (!libged) {
        LOGE("failed to load libged.so: %s", dlerror());
        return 1;
    }

    ged_create_fn create = (ged_create_fn)dlsym(libged, "ged_create");
    ged_destroy_fn destroy = (ged_destroy_fn)dlsym(libged, "ged_destroy");
    g_probe = (ged_dvfs_probe_fn)dlsym(libged, "ged_dvfs_probe");
    ged_dvfs_set_vsync_offset_fn set_vsync =
        (ged_dvfs_set_vsync_offset_fn)dlsym(libged, "ged_dvfs_set_vsync_offset");
    ged_log_connect_fn log_connect =
        (ged_log_connect_fn)dlsym(libged, "ged_log_connect");
    g_tpt_print = (ged_log_tpt_print_fn)dlsym(libged, "ged_log_tpt_print");

    if (!create || !destroy || !g_probe || !set_vsync || !log_connect) {
        LOGE("failed to resolve ged symbols");
        dlclose(libged);
        return 1;
    }

    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGUSR1, sighandler);

    g_log = log_connect("ged_srv_Log");
    g_ged = create();
    if (!g_ged) {
        LOGE("ged_create failed");
        dlclose(libged);
        return 1;
    }

    set_vsync((int)0xff85ee00);
    g_probe(g_ged, (int)getpid());
    LOGI("ged DVFS active, entering pause loop");

    while (g_running) {
        pause();
        if (!g_running)
            break;
        g_probe(g_ged, -1);
        if (g_log && g_tpt_print)
            g_tpt_print(g_log, NULL);
    }

    g_probe(g_ged, -1);
    destroy(g_ged);
    dlclose(libged);
    return 0;
}
