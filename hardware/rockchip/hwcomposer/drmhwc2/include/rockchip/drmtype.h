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

// definition from kernel-5.10/include/drm/drm_connector.h.

/*
 * This is a consolidated colorimetry list supported by HDMI and
 * DP protocol standard. The respective connectors will register
 * a property with the subset of this list (supported by that
 * respective protocol). Userspace will set the colorspace through
 * a colorspace property which will be created and exposed to
 * userspace.
 */

/* For Default case, driver will set the colorspace */
#define DRM_MODE_COLORIMETRY_DEFAULT                    0
/* CEA 861 Normal Colorimetry options */
#define DRM_MODE_COLORIMETRY_NO_DATA                    0
#define DRM_MODE_COLORIMETRY_SMPTE_170M_YCC             1
#define DRM_MODE_COLORIMETRY_BT709_YCC                  2
/* CEA 861 Extended Colorimetry Options */
#define DRM_MODE_COLORIMETRY_XVYCC_601                  3
#define DRM_MODE_COLORIMETRY_XVYCC_709                  4
#define DRM_MODE_COLORIMETRY_SYCC_601                   5
#define DRM_MODE_COLORIMETRY_OPYCC_601                  6
#define DRM_MODE_COLORIMETRY_OPRGB                      7
#define DRM_MODE_COLORIMETRY_BT2020_CYCC                8
#define DRM_MODE_COLORIMETRY_BT2020_RGB                 9
#define DRM_MODE_COLORIMETRY_BT2020_YCC                 10
/* Additional Colorimetry extension added as part of CTA 861.G */
#define DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65             11
#define DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER         12
/* Additional Colorimetry Options added for DP 1.4a VSC Colorimetry Format */
#define DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED             13
#define DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT             14
#define DRM_MODE_COLORIMETRY_BT601_YCC                  15

enum DrmColorspaceType {
  DEFAULT            = DRM_MODE_COLORIMETRY_DEFAULT,
  SMPTE_170M_YCC     = DRM_MODE_COLORIMETRY_SMPTE_170M_YCC,
  BT709_YCC          = DRM_MODE_COLORIMETRY_BT709_YCC,
  XVYCC_601          = DRM_MODE_COLORIMETRY_XVYCC_601,
  XVYCC_709          = DRM_MODE_COLORIMETRY_XVYCC_709,
  SYCC_601           = DRM_MODE_COLORIMETRY_SYCC_601,
  OPYCC_601          = DRM_MODE_COLORIMETRY_OPYCC_601,
  OPRGB              = DRM_MODE_COLORIMETRY_OPRGB,
  BT2020_CYCC        = DRM_MODE_COLORIMETRY_BT2020_CYCC,
  BT2020_RGB         = DRM_MODE_COLORIMETRY_BT2020_RGB,
  BT2020_YCC         = DRM_MODE_COLORIMETRY_BT2020_YCC,
  DCI_P3_RGB_D65     = DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65,
  DCI_P3_RGB_THEATER = DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER,
  RGB_WIDE_FIXED     = DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED,
  RGB_WIDE_FLOAT     = DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT,
  BT601_YCC          = DRM_MODE_COLORIMETRY_BT601_YCC
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
  uint32_t soc_id=0;
  bool bStandardSwitchResolution=false;
  int framebuffer_width=0;
  int framebuffer_height=0;
  int vrefresh=0;
  int rel_xoffset=0;
  int rel_yoffset=0;
  int rel_xres=0;
  int rel_yres=0;
  uint32_t dclk=0;
  uint32_t aclk=0;
  float w_scale=0;
  float h_scale=0;
  int bcsh_timeline=0;
  int display_timeline=0;
  int hotplug_timeline=0;
  bool hdr_mode=false;
  char overscan_value[PROPERTY_VALUE_MAX]={0};
  const struct disp_info* baseparameter_info;
} hwc2_drm_display_t;

uint32_t ConvertHalFormatToDrm(uint32_t hal_format);

#endif
