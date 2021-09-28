/*
 *
 * Copyright 2017 Rockchip Electronics Co. LTD
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

#ifndef __RkOMX_Core__H
#define __RkOMX_Core__H

static const omx_core_cb_type enc_core[] = {
    {
        "OMX.rk.video_encoder.avc",
        "video_encoder.avc"
    },
#ifdef SUPPORT_HEVC_ENC
    {
        "OMX.rk.video_encoder.hevc",
        "video_encoder.hevc"
    },
#endif

#ifdef SUPPORT_VP8_ENC
    {
        "OMX.rk.video_encoder.vp8",
        "video_encoder.vp8"
    },
#endif
};

const unsigned int SIZE_OF_ENC_CORE = sizeof(enc_core) / sizeof(enc_core[0]);

#endif  // RkOMX_Core.h
