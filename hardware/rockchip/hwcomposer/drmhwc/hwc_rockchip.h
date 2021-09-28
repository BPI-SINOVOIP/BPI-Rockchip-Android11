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
#include "drmresources.h"
#include "vsyncworker.h"
#include "drmframebuffer.h"
#include <fcntl.h>

/*
 * In order to pass VTS should amend property to follow google standard.
 * From Android P version, vendor need to use the "vendor.xx.xx" property
 * instead of the "sys.xx.xx" property.
 * For example format as follows:
       hwc.        ->  vendor.hwc.
       sys.        ->  vendor.
       persist.sys ->  persist.vendor
 */
#ifdef ANDROID_P
#define PROPERTY_TYPE "vendor"
#else
#define PROPERTY_TYPE "sys"
#endif

#if DRM_DRIVER_VERSION == 2
typedef struct hdr_output_metadata hdr_metadata_s;
#define HDR_METADATA_EOTF_T(hdr_metadata) hdr_metadata.hdmi_metadata_type.eotf
#define HDR_METADATA_EOTF_P(hdr_metadata) hdr_metadata->hdmi_metadata_type.eotf
#else
typedef struct hdr_static_metadata hdr_metadata_s;
#define HDR_METADATA_EOTF_T(hdr_metadata) hdr_metadata.eotf
#define HDR_METADATA_EOTF_P(hdr_metadata) hdr_metadata->eotf
#endif


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

typedef std::map<int, std::vector<DrmHwcLayer*>> LayerMap;
typedef LayerMap::iterator LayerMapIter;
struct hwc_context_t;
class VSyncWorker;

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
#if RK_VIDEO_UI_OPT
  int iUiFd;
  bool bHideUi;
#endif
  bool is10bitVideo;
  MixMode mixMode;
  bool isVideo;
  bool isHdr;
  bool hasEotfPlane;
  hdr_metadata_s last_hdr_metadata;
  int colorimetry;
  int color_format;
  int color_depth;
  int framebuffer_width;
  int framebuffer_height;
  int rel_xres;
  int rel_yres;
  int v_total;
  int vrefresh;
  int iPlaneSize;
  float w_scale;
  float h_scale;
  bool active;
  bool is_3d;
  bool is_interlaced;
  Mode3D stereo_mode;
  HDMI_STAT last_hdmi_status;
  int display_timeline;
  int hotplug_timeline;
  bool bPreferMixDown;
#if  RK_RGA_PREPARE_ASYNC
    int rgaBuffer_index;
    DrmRgaBuffer rgaBuffers[MaxRgaBuffers];
    bool mUseRga;
#endif
    int transform_nv12;
    int transform_normal;
#if RK_ROTATE_VIDEO_MODE
    int original_min_freq;
    bool bRotateVideoMode;
#endif
#if RK_CTS_WORKROUND
    bool bPerfMode;
#endif
#if DUAL_VIEW_MODE
    bool bDualViewMode;
#endif


} hwc_drm_display_t;

/*
 * Base_parameter is used for 3328_8.0  , by libin start.
 */
#define AUTO_BIT_RESET 0x00
#define RESOLUTION_AUTO			(1<<0)
#define COLOR_AUTO				(1<<1)
#define HDCP1X_EN				(1<<2)
#define RESOLUTION_WHITE_EN		(1<<3)
#define SCREEN_LIST_MAX 5
#define DEFAULT_BRIGHTNESS  50
#define DEFAULT_CONTRAST  50
#define DEFAULT_SATURATION  50
#define DEFAULT_HUE  50
#define DEFAULT_OVERSCAN_VALUE 100


struct drm_display_mode {
    /* Proposed mode values */
    int clock;      /* in kHz */
    int hdisplay;
    int hsync_start;
    int hsync_end;
    int htotal;
    int vdisplay;
    int vsync_start;
    int vsync_end;
    int vtotal;
    int vrefresh;
    int vscan;
    unsigned int flags;
    int picture_aspect_ratio;
};

enum output_format {
    output_rgb=0,
    output_ycbcr444=1,
    output_ycbcr422=2,
    output_ycbcr420=3,
    output_ycbcr_high_subsampling=4,  // (YCbCr444 > YCbCr422 > YCbCr420 > RGB)
    output_ycbcr_low_subsampling=5  , // (RGB > YCbCr420 > YCbCr422 > YCbCr444)
    invalid_output=6,
};

enum  output_depth{
    Automatic=0,
    depth_24bit=8,
    depth_30bit=10,
};

struct overscan {
    unsigned int maxvalue;
    unsigned short leftscale;
    unsigned short rightscale;
    unsigned short topscale;
    unsigned short bottomscale;
};

struct hwc_inital_info{
    char device[128];
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    float fps;
};

struct bcsh_info {
    unsigned short brightness;
    unsigned short contrast;
    unsigned short saturation;
    unsigned short hue;
};
struct lut_data{
    uint16_t size;
    uint16_t lred[1024];
    uint16_t lgreen[1024];
    uint16_t lblue[1024];
};
struct screen_info {
	  int type;
    struct drm_display_mode resolution;// 52 bytes
    enum output_format  format; // 4 bytes
    enum output_depth depthc; // 4 bytes
    unsigned int feature;     //4 bytes
};


