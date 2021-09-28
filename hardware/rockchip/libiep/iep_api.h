#ifndef IEP_API_H_
#define IEP_API_H_

#include "iep.h"

typedef enum IEP_VIDEO_MODE {
    IEP_VIDEO_MODE_BLACK_SCREEN,
    IEP_VIDEO_MODE_BLUE_SCREEN,
    IEP_VIDEO_MODE_COLOR_BAR,
    IEP_VIDEO_MODE_NORMAL_VIDEO
} IEP_VIDEO_MODE_t;

typedef struct iep_param_YUV_color_enhance {
    float               yuv_enh_saturation; // [0, 1.992]
    float               yuv_enh_contrast;   // [0, 1.992]
    signed char         yuv_enh_brightness; // [-32, 31]
    float               yuv_enh_hue_angle;  // [-30, 30]
    IEP_VIDEO_MODE_t    video_mode;
    unsigned char       color_bar_y;        // [0, 127]
    unsigned char       color_bar_u;        // [0, 127]
    unsigned char       color_bar_v;        // [0, 127]
} iep_param_YUV_color_enhance_t;

typedef enum IEP_RGB_COLOR_ENHANCE_MODE {
    IEP_RGB_COLOR_ENHANCE_MODE_NO_OPERATION,
    IEP_RGB_COLOR_ENHANCE_MODE_DENOISE,
    IEP_RGB_COLOR_ENHANCE_MODE_DETAIL_ENHANCE,
    IEP_RGB_COLOR_ENHANCE_MODE_EDGE_ENHANCE
} IEP_RGB_COLOR_ENHANCE_MODE_t;

typedef enum IEP_RGB_COLOR_ENHANCE_ORDER {
    IEP_RGB_COLOR_ENHANCE_ORDER_CG_DDE, // CG(Contrast & Gammar) prior to DDE (De-noise, Detail & Edge Enhance)
    IEP_RGB_COLOR_ENHANCE_ORDER_DDE_CG  // DDE prior to CG
} IEP_RGB_COLOR_ENHANCE_ORDER_t;

typedef struct iep_param_RGB_color_enhance {
    float                           rgb_enh_coe;        // [0, 3.96875]
    IEP_RGB_COLOR_ENHANCE_MODE_t    rgb_enhance_mode;
    uint8_t                         rgb_cg_en;          //sw_rgb_con_gam_en
    double                          cg_rr;
    double                          cg_rg;
    double                          cg_rb;
    IEP_RGB_COLOR_ENHANCE_ORDER_t   rgb_contrast_enhance_mode;

    /// more than this value considered as detail, and less than this value considered as noise.
    int                             enh_threshold;      // [0, 255]

    /// combine the original pixel and enhanced pixel
    /// if (enh_alpha_num / enh_alpha_base <= 1 ) enh_alpha_base = 8, else enh_alpha_base = 4
    /// (1/8 ... 8/8) (5/4 ... 24/4)
    int                             enh_alpha_num;      // [0, 24]
    int                             enh_alpha_base;     // {4, 8}
    int                             enh_radius;         // [1, 4]
} iep_param_RGB_color_enhance_t;

typedef enum IEP_SCALE_UP_MODE {
    IEP_SCALE_UP_MODE_HERMITE,
    IEP_SCALE_UP_MODE_SPLINE,
    IEP_SCALE_UP_MODE_CATROM,
    IEP_SCALE_UP_MODE_MITCHELL
} IEP_SCALE_UP_MODE_t;

typedef struct iep_param_scale {
    IEP_SCALE_UP_MODE_t                 scale_up_mode;
    //unsigned int                    dst_width_tile0;    // [0, 4095]
    //unsigned int                    dst_width_tile1;    // [0, 4095]
    //unsigned int                    dst_width_tile2;    // [0, 4095]
    //unsigned int                    dst_width_tile3;    // [0, 4095]
} iep_param_scale_t;

typedef enum IEP_FIELD_ORDER
{
    FIELD_ORDER_TOP_FIRST,
    FIELD_ORDER_BOTTOM_FIRST
} IEP_FIELD_ORDER_t;

typedef enum IEP_YUV_DEINTERLACE_MODE {
    IEP_DEINTERLACE_MODE_DISABLE,
    IEP_DEINTERLACE_MODE_I2O1,
    IEP_DEINTERLACE_MODE_I4O1,
    IEP_DEINTERLACE_MODE_I4O2,
    IEP_DEINTERLACE_MODE_BYPASS
} IEP_YUV_DEINTERLACE_MODE_t;

typedef struct iep_param_yuv_deinterlace {
    uint8_t                     high_freq_en;
    IEP_YUV_DEINTERLACE_MODE_t  dil_mode;
    IEP_FIELD_ORDER_t           field_order;
    uint8_t                     dil_high_freq_fct;  // [0, 127]
    uint8_t                     dil_ei_mode;        // deinterlace edge interpolation 0: disable, 1: enable
    uint8_t                     dil_ei_smooth;      // deinterlace edge interpolation for smooth effect 0: disable, 1: enable
    uint8_t                     dil_ei_sel;         // deinterlace edge interpolation select
    uint8_t                     dil_ei_radius;      // deinterlace edge interpolation radius [0, 3]
} iep_param_yuv_deinterlace_t;

