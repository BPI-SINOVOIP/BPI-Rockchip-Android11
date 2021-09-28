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

static const omx_core_cb_type dec_core[] = {
    {
        "OMX.rk.video_decoder.avc",
        "video_decoder.avc"
    },

    {
        "OMX.rk.video_decoder.m4v",
        "video_decoder.mpeg4"
    },

    {
        "OMX.rk.video_decoder.h263",
        "video_decoder.h263"
    },

    {
        "OMX.rk.video_decoder.flv1",
        "video_decoder.flv1"
    },

    {
        "OMX.rk.video_decoder.m2v",
        "video_decoder.mpeg2"
    },
#ifndef AVS80
    {
        "OMX.rk.video_decoder.rv",
        "video_decoder.rv"
    },
#endif

#ifdef SUPPORT_VP6
    {
        "OMX.rk.video_decoder.vp6",
        "video_decoder.vp6"
    },
#endif

    {
        "OMX.rk.video_decoder.vp8",
        "video_decoder.vp8"
    },
#ifdef SUPPORT_VP9
    {
        "OMX.rk.video_decoder.vp9",
        "video_decoder.vp9"
    },
#endif

    {
        "OMX.rk.video_decoder.vc1",
        "video_decoder.vc1"
    },

    {
        "OMX.rk.video_decoder.wmv3",
        "video_decoder.wmv3"
    },
#ifdef SUPPORT_HEVC
    {
        "OMX.rk.video_decoder.hevc",
        "video_decoder.hevc"
    },
#endif
    {
        "OMX.rk.video_decoder.mjpeg",
        "video_decoder.mjpeg"
    },
#ifdef HAVE_L1_SVP_MODE
    {
        "OMX.rk.video_decoder.avc.secure",
        "video_decoder.avc"
    },

    {
        "OMX.rk.video_decoder.hevc.secure",
        "video_decoder.hevc"
    },

    {
        "OMX.rk.video_decoder.m2v.secure",
        "video_decoder.mpeg2"
    },

    {
        "OMX.rk.video_decoder.m4v.secure",
        "video_decoder.mpeg4"
    },

    {
        "OMX.rk.video_decoder.vp8.secure",
        "video_decoder.vp8"
    },

    {
        "OMX.rk.video_decoder.vp9.secure",
        "video_decoder.vp9"
    },
#endif
};

const unsigned int SIZE_OF_DEC_CORE = sizeof(dec_core) / sizeof(dec_core[0]);

#endif  // RkOMX_Core.h
