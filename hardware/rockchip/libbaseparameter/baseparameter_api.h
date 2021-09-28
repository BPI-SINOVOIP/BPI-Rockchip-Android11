/*
 *
 * Copyright 2021 Rockchip Electronics S.LSI Co. LTD
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

#ifndef BASEPARAMETER_API_H_
#define BASEPARAMETER_API_H_

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <pthread.h>

#define BASEPARAMETER_MAJOR_VERSION 2
#define BASEPARAMETER_MINOR_VERSION 0
#define AUTO_BIT_RESET      0x00
#define RESOLUTION_AUTO     (1<<0)
#define COLOR_AUTO          (1<<1)
#define HDCP1X_EN           (1<<2)
#define RESOLUTION_WHITE_EN (1<<3)
#define DEFAULT_BRIGHTNESS 50
#define DEFAULT_CONTRAST 50
#define DEFAULT_SATURATION 50
#define DEFAULT_HUE 50
#define BASE_PARAMETER 0
#define BACKUP_PARAMETER 1

typedef unsigned short u16;
typedef unsigned int u32;

enum output_format {
    output_rgb=0,
    output_ycbcr444=1,
    output_ycbcr422=2,
    output_ycbcr420=3,
    output_ycbcr_high_subsampling=4,  /* (YCbCr444 > YCbCr422 > YCbCr420 > RGB) */
    output_ycbcr_low_subsampling=5, /* (RGB > YCbCr420 > YCbCr422 > YCbCr444) */
    invalid_output=6,
};

enum  output_depth{
    Automatic=0,
    depth_24bit=8,
    depth_30bit=10,
};

struct disp_header {
    u32 connector_type; /* 显示设备connector type */
    u32 connector_id; /* 显示设备connector id */
    u32 offset; /* disp_info偏移 */
};

struct drm_display_mode {
    /* Proposed mode values */
    int clock; /* in kHz */
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

struct screen_info {
    u32 type; /* connector 类型， 4 bytes */
    u32 id; /* 4 byte, 用于区别相同 type 的不同设备 */
    struct drm_display_mode resolution; /* 52 bytes */
    enum output_format  format;  /* 4 bytes */
    enum output_depth depthc; /* 4 bytes */
    u32 feature; /* 4 bytes */
};

struct bcsh_info {
    unsigned short brightness;
    unsigned short contrast;
    unsigned short saturation;
    unsigned short hue;
};

struct overscan_info {
    unsigned int maxvalue;
    unsigned short leftscale;
    unsigned short rightscale;
    unsigned short topscale;
    unsigned short bottomscale;
};

struct gamma_lut_data{
    u16 size;
    u16 lred[1024];
    u16 lgreen[1024];
    u16 lblue[1024];
};

struct cubic_lut_data{
    u16 size;
    u16 lred[4913];
    u16 lgreen[4913];
    u16 lblue[4913];
};

struct framebuffer_info {
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 fps;
};

struct disp_info {
    char disp_head_flag[6]; /* disp 头标识，"DISP_N", N 可以是0-7, size: 6 Byte*/
    struct screen_info screen_info[4]; /* 支持热插拔的设备接不同的设备，如DP出来可能接 DP->HDMI 或者 DP->VGA, size: 72 * 4 = 288 Byte */
    struct bcsh_info bcsh_info; /* 调节亮度、对比度、饱和度、色度信息, size: 8 Byte */
    struct overscan_info overscan_info; /* 过扫描信息, size: 16 Byte */
    struct gamma_lut_data gamma_lut_data; /* gamma 信息, size: 6146 Byte */
    struct cubic_lut_data cubic_lut_data; /* 3D lut信息, size: 29480 Byte */
    struct framebuffer_info framebuffer_info; /* framebuffer信息, size: 12 Byte */
    u32 reserved[244]; /* 预留, size: 244 Byte*/
    u32 crc; /* CRC 校验, size: 4 Byte */
};

struct baseparameter_info {
    char head_flag[4]; /* 头标识， "BASP" */
    u16 major_version; /* Baseparameter 大版本信息 */
    u16 minor_version; /* Baseparameter 小版本信息 */
    struct disp_header disp_header[8]; /* 通过head可以正确找到每一个显示设备的偏移,按现在每个disp_info的大小，最多支持8个disp */
    struct disp_info disp_info[8]; /* 显示设备信息 */
};


static char const *const device_template[] =
{
    "/dev/block/platform/1021c000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30020000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/fe330000.sdhci/by-name/baseparameter",
    "/dev/block/platform/ff520000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/ff0f0000.dwmmc/by-name/baseparameter",
    "/dev/block/platform/30030000.nandc/by-name/baseparameter",
    "/dev/block/rknand_baseparameter",
    "/dev/block/by-name/baseparameter",
    NULL
};

class baseparameter_api {
public:
    baseparameter_api();
    ~baseparameter_api();
    bool have_baseparameter();
    int dump_baseparameter(const char *file_path);
    int get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    int set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info);
    int get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    int set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *screen_info);
    unsigned short get_brightness(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_contrast(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_saturation(unsigned int connector_type, unsigned int connector_id);
    unsigned short get_hue(unsigned int connector_type, unsigned int connector_id);
    int set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value);
    int get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    int set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *overscan_info);
    int get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    int set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data);
    int get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    int set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data);
    int set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id);
    int get_all_disp_header(struct disp_header *headers);
    bool validate();
    int get_framebuffer_info(unsigned int connector_type, unsigned int connector_id, framebuffer_info *info);
    int set_framebuffer_info(unsigned int connector_type, unsigned int connector_id, framebuffer_info *info);
    int get_baseparameter_info(unsigned int index, baseparameter_info *info);
    int set_baseparameter_info(unsigned int index, baseparameter_info *info);
private:
    const char* get_baseparameter_file();
    u32 get_crc32(unsigned char *buf, unsigned int size);
};

#endif
