#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <android/log.h>

#define TAG "gas_srv"

static volatile sig_atomic_t g_running = 1;

static void sighandler(int sig __attribute__((unused))) {
    g_running = 0;
}

int main(void) {
    __android_log_print(ANDROID_LOG_INFO, TAG,
        "GpuAppSpectatorService monitoring disabled "
        "(M-era binary incompatible with Android Q IServiceManager vtable)");

    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);

    while (g_running)
        pause();

    return 0;
}
