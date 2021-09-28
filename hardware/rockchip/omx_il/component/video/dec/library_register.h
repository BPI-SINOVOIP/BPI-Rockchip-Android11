/*
 *
 * Copyright 2013 Rockchip Electronics Co. LTD
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

/*
 * @file        library_register.h
 * @brief
 * @author      Csy (csy@rockchip.com)
 * @version     1.0.0
 * @history
 *   2013.11.27 : Create
 */

#ifndef ROCKCHIP_OMX_DEC_REG
#define ROCKCHIP_OMX_DEC_REG

#include "Rockchip_OMX_Def.h"
#include "OMX_Component.h"
#include "Rockchip_OMX_Component_Register.h"
#include "Rockchip_OMX_Core.h"


#define OSCL_EXPORT_REF __attribute__((visibility("default")))

#define RK_OMX_COMPONENT_H264_DEC "OMX.rk.video_decoder.avc"
#define RK_OMX_COMPONENT_H264_DRM_DEC "OMX.rk.video_decoder.avc.secure"

#define RK_OMX_COMPONENT_H264_DEC_ROLE "video_decoder.avc"

#define RK_OMX_COMPONENT_MPEG4_DEC "OMX.rk.video_decoder.m4v"
#define RK_OMX_COMPONENT_MPEG4_DRM_DEC "OMX.rk.video_decoder.m4v.secure"

#define RK_OMX_COMPONENT_MPEG4_DEC_ROLE "video_decoder.mpeg4"

#define RK_OMX_COMPONENT_H263_DEC "OMX.rk.video_decoder.h263"
#define RK_OMX_COMPONENT_H263_DEC_ROLE "video_decoder.h263"

#define RK_OMX_COMPONENT_FLV_DEC "OMX.rk.video_decoder.flv1"
#define RK_OMX_COMPONENT_FLV_DEC_ROLE "video_decoder.flv1"

#define RK_OMX_COMPONENT_MPEG2_DEC "OMX.rk.video_decoder.m2v"
#define RK_OMX_COMPONENT_MPEG2_DRM_DEC "OMX.rk.video_decoder.m2v.secure"

#define RK_OMX_COMPONENT_MPEG2_DEC_ROLE "video_decoder.mpeg2"

#define RK_OMX_COMPONENT_RMVB_DEC "OMX.rk.video_decoder.rv"
#define RK_OMX_COMPONENT_RMVB_DEC_ROLE "video_decoder.rv"

#define RK_OMX_COMPONENT_VP8_DEC "OMX.rk.video_decoder.vp8"
#define RK_OMX_COMPONENT_VP8_DEC_ROLE "video_decoder.vp8"

#define RK_OMX_COMPONENT_VC1_DEC "OMX.rk.video_decoder.vc1"
#define RK_OMX_COMPONENT_VC1_DEC_ROLE "video_decoder.vc1"

#define RK_OMX_COMPONENT_WMV3_DEC "OMX.rk.video_decoder.wmv3"
#define RK_OMX_COMPONENT_WMV3_DEC_ROLE "video_decoder.wmv3"

#define RK_OMX_COMPONENT_VP6_DEC "OMX.rk.video_decoder.vp6"
#define RK_OMX_COMPONENT_VP6_DEC_ROLE "video_decoder.vp6"

#define RK_OMX_COMPONENT_HEVC_DEC "OMX.rk.video_decoder.hevc"
#define RK_OMX_COMPONENT_HEVC_DRM_DEC "OMX.rk.video_decoder.hevc.secure"

#define RK_OMX_COMPONENT_HEVC_DEC_ROLE "video_decoder.hevc"

#define RK_OMX_COMPONENT_MJPEG_DEC "OMX.rk.video_decoder.mjpeg"
#define RK_OMX_COMPONENT_MJPEG_DEC_ROLE "video_decoder.mjpeg"

#define RK_OMX_COMPONENT_VP9_DEC "OMX.rk.video_decoder.vp9"
#define RK_OMX_COMPONENT_VP9_DEC_ROLE "video_decoder.vp9"


#define RK_OMX_COMPONENT_VP9_DRM_DEC "OMX.rk.video_decoder.vp9.secure"

#define RK_OMX_COMPONENT_VP8_DRM_DEC "OMX.rk.video_decoder.vp8.secure"

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF int Rockchip_OMX_COMPONENT_Library_Register(RockchipRegisterComponentType **rockchipComponents);

#ifdef __cplusplus
};
#endif

#endif

