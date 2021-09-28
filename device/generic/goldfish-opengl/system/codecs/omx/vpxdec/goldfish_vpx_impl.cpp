#include <log/log.h>

#include <linux/types.h>
#include <linux/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <errno.h>
#include "goldfish_vpx_defs.h"
#include "goldfish_media_utils.h"

#include <memory>
#include <mutex>
#include <vector>

#define DEBUG 0
#if DEBUG
#define DDD(...) ALOGD(__VA_ARGS__)
#else
#define DDD(...) ((void)0)
#endif

// static vpx_image_t myImg;
static uint64_t s_CtxId = 0;
static std::mutex sCtxidMutex;

static uint64_t applyForOneId() {
    DDD("%s %d", __func__, __LINE__);
    std::lock_guard<std::mutex> g{sCtxidMutex};
    ++s_CtxId;
    return s_CtxId;
}

static void sendVpxOperation(vpx_codec_ctx_t* ctx, MediaOperation op) {
    DDD("%s %d", __func__, __LINE__);
    if (ctx->memory_slot < 0) {
        ALOGE("ERROR: Failed %s %d: there is no memory slot", __func__,
              __LINE__);
    }
    auto transport = GoldfishMediaTransport::getInstance();
    transport->sendOperation(ctx->vpversion == 9 ? MediaCodecType::VP9Codec
                                                 : MediaCodecType::VP8Codec,
                             op, ctx->address_offset);
}

int vpx_codec_destroy(vpx_codec_ctx_t* ctx) {
    DDD("%s %d", __func__, __LINE__);
    auto transport = GoldfishMediaTransport::getInstance();
    transport->writeParam(ctx->id, 0, ctx->address_offset);
    sendVpxOperation(ctx, MediaOperation::DestroyContext);
    transport->returnMemorySlot(ctx->memory_slot);
    ctx->memory_slot = -1;
    return 0;
}

int vpx_codec_dec_init(vpx_codec_ctx_t* ctx) {
    DDD("%s %d", __func__, __LINE__);
    auto transport = GoldfishMediaTransport::getInstance();
    int slot = transport->getMemorySlot();
    if (slot < 0) {
        ALOGE("ERROR: Failed %s %d: cannot get memory slot", __func__,
              __LINE__);
        return -1;
    } else {
        DDD("got slot %d", slot);
    }
    ctx->id = applyForOneId();
    ctx->memory_slot = slot;
    ctx->address_offset = static_cast<unsigned int>(slot) * 8 * (1 << 20);
    DDD("got address offset 0x%x version %d", (int)(ctx->address_offset),
        ctx->version);

    // data and dst are on the host side actually
    ctx->data = transport->getInputAddr(ctx->address_offset);
    ctx->dst = transport->getInputAddr(
            ctx->address_offset);  // re-use input address
    transport->writeParam(ctx->id, 0, ctx->address_offset);
    transport->writeParam(ctx->version, 1, ctx->address_offset);
    sendVpxOperation(ctx, MediaOperation::InitContext);
    return 0;
}

static int getReturnCode(uint8_t* ptr) {
    int* pint = (int*)(ptr);
    return *pint;
}

// vpx_image_t myImg;
static void getVpxFrame(uint8_t* ptr, vpx_image_t& myImg) {
    DDD("%s %d", __func__, __LINE__);
    uint8_t* imgptr = (ptr + 8);
    myImg.fmt = *(vpx_img_fmt_t*)imgptr;
    imgptr += 8;
    myImg.d_w = *(unsigned int *)imgptr;
    imgptr += 8;
    myImg.d_h = *(unsigned int *)imgptr;
    imgptr += 8;
    myImg.user_priv = (void*)(*(uint64_t*)imgptr);
    DDD("fmt %d dw %d dh %d userpriv %p", (int)myImg.fmt, (int)myImg.d_w,
        (int)myImg.d_h, myImg.user_priv);
}

//TODO: we might not need to do the putting all the time
vpx_image_t* vpx_codec_get_frame(vpx_codec_ctx_t* ctx, int hostColorBufferId) {
    DDD("%s %d %p", __func__, __LINE__);
    auto transport = GoldfishMediaTransport::getInstance();

    transport->writeParam(ctx->id, 0, ctx->address_offset);
    transport->writeParam(ctx->outputBufferWidth, 1, ctx->address_offset);
    transport->writeParam(ctx->outputBufferHeight, 2, ctx->address_offset);
    transport->writeParam(ctx->width, 3, ctx->address_offset);
    transport->writeParam(ctx->height, 4, ctx->address_offset);
    transport->writeParam(ctx->bpp, 5, ctx->address_offset);
    transport->writeParam(ctx->hostColorBufferId, 6, ctx->address_offset);
    transport->writeParam(
            transport->offsetOf((uint64_t)(ctx->dst)) - ctx->address_offset, 7,
            ctx->address_offset);

    sendVpxOperation(ctx, MediaOperation::GetImage);

    auto* retptr = transport->getReturnAddr(ctx->address_offset);
    int ret = getReturnCode(retptr);
    if (ret) {
        return nullptr;
    }
    getVpxFrame(retptr, ctx->myImg);
    return &(ctx->myImg);
}

int vpx_codec_flush(vpx_codec_ctx_t* ctx) {
    DDD("%s %d", __func__, __LINE__);
    auto transport = GoldfishMediaTransport::getInstance();
    transport->writeParam(ctx->id, 0, ctx->address_offset);
    sendVpxOperation(ctx, MediaOperation::Flush);
    return 0;
}

int vpx_codec_decode(vpx_codec_ctx_t *ctx,
                     const uint8_t* data,
                     unsigned int data_sz,
                     void* user_priv,
                     long deadline) {
    DDD("%s %d data size %d userpriv %p", __func__, __LINE__, (int)data_sz,
        user_priv);
    auto transport = GoldfishMediaTransport::getInstance();
    memcpy(ctx->data, data, data_sz);

    transport->writeParam(ctx->id, 0, ctx->address_offset);
    transport->writeParam(
            transport->offsetOf((uint64_t)(ctx->data)) - ctx->address_offset, 1,
            ctx->address_offset);
    transport->writeParam((__u64)data_sz, 2, ctx->address_offset);
    transport->writeParam((__u64)user_priv, 3, ctx->address_offset);
    sendVpxOperation(ctx, MediaOperation::DecodeImage);
    return 0;
}
