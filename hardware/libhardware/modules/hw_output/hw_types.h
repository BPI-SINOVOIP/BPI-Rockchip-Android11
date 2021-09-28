#ifndef ANDROID_HW_TYPES_H
#define ANDROID_HW_TYPES_H
#include <stdint.h>

#include "baseparameter_api.h"

#define BASE_OFFSET 8*1024
#define DEFAULT_BRIGHTNESS  50
#define DEFAULT_CONTRAST  50
#define DEFAULT_SATURATION  50
#define DEFAULT_HUE  50
#define DEFAULT_OVERSCAN_VALUE 100

#define OVERSCAN_LEFT 0
#define OVERSCAN_TOP 1
#define OVERSCAN_RIGHT 2
#define OVERSCAN_BOTTOM 3

struct lut_data{
    uint16_t size;
    uint16_t lred[1024];
    uint16_t lgreen[1024];
    uint16_t lblue[1024];
};

struct lut_info{
    struct lut_data main;
    struct lut_data aux;
};

#define BUFFER_LENGTH    256
#define AUTO_BIT_RESET 0x00
#define COLOR_AUTO (1<<1)
#define HDCP1X_EN (1<<2)
#define RESOLUTION_WHITE_EN (1<<3)

struct hwc_inital_info{
    char device[128];
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    float fps;
};

struct screen_info_v1 {
    int type;
    struct drm_display_mode resolution;// 52 bytes
    enum output_format  format; // 4 bytes
    enum output_depth depthc; // 4 bytes
    unsigned int feature;//4 //4 bytes
};

struct disp_info_v1 {
    struct screen_info_v1 screen_list[5];
    struct overscan_info scan;//12 bytes
    struct hwc_inital_info hwc_info; //140 bytes
    struct bcsh_info bcsh;
    unsigned int reserve[128];
    struct lut_data mlutdata;/*6k+4*/
};

struct file_base_paramer_v1
{
    struct disp_info_v1 main;
    struct disp_info_v1 aux;
};

#endif //ANDROID_HW_TYPES_H