struct disp_info {
	struct screen_info screen_list[SCREEN_LIST_MAX];
  struct overscan scan;//12 bytes
	struct hwc_inital_info hwc_info; //140 bytes
	struct bcsh_info bcsh;
  unsigned int reserve[128];
  struct lut_data mlutdata;/*6k+4*/
};


struct file_base_parameter
{
    struct disp_info main;
    struct disp_info aux;
};

static char const *const device_template[] =
{
    "/dev/block/platform/1021c000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30020000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/fe330000.sdhci/by-name/baseparameter",
    "/dev/block/platform/ff520000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/ff0f0000.dwmmc/by-name/baseparameter",
    "/dev/block/rknand_baseparameter",
    "/dev/block/by-name/baseparameter",
    "/dev/block/platform/30030000.nandc/by-name/baseparameter",
    NULL
};

enum flagBaseParameter
{
    BP_UPDATE = 0,
    BP_RESOLUTION,
    BP_FB_SIZE,
    BP_DEVICE,
    BP_COLOR,
    BP_BRIGHTNESS,
    BP_CONTRAST,
    BP_SATURATION,
    BP_HUE,
    BP_OVERSCAN,
};

const char* hwc_get_baseparameter_file(void);

bool hwc_have_baseparameter(void);
int  hwc_get_baseparameter_config(char *parameter, int display, int flag, int type);
void hwc_set_baseparameter_config(DrmResources *drm);
void hwc_save_BcshConfig(int dpy);
int  hwc_findSuitableInfoSlot(struct disp_info* info, int type);
int  hwc_parse_format_into_prop(int display,unsigned int format,unsigned int depthc);
int  hwc_SetGamma(DrmResources *drm);
bool hwc_isGammaSetEnable(int type);
int  hwc_setGamma(int fd, uint32_t crtc_id, uint32_t size,uint16_t *red, uint16_t *green, uint16_t *blue);

/*
 * Base_parameter is used for 3328_8.0 , by libin end.
 */
enum
{
    VIDEO_SCALE_FULL_SCALE = 0,
    VIDEO_SCALE_AUTO_SCALE,
    VIDEO_SCALE_4_3_SCALE ,
    VIDEO_SCALE_16_9_SCALE,
    VIDEO_SCALE_ORIGINAL,
    VIDEO_SCALE_OVERSCREEN,
    VIDEO_SCALE_LR_BOX,
    VIDEO_SCALE_TB_BOX,
};

bool hwc_video_to_area(DrmHwcRect<float> &source_yuv,DrmHwcRect<int> &display_yuv,int scaleMode);



int hwc_init_version();

#if USE_AFBC_LAYER
bool isAfbcInternalFormat(uint64_t internal_format);
#endif
#if RK_INVALID_REFRESH
int init_thread_pamaters(threadPamaters* mThreadPamaters);
int free_thread_pamaters(threadPamaters* mThreadPamaters);
int hwc_static_screen_opt_set(bool isGLESComp);
#endif

#if 1
int detect_3d_mode(hwc_drm_display_t *hd, hwc_display_contents_1_t *display_content, int display);
#endif
#if 0
int hwc_control_3dmode(int fd_3d, int value, int flag);
#endif
float getPixelWidthByAndroidFormat(int format);
#ifdef USE_HWC2
int hwc_get_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_set_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t displayStereo);
int hwc_get_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_set_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t alreadyStereo);
int hwc_get_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, char* layername, unsigned long len);
int hwc_set_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, const char* layername);
#endif
int hwc_get_handle_width(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_height(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_format(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_byte_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_usage(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_size(const gralloc_module_t *gralloc, buffer_handle_t hnd);
int hwc_get_handle_attributes(const gralloc_module_t *gralloc, buffer_handle_t hnd, std::vector<int> *attrs);
int hwc_get_handle_attibute(const gralloc_module_t *gralloc, buffer_handle_t hnd, attribute_flag_t flag);
int hwc_get_handle_primefd(const gralloc_module_t *gralloc, buffer_handle_t hnd);
#if RK_DRM_GRALLOC
uint32_t hwc_get_handle_phy_addr(const gralloc_module_t *gralloc, buffer_handle_t hnd);
#endif
uint32_t hwc_get_layer_colorspace(hwc_layer_1_t *layer);
uint32_t colorspace_convert_to_linux(uint32_t colorspace);
bool vop_support_format(uint32_t hal_format);
bool vop_support_scale(hwc_layer_1_t *layer,hwc_drm_display_t *hd);
bool GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask);
bool match_process(DrmResources* drm, DrmCrtc *crtc, bool is_interlaced,
                        std::vector<DrmHwcLayer>& layers, int iPlaneSize, int fbSize,
                        std::vector<DrmCompositionPlane>& composition_planes);
bool mix_policy(DrmResources* drm, DrmCrtc *crtc, hwc_drm_display_t *hd,
                std::vector<DrmHwcLayer>& layers, int iPlaneSize, int fbSize,
                std::vector<DrmCompositionPlane>& composition_planes);
#if RK_VIDEO_UI_OPT
void video_ui_optimize(const gralloc_module_t *gralloc, hwc_display_contents_1_t *display_content, hwc_drm_display_t *hd);
#endif
void hwc_list_nodraw(hwc_display_contents_1_t  *list);
void hwc_sync_release(hwc_display_contents_1_t  *list);


}

#endif
