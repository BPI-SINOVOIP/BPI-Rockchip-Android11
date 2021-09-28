/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef _DRM_TYPE_H_
#define _DRM_TYPE_H_
#include "rockchip/drmbaseparameter.h"

#include <libsystem/include/system/graphics-base-v1.0.h>

#ifdef ANDROID_S
#include <hardware/hardware_rockchip.h>
#include <hardware/gralloc_rockchip.h>
#endif

#include <hardware/gralloc.h>
#define PROPERTY_TYPE "vendor"

/* hdr usage */
/*usage & 0x0F000000
  0x1000000 bt2020
  0x2000000 st2084
  0x3000000 hlg
  0x4000000 dobly version
 */
#define HDR_ST2084_USAGE                                       0x2000000
#define HDR_HLG_USAGE                                          0x3000000

#define GRALLOC_ARM_INTFMT_EXTENSION_BIT_START          32
/* This format will use AFBC */
#define	GRALLOC_ARM_INTFMT_AFBC                     (1ULL << (GRALLOC_ARM_INTFMT_EXTENSION_BIT_START+0))
#define MAGIC_USAGE_FOR_AFBC_LAYER                      (0x88)

typedef enum DrmHdrType{
    DRM_HWC_DOLBY_VISION = 1,
    DRM_HWC_HDR10 = 2,
    DRM_HWC_HLG = 3,
    DRM_HWC_HDR10_PLUS = 4
}DrmHdrType_t;

class DrmHdr{
public:
    DrmHdr(DrmHdrType_t drm_hdr_type, float out_max_luminance, float out_max_average_luminance, float out_min_luminance){
        drmHdrType = drm_hdr_type;
        outMaxLuminance = out_max_luminance;
        outMaxAverageLuminance = out_max_average_luminance;
        outMinLuminance = out_min_luminance;
    }

    DrmHdrType_t drmHdrType;
    float outMaxLuminance;
    float outMaxAverageLuminance;
    float outMinLuminance;
};


/* see also http://vektor.theorem.ca/graphics/ycbcr/ */
enum v4l2_colorspace {
        /*
         * Default colorspace, i.e. let the driver figure it out.
         * Can only be used with video capture.
         */
        V4L2_COLORSPACE_DEFAULT       = 0,

        /* SMPTE 170M: used for broadcast NTSC/PAL SDTV */
        V4L2_COLORSPACE_SMPTE170M     = 1,

        /* Obsolete pre-1998 SMPTE 240M HDTV standard, superseded by Rec 709 */
        V4L2_COLORSPACE_SMPTE240M     = 2,

        /* Rec.709: used for HDTV */
        V4L2_COLORSPACE_REC709        = 3,

        /*
         * Deprecated, do not use. No driver will ever return this. This was
         * based on a misunderstanding of the bt878 datasheet.
         */
        V4L2_COLORSPACE_BT878         = 4,

        /*
         * NTSC 1953 colorspace. This only makes sense when dealing with
         * really, really old NTSC recordings. Superseded by SMPTE 170M.
         */
        V4L2_COLORSPACE_470_SYSTEM_M  = 5,

        /*
         * EBU Tech 3213 PAL/SECAM colorspace. This only makes sense when
         * dealing with really old PAL/SECAM recordings. Superseded by
         * SMPTE 170M.
         */
        V4L2_COLORSPACE_470_SYSTEM_BG = 6,

        /*
         * Effectively shorthand for V4L2_COLORSPACE_SRGB, V4L2_YCBCR_ENC_601
         * and V4L2_QUANTIZATION_FULL_RANGE. To be used for (Motion-)JPEG.
         */
        V4L2_COLORSPACE_JPEG          = 7,

        /* For RGB colorspaces such as produces by most webcams. */
        V4L2_COLORSPACE_SRGB          = 8,

        /* AdobeRGB colorspace */
        V4L2_COLORSPACE_ADOBERGB      = 9,

        /* BT.2020 colorspace, used for UHDTV. */
        V4L2_COLORSPACE_BT2020        = 10,

        /* Raw colorspace: for RAW unprocessed images */
        V4L2_COLORSPACE_RAW           = 11,

        /* DCI-P3 colorspace, used by cinema projectors */
        V4L2_COLORSPACE_DCI_P3        = 12,
};

typedef enum attribute_flag {
    ATT_WIDTH = 0,
    ATT_HEIGHT,
    ATT_STRIDE,
    ATT_FORMAT,
    ATT_SIZE,
    ATT_BYTE_STRIDE,
    ATT_BYTE_STRIDE_WORKROUND
}attribute_flag_t;

typedef struct hwc2_drm_display {
  bool bStandardSwitchResolution = false;
  int framebuffer_width;
  int framebuffer_height;
  int vrefresh;
  int rel_xres;
  int rel_yres;
  uint32_t dclk;
  uint32_t aclk;
  float w_scale;
  float h_scale;
  int bcsh_timeline;
  int display_timeline;
  int hotplug_timeline;
  bool hdr_mode;
  const struct disp_info* baseparameter_info;
} hwc2_drm_display_t;

uint32_t ConvertHalFormatToDrm(uint32_t hal_format);

#endif
