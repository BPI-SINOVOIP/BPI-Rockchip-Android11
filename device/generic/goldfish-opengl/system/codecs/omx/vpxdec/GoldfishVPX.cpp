/*
 * Copyright (C) 2011 The Android Open Source Project
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
#include <utils/Log.h>
#include <utils/misc.h>
//#include "OMX_VideoExt.h"

#define DEBUG 0
#if DEBUG
#define DDD(...) ALOGD(__VA_ARGS__)
#else
#define DDD(...) ((void)0)
#endif

#include "GoldfishVPX.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>

#include <OMX_VideoExt.h>
#include <inttypes.h>

#include <nativebase/nativebase.h>

#include <android/hardware/graphics/allocator/3.0/IAllocator.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <hidl/LegacySupport.h>

using ::android::hardware::graphics::common::V1_0::BufferUsage;
using ::android::hardware::graphics::common::V1_2::PixelFormat;

namespace android {

// Only need to declare the highest supported profile and level here.
static const CodecProfileLevel kVP9ProfileLevels[] = {
    { OMX_VIDEO_VP9Profile0, OMX_VIDEO_VP9Level5 },
    { OMX_VIDEO_VP9Profile2, OMX_VIDEO_VP9Level5 },
    { OMX_VIDEO_VP9Profile2HDR, OMX_VIDEO_VP9Level5 },
    { OMX_VIDEO_VP9Profile2HDR10Plus, OMX_VIDEO_VP9Level5 },
};

GoldfishVPX::GoldfishVPX(const char* name,
                         const char* componentRole,
                         OMX_VIDEO_CODINGTYPE codingType,
                         const OMX_CALLBACKTYPE* callbacks,
                         OMX_PTR appData,
                         OMX_COMPONENTTYPE** component,
                         RenderMode renderMode)
    : GoldfishVideoDecoderOMXComponent(
              name,
              componentRole,
              codingType,
              codingType == OMX_VIDEO_CodingVP8 ? NULL : kVP9ProfileLevels,
              codingType == OMX_VIDEO_CodingVP8 ? 0 : NELEM(kVP9ProfileLevels),
              320 /* width */,
              240 /* height */,
              callbacks,
              appData,
              component),
      mMode(codingType == OMX_VIDEO_CodingVP8 ? MODE_VP8 : MODE_VP9),
      mRenderMode(renderMode),
      mEOSStatus(INPUT_DATA_AVAILABLE),
      mCtx(NULL),
      mFrameParallelMode(false),
      mTimeStampIdx(0),
      mImg(NULL) {
    // arbitrary from avc/hevc as vpx does not specify a min compression ratio
    const size_t kMinCompressionRatio = mMode == MODE_VP8 ? 2 : 4;
    const char* mime = mMode == MODE_VP8 ? MEDIA_MIMETYPE_VIDEO_VP8
                                         : MEDIA_MIMETYPE_VIDEO_VP9;
    const size_t kMaxOutputBufferSize = 3840 * 2160 * 3 / 2;  // 4k
    initPorts(kNumBuffers,
              kMaxOutputBufferSize / kMinCompressionRatio /* inputBufferSize */,
              kNumBuffers, mime, kMinCompressionRatio);
    ALOGI("calling constructor of GoldfishVPX");
    // wait till later
    // CHECK_EQ(initDecoder(), (status_t)OK);
}

GoldfishVPX::~GoldfishVPX() {
    ALOGI("calling destructor of GoldfishVPX");
    destroyDecoder();
}

bool GoldfishVPX::supportDescribeHdrStaticInfo() {
    return true;
}

bool GoldfishVPX::supportDescribeHdr10PlusInfo() {
    return true;
}

status_t GoldfishVPX::initDecoder() {
    mCtx = new vpx_codec_ctx_t;
    mCtx->vpversion = mMode == MODE_VP8 ? 8 : 9;

    mCtx->version = mEnableAndroidNativeBuffers ? 200 : 100;

    int vpx_err = 0;
    if ((vpx_err = vpx_codec_dec_init(mCtx))) {
        ALOGE("vpx decoder failed to initialize. (%d)", vpx_err);
        return UNKNOWN_ERROR;
    }

    ALOGI("calling init GoldfishVPX ctx %p", mCtx);
    return OK;
}

