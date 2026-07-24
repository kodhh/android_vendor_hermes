#include <dlfcn.h>
#include <string>
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

void _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handlejRKNS_4RectEP13android_ycbcr(buffer_handle_t, uint32_t, const android::Rect&, android_ycbcr*);

void _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handleiRKNS_4RectEP13android_ycbcr(buffer_handle_t handle, int usage, const android::Rect& bounds, android_ycbcr* ycbcr) {
    _ZN7android19GraphicBufferMapper9lockYCbCrEPK13native_handlejRKNS_4RectEP13android_ycbcr(handle, static_cast<uint32_t>(usage), bounds, ycbcr);
}

void _ZN7android13GraphicBuffer4lockEjPPvPiS3_(void* thisptr, uint32_t inUsage,
void** vaddr, int32_t* outBytesPerPixel, int32_t* outBytesPerStride);

void _ZN7android13GraphicBuffer4lockEjPPv(void* thisptr, uint32_t inUsage, void** vaddr) {
    _ZN7android13GraphicBuffer4lockEjPPvPiS3_(thisptr, inUsage, vaddr, nullptr, nullptr);
}

void _ZN7android13GraphicBufferC1Ejjij(void* instance, uint32_t inWidth, uint32_t inHeight, int inFormat, uint32_t inUsage) {
    static auto func = (void (*)(void*, uint32_t, uint32_t, int, uint32_t, std::string))dlsym(RTLD_NEXT,
        "_ZN7android13GraphicBufferC1EjjijNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    if (func) {
        std::string my_requestorName("<Unknown>");
        func(instance, inWidth, inHeight, inFormat, inUsage, my_requestorName);
    }
}

extern void _ZN7android13GraphicBufferC1EPK13native_handleNS0_16HandleWrapMethodEjjijmj(void*, const native_handle_t*,
    android::GraphicBuffer::HandleWrapMethod, uint32_t, uint32_t, android::PixelFormat, uint32_t, uint64_t, uint32_t);

void _ZN7android13GraphicBufferC1EjjijjP13native_handleb(void* instance, uint32_t inWidth, uint32_t inHeight,
    int inFormat, uint32_t inUsage, uint32_t inStride, native_handle_t* inHandle, bool keepOwnership) {
    auto inMethod = keepOwnership ? android::GraphicBuffer::TAKE_HANDLE : android::GraphicBuffer::WRAP_HANDLE;
    _ZN7android13GraphicBufferC1EPK13native_handleNS0_16HandleWrapMethodEjjijmj(
        instance, inHandle, inMethod, inWidth, inHeight, static_cast<android::PixelFormat>(inFormat),
        static_cast<uint32_t>(1), static_cast<uint64_t>(inUsage), inStride);
}
}

}; // namespace android

extern "C" {
#include <netdb.h>
#include <openssl/ssl.h>

extern void _ZN7android11BufferQueue17createBufferQueueEPNS_2spINS_22IGraphicBufferProducerEEEPNS1_INS_22IGraphicBufferConsumerEEERKNS1_INS_19IGraphicBufferAllocEEEb(
    void*, void*, void*, bool);

void _ZN7android11BufferQueue17createBufferQueueEPNS_2spINS_22IGraphicBufferProducerEEEPNS1_INS_22IGraphicBufferConsumerEEEb(
    void* outProducer, void* outConsumer, bool consumerIsSurfaceFlinger)
{
    _ZN7android11BufferQueue17createBufferQueueEPNS_2spINS_22IGraphicBufferProducerEEEPNS1_INS_22IGraphicBufferConsumerEEERKNS1_INS_19IGraphicBufferAllocEEEb(
        outProducer, outConsumer, 0, consumerIsSurfaceFlinger);
}

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
