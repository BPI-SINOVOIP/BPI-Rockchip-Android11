/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RTMetadataRetrieverCallback"

#include <vector>
#include <utils/RefBase.h>
#include <private/media/VideoFrame.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/stagefright/ColorConverter.h>

#include "RTMetadataRetrieverCallback.h"
#include "RTLibDefine.h"
#include "RTMediaMetaKeys.h"

namespace android {

#if 1
#define Debug ALOGV
#else
#define Debug ALOGD
#endif

struct MetaDataCallBackCtx {
    // bit per sample
    int mBpp;
    // input yuv format
    int mSrcFormat;
    // output yuv format
    int mDstFormat;
    // width of input yuv
    int mWidth;
    // height of input yuv
    int mHeight;

    // width of input buffer
    int mWStride;
    // height of input buffer
    int mHStride;
    // rotation of input video frame
    int mRotation;

    std::vector<sp<IMemory> > mFrames;
};

static MetaDataCallBackCtx* getContext(void* context) {
    MetaDataCallBackCtx* ctx = static_cast<MetaDataCallBackCtx*>(context);
    return ctx;
}

static void rtFormatDump(int format) {
    switch (format) {
        case RT_FMT_YUV420SP:
            Debug("%s YUV420 SP", __FUNCTION__);
            break;
        case RT_FMT_YUV420SP_10BIT:
            Debug("%s YUV420 SP 10bit", __FUNCTION__);
            break;
        case RT_FMT_ARGB8888:
            Debug("%s ARGB 8888", __FUNCTION__);
            break;
        case RT_FMT_RGB565:
            Debug("%s RGB 565", __FUNCTION__);
            break;
        default:
            ALOGD("%s: add more here, format = %d", __FUNCTION__, format);
            break;
    }
}

static void omxFormatDump(int format) {
    switch (format) {
        case OMX_COLOR_FormatYUV420SemiPlanar:
            Debug("%s YUV420 SP", __FUNCTION__);
            break;
        case OMX_COLOR_Format32bitARGB8888:
            Debug("%s ARGB 8888", __FUNCTION__);
            break;
        case OMX_COLOR_Format16bitRGB565:
            Debug("%s RGB 565", __FUNCTION__);
            break;
        default:
            ALOGD("%s: add more here, format = %d", __FUNCTION__, format);
            break;
    }
}

RTMetadataRetrieverCallback::RTMetadataRetrieverCallback() {
    MetaDataCallBackCtx* ctx = new MetaDataCallBackCtx();
    assert(ctx != NULL);

    ctx->mBpp = 2;
    ctx->mSrcFormat = RT_FMT_YUV420SP;
    ctx->mDstFormat = OMX_COLOR_Format16bitRGB565;
    ctx->mWidth = RT_FMT_RGB565;
    ctx->mHeight = 0;
    ctx->mRotation = 0;

    mCtx = static_cast<void*>(ctx);
}

RTMetadataRetrieverCallback::~RTMetadataRetrieverCallback() {
    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx != NULL) {
        ctx->mFrames.clear();

        delete ctx;
        ctx = NULL;
    }
}

int RTMetadataRetrieverCallback::init(RtMetaData* meta) {
    if (meta == NULL)
        return -1;

    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx == NULL)
        return -1;

    int width = 0, height = 0;
    int displayWidth = 0, displayHeight = 0;
    int format = 0, rotation = 0;
    int dstFormat = 0;

    // stride of width and height
    if (!meta->findInt32(kKeyFrameW, &width)) {
        ALOGD("%s not find width stride in meta", __FUNCTION__);
    }
    if (!meta->findInt32(kKeyFrameH, &height)) {
        ALOGD("%s not find height stride in meta", __FUNCTION__);
    }

    // valid width and height of datas
    if (!meta->findInt32(kKeyVCodecWidth, &displayWidth)) {
        ALOGD("%s not find width in meta", __FUNCTION__);
    }

    if (!meta->findInt32(kKeyVCodecHeight, &displayHeight)) {
        ALOGD("%s not find height in meta", __FUNCTION__);
    }

    if (!meta->findInt32(kKeyCodecFormat, &format)) {
        format = RT_FMT_YUV420SP;
        ALOGD("%s not find src format in meta, use NV12 for default", __FUNCTION__);
    }

    if (!meta->findInt32(kKeyVCodecRotation, &rotation)) {
        rotation = 0;
    }

    if (!meta->findInt32(kRetrieverDstColorFormat, &dstFormat)) {
        dstFormat = OMX_COLOR_Format16bitRGB565;
        ALOGD("%s not find src format in meta, use RGB56 for default", __FUNCTION__);
    }

    uint32_t bpp = 2; /*rgb565 -  2bytes*/
    if (dstFormat == OMX_COLOR_Format16bitRGB565) {
        bpp = 2;
    } else {
        bpp = 4;
    }

    ctx->mWidth = displayWidth;
    ctx->mHeight = displayHeight;
    ctx->mWStride = width;
    ctx->mHStride = height;
    ctx->mSrcFormat = format;
    ctx->mDstFormat = dstFormat;
    ctx->mBpp = bpp;
    ctx->mRotation = rotation;

    rtFormatDump(ctx->mSrcFormat);
    omxFormatDump(ctx->mDstFormat);
    Debug("%s: Stride(%d x %d), Video(%d x %d), bpp = %d, rotation = %d",
        __FUNCTION__, ctx->mWStride, ctx->mHStride,
        ctx->mWidth, ctx->mHeight, ctx->mBpp, ctx->mRotation);

    return 0;
}