status_t GoldfishVPX::destroyDecoder() {
    if (mCtx) {
        ALOGI("calling destroying GoldfishVPX ctx %p", mCtx);
        vpx_codec_destroy(mCtx);
        delete mCtx;
        mCtx = NULL;
    }
    return OK;
}

void GoldfishVPX::setup_ctx_parameters(vpx_codec_ctx_t* ctx,
                                       int hostColorBufferId) {
    ctx->width = mWidth;
    ctx->height = mHeight;
    ctx->hostColorBufferId = hostColorBufferId;
    ctx->outputBufferWidth = outputBufferWidth();
    ctx->outputBufferHeight = outputBufferHeight();
    OMX_PARAM_PORTDEFINITIONTYPE *outDef = &editPortInfo(kOutputPortIndex)->mDef;
    int32_t bpp = (outDef->format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar16) ? 2 : 1;
    ctx->bpp =  bpp;
}

bool GoldfishVPX::outputBuffers(bool flushDecoder, bool display, bool eos, bool *portWillReset) {
    List<BufferInfo *> &outQueue = getPortQueue(1);
    BufferInfo *outInfo = NULL;
    OMX_BUFFERHEADERTYPE *outHeader = NULL;
    DDD("%s %d", __func__, __LINE__);

    if (flushDecoder && mFrameParallelMode) {
        // Flush decoder by passing NULL data ptr and 0 size.
        // Ideally, this should never fail.
        if (vpx_codec_flush(mCtx)) {
            ALOGE("Failed to flush on2 decoder.");
            return false;
        }
    }

    if (!display) {
        if (!flushDecoder) {
            ALOGE("Invalid operation.");
            return false;
        }
        // Drop all the decoded frames in decoder.
        // TODO: move this to host, with something like
        // vpx_codec_drop_all_frames(mCtx);
        setup_ctx_parameters(mCtx);
        while ((mImg = vpx_codec_get_frame(mCtx))) {
        }
        return true;
    }

    while (!outQueue.empty()) {
        DDD("%s %d", __func__, __LINE__);
        outInfo = *outQueue.begin();
        outHeader = outInfo->mHeader;
        if (mImg == NULL) {
            setup_ctx_parameters(mCtx, getHostColorBufferId(outHeader));
            mImg = vpx_codec_get_frame(mCtx);
            if (mImg == NULL) {
                break;
            }
        }
        uint32_t width = mImg->d_w;
        uint32_t height = mImg->d_h;
        CHECK(mImg->fmt == VPX_IMG_FMT_I420 || mImg->fmt == VPX_IMG_FMT_I42016);
        OMX_COLOR_FORMATTYPE outputColorFormat = OMX_COLOR_FormatYUV420Planar;
        int32_t bpp = 1;
        if (mImg->fmt == VPX_IMG_FMT_I42016) {
            outputColorFormat = OMX_COLOR_FormatYUV420Planar16;
            bpp = 2;
        }
        handlePortSettingsChange(portWillReset, width, height, outputColorFormat);
        if (*portWillReset) {
            return true;
        }

        outHeader->nOffset = 0;
        outHeader->nFlags = 0;
        outHeader->nFilledLen = (outputBufferWidth() * outputBufferHeight() * bpp * 3) / 2;
        PrivInfo *privInfo = (PrivInfo *)mImg->user_priv;
        outHeader->nTimeStamp = privInfo->mTimeStamp;
        if (privInfo->mHdr10PlusInfo != nullptr) {
            queueOutputFrameConfig(privInfo->mHdr10PlusInfo);
        }

        if (outputBufferSafe(outHeader) &&
            getHostColorBufferId(outHeader) < 0) {
            uint8_t *dst = outHeader->pBuffer;
            memcpy(dst, mCtx->dst, outHeader->nFilledLen);
        } else {
            // outHeader->nFilledLen = 0;
        }

        mImg = NULL;
        outInfo->mOwnedByUs = false;
        outQueue.erase(outQueue.begin());
        outInfo = NULL;
        notifyFillBufferDone(outHeader);
        outHeader = NULL;
    }

    if (!eos) {
        return true;
    }

    if (!outQueue.empty()) {
        outInfo = *outQueue.begin();
        outQueue.erase(outQueue.begin());
        outHeader = outInfo->mHeader;
        outHeader->nTimeStamp = 0;
        outHeader->nFilledLen = 0;
        outHeader->nFlags = OMX_BUFFERFLAG_EOS;
        outInfo->mOwnedByUs = false;
        notifyFillBufferDone(outHeader);
        mEOSStatus = OUTPUT_FRAMES_FLUSHED;
    }
    return true;
}

