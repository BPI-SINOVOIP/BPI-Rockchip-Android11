/*
 * Copyright 2015 The Android Open Source Project
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

#include "GoldfishAVCDec.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <OMX_VideoExt.h>
#include <inttypes.h>

#include <nativebase/nativebase.h>

#include <android/hardware/graphics/allocator/3.0/IAllocator.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <hidl/LegacySupport.h>

using ::android::hardware::graphics::common::V1_2::PixelFormat;
using ::android::hardware::graphics::common::V1_0::BufferUsage;

namespace android {

#define componentName                   "video_decoder.avc"
#define codingType                      OMX_VIDEO_CodingAVC
#define CODEC_MIME_TYPE                 MEDIA_MIMETYPE_VIDEO_AVC

/** Function and structure definitions to keep code similar for each codec */
#define ivdec_api_function              ih264d_api_function
#define ivdext_create_ip_t              ih264d_create_ip_t
#define ivdext_create_op_t              ih264d_create_op_t
#define ivdext_delete_ip_t              ih264d_delete_ip_t
#define ivdext_delete_op_t              ih264d_delete_op_t
#define ivdext_ctl_set_num_cores_ip_t   ih264d_ctl_set_num_cores_ip_t
#define ivdext_ctl_set_num_cores_op_t   ih264d_ctl_set_num_cores_op_t

