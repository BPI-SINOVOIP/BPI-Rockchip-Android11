/*
 *
 * Copyright 2016 Rockchip Electronics Co. LTD
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
 * @file   Rockchip_OSAL_RGA_Process.h
 * @brief   RGA COPY & Color convert
 * @author  csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *   2016.10.26 : Create
 */
#ifndef ROCKCHIP_OSAL_RGA_PROCESS
#define ROCKCHIP_OSAL_RGA_PROCESS


#include "vpu_type.h"
#include "Rockchip_OMX_Def.h"
#include "vpu_global.h"

OMX_S32 rga_dev_open(void **rga_ctx);
OMX_S32 rga_dev_close(void *rga_ctx);
void rga_nv12_copy(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, void *rga_ctx);
void rga_rgb_copy(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, uint32_t Width, uint32_t Height, void *rga_ctx);
void rga_rgb2nv12(RockchipVideoPlane *plane,  VPUMemLinear_t *vpumem , uint32_t Width, uint32_t Height, uint32_t dstWidth, uint32_t dstHeight, void *rga_ctx);
void rga_nv12_crop_scale(RockchipVideoPlane *plane, VPUMemLinear_t *vpumem, OMX_VIDEO_PARAMS_EXTENDED *param_video,
                         RK_U32 orgin_w, RK_U32 orgin_h, void *rga_ctx);
void rga_nv122rgb( RockchipVideoPlane *planes, VPUMemLinear_t *vpumem, uint32_t mWidth, uint32_t mHeight, int dst_format, void *rga_ctx);
void rga_nv12_crop(void *srcData, void *dstData, uint32_t width, uint32_t height,
                   uint32_t stride, uint32_t strideHeight, void *rga_ctx);

#endif