bool GoldfishVPX::outputBufferSafe(OMX_BUFFERHEADERTYPE *outHeader) {
    DDD("%s %d", __func__, __LINE__);
    uint32_t width = outputBufferWidth();
    uint32_t height = outputBufferHeight();
    uint64_t nFilledLen = width;
    nFilledLen *= height;
    if (nFilledLen > UINT32_MAX / 3) {
        ALOGE("b/29421675, nFilledLen overflow %llu w %u h %u",
                (unsigned long long)nFilledLen, width, height);
        android_errorWriteLog(0x534e4554, "29421675");
        return false;
    } else if (outHeader->nAllocLen < outHeader->nFilledLen) {
        ALOGE("b/27597103, buffer too small");
        android_errorWriteLog(0x534e4554, "27597103");
        return false;
    }

    return true;
}

void GoldfishVPX::onQueueFilled(OMX_U32 /* portIndex */) {
    DDD("%s %d", __func__, __LINE__);
    if (mOutputPortSettingsChange != NONE || mEOSStatus == OUTPUT_FRAMES_FLUSHED) {
        return;
    }

    if (mCtx == nullptr) {
        if (OK != initDecoder()) {
            ALOGE("Failed to initialize decoder");
            notify(OMX_EventError, OMX_ErrorUnsupportedSetting, 0, NULL);
            return;
        }
    }

    List<BufferInfo *> &inQueue = getPortQueue(0);
    List<BufferInfo *> &outQueue = getPortQueue(1);
    bool EOSseen = false;
    bool portWillReset = false;

    while ((mEOSStatus == INPUT_EOS_SEEN || !inQueue.empty())
            && !outQueue.empty()) {
        // Output the pending frames that left from last port reset or decoder flush.
        if (mEOSStatus == INPUT_EOS_SEEN || mImg != NULL) {
            if (!outputBuffers(
                     mEOSStatus == INPUT_EOS_SEEN, true /* display */,
                     mEOSStatus == INPUT_EOS_SEEN, &portWillReset)) {
                ALOGE("on2 decoder failed to output frame.");
                notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                return;
            }
            if (portWillReset || mEOSStatus == OUTPUT_FRAMES_FLUSHED ||
                    mEOSStatus == INPUT_EOS_SEEN) {
                return;
            }
            // Continue as outQueue may be empty now.
            continue;
        }

        BufferInfo *inInfo = *inQueue.begin();
        OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

        // Software VP9 Decoder does not need the Codec Specific Data (CSD)
        // (specified in http://www.webmproject.org/vp9/profiles/). Ignore it if
        // it was passed.
        if (inHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
            // Only ignore CSD buffer for VP9.
            if (mMode == MODE_VP9) {
                inQueue.erase(inQueue.begin());
                inInfo->mOwnedByUs = false;
                notifyEmptyBufferDone(inHeader);
                continue;
            } else {
                // Tolerate the CSD buffer for VP8. This is a workaround
                // for b/28689536.
                ALOGW("WARNING: Got CSD buffer for VP8.");
            }
        }

        mPrivInfo[mTimeStampIdx].mTimeStamp = inHeader->nTimeStamp;

        if (inInfo->mFrameConfig) {
            mPrivInfo[mTimeStampIdx].mHdr10PlusInfo = dequeueInputFrameConfig();
        } else {
            mPrivInfo[mTimeStampIdx].mHdr10PlusInfo.clear();
        }

        if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
            mEOSStatus = INPUT_EOS_SEEN;
            EOSseen = true;
        }

        if (inHeader->nFilledLen > 0) {
            int err = vpx_codec_decode(mCtx, inHeader->pBuffer + inHeader->nOffset,
                    inHeader->nFilledLen, &mPrivInfo[mTimeStampIdx], 0);
            if (err == VPX_CODEC_OK) {
                inInfo->mOwnedByUs = false;
                inQueue.erase(inQueue.begin());
                inInfo = NULL;
                notifyEmptyBufferDone(inHeader);
                inHeader = NULL;
            } else {
                ALOGE("on2 decoder failed to decode frame. err: %d", err);
                notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                return;
            }
        }

        mTimeStampIdx = (mTimeStampIdx + 1) % kNumBuffers;

        if (!outputBuffers(
                 EOSseen /* flushDecoder */, true /* display */, EOSseen, &portWillReset)) {
            ALOGE("on2 decoder failed to output frame.");
            notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            return;
        }
        if (portWillReset) {
            return;
        }
    }
}

