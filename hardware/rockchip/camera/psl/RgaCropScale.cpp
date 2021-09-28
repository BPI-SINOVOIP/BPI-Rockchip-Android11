/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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
#include "RgaCropScale.h"
#include "LogHelper.h"
#include <utils/Singleton.h>
#include <RockchipRga.h>
#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <hardware/hardware_rockchip.h>
#endif


namespace android {
namespace camera2 {

#if defined(TARGET_RK312X)
#define RGA_VER (1.0)
#define RGA_ACTIVE_W (2048)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (2048)
#define RGA_VIRTUAL_H (2048)

#else
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)

#endif

int RgaCropScale::CropScaleNV12Or21(struct Params* in, struct Params* out)
{
	rga_info_t src, dst;

    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));

    if (!in || !out)
        return -1;

	if((out->width > RGA_VIRTUAL_W) || (out->height > RGA_VIRTUAL_H)){
			ALOGE("%s(%d): out wxh %dx%d beyond rga capability",
                 __FUNCTION__, __LINE__,
                 out->width, out->height);
            return -1;
	}

    if ((in->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        in->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		in->fmt != HAL_PIXEL_FORMAT_RGBA_8888) ||
        (out->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        out->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		out->fmt != HAL_PIXEL_FORMAT_RGBA_8888)) {
        ALOGE("%s(%d): only accept NV12 or NV21 now. in fmt %d, out fmt %d",
             __FUNCTION__, __LINE__,
             in->fmt, out->fmt);
        return -1;
    }
	RockchipRga& rkRga(RockchipRga::get());
	
	if (in->fd == -1) {
		src.fd = -1;
		src.virAddr = (void*)in->vir_addr;
	} else {
		src.fd = in->fd;
	}
	src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

	if (out->fd == -1 ) {
		dst.fd = -1;
		dst.virAddr = (void*)out->vir_addr;
	} else {
		dst.fd = out->fd;
	}
	dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

	rga_set_rect(&src.rect,
		     in->offset_x,
		     in->offset_y,
		     in->width,
		     in->height,
		     in->width_stride,
		     in->height_stride,
		     in->fmt);

	rga_set_rect(&dst.rect,
		     out->offset_x,
		     out->offset_y,
		     out->width,
		     out->height,
		     out->width_stride,
		     out->height_stride,
		     out->fmt);
    if (in->mirror)
		src.rotation = DRM_RGA_TRANSFORM_FLIP_H;

	if (rkRga.RkRgaBlit(&src, &dst, NULL)) {
		ALOGE("%s:rga blit failed", __FUNCTION__);
        return -1;
	}

    return 0;
}

} /* namespace camera2 */
} /* namespace android */
