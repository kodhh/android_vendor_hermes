#define LOG_TAG "PerfServiceNative"
#include <log/log.h>

#include <utils/String16.h>

// Stub replacement for the prebuilt hermes libperfservicenative.so.
//
// The original library was built for Android 9 and talks to the perfservice
// daemon over binder. Android 10 removed that daemon, and the old binder
// code corrupts the caller's stack frame (outdated Parcel layout) which
// crashes mediaserver in CpuCtrlImp::enable() during camera preview start.
//
// The LeEco Le X3 (x3, same MT6795) ships a libperfservicenative that is a
// pure stub: every exported function only logs and returns success. We do
// the same here, built from source for Android 10.

static inline int perf_stub(const char* fn) {
    ALOGD("%s: stub, no-op", fn);
    return 0;
}

extern "C" {

int PerfServiceNative_userReg(int tid) {
    return perf_stub("userReg");
}

int PerfServiceNative_userUnreg(int handle) {
    return perf_stub("userUnreg");
}

int PerfServiceNative_userEnable(int handle) {
    return perf_stub("userEnable");
}

int PerfServiceNative_userDisable(int handle) {
    return perf_stub("userDisable");
}

int PerfServiceNative_userResetAll() {
    return perf_stub("userResetAll");
}

int PerfServiceNative_userDisableAll() {
    return perf_stub("userDisableAll");
}

int PerfServiceNative_userEnableTimeout(int handle) {
    return perf_stub("userEnableTimeout");
}

int PerfServiceNative_userEnableTimeoutMs(int handle, int timeout_ms) {
    return perf_stub("userEnableTimeoutMs");
}

int PerfServiceNative_userRegScn(int scn_core, int scn_cpu, int scn_freq) {
    return perf_stub("userRegScn");
}

int PerfServiceNative_userUnregScn(int handle) {
    return perf_stub("userUnregScn");
}

int PerfServiceNative_userRegScnConfig(int handle, int cmd, int param_1,
                                       int param_2, int param_3) {
    return perf_stub("userRegScnConfig");
}

int PerfServiceNative_userRegBigLittle(int big_core, int big_freq,
                                       int little_core, int little_freq,
                                       int timeout_ms, char* user_name,
                                       int enable) {
    return perf_stub("userRegBigLittle");
}

int PerfServiceNative_userGetCapability(int cmd) {
    return perf_stub("userGetCapability");
}

int PerfServiceNative_boostEnable(int scenario) {
    return perf_stub("boostEnable");
}

int PerfServiceNative_boostEnableAsync(int scenario) {
    return perf_stub("boostEnableAsync");
}

int PerfServiceNative_boostDisable(int scenario) {
    return perf_stub("boostDisable");
}

int PerfServiceNative_boostDisableAsync(int scenario) {
    return perf_stub("boostDisableAsync");
}

int PerfServiceNative_boostEnableTimeout(int scenario, int timeout_ms) {
    return perf_stub("boostEnableTimeout");
}

int PerfServiceNative_boostEnableTimeoutAsync(int scenario, int timeout_ms) {
    return perf_stub("boostEnableTimeoutAsync");
}

int PerfServiceNative_boostEnableTimeoutMs(int scenario, int timeout_ms) {
    return perf_stub("boostEnableTimeoutMs");
}

int PerfServiceNative_boostEnableTimeoutMsAsync(int scenario, int timeout_ms) {
    return perf_stub("boostEnableTimeoutMsAsync");
}

int PerfServiceNative_notifyUserStatus(int type, int status) {
    return perf_stub("notifyUserStatus");
}

int PerfServiceNative_notifyFrameUpdate(int frame_num) {
    return perf_stub("notifyFrameUpdate");
}

int PerfServiceNative_notifyDisplayType(int display_type) {
    return perf_stub("notifyDisplayType");
}

int PerfServiceNative_restoreBoostThread() {
    return perf_stub("restoreBoostThread");
}

int PerfServiceNative_setBoostThread(int tid, int priority) {
    return perf_stub("setBoostThread");
}

int PerfServiceNative_setFavorPid(int pid) {
    return perf_stub("setFavorPid");
}

int PerfServiceNative_getLastBoostPid() {
    return perf_stub("getLastBoostPid");
}

void PerfServiceNative_dumpAll() {
    ALOGD("dumpAll: stub, no-op");
}

}  // extern "C"

// getPackName() returns android::String16, which is incompatible with C
// linkage (-Werror,-Wreturn-type-c-linkage), so it must be defined outside
// the extern "C" block. The asm() label on the declaration keeps the
// unmangled symbol name that the prebuilt library exported for dlsym()
// callers.
android::String16 PerfServiceNative_getPackName(int pid) __asm__("PerfServiceNative_getPackName");

android::String16 PerfServiceNative_getPackName(int pid) {
    ALOGD("getPackName(%d): stub, empty", pid);
    return android::String16();
}