void GoldfishVPX::onPortFlushCompleted(OMX_U32 portIndex) {
    DDD("%s %d", __func__, __LINE__);
    if (portIndex == kInputPortIndex) {
        bool portWillReset = false;
        if (!outputBuffers(
                 true /* flushDecoder */, false /* display */, false /* eos */, &portWillReset)) {
            ALOGE("Failed to flush decoder.");
            notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
            return;
        }
        mEOSStatus = INPUT_DATA_AVAILABLE;
    }
}

void GoldfishVPX::onReset() {
    DDD("%s %d", __func__, __LINE__);
    bool portWillReset = false;
    if (!outputBuffers(
             true /* flushDecoder */, false /* display */, false /* eos */, &portWillReset)) {
        ALOGW("Failed to flush decoder. Try to hard reset decoder");
        destroyDecoder();
        initDecoder();
    }
    mEOSStatus = INPUT_DATA_AVAILABLE;
}

int GoldfishVPX::getHostColorBufferId(void* header) {
    DDD("%s %d", __func__, __LINE__);
    if (mNWBuffers.find(header) == mNWBuffers.end()) {
        DDD("cannot find color buffer for header %p", header);
        return -1;
    }
    sp<ANativeWindowBuffer> nBuf = mNWBuffers[header];
    cb_handle_t* handle = (cb_handle_t*)nBuf->handle;
    DDD("found color buffer for header %p --> %d", header, handle->hostHandle);
    return handle->hostHandle;
}

OMX_ERRORTYPE GoldfishVPX::internalGetParameter(OMX_INDEXTYPE index,
                                                OMX_PTR params) {
    const int32_t indexFull = index;
    switch (indexFull) {
        case kGetAndroidNativeBufferUsageIndex: {
            DDD("calling kGetAndroidNativeBufferUsageIndex");
            GetAndroidNativeBufferUsageParams* nativeBuffersUsage =
                    (GetAndroidNativeBufferUsageParams*)params;
            nativeBuffersUsage->nUsage =
                    (unsigned int)(BufferUsage::GPU_DATA_BUFFER);
            return OMX_ErrorNone;
        }

        default:
            return GoldfishVideoDecoderOMXComponent::internalGetParameter(
                    index, params);
    }
}