#define IVDEXT_CMD_CTL_SET_NUM_CORES    \
        (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_SET_NUM_CORES

static const CodecProfileLevel kProfileLevels[] = {
    { OMX_VIDEO_AVCProfileConstrainedBaseline, OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileMain,     OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileConstrainedHigh,     OMX_VIDEO_AVCLevel52 },

    { OMX_VIDEO_AVCProfileHigh,     OMX_VIDEO_AVCLevel52 },
};

GoldfishAVCDec::GoldfishAVCDec(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component, RenderMode renderMode)
    : GoldfishVideoDecoderOMXComponent(
            name, componentName, codingType,
            kProfileLevels, ARRAY_SIZE(kProfileLevels),
            320 /* width */, 240 /* height */, callbacks,
            appData, component),
      mOmxColorFormat(OMX_COLOR_FormatYUV420Planar),
      mChangingResolution(false),
      mSignalledError(false),
      mInputOffset(0), mRenderMode(renderMode){
    initPorts(
            1 /* numMinInputBuffers */, kNumBuffers, INPUT_BUF_SIZE,
            1 /* numMinOutputBuffers */, kNumBuffers, CODEC_MIME_TYPE);

    mTimeStart = mTimeEnd = systemTime();

    // If input dump is enabled, then open create an empty file
    GENERATE_FILE_NAMES();
    CREATE_DUMP_FILE(mInFile);
    ALOGD("created %s %d object %p", __func__, __LINE__, this);
}

GoldfishAVCDec::~GoldfishAVCDec() {
    CHECK_EQ(deInitDecoder(), (status_t)OK);
    ALOGD("destroyed %s %d object %p", __func__, __LINE__, this);
}

void GoldfishAVCDec::logVersion() {
    // TODO: get emulation decoder implementation version from the host.
    ALOGV("GoldfishAVC decoder version 1.0");
}

status_t GoldfishAVCDec::resetPlugin() {
    mIsInFlush = false;
    mReceivedEOS = false;

    /* Initialize both start and end times */
    mTimeStart = mTimeEnd = systemTime();

    return OK;
}

status_t GoldfishAVCDec::resetDecoder() {
    if (mContext) {
    // The resolution may have changed, so our safest bet is to just destroy the
    // current context and recreate another one, with the new width and height.
    mContext->destroyH264Context();
    mContext.reset(nullptr);

    }
    return OK;
}

status_t GoldfishAVCDec::setFlushMode() {
    /* Set the decoder in Flush mode, subsequent decode() calls will flush */
    mIsInFlush = true;
    mContext->flush();
    return OK;
}

status_t GoldfishAVCDec::initDecoder() {
    /* Initialize the decoder */
    mContext.reset(new MediaH264Decoder(mRenderMode));
    mContext->initH264Context(mWidth,
                              mHeight,
                              mWidth,
                              mHeight,
                              MediaH264Decoder::PixelFormat::YUV420P);

    /* Reset the plugin state */
    resetPlugin();

    /* Get codec version */
    logVersion();

    return OK;
}

status_t GoldfishAVCDec::deInitDecoder() {
    if (mContext) {
        mContext->destroyH264Context();
        mContext.reset();
    }

    mChangingResolution = false;

    return OK;
}

void GoldfishAVCDec::onReset() {
    GoldfishVideoDecoderOMXComponent::onReset();

    mSignalledError = false;
    mInputOffset = 0;
    resetDecoder();
    resetPlugin();
}

bool GoldfishAVCDec::getVUIParams(h264_image_t& img) {
    int32_t primaries = img.color_primaries;
    bool fullRange = img.color_range == 2 ? true : false;
    int32_t transfer = img.color_trc;
    int32_t coeffs = img.colorspace;

    ColorAspects colorAspects;
    ColorUtils::convertIsoColorAspectsToCodecAspects(
            primaries, transfer, coeffs, fullRange, colorAspects);

    ALOGD("img pts %lld, primaries %d, range %d transfer %d colorspace %d", (long long)img.pts,
            (int)img.color_primaries, (int)img.color_range, (int)img.color_trc, (int)img.colorspace);

    // Update color aspects if necessary.
    if (colorAspectsDiffer(colorAspects, mBitstreamColorAspects)) {
        mBitstreamColorAspects = colorAspects;
        status_t err = handleColorAspectsChange();
        CHECK(err == OK);
    }
    return true;
}

bool GoldfishAVCDec::setDecodeArgs(
        OMX_BUFFERHEADERTYPE *inHeader,
        OMX_BUFFERHEADERTYPE *outHeader) {
    size_t sizeY = outputBufferWidth() * outputBufferHeight();
    size_t sizeUV = sizeY / 4;

    /* When in flush and after EOS with zero byte input,
     * inHeader is set to zero. Hence check for non-null */
    if (inHeader) {
        mConsumedBytes = inHeader->nFilledLen - mInputOffset;
        mInPBuffer = inHeader->pBuffer + inHeader->nOffset + mInputOffset;
        ALOGD("got input timestamp %lld in-addr-base %p real-data-offset %d inputoffset %d", (long long)(inHeader->nTimeStamp),
                inHeader->pBuffer, (int)(inHeader->nOffset + mInputOffset), (int)mInputOffset);
    } else {
        mConsumedBytes = 0;
        mInPBuffer = nullptr;
    }

    if (outHeader) {
        if (outHeader->nAllocLen < sizeY + (sizeUV * 2)) {
            ALOGE("outHeader->nAllocLen %d < needed size %d", outHeader->nAllocLen, (int)(sizeY + sizeUV * 2));
            android_errorWriteLog(0x534e4554, "27833616");
            return false;
        }
        mOutHeaderBuf = outHeader->pBuffer;
    } else {
        // We flush out on the host side
        mOutHeaderBuf = nullptr;
    }

    return true;
}

void GoldfishAVCDec::readAndDiscardAllHostBuffers() {
    while (mContext) {
        h264_image_t img = mContext->getImage();
        if (img.data != nullptr) {
            ALOGD("img pts %lld is discarded", (long long)img.pts);
        } else {
            return;
        }
    }
}

void GoldfishAVCDec::onPortFlushCompleted(OMX_U32 portIndex) {
    /* Once the output buffers are flushed, ignore any buffers that are held in decoder */
    if (kOutputPortIndex == portIndex) {
        setFlushMode();
        ALOGD("%s %d", __func__, __LINE__);
        readAndDiscardAllHostBuffers();
        mContext->resetH264Context(mWidth, mHeight, mWidth, mHeight, MediaH264Decoder::PixelFormat::YUV420P);
        if (!mCsd0.empty() && !mCsd1.empty()) {
            mContext->decodeFrame(&(mCsd0[0]), mCsd0.size(), 0);
            mContext->getImage();
            mContext->decodeFrame(&(mCsd1[0]), mCsd1.size(), 0);
            mContext->getImage();
        }
        resetPlugin();
    } else {
        mInputOffset = 0;
    }
}

void GoldfishAVCDec::copyImageData( OMX_BUFFERHEADERTYPE *outHeader, h264_image_t & img) {
    int myStride = outputBufferWidth();
    for (int i=0; i < mHeight; ++i) {
        memcpy(outHeader->pBuffer + i * myStride, img.data + i * mWidth, mWidth);
    }
    int Y = myStride * outputBufferHeight();
    for (int i=0; i < mHeight/2; ++i) {
        memcpy(outHeader->pBuffer + Y + i * myStride / 2 , img.data + mWidth * mHeight + i * mWidth/2, mWidth/2);
    }
    int UV = Y/4;
    for (int i=0; i < mHeight/2; ++i) {
        memcpy(outHeader->pBuffer + Y + UV + i * myStride / 2 , img.data + mWidth * mHeight * 5/4 + i * mWidth/2, mWidth/2);
    }
}

int GoldfishAVCDec::getHostColorBufferId(void* header) {
  if (mNWBuffers.find(header) == mNWBuffers.end()) {
      ALOGD("cannot find color buffer for header %p", header);
    return -1;
  }
  sp<ANativeWindowBuffer> nBuf = mNWBuffers[header];
  cb_handle_t *handle = (cb_handle_t*)nBuf->handle;
  ALOGD("found color buffer for header %p --> %d", header, handle->hostHandle);
  return handle->hostHandle;
}

void GoldfishAVCDec::onQueueFilled(OMX_U32 portIndex) {
    static int count1=0;
    ALOGD("calling %s count %d object %p", __func__, ++count1, this);
    UNUSED(portIndex);
    OMX_BUFFERHEADERTYPE *inHeader = NULL;
    BufferInfo *inInfo = NULL;

    if (mSignalledError) {
        return;
    }
    if (mOutputPortSettingsChange != NONE) {
        return;
    }

    if (mContext == nullptr) {
        if (OK != initDecoder()) {
            ALOGE("Failed to initialize decoder");
            notify(OMX_EventError, OMX_ErrorUnsupportedSetting, 0, NULL);
            mSignalledError = true;
            return;
        }
    }

    List<BufferInfo *> &inQueue = getPortQueue(kInputPortIndex);
    List<BufferInfo *> &outQueue = getPortQueue(kOutputPortIndex);

    int count2=0;
    while (!outQueue.empty()) {
        ALOGD("calling %s in while loop count %d", __func__, ++count2);
        BufferInfo *outInfo;
        OMX_BUFFERHEADERTYPE *outHeader;

        if (!mIsInFlush && (NULL == inHeader)) {
            if (!inQueue.empty()) {
                inInfo = *inQueue.begin();
                inHeader = inInfo->mHeader;
                if (inHeader == NULL) {
                    inQueue.erase(inQueue.begin());
                    inInfo->mOwnedByUs = false;
                    continue;
                }
            } else {
                break;
            }
        }

        outInfo = *outQueue.begin();
        outHeader = outInfo->mHeader;
        outHeader->nFlags = 0;
        outHeader->nTimeStamp = 0;
        outHeader->nOffset = 0;

        if (inHeader != NULL) {
            if (inHeader->nFilledLen == 0) {
                // An empty buffer can be end of stream (EOS) buffer, so
                // we'll set the decoder in flush mode if so. If it's not EOS,
                // then just release the buffer.
                inQueue.erase(inQueue.begin());
                inInfo->mOwnedByUs = false;
                notifyEmptyBufferDone(inHeader);

                if (!(inHeader->nFlags & OMX_BUFFERFLAG_EOS)) {
                    return;
                }

                mReceivedEOS = true;
                inHeader = NULL;
                setFlushMode();
            } else if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
                mReceivedEOS = true;
            }
        }

        {
            nsecs_t timeDelay, timeTaken;

            if (!setDecodeArgs(inHeader, outHeader)) {
                ALOGE("Decoder arg setup failed");
                notify(OMX_EventError, OMX_ErrorUndefined, 0, NULL);
                mSignalledError = true;
                return;
            }

            mTimeStart = systemTime();
            /* Compute time elapsed between end of previous decode()
             * to start of current decode() */
            timeDelay = mTimeStart - mTimeEnd;

            // TODO: We also need to send the timestamp
            h264_result_t h264Res = {(int)MediaH264Decoder::Err::NoErr, 0};
            if (inHeader != nullptr) {
                if (inHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG) { 
                    unsigned long mysize = (inHeader->nFilledLen - mInputOffset);
                    uint8_t* mydata = mInPBuffer;
                    if (mCsd0.empty()) {
                        mCsd0.assign(mydata, mydata + mysize);
                    } else if (mCsd1.empty()) {
                        mCsd1.assign(mydata, mydata + mysize);
                    }
                }
                ALOGD("Decoding frame(sz=%lu)", (unsigned long)(inHeader->nFilledLen - mInputOffset));
                h264Res = mContext->decodeFrame(mInPBuffer,
                                                inHeader->nFilledLen - mInputOffset,
                                                inHeader->nTimeStamp);
                mConsumedBytes = h264Res.bytesProcessed;
                if (h264Res.ret == (int)MediaH264Decoder::Err::DecoderRestarted) {
                    mChangingResolution = true;
                }
            } else {
                ALOGD("No more input data. Attempting to get a decoded frame, if any.");
            }
            h264_image_t img = {};

            bool readBackPixels = true;
            if (mRenderMode == RenderMode::RENDER_BY_GUEST_CPU) {
              img = mContext->getImage();
            } else {
                int hostColorBufferId = getHostColorBufferId(outHeader);
                if (hostColorBufferId >= 0) {
                    img = mContext->renderOnHostAndReturnImageMetadata(getHostColorBufferId(outHeader));
                    readBackPixels = false;
                } else {
                    img = mContext->getImage();
                }
            }


            if (img.data != nullptr) {
                getVUIParams(img);
            }

            mTimeEnd = systemTime();
            /* Compute time taken for decode() */
            timeTaken = mTimeEnd - mTimeStart;


            if (inHeader) {
                ALOGD("input time stamp %lld flag %d", inHeader->nTimeStamp, (int)(inHeader->nFlags));
            }

            // If the decoder is in the changing resolution mode and there is no output present,
            // that means the switching is done and it's ready to reset the decoder and the plugin.
            if (mChangingResolution && img.data == nullptr) {
                mChangingResolution = false;
                ALOGD("re-create decoder because resolution changed");
                bool portWillReset = false;
                handlePortSettingsChange(&portWillReset, img.width, img.height);
                {
                    ALOGD("handling port reset");
                    ALOGD("port resetting (img.width=%u, img.height=%u, mWidth=%u, mHeight=%u)",
                          img.width, img.height, mWidth, mHeight);
                    //resetDecoder();
                    resetPlugin();

                //mContext->destroyH264Context();
                //mContext.reset(new MediaH264Decoder());
                mContext->resetH264Context(mWidth,
                              mHeight,
                              mWidth,
                              mHeight,
                              MediaH264Decoder::PixelFormat::YUV420P);
                //mInputOffset += mConsumedBytes;
                return;
                }
            }

            if (img.data != nullptr) {
                int myWidth = img.width;
                int myHeight = img.height;
                if (myWidth != mWidth || myHeight != mHeight) {
                    bool portWillReset = false;
                    handlePortSettingsChange(&portWillReset, myWidth, myHeight);
                    resetPlugin();
                    mWidth = myWidth;
                    mHeight = myHeight;
                    if (portWillReset) {
                        ALOGD("port will reset return now");
                        return;
                    } else {
                        ALOGD("port will NOT reset keep going now");
                    }
                }
                outHeader->nFilledLen =  (outputBufferWidth() * outputBufferHeight() * 3) / 2;
                if (readBackPixels) {
                  if (outputBufferWidth() == mWidth && outputBufferHeight() == mHeight) {
                    memcpy(outHeader->pBuffer, img.data, outHeader->nFilledLen);
                  } else {
                    copyImageData(outHeader, img);
                  }
                }

                outHeader->nTimeStamp = img.pts;
                ALOGD("got output timestamp %lld", (long long)(img.pts));

                outInfo->mOwnedByUs = false;
                outQueue.erase(outQueue.begin());
                outInfo = NULL;
                notifyFillBufferDone(outHeader);
                outHeader = NULL;
            } else if (mIsInFlush) {
                ALOGD("not img.data and it is in flush mode");
                /* If in flush mode and no output is returned by the codec,
                 * then come out of flush mode */
                mIsInFlush = false;

                /* If EOS was recieved on input port and there is no output
                 * from the codec, then signal EOS on output port */
                if (mReceivedEOS) {
                    ALOGD("recived EOS, re-create host context");
                    outHeader->nFilledLen = 0;
                    outHeader->nFlags |= OMX_BUFFERFLAG_EOS;

                    outInfo->mOwnedByUs = false;
                    outQueue.erase(outQueue.begin());
                    outInfo = NULL;
                    notifyFillBufferDone(outHeader);
                    outHeader = NULL;
                    resetPlugin();

                    //mContext->destroyH264Context();
                //mContext.reset(new MediaH264Decoder());
                    mContext->resetH264Context(mWidth,
                              mHeight,
                              mWidth,
                              mHeight,
                              MediaH264Decoder::PixelFormat::YUV420P);

                }
            }
            mInputOffset += mConsumedBytes;
        }

        // If more than 4 bytes are remaining in input, then do not release it
        if (inHeader != NULL && ((inHeader->nFilledLen - mInputOffset) <= 4)) {
            inInfo->mOwnedByUs = false;
            inQueue.erase(inQueue.begin());
            inInfo = NULL;
            notifyEmptyBufferDone(inHeader);
            inHeader = NULL;
            mInputOffset = 0;

            /* If input EOS is seen and decoder is not in flush mode,
             * set the decoder in flush mode.
             * There can be a case where EOS is sent along with last picture data
             * In that case, only after decoding that input data, decoder has to be
             * put in flush. This case is handled here  */

            if (mReceivedEOS && !mIsInFlush) {
                setFlushMode();
            }
        }
    }
}

