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

#ifndef _HWC_ROCKCHIP_H_
#define _HWC_ROCKCHIP_H_

#include <map>
#include <vector>
#include "drmhwcomposer.h"
#include "drmframebuffer.h"
#include <fcntl.h>

namespace android {
//G6110_SUPPORT_FBDC
#define FBDC_BGRA_8888                  0x125 //HALPixelFormatSetCompression(HAL_PIXEL_FORMAT_BGRA_8888,HAL_FB_COMPRESSION_DIRECT_16x4)
#define FBDC_RGBA_8888                  0x121 //HALPixelFormatSetCompression(HAL_PIXEL_FORMAT_RGBA_8888,HAL_FB_COMPRESSION_DIRECT_16x4)

#define MOST_WIN_ZONES                  4
#if RK_STEREO
#define READ_3D_MODE  			(0)
#define WRITE_3D_MODE 			(1)
#endif


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


/* HDMI output pixel format */
enum drm_hdmi_output_type {
	DRM_HDMI_OUTPUT_DEFAULT_RGB, /* default RGB */
	DRM_HDMI_OUTPUT_YCBCR444, /* YCBCR 444 */
	DRM_HDMI_OUTPUT_YCBCR422, /* YCBCR 422 */
	DRM_HDMI_OUTPUT_YCBCR420, /* YCBCR 420 */
	DRM_HDMI_OUTPUT_YCBCR_HQ, /* Highest subsampled YUV */
	DRM_HDMI_OUTPUT_YCBCR_LQ, /* Lowest subsampled YUV */
	DRM_HDMI_OUTPUT_INVALID, /* Guess what ? */
};

enum dw_hdmi_rockchip_color_depth {
	ROCKCHIP_DEPTH_DEFAULT = 0,
	ROCKCHIP_HDMI_DEPTH_8 = 8,
	ROCKCHIP_HDMI_DEPTH_10 = 10,
};

typedef enum attribute_flag {
    ATT_WIDTH = 0,
    ATT_HEIGHT,
    ATT_STRIDE,
    ATT_FORMAT,
    ATT_SIZE,
    ATT_BYTE_STRIDE
}attribute_flag_t;

typedef enum tagMixMode
{
    HWC_DEFAULT,
    HWC_MIX_DOWN,
    HWC_MIX_UP,
    HWC_MIX_CROSS,
    HWC_MIX_3D,
    HWC_POLICY_NUM
}MixMode;

enum HDMI_STAT
{
    HDMI_INVALID,
    HDMI_ON,
    HDMI_OFF
};

#if RK_INVALID_REFRESH
typedef struct _threadPamaters
{
    int count;
    pthread_mutex_t mlk;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
}threadPamaters;
#endif

typedef struct hwc_drm_display {
  struct hwc_context_t *ctx;
  const gralloc_module_t *gralloc;
  int display;
  int framebuffer_width;
  int framebuffer_height;
  int vrefresh;
  int rgaBuffer_index;
  DrmRgaBuffer rgaBuffers[2];
  bool mUseRga;
} hwc_drm_display_t;


int hwc_init_version();

float getPixelWidthByAndroidFormat(int format);
int hwc_lock(buffer_handle_t hnd,int usage, int x, int y, int w, int h, void **cpu_addr);
int hwc_unlock(buffer_handle_t hnd);
int hwc_get_handle_width(buffer_handle_t hnd);
int hwc_get_handle_height(buffer_handle_t hnd);
int hwc_get_handle_format(buffer_handle_t hnd);
int hwc_get_handle_stride(buffer_handle_t hnd);
int hwc_get_handle_byte_stride(buffer_handle_t hnd);
int hwc_get_handle_usage(buffer_handle_t hnd);
int hwc_get_handle_size(buffer_handle_t hnd);
int hwc_get_handle_attributes(buffer_handle_t hnd, std::vector<int> *attrs);
int hwc_get_handle_attibute(buffer_handle_t hnd, attribute_flag_t flag);
int hwc_get_handle_primefd(buffer_handle_t hnd);
void hwc_list_nodraw(hwc_display_contents_1_t  *list);
void hwc_sync_release(hwc_display_contents_1_t  *list);


}

#endif
