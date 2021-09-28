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

#ifndef GOLDFISH_VPX_H_

#define GOLDFISH_VPX_H_

#include "GoldfishVideoDecoderOMXComponent.h"
#include "goldfish_vpx_defs.h"

#include <sys/time.h>

#include <map>
#include <vector>

#include <ui/GraphicBuffer.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>
#include <utils/threads.h>
#include <gralloc_cb_bp.h>

namespace android {

struct ABuffer;

struct GoldfishVPX : public GoldfishVideoDecoderOMXComponent {
    GoldfishVPX(const char* name,
                const char* componentRole,
                OMX_VIDEO_CODINGTYPE codingType,
                const OMX_CALLBACKTYPE* callbacks,
                OMX_PTR appData,
                OMX_COMPONENTTYPE** component,
                RenderMode renderMode);

protected:
    virtual ~GoldfishVPX();

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onReset();
    virtual bool supportDescribeHdrStaticInfo();
    virtual bool supportDescribeHdr10PlusInfo();

    virtual OMX_ERRORTYPE internalGetParameter(OMX_INDEXTYPE index,
                                               OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(OMX_INDEXTYPE index,
                                               const OMX_PTR params);

    virtual OMX_ERRORTYPE getExtensionIndex(const char* name,
                                            OMX_INDEXTYPE* index);

private:
    enum {
        kNumBuffers = 10
    };

    enum {
        MODE_VP8,
        MODE_VP9
    } mMode;

    RenderMode mRenderMode = RenderMode::RENDER_BY_GUEST_CPU;
    bool mEnableAndroidNativeBuffers = false;
    std::map<void*, sp<ANativeWindowBuffer>> mNWBuffers;

    int getHostColorBufferId(void* header);

    enum {
        INPUT_DATA_AVAILABLE,  // VPX component is ready to decode data.
        INPUT_EOS_SEEN,        // VPX component saw EOS and is flushing On2 decoder.
        OUTPUT_FRAMES_FLUSHED  // VPX component finished flushing On2 decoder.
    } mEOSStatus;

    vpx_codec_ctx_t *mCtx;
    bool mFrameParallelMode;  // Frame parallel is only supported by VP9 decoder.
    struct PrivInfo {
        OMX_TICKS mTimeStamp;
        sp<ABuffer> mHdr10PlusInfo;
    };
    PrivInfo mPrivInfo[kNumBuffers];
    uint8_t mTimeStampIdx;
    vpx_image_t *mImg;

    status_t initDecoder();
    status_t destroyDecoder();
    bool outputBuffers(bool flushDecoder, bool display, bool eos, bool *portWillReset);
    bool outputBufferSafe(OMX_BUFFERHEADERTYPE *outHeader);

    void setup_ctx_parameters(vpx_codec_ctx_t*, int hostColorBufferId = -1);

    DISALLOW_EVIL_CONSTRUCTORS(GoldfishVPX);
};

}  // namespace android

#endif  // GOLDFISH_VPX_H_