OMX_ERRORTYPE GoldfishAVCDec::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    const int32_t indexFull = index;
    switch (indexFull) {
        case kGetAndroidNativeBufferUsageIndex:
        {
            ALOGD("calling kGetAndroidNativeBufferUsageIndex");
            GetAndroidNativeBufferUsageParams* nativeBuffersUsage = (GetAndroidNativeBufferUsageParams *) params;
            nativeBuffersUsage->nUsage = (unsigned int)(BufferUsage::GPU_DATA_BUFFER);
            return OMX_ErrorNone;
        }

        default:
            return GoldfishVideoDecoderOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE GoldfishAVCDec::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    // Include extension index OMX_INDEXEXTTYPE.
    const int32_t indexFull = index;

    switch (indexFull) {
        case kEnableAndroidNativeBuffersIndex:
        {
            ALOGD("calling kEnableAndroidNativeBuffersIndex");
            EnableAndroidNativeBuffersParams* enableNativeBuffers = (EnableAndroidNativeBuffersParams *) params;
            if (enableNativeBuffers) {
                mEnableAndroidNativeBuffers = enableNativeBuffers->enable;
                if (mEnableAndroidNativeBuffers == false) {
                    mNWBuffers.clear();
                    ALOGD("disabled kEnableAndroidNativeBuffersIndex");
                } else {
                    ALOGD("enabled kEnableAndroidNativeBuffersIndex");
                }
            }
            return OMX_ErrorNone;
        }

        case kUseAndroidNativeBufferIndex:
        {
            if (mEnableAndroidNativeBuffers == false) {
                ALOGE("Error: not enabled Android Native Buffers");
                return OMX_ErrorBadParameter;
            }
            UseAndroidNativeBufferParams *use_buffer_params = (UseAndroidNativeBufferParams *)params;
            if (use_buffer_params) {
                sp<ANativeWindowBuffer> nBuf = use_buffer_params->nativeBuffer;
                cb_handle_t *handle = (cb_handle_t*)nBuf->handle;
                void* dst = NULL;
                ALOGD("kUseAndroidNativeBufferIndex with handle %p host color handle %d calling usebuffer", handle,
                      handle->hostHandle);
                useBufferCallerLockedAlready(use_buffer_params->bufferHeader,use_buffer_params->nPortIndex,
                        use_buffer_params->pAppPrivate,handle->allocatedSize(), (OMX_U8*)dst);
                mNWBuffers[*(use_buffer_params->bufferHeader)] = use_buffer_params->nativeBuffer;;
            }
            return OMX_ErrorNone;
        }

        default:
            return GoldfishVideoDecoderOMXComponent::internalSetParameter(index, params);
    }
}

