// ARM EABI saturating float/double to long long conversions
// 32-bit only: AArch64 does not use __aeabi_* symbols
// Needed by libGLESv2_mtk.so, liboclcompiler.so, libmnl.so

#include <stdint.h>

extern "C" {

int64_t __aeabi_f2lz(float f) {
    if (!__builtin_isfinite(f)) return 0;
    if (f >= 9223372036854775808.0f) return INT64_MAX;
    if (f < -9223372036854775808.0f) return INT64_MIN;
    return (int64_t)f;
}

int64_t __aeabi_d2lz(double d) {
    if (!__builtin_isfinite(d)) return 0;
    if (d >= 9223372036854775808.0) return INT64_MAX;
    if (d < -9223372036854775808.0) return INT64_MIN;
    return (int64_t)d;
}

uint64_t __aeabi_f2ulz(float f) {
    if (!__builtin_isfinite(f)) return 0;
    if (f >= 18446744073709551616.0f) return UINT64_MAX;
    if (f < 0.0f) return 0;
    return (uint64_t)f;
}

uint64_t __aeabi_d2ulz(double d) {
    if (!__builtin_isfinite(d)) return 0;
    if (d >= 18446744073709551616.0) return UINT64_MAX;
    if (d < 0.0) return 0;
    return (uint64_t)d;
}

}
