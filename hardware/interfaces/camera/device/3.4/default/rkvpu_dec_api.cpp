/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 * module: RKHWDecApi
 * date  : 2021/02/07
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RKHWDecApi"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rkvpu_dec_api.h"

RKHWDecApi::RKHWDecApi()
{
    ALOGV("RKHWDecApi constructor");

    mVpuCtx = NULL;
    mInitOK = 0;
    mFrameCount = 0;
}

RKHWDecApi::~RKHWDecApi()
{
    ALOGV("RKHWDecApi destructor");

    if (mVpuCtx != NULL) {
        mVpuCtx->flush(mVpuCtx);
        vpu_close_context(&mVpuCtx);
        free(mVpuCtx);
        mVpuCtx = NULL;
    }
}

VPU_RET RKHWDecApi::prepare(int32_t width, int32_t height,
                            OMX_RK_VIDEO_CODINGTYPE coding)
{
    int32_t ret;

    mVpuCtx = (VpuCodecContext_t *)malloc(sizeof(VpuCodecContext_t));
    ret = vpu_open_context(&mVpuCtx);
    if (ret) {
        ALOGE("ERROR: faild to open vpuCtx(err=%d)", ret);
        return VPU_ERR_INIT;
    }

    mVpuCtx->codecType = CODEC_DECODER;
    mVpuCtx->videoCoding = coding;
    mVpuCtx->width = width;
    mVpuCtx->height = height;
    mVpuCtx->extradata = NULL;
    mVpuCtx->extradata_size = 0;

    // keep the vpu split mode open if we can't make sure a complete
    // frame will be sent each time.
    int32_t split = 1;
    mVpuCtx->control(mVpuCtx, VPU_API_SET_PARSER_SPLIT_MODE, (void*)&split);

    ret = mVpuCtx->init(mVpuCtx, NULL, 0);
    if (ret) {
        ALOGE("ERROR: faild to init vpuCtx(err=%d)", ret);
        return VPU_ERR_INIT;
    }

    mInitOK = 1;

    return VPU_OK;
}

VPU_RET RKHWDecApi::sendStream(char *data, int32_t size, int64_t pts, int32_t flag)
{
    int32_t ret;
    VideoPacket_t pkt;

    if (!mInitOK) {
        ALOGW("W - prepare RKHWDecApi first");
        return VPU_ERR_UNKNOW;
    }

    if (data == NULL) {
        ALOGE("sendstream get NULL input");
        return VPU_ERR_UNKNOW;
    }

    pkt.data = (unsigned char*)data;
    pkt.size = size;
    if (pts > 0) {
        pkt.pts = pts;
        pkt.dts = pts;
    } else {
        pkt.pts = VPU_API_NOPTS_VALUE;
        pkt.dts = VPU_API_NOPTS_VALUE;
    }
    pkt.nFlags = flag;

    ret = mVpuCtx->decode_sendstream(mVpuCtx, &pkt);
    if (ret < 0) {
        ALOGE("failed to send pkt(err=%d)", ret);
        return VPU_ERR_UNKNOW;
    } else if (pkt.size != 0) {
        return VPU_EAGAIN;
    }

    ALOGV("send pkt size %d pts %lld flag %d", size, pts, flag);

    return VPU_OK;
}

VPU_RET RKHWDecApi::getOutFrame(VPU_FRAME *vframe)
{
    int32_t ret;
    DecoderOut_t decOut;

    if (!mInitOK) {
        ALOGW("W - prepare RKHWDecApi first");
        return VPU_ERR_UNKNOW;
    }

    memset(&decOut, 0, sizeof(DecoderOut_t));
    memset(vframe, 0, sizeof(VPU_FRAME));

    decOut.data = (unsigned char*)vframe;

    ret = mVpuCtx->decode_getframe(mVpuCtx, &decOut);
    if (ret < 0) {
        if (ret == VPU_API_EOS_STREAM_REACHED && !vframe->ErrorInfo) {
            return VPU_EOS_STREAM_REACHED;
        } else {
            ALOGE("failed to get frame(err=%d)", ret);
            return VPU_ERR_UNKNOW;
        }
    }

    if (decOut.size > 0) {
        mFrameCount++;
        ALOGV("get frame_num %d fd 0x%x dimen %dx%d(%dx%d) errinfo %x pts %lld",
              mFrameCount, vframe->vpumem.phy_addr, vframe->FrameWidth,
              vframe->FrameHeight, vframe->DisplayWidth, vframe->DisplayHeight,
              vframe->ErrorInfo, (long long)vframe->ShowTime.TimeLow);

        return VPU_OK;
    }

    return VPU_EAGAIN;
}

void RKHWDecApi::deinitOutFrame(VPU_FRAME *vframe)
{
    if (vframe->vpumem.phy_addr > 0) {
        VPUMemLink(&vframe->vpumem);
        VPUFreeLinear(&vframe->vpumem);
    }
}

void RKHWDecApi::release()
{
    ALOGV("RKHWDecApi release");

    if (mVpuCtx != NULL) {       
        mVpuCtx->flush(mVpuCtx);
        vpu_close_context(&mVpuCtx);
        free(mVpuCtx);
        mVpuCtx = NULL;
    }
}

