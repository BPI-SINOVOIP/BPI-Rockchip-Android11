#ifndef _IEP_H_
#define _IEP_H_

#include <sys/ioctl.h>

/* Capability for current iep version
using by userspace to determine iep features */
struct IEP_CAP {
    uint8_t scaling_supported;
    uint8_t i4_deinterlace_supported;
    uint8_t i2_deinterlace_supported;
    uint8_t compression_noise_reduction_supported;
    uint8_t sampling_noise_reduction_supported;
    uint8_t hsb_enhancement_supported;
    uint8_t cg_enhancement_supported;
    uint8_t direct_path_supported;
    uint16_t max_dynamic_width;
    uint16_t max_dynamic_height;
    uint16_t max_static_width;
    uint16_t max_static_height;
    uint8_t max_enhance_radius;
};

#define IEP_IOC_MAGIC 'i'

#define IEP_SET_PARAMETER_REQ _IOW(IEP_IOC_MAGIC, 1, unsigned long)
#define IEP_SET_PARAMETER_DEINTERLACE _IOW(IEP_IOC_MAGIC, 2, unsigned long)
#define IEP_SET_PARAMETER_ENHANCE _IOW(IEP_IOC_MAGIC, 3, unsigned long)
#define IEP_SET_PARAMETER_CONVERT _IOW(IEP_IOC_MAGIC, 4, unsigned long)
#define IEP_SET_PARAMETER_SCALE _IOW(IEP_IOC_MAGIC, 5, unsigned long)
#define IEP_GET_RESULT_SYNC _IOW(IEP_IOC_MAGIC, 6, unsigned long)
#define IEP_GET_RESULT_ASYNC _IOW(IEP_IOC_MAGIC, 7, unsigned long)
#define IEP_SET_PARAMETER _IOW(IEP_IOC_MAGIC, 8, unsigned long)
#define IEP_RELEASE_CURRENT_TASK _IOW(IEP_IOC_MAGIC, 9, unsigned long)
#define IEP_GET_IOMMU_STATE _IOR(IEP_IOC_MAGIC,10, unsigned long)
#define IEP_QUERY_CAP _IOR(IEP_IOC_MAGIC, 11, struct IEP_CAP)

enum {
    yuv2rgb_BT_601_l            = 0x0,     /* BT.601_1 */
    yuv2rgb_BT_601_f            = 0x1,     /* BT.601_f */
    yuv2rgb_BT_709_l            = 0x2,     /* BT.709_1 */
    yuv2rgb_BT_709_f            = 0x3,     /* BT.709_f */
};

enum
{
    rgb2yuv_BT_601_l            = 0x0,     /* BT.601_1 */
    rgb2yuv_BT_601_f            = 0x1,     /* BT.601_f */
    rgb2yuv_BT_709_l            = 0x2,     /* BT.709_1 */
    rgb2yuv_BT_709_f            = 0x3,     /* BT.709_f */
};

enum {
    dein_mode_bypass_dis         = 0x0,
    dein_mode_I4O2               = 0x1,
    dein_mode_I4O1B              = 0x2,
    dein_mode_I4O1T              = 0x3,
    dein_mode_I2O1B              = 0x4,
    dein_mode_I2O1T              = 0x5,
    dein_mode_bypass             = 0x6,
};

enum {
    rgb_enhance_bypass          = 0x0,
    rgb_enhance_denoise         = 0x1,
    rgb_enhance_detail          = 0x2,
    rgb_enhance_edge            = 0x3,
};//for rgb_enhance_mode

enum
{
    rgb_contrast_CC_P_DDE          = 0x0, //cg prior to dde
    rgb_contrast_DDE_P_CC          = 0x1, //dde prior to cg
}; //for rgb_contrast_enhance_mode

enum {
    black_screen                   = 0x0,
    blue_screen                    = 0x1,
    color_bar                      = 0x2,
    normal_mode                    = 0x3,
}; //for video mode