OMX_ERRORTYPE GoldfishAVCDec::getExtensionIndex(
        const char *name, OMX_INDEXTYPE *index) {

    if (mRenderMode == RenderMode::RENDER_BY_HOST_GPU) {
        if (!strcmp(name, "OMX.google.android.index.enableAndroidNativeBuffers")) {
            ALOGD("calling getExtensionIndex for enable ANB");
            *(int32_t*)index = kEnableAndroidNativeBuffersIndex;
            return OMX_ErrorNone;
        } else if (!strcmp(name, "OMX.google.android.index.useAndroidNativeBuffer")) {
            *(int32_t*)index = kUseAndroidNativeBufferIndex;
            return OMX_ErrorNone;
        } else if (!strcmp(name, "OMX.google.android.index.getAndroidNativeBufferUsage")) {
            *(int32_t*)index = kGetAndroidNativeBufferUsageIndex;
            return OMX_ErrorNone;
        }
    }
    return GoldfishVideoDecoderOMXComponent::getExtensionIndex(name, index);
}

int GoldfishAVCDec::getColorAspectPreference() {
    return kPreferBitstream;
}

}  // namespace android

android::GoldfishOMXComponent *createGoldfishOMXComponent(
        const char *name, const OMX_CALLBACKTYPE *callbacks, OMX_PTR appData,
        OMX_COMPONENTTYPE **component) {
    if (!strncmp("OMX.android.goldfish", name, 20)) {
      return new android::GoldfishAVCDec(name, callbacks, appData, component, RenderMode::RENDER_BY_HOST_GPU);
    } else {
      return new android::GoldfishAVCDec(name, callbacks, appData, component, RenderMode::RENDER_BY_GUEST_CPU);
    }
}

