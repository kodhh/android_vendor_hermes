// Stub for MTK's proprietary libviagpsrpc.so (VIA GPS / CDMA RPC).
//
// mtk_agpsd dlopens this library at runtime and dlsyms:
//   - get_rpc_interface -> returns the VIA RPC interface struct
//   - viarpc_init       -> one-shot RPC init hook
//   - rpc_get_version   -> version string (c2k_proxy_get_rpc_version)
//
// The stock binary talks to the modem's C2K / VIA GPS block over a socket.
// We don't have the interface struct layout, so get_rpc_interface() returns
// NULL; mtk_agpsd already handles that gracefully (it logs a warning and
// falls back to its built-in cdma_mock_via_rpc_* path, keeping SUPL GPS
// working). To fully enable CDMA/VIA GPS, drop the stock MIUI
// libviagpsrpc.so into /vendor/lib instead of using this stub.

extern "C" {

void* get_rpc_interface(void) {
    return nullptr;
}

int viarpc_init(void) {
    return -1;
}

const char* rpc_get_version(void) {
    return "shim";
}

}
