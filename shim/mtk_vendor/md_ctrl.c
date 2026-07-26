#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <android/log.h>

#define TAG "md_ctrl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static int run_muxreport(const char *arg) {
    pid_t pid = fork();
    if (pid < 0) {
        LOGE("fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execl("/vendor/bin/muxreport", "muxreport", arg, (char *)NULL);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;
    LOGE("muxreport %s failed with status %d", arg, WEXITSTATUS(status));
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2 ||
        (strcmp(argv[1], "0") != 0 && strcmp(argv[1], "1") != 0)) {
        fprintf(stderr, "usage: md_ctrl 0,   stop modem\n");
        fprintf(stderr, "       md_ctrl 1,   start modem\n");
        return 1;
    }

    char crypto_state[PROP_VALUE_MAX] = {0};
    char decrypt[PROP_VALUE_MAX] = {0};
    char enc_type[PROP_VALUE_MAX] = {0};

    __system_property_get("ro.crypto.state", crypto_state);
    __system_property_get("vold.decrypt", decrypt);
    __system_property_get("vold.encryption.type", enc_type);

    LOGI("ro.crypto.state=%s, vold.decrypt=%s, vold.encryption.type=%s, start/stop=%c",
         crypto_state, decrypt, enc_type, argv[1][0]);

    if (strcmp(crypto_state, "") == 0 && strcmp(decrypt, "") == 0) {
        LOGI("first boot, setting vold.encryption.type to default");
        __system_property_set("vold.encryption.type", "default");
    }

    char enc_type_val[PROP_VALUE_MAX] = {0};
    __system_property_get("vold.encryption.type", enc_type_val);

    if (strcmp(enc_type_val, "default") == 0) {
        if (strcmp(decrypt, "trigger_restart_min_framework") == 0) {
            LOGI("encryption.type is default, ccci waiting, no need to start/stop modem");
            return 0;
        }
    } else {
        if (strcmp(decrypt, "trigger_restart_framework") == 0) {
            LOGI("encryption.type is NOT default, ccci waiting, no need to start/stop modem");
            return 0;
        }
    }

    int ret = 0;
    if (strcmp(argv[1], "1") == 0) {
        LOGI("starting modem");
        if (run_muxreport("start md1") != 0)
            ret = 1;
        if (run_muxreport("start md2") != 0)
            ret = 1;
    } else {
        LOGI("stopping modem");
        if (run_muxreport("stop md1") != 0)
            ret = 1;
        if (run_muxreport("stop md2") != 0)
            ret = 1;
    }

    return ret;
}