int RTMetadataRetrieverCallback::transfRT2OmxColorFormat(int src) {
    int dst = OMX_COLOR_FormatYUV420SemiPlanar;
    switch (src) {
        case RT_FMT_YUV420SP:
        case RT_FMT_YUV420SP_10BIT: // yuv420sp 10bit will convert to yuv420sp 8bit
            dst = OMX_COLOR_FormatYUV420SemiPlanar;
            break;
        case RT_FMT_RGB565:
            dst = OMX_COLOR_Format16bitRGB565;
            break;
        case RT_FMT_ARGB8888:
            dst = OMX_COLOR_Format32bitARGB8888;
            break;
        default:
            ALOGD("%s: src format = %d not support", __FUNCTION__, src);
            dst = OMX_COLOR_FormatUnused;
            break;
    }

    return (int)dst;
}

int RTMetadataRetrieverCallback::fillVideoFrame(RtMetaData* meta) {
    if (meta == NULL)
        return -1;

    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx == NULL)
        return -1;

    sp<IMemory> frameMem = allocVideoFrame(meta);
    if (frameMem.get() == NULL)
        return -1;

    VideoFrame* frame = static_cast<VideoFrame*>(frameMem->unsecurePointer());
    setFrame(frameMem);

    OMX_COLOR_FORMATTYPE srcFormat = (OMX_COLOR_FORMATTYPE)transfRT2OmxColorFormat(ctx->mSrcFormat);
    OMX_COLOR_FORMATTYPE drcFormat = (OMX_COLOR_FORMATTYPE)ctx->mDstFormat;
    // (OMX_COLOR_FORMATTYPE)transfRT2OmxColorFormat(ctx->mDstFormat);

    if (srcFormat == OMX_COLOR_FormatUnused || (drcFormat == OMX_COLOR_FormatUnused)) {
        return -1;
    }

    ColorConverter converter(srcFormat, drcFormat);

    uint8_t *srcYuvAddr = 0;
    uint8_t *dstYuvAddr = NULL;
    uint8_t *convertAddr = NULL;

    int32_t width = ctx->mWStride;
    int32_t height = ctx->mHStride;
    int32_t stride = ctx->mWStride;

    // find src yuv adress
    if (meta->findPointer(kRetrieverBufferAddress, (void**)&srcYuvAddr) != RT_TRUE) {
        ALOGD("%s: not find srcYuvAddr", __FUNCTION__);
        goto _FAIL;
    }

    int32_t crop_left, crop_top, crop_right, crop_bottom;
    if (1) {
        crop_left = crop_top = 0;
        crop_right = ctx->mWidth - 1;
        crop_bottom = ctx->mHeight - 1;
    }

    if (converter.isValid()) {
        // first. if input format is yuv420sp 10bit, convert it to yuv420sp 8 bit
        if (ctx->mSrcFormat == RT_FMT_YUV420SP_10BIT) {
            dstYuvAddr = (uint8_t *)malloc(ctx->mHStride * ctx->mWStride * 3 / 2);
            if (dstYuvAddr == NULL) {
                ALOGE("fail to malloc yuv address.");
                return -1;
            }
            convert10bitTo8bit(srcYuvAddr, dstYuvAddr);
            stride = width;
        }

        convertAddr = (dstYuvAddr != NULL) ? dstYuvAddr : srcYuvAddr;
        #if 0
        if (mYuvFile) {
            int32_t size = width * height * 3 / 2;
            fwrite(convertAddr, 1, size, mYuvFile);
            fflush(mYuvFile);
            ALOGI("stagefright dump yuv [%d x %d] size %d stride %d",width,height,size,stride);
            fclose(mYuvFile);
            mYuvFile = NULL;
        }
        #endif
        // second. convert src/input's format to dst's format
        converter.convert(
                convertAddr,
                width, height, stride,
                crop_left, crop_top, crop_right, crop_bottom,
                frame->getFlattenedData(),
                frame->mWidth, frame->mHeight, frame->mRowBytes,
                0, 0, frame->mWidth - 1, frame->mHeight - 1);

        if (dstYuvAddr) {
            free(dstYuvAddr);
            dstYuvAddr = NULL;
        }

    }

    return 0;

