#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>
#include <stdint.h>

namespace android {

extern "C" {
void _ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPvPiS9_(void* thisptr, buffer_handle_t handle,
uint32_t usage, const Rect& bounds, void** vaddr, int32_t* outBytesPerPixel, int32_t* outBytesPerStride );

void _ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPv(void* thisptr, buffer_handle_t handle,
uint32_t usage, const Rect& bounds,void** vaddr) {
    _ZN7android19GraphicBufferMapper4lockEPK13native_handlejRKNS_4RectEPPvPiS9_(thisptr, handle, usage, bounds, vaddr, nullptr, nullptr);
}

void _ZN7android13GraphicBuffer4lockEjPPvPiS3_(void* thisptr, uint32_t inUsage,
void** vaddr, int32_t* outBytesPerPixel, int32_t* outBytesPerStride);

void _ZN7android13GraphicBuffer4lockEjPPv(void* thisptr, uint32_t inUsage, void** vaddr) {
    _ZN7android13GraphicBuffer4lockEjPPvPiS3_(thisptr, inUsage, vaddr, nullptr, nullptr);
}
}

}; // namespace android

extern "C" {
#include <netdb.h>
#include <openssl/ssl.h>

const SSL_METHOD* SSLv3_client_method(void) {
    return SSLv23_client_method();
}

const SSL_METHOD* SSLv3_server_method(void) {
    return SSLv23_server_method();
}

int android_getaddrinfofornet(const char* hostname, const char* servname,
        const struct addrinfo* hints, unsigned netid, unsigned mark,
        struct addrinfo** res) {
    return getaddrinfo(hostname, servname, hints, res);
}
}