typedef enum IEP_COLOR_CONVERT_MODE {
    IEP_COLOR_CONVERT_MODE_BT601_L,
    IEP_COLOR_CONVERT_MODE_BT601_F,
    IEP_COLOR_CONVERT_MODE_BT709_L,
    IEP_COLOR_CONVERT_MODE_BT709_F
} IEP_COLOR_CONVERT_MODE_t;

typedef struct iep_param_color_space_convertion {
    IEP_COLOR_CONVERT_MODE_t        rgb2yuv_mode;
    IEP_COLOR_CONVERT_MODE_t        yuv2rgb_mode;
    unsigned char                   rgb2yuv_input_clip; // 0:R/G/B [0, 255], 1:R/G/B [16, 235]
    unsigned char                   yuv2rgb_input_clip; // 0:Y/U/V [0, 255], 1:Y [16, 235] U/V [16, 240]
    unsigned char                   global_alpha_value; // global alpha value for output ARGB
    unsigned char                   dither_up_en;
    unsigned char                   dither_down_en;
} iep_param_color_space_convertion_t;

typedef struct iep_param_direct_path_interface {
    uint8_t                         enable;
    int                             off_x;
    int                             off_y;
    int                             width;
    int                             height;
    uint8_t                         layer;
} iep_param_direct_path_interface_t;

typedef enum IEP_QUERY_INTERLACE {
    IEP_QUERY_INTERLACE_UNSUPPORTED,
    IEP_QUERY_INTERLACE_I2O1_ONLY,
    IEP_QUERY_INTERLACE_FULL_FUNC
} IEP_QUERY_INTERLACE;

typedef enum IEP_QUERY_DIMENSION {
    IEP_QUERY_DIMENSION_1920,
    IEP_QUERY_DIMENSION_4096
} IEP_QUERY_DIMENSION;

typedef void (*iep_notify)(int result);

class iep_interface {
public:
    virtual ~iep_interface() {;}

    virtual int init(iep_img *src, iep_img *dst) = 0;
    virtual int config_src_dst(iep_img *src, iep_img *dst) = 0;
    virtual int config_yuv_enh() = 0;
    virtual int config_yuv_enh(iep_param_YUV_color_enhance_t *yuv_enh) = 0;
    virtual int config_color_enh() = 0;
    virtual int config_color_enh(iep_param_RGB_color_enhance_t *rgb_enh) = 0;
    virtual int config_scale() = 0;
    virtual int config_scale(iep_param_scale_t *scale) = 0;
    virtual int config_yuv_denoise() = 0;
    virtual int config_yuv_denoise(iep_img *src_itemp, iep_img *src_ftemp, iep_img *dst_itemp, iep_img *dst_ftemp) = 0;
    virtual int config_yuv_deinterlace() = 0;
    virtual int config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil) = 0;
    virtual int config_yuv_dil_src_dst(iep_img *src1, iep_img *dst1) = 0;
    virtual int config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil, iep_img *src1, iep_img *dst1) = 0;
    virtual int config_color_space_convertion() = 0;
    virtual int config_color_space_convertion(iep_param_color_space_convertion_t *clr_convert) = 0;
    virtual int config_direct_lcdc_path(iep_param_direct_path_interface_t *dpi) = 0;
    virtual int run_sync() = 0;
    virtual int run_async(iep_notify notify) = 0;
    virtual int poll() = 0;
    virtual struct IEP_CAP* query() = 0;
    virtual IEP_QUERY_INTERLACE query_interlace() = 0;
    virtual IEP_QUERY_DIMENSION query_dimension() = 0;
    
    static iep_interface* create_new();
    static void reclaim(iep_interface *iep_inf);
};

/// for dlopen 
iep_interface* iep_interface_create_new();
void iep_interface_reclaim(iep_interface *iep_inf);

/// for "C"
struct iep_ops {
    void* (*claim)();
    int (*init)(void *iep_obj, iep_img *src, iep_img *dst);
    int (*config_src_dst)(void *iep_obj, iep_img *src, iep_img *dst);
    int (*config_yuv_enh)(void *iep_obj);
    int (*config_yuv_enh_param)(void *iep_obj, iep_param_YUV_color_enhance_t *yuv_enh);
    int (*config_color_enh)(void *iep_obj);
    int (*config_color_enh_param)(void *iep_obj, iep_param_RGB_color_enhance_t *rgb_enh);
    int (*config_scale)(void *iep_obj);
    int (*config_scale_param)(void *iep_obj, iep_param_scale_t *scale);
    int (*config_yuv_denoise)(void *iep_obj);
    int (*config_yuv_deinterlace)(void *iep_obj);
    int (*config_yuv_deinterlace_param1)(void *iep_obj, iep_param_yuv_deinterlace_t *yuv_dil);
    int (*config_yuv_dil_src_dst)(void *iep_obj, iep_img *src1, iep_img *dst1);
    int (*config_yuv_deinterlace_param2)(void *iep_obj, iep_param_yuv_deinterlace_t *yuv_dil, iep_img *src1, iep_img *dst1);
    int (*config_color_space_convertion)(void *iep_obj);
    int (*config_color_space_convertion_param)(void *iep_obj, iep_param_color_space_convertion_t *clr_convert);
    int (*config_direct_lcdc_path)(void *iep_obj, iep_param_direct_path_interface_t *dpi);
    int (*run_sync)(void *iep_obj);
    int (*run_async)(void *iep_obj, iep_notify notify);
    int (*poll)(void *iep_obj);
    void (*reclaim)(void *iep_obj);
};

struct iep_ops* alloc_iep_ops();
void free_iep_ops(struct iep_ops *ops);

#endif
