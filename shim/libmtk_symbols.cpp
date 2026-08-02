#include <dlfcn.h>
#include <string>
#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>
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

void _ZN7android5Fence4waitEi(int);

void _ZN7android5Fence4waitEj(unsigned int timeout) {
    _ZN7android5Fence4waitEi(static_cast<int>(timeout));
}

}

}; // namespace android

extern "C" {
#include <netdb.h>
#include <openssl/ssl.h>

void jpeg_std_error_MTK() {}
void jpeg_CreateCompress_MTK() {}
void jpeg_CreateDecompress_MTK() {}
void jpeg_destroy_compress_MTK() {}
void jpeg_destroy_decompress_MTK() {}
void jpeg_set_defaults_MTK() {}
void jpeg_set_quality_MTK() {}
void jpeg_start_compress_MTK() {}
void jpeg_finish_compress_MTK() {}
void jpeg_read_header_MTK() {}
void jpeg_read_scanlines_MTK() {}
void jpeg_start_decompress_MTK() {}
void jpeg_finish_decompress_MTK() {}
void jpeg_write_raw_data_MTK() {}
void jpeg_mem_dest_MTK() {}

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