/*
//          Alpha    Red     Green   Blue
{  4, 32, {{32,24,   24,16,  16, 8,  8, 0 }}, GGL_RGBA },   // IEP_FORMAT_ARGB_8888
{  4, 32, {{32,24,   8, 0,  16, 8,  24,16 }}, GGL_RGB  },   // IEP_FORMAT_ABGR_8888
{  4, 32, {{ 8, 0,  32,24,  24,16,  16, 8 }}, GGL_RGB  },   // IEP_FORMAT_RGBA_8888
{  4, 32, {{ 8, 0,  16, 8,  24,16,  32,24 }}, GGL_BGRA },   // IEP_FORMAT_BGRA_8888
{  2, 16, {{ 0, 0,  16,11,  11, 5,   5, 0 }}, GGL_RGB  },   // IEP_FORMAT_RGB_565
{  2, 16, {{ 0, 0,   5, 0,  11, 5,  16,11 }}, GGL_RGB  },   // IEP_FORMAT_RGB_565
*/
enum {
    IEP_FORMAT_ARGB_8888    = 0x0,
    IEP_FORMAT_ABGR_8888    = 0x1,
    IEP_FORMAT_RGBA_8888    = 0x2,
    IEP_FORMAT_BGRA_8888    = 0x3,
    IEP_FORMAT_RGB_565      = 0x4,
    IEP_FORMAT_BGR_565      = 0x5,

    IEP_FORMAT_YCbCr_422_SP = 0x10,
    IEP_FORMAT_YCbCr_422_P  = 0x11,
    IEP_FORMAT_YCbCr_420_SP = 0x12,
    IEP_FORMAT_YCbCr_420_P  = 0x13,
    IEP_FORMAT_YCrCb_422_SP = 0x14,
    IEP_FORMAT_YCrCb_422_P  = 0x15,//same as IEP_FORMAT_YCbCr_422_P
    IEP_FORMAT_YCrCb_420_SP = 0x16,
    IEP_FORMAT_YCrCb_420_P  = 0x17,//same as IEP_FORMAT_YCbCr_420_P
}; //for format

typedef struct iep_img {
    uint16_t act_w;         // act_width
    uint16_t act_h;         // act_height
    int16_t x_off;         // x offset for the vir,word unit
    int16_t y_off;         // y offset for the vir,word unit

    uint16_t vir_w;         //unit :pix
    uint16_t vir_h;         //unit :pix
    uint32_t format;
    uint32_t mem_addr;
    uint32_t uv_addr;
    uint32_t v_addr;

    uint8_t rb_swap;//not be used
    uint8_t uv_swap;//not be used

    uint8_t alpha_swap;//not be used
} iep_img;


typedef struct IEP_MSG {
    iep_img src;    // src active window
    iep_img dst;    // src virtual window

    iep_img src1;
    iep_img dst1;

    iep_img src_itemp;
    iep_img src_ftemp;

    iep_img dst_itemp;
    iep_img dst_ftemp;

    uint8_t dither_up_en;
    uint8_t dither_down_en;//not to be used

    uint8_t yuv2rgb_mode;
    uint8_t rgb2yuv_mode;

    uint8_t global_alpha_value;

    uint8_t rgb2yuv_clip_en;
    uint8_t yuv2rgb_clip_en;

    uint8_t lcdc_path_en;
    int32_t off_x;
    int32_t off_y;
    int32_t width;
    int32_t height;
    int32_t layer;

    uint8_t yuv_3D_denoise_en;

    /// yuv color enhance
    uint8_t yuv_enhance_en;
    int32_t sat_con_int;
    int32_t contrast_int;
    int32_t cos_hue_int;
    int32_t sin_hue_int;
    int8_t yuv_enh_brightness;//-32<brightness<31
    uint8_t video_mode;//0-3
    uint8_t color_bar_y;//0-127
    uint8_t color_bar_u;//0-127
    uint8_t color_bar_v;//0-127


    uint8_t rgb_enhance_en;//i don't konw what is used

    uint8_t rgb_color_enhance_en;//sw_rgb_color_enh_en
    uint32_t rgb_enh_coe;

    uint8_t rgb_enhance_mode;//sw_rgb_enh_sel,dde sel

    uint8_t rgb_cg_en;//sw_rgb_con_gam_en
    uint32_t cg_tab[192];

    uint8_t rgb_contrast_enhance_mode;//sw_con_gam_order;0 cg prior to dde,1 dde prior to cg

    int32_t enh_threshold;
    int32_t enh_alpha;
    int32_t enh_radius;

    uint8_t scale_up_mode;

    uint8_t field_order;
    uint8_t dein_mode;
    //DIL HF
    uint8_t dein_high_fre_en;
    uint8_t dein_high_fre_fct;
    //DIL EI
    uint8_t dein_ei_mode;
    uint8_t dein_ei_smooth;
    uint8_t dein_ei_sel;
    uint8_t dein_ei_radius;//when dein_ei_sel=0 will be used

    uint8_t vir_addr_enable;

    void *base;
} IEP_MSG;

#endif
