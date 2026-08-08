// REMOVED from the build (see Android.bp).
//
// This file used to interpose getaddrinfo/android_getaddrinfofornet and
// delegate to the real bionic implementations via dlsym(RTLD_NEXT).  That
// interposition caused an infinite recursion -> stack overflow in mtk_agpsd
// whenever DNS was actually exercised (i.e. only when networked):
//
//   shim.getaddrinfo -> libc.getaddrinfo
//     -> android_getaddrinfofornet (interposed -> shim)
//     -> libc.android_getaddrinfofornet -> ... -> resolv (res_cache)
//     -> internal getaddrinfo (interposed again -> shim) -> ...
//
// The comment below that original file claimed the dlsym(RTLD_NEXT) delegation
// prevented re-entry, but bionic's resolver calls getaddrinfo internally, so
// the interposed symbols were always re-entered.
//
// Fix: do not interpose getaddrinfo/android_getaddrinfofornet at all.
// mtk_agpsd's getaddrinfo@LIBC resolves straight to bionic's real
// implementation (which is the correct, stock behavior) and the only load-time
// symbols mtk_agpsd still needs, SSLv3_client_method/SSLv3_server_method, are
// provided by libmtk_symbols.cpp.