OMX_ERRORTYPE GoldfishVPX::internalSetParameter(OMX_INDEXTYPE index,
                                                const OMX_PTR params) {
    // Include extension index OMX_INDEXEXTTYPE.
    const int32_t indexFull = index;

    switch (indexFull) {
        case kEnableAndroidNativeBuffersIndex: {
            DDD("calling kEnableAndroidNativeBuffersIndex");
            EnableAndroidNativeBuffersParams* enableNativeBuffers =
                    (EnableAndroidNativeBuffersParams*)params;
            if (enableNativeBuffers) {
                mEnableAndroidNativeBuffers = enableNativeBuffers->enable;
                if (mEnableAndroidNativeBuffers == false) {
                    mNWBuffers.clear();
                    DDD("disabled kEnableAndroidNativeBuffersIndex");
                } else {
                    DDD("enabled kEnableAndroidNativeBuffersIndex");
                }
            }
            return OMX_ErrorNone;
        }

        case kUseAndroidNativeBufferIndex: {
            if (mEnableAndroidNativeBuffers == false) {
                ALOGE("Error: not enabled Android Native Buffers");
                return OMX_ErrorBadParameter;
            }
            UseAndroidNativeBufferParams* use_buffer_params =
                    (UseAndroidNativeBufferParams*)params;
            if (use_buffer_params) {
                sp<ANativeWindowBuffer> nBuf = use_buffer_params->nativeBuffer;
                cb_handle_t* handle = (cb_handle_t*)nBuf->handle;
                void* dst = NULL;
                DDD("kUseAndroidNativeBufferIndex with handle %p host color "
                    "handle %d "
                    "calling usebuffer",
                    handle, handle->hostHandle);
                useBufferCallerLockedAlready(use_buffer_params->bufferHeader,
                                             use_buffer_params->nPortIndex,
                                             use_buffer_params->pAppPrivate,
                                             handle->allocatedSize(),
                                             (OMX_U8*)dst);
                mNWBuffers[*(use_buffer_params->bufferHeader)] =
                        use_buffer_params->nativeBuffer;
                ;
            }
            return OMX_ErrorNone;
        }

        default:
            return GoldfishVideoDecoderOMXComponent::internalSetParameter(
                    index, params);
    }
}

OMX_ERRORTYPE GoldfishVPX::getExtensionIndex(const char* name,
                                             OMX_INDEXTYPE* index) {
    if (mRenderMode == RenderMode::RENDER_BY_HOST_GPU) {
        if (!strcmp(name,
                    "OMX.google.android.index.enableAndroidNativeBuffers")) {
            DDD("calling getExtensionIndex for enable ANB");
            *(int32_t*)index = kEnableAndroidNativeBuffersIndex;
            return OMX_ErrorNone;
        } else if (!strcmp(name,
                           "OMX.google.android.index.useAndroidNativeBuffer")) {
            *(int32_t*)index = kUseAndroidNativeBufferIndex;
            return OMX_ErrorNone;
        } else if (!strcmp(name,
                           "OMX.google.android.index."
                           "getAndroidNativeBufferUsage")) {
            *(int32_t*)index = kGetAndroidNativeBufferUsageIndex;
            return OMX_ErrorNone;
        }
    }
    return GoldfishVideoDecoderOMXComponent::getExtensionIndex(name, index);
}

}  // namespace android

android::GoldfishOMXComponent *createGoldfishOMXComponent(
        const char *name, const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData, OMX_COMPONENTTYPE **component) {
    DDD("%s %d", __func__, __LINE__);
    // only support vp9 to use host hardware decoder, for now
    if (!strncmp("OMX.android.goldfish.vp9.decoder", name, 32)) {
        return new android::GoldfishVPX(
                name, "video_decoder.vp9", OMX_VIDEO_CodingVP9, callbacks,
                appData, component, RenderMode::RENDER_BY_HOST_GPU);
    }
    if (!strncmp("OMX.android.goldfish.vp8.decoder", name, 32)) {
        return new android::GoldfishVPX(
                name, "video_decoder.vp8", OMX_VIDEO_CodingVP8, callbacks,
                appData, component, RenderMode::RENDER_BY_HOST_GPU);
    }
    if (!strncmp("OMX.google.goldfish.vp9.decoder", name, 30)) {
        return new android::GoldfishVPX(
                name, "video_decoder.vp9", OMX_VIDEO_CodingVP9, callbacks,
                appData, component, RenderMode::RENDER_BY_GUEST_CPU);
    }
    if (!strncmp("OMX.google.goldfish.vp8.decoder", name, 30)) {
        return new android::GoldfishVPX(
                name, "video_decoder.vp8", OMX_VIDEO_CodingVP8, callbacks,
                appData, component, RenderMode::RENDER_BY_GUEST_CPU);
    }
    { CHECK(!"Unknown component"); }
    return NULL;
}
