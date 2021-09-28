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

#ifndef __RKVPU_DEC_API_H__
#define __RKVPU_DEC_API_H__

#include "vpu_api.h"

#define OMX_BUFFERFLAG_EOS              0x00000001

typedef enum VPU_RET {
    VPU_OK                      = 0,
    VPU_ERR_UNKNOW              = -1,
    VPU_ERR_BASE                = -1000,
    VPU_ERR_LIST_STREAM         = VPU_API_ERR_BASE - 1,
    VPU_ERR_INIT                = VPU_API_ERR_BASE - 2,
    VPU_ERR_VPU_CODEC_INIT      = VPU_API_ERR_BASE - 3,
    VPU_ERR_STREAM              = VPU_API_ERR_BASE - 4,
    VPU_ERR_FATAL_THREAD        = VPU_API_ERR_BASE - 5,
    VPU_EAGAIN                  = VPU_API_ERR_BASE - 6,
    VPU_EOS_STREAM_REACHED      = VPU_API_ERR_BASE - 11,
} VPU_RET;

class RKHWDecApi
{
public:
    RKHWDecApi();
    ~RKHWDecApi();

    VPU_RET prepare(int32_t width, int32_t height, OMX_RK_VIDEO_CODINGTYPE coding);

    /*
     * send video stream packet to decoder only, async interface
     */
    VPU_RET sendStream(char *data, int32_t size, int64_t pts, int32_t flag);

    /*
     * get video frame from decoder only, async interface
     * Note: @deinitOutFrame if we has done everything with VPU_FRAME
     */
    VPU_RET getOutFrame(VPU_FRAME *vframe);

    /*
     * VPU_FRAME buffers used recycled inside decoder, so release
     * that buffer which has been display success.
     */
    void deinitOutFrame(VPU_FRAME *vframe);

	void release();

private:
    VpuCodecContext *mVpuCtx;
    int32_t mInitOK;
    int32_t mFrameCount;
};

#endif  // __RKVPU_DEC_API_H__