_FAIL:
    return -1;
}

sp<IMemory> RTMetadataRetrieverCallback::allocVideoFrame(RtMetaData* meta) {
    if (meta == NULL)
        return NULL;

    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx == NULL)
        return NULL;

    int displayWidth = ctx->mWidth;
    int displayHeight = ctx->mHeight;
    int width = ctx->mWidth;
    int height = ctx->mHeight;
    int bpp = ctx->mBpp;
    int rotation = ctx->mRotation;

    uint32_t tileWidth = 0;
    uint32_t tileHeight = 0;

    VideoFrame frame(width, height, displayWidth, displayHeight,
            tileWidth, tileHeight, rotation, bpp, true/*hasData*/, 0/*iccSize*/);

    size_t size = frame.getFlattenedSize();
    sp<MemoryHeapBase> heap = new MemoryHeapBase(size, 0, "RTMetadataRetrieverClient");
    sp<IMemory> frameMem = NULL;
    VideoFrame* frameCopy = NULL;
    if (heap == NULL) {
        ALOGE("failed to create MemoryDealer");
        goto _FAIL;
    }

    frameMem = new MemoryBase(heap, 0, size);
    if (frameMem == NULL) {
        ALOGE("not enough memory for VideoFrame size=%zu", size);
        heap.clear();
        goto _FAIL;
    }

    frameCopy = static_cast<VideoFrame*>(frameMem->unsecurePointer());
    frameCopy->init(frame,NULL,0);

    return frameMem;

_FAIL:
    return NULL;
}

void RTMetadataRetrieverCallback::freeVideoFrame(sp<IMemory> frame)
{
    if (frame.get() != NULL) {
        frame.clear();
    }
}

uint8_t RTMetadataRetrieverCallback::fetch_data(uint8_t *line, uint32_t num) {
    uint32_t offset = 0;
    uint32_t value = 0;

    offset = (num * 2) & 7;
    value = (line[num * 10 / 8] >> offset) | (line[num * 10 / 8 + 1] << (8 - offset));

    value = (value & 0x3ff) >> 2;

    return (uint8_t)value;
}

int RTMetadataRetrieverCallback::convert10bitTo8bit(uint8_t *src, uint8_t *dst) {
    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx == NULL)
        return -1;

    if ((src == NULL) || (dst == NULL))
        return -1;

    int horStride = ctx->mWStride;
    int verStride = ctx->mHStride;
    int width = ctx->mWStride;
    int height = ctx->mHStride;

    uint8_t *srcBase = src;
    uint8_t *dstBase = dst;

    int i = 0;
    int j = 0;

    Debug("%s width = %d height = %d horStride = %d verStride = %d",
        __FUNCTION__, width, height, horStride, verStride);

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++)
            dst[j] = fetch_data(src, j);
        dst += width;
        src += horStride;
    }

    src = srcBase + verStride * horStride;
    dst = dstBase + width * height;

    for (i = 0; i < height / 2; i++) {
        for (j = 0; j < width; j++)
            dst[j] = fetch_data(src, j);
        dst += width;
        src += horStride;
    }

    return 0;
}

sp<IMemory> RTMetadataRetrieverCallback::extractFrame(RTFrameRect *rect) {
    (void)rect;
    MetaDataCallBackCtx* ctx = getContext(mCtx);
    if (ctx == NULL)
        return NULL;

    return ctx->mFrames.size() > 0 ? ctx->mFrames[0] : NULL;
}

sp<IMemory> RTMetadataRetrieverCallback::extractFrames() {

    return mFrameMemory;
}

}
