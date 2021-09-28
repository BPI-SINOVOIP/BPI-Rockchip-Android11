#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <cutils/properties.h>
#include "cutils/log.h"
#include "iep_api.h"
#include "rga.h"

#define PI  (3.1415926)

#define X   (-1)
static int enh_alpha_table[][25] = {
    //      0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19  20  21  22  23  24
    /*0*/  {X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X },
    /*1*/  {0,  8, 12, 16, 20, 24, 28,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X },
    /*2*/  {0,  4,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X,  X },
    /*3*/  {0,  X,  X,  8,  X,  X, 12,  X,  X, 16,  X,  X, 20,  X,  X, 24,  X,  X, 28,  X,  X,  X,  X,  X,  X },
    /*4*/  {0,  2,  4,  6,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 },
    /*5*/  {0,  X,  X,  X,  X,  8,  X,  X,  X,  X, 12,  X,  X,  X,  X, 16,  X,  X,  X,  X, 20,  X,  X,  X,  X },
    /*6*/  {0,  X,  X,  4,  X,  X,  8,  X,  X, 10,  X,  X, 12,  X,  X, 14,  X,  X, 16,  X,  X, 18,  X,  X, 20 },
    /*7*/  {0,  X,  X,  X,  X,  X,  X,  8,  X,  X,  X,  X,  X,  X, 12,  X,  X,  X,  X,  X,  X, 16,  X,  X,  X },
    /*8*/  {0,  1,  2,  3,  4,  5,  6,  7,  8,  X,  9,  X, 10,  X, 11,  X, 12,  X, 13,  X, 14,  X,  15, X, 16 }
};

// rr = 1.7 rg = 1 rb = 0.6
static unsigned int cg_tab[] =
{
0x01010100, 0x03020202, 0x04030303, 0x05040404, 
0x05050505, 0x06060606, 0x07070606, 0x07070707, 
0x08080807, 0x09080808, 0x09090909, 0x0a090909, 
0x0a0a0a0a, 0x0b0a0a0a, 0x0b0b0b0b, 0x0c0b0b0b, 
0x0c0c0c0c, 0x0c0c0c0c, 0x0d0d0d0d, 0x0d0d0d0d, 
0x0e0e0d0d, 0x0e0e0e0e, 0x0e0e0e0e, 0x0f0f0f0f, 
0x0f0f0f0f, 0x10100f0f, 0x10101010, 0x10101010, 
0x11111110, 0x11111111, 0x11111111, 0x12121212, 
0x12121212, 0x12121212, 0x13131313, 0x13131313, 
0x13131313, 0x14141414, 0x14141414, 0x14141414, 
0x15151515, 0x15151515, 0x15151515, 0x16161615, 
0x16161616, 0x16161616, 0x17161616, 0x17171717, 
0x17171717, 0x17171717, 0x18181818, 0x18181818, 
0x18181818, 0x19191818, 0x19191919, 0x19191919, 
0x19191919, 0x1a1a1a19, 0x1a1a1a1a, 0x1a1a1a1a, 
0x1a1a1a1a, 0x1b1b1b1b, 0x1b1b1b1b, 0x1b1b1b1b, 
0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c, 
0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c, 
0x23222120, 0x27262524, 0x2b2a2928, 0x2f2e2d2c, 
0x33323130, 0x37363534, 0x3b3a3938, 0x3f3e3d3c, 
0x43424140, 0x47464544, 0x4b4a4948, 0x4f4e4d4c, 
0x53525150, 0x57565554, 0x5b5a5958, 0x5f5e5d5c, 
0x63626160, 0x67666564, 0x6b6a6968, 0x6f6e6d6c, 
0x73727170, 0x77767574, 0x7b7a7978, 0x7f7e7d7c, 
0x83828180, 0x87868584, 0x8b8a8988, 0x8f8e8d8c, 
0x93929190, 0x97969594, 0x9b9a9998, 0x9f9e9d9c, 
0xa3a2a1a0, 0xa7a6a5a4, 0xabaaa9a8, 0xafaeadac, 
0xb3b2b1b0, 0xb7b6b5b4, 0xbbbab9b8, 0xbfbebdbc, 
0xc3c2c1c0, 0xc7c6c5c4, 0xcbcac9c8, 0xcfcecdcc, 
0xd3d2d1d0, 0xd7d6d5d4, 0xdbdad9d8, 0xdfdedddc, 
0xe3e2e1e0, 0xe7e6e5e4, 0xebeae9e8, 0xefeeedec, 
0xf3f2f1f0, 0xf7f6f5f4, 0xfbfaf9f8, 0xfffefdfc, 
0x06030100, 0x1b150f0a, 0x3a322922, 0x63584e44, 
0x95887b6f, 0xcebfb0a2, 0xfffeedde, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

#define IEP_DEB(fmt, args...)   do { \
                                    if (g_log_level > 0) { \
                                        ALOGD(__FILE__ ",%s,:%d: " fmt, __func__, __LINE__, ## args); \
                                    } \
                                } while (0)

#define IEP_ERR(fmt, args...) ALOGE(__FILE__ ",%s,:%d: " fmt, __func__, __LINE__, ## args)

class iep_api : public iep_interface {
private:
    IEP_MSG *msg;
    int fd;
    pthread_t td_notify;
    iep_notify notify;
    int pid;
    int rga_fd;
    int iommu;

public:
    iep_api();
    ~iep_api();

    virtual int init(iep_img *src, iep_img *dst);
    virtual int config_src_dst(iep_img *src, iep_img *dst);
    virtual int config_yuv_enh();
    virtual int config_yuv_enh(iep_param_YUV_color_enhance_t *yuv_enh);
    virtual int config_color_enh();
    virtual int config_color_enh(iep_param_RGB_color_enhance_t *rgb_enh);
    virtual int config_scale();
    virtual int config_scale(iep_param_scale *scale);
    virtual int config_yuv_denoise();
    virtual int config_yuv_denoise(iep_img *src_itemp, iep_img *src_ftemp,
                                   iep_img *dst_itemp, iep_img *dst_ftemp);
    virtual int config_yuv_deinterlace();
    virtual int config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil);
    virtual int config_yuv_dil_src_dst(iep_img *src1, iep_img *dst1);
    virtual int config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil,
                                       iep_img *src1, iep_img *dst1);
    virtual int config_color_space_convertion();
    virtual int config_color_space_convertion(
        iep_param_color_space_convertion_t *clr_convert);
    virtual int config_direct_lcdc_path(
        iep_param_direct_path_interface_t *dpi);
    virtual int run_sync();
    virtual int run_async(iep_notify notify);
    virtual int poll();
    virtual struct IEP_CAP* query();
    virtual IEP_QUERY_INTERLACE query_interlace();
    virtual IEP_QUERY_DIMENSION query_dimension();

private:
    static void* notify_func(void *param);

    int yuv_enh_sanity_check(iep_param_YUV_color_enhance_t *yuv_enh);
    int rgb_enh_sanity_check(iep_param_RGB_color_enhance_t *rgb_enh);
    int direct_lcdc_path_sanity_check(iep_param_direct_path_interface_t *dpi);
    int yuv_denoise_sanity_check(iep_img *src_itemp, iep_img *src_ftemp, iep_img *dst_itemp, iep_img *dst_ftemp);
    int dil_src_dst_sanity_check(iep_img *src1, iep_img *dst1);
    int deinterlace_sanity_check(iep_param_yuv_deinterlace_t *yuv_dil, iep_img *src1, iep_img *dst1);
    int color_space_convertion_sanity_check(iep_param_color_space_convertion_t *clr_convert);
    int init_sanity_check(iep_img *src, iep_img *dst);
    void recover_image(struct rga_req *req, iep_img *dst);
    void recover_image1(struct rga_req *req);
};

static int g_mode = 0;
static int g_log_level = 0;
static pthread_once_t g_get_env_value_once = PTHREAD_ONCE_INIT;
static struct IEP_CAP cap;

static void get_env_value()
{
    char prop_value[PROPERTY_VALUE_MAX];

    if (property_get("iep.mode.control", prop_value, NULL))
        g_mode = atoi(prop_value);

    if (property_get("iep.log_level.control", prop_value, NULL))
        g_log_level = atoi(prop_value);
}

iep_api::iep_api() : rga_fd(-1)
{
    //pthread_once(&g_get_env_value_once, get_env_value);
    get_env_value();

    ALOGD("g_mode %d, g_log_level %d\n", g_mode, g_log_level);

    msg = new IEP_MSG();
    if (msg == NULL) {
        IEP_ERR("memory allocated failed\n");
        abort();
    }

    memset(msg, 0, sizeof(IEP_MSG));

    fd = open("/dev/iep", O_RDWR);
    if (fd < 0) {
        IEP_ERR("file open failed\n");
        abort();
    }
    pid = getpid();

    IEP_DEB("query capabilities\n");
    if (0 > ioctl(fd, IEP_QUERY_CAP, &cap)) {
        IEP_DEB("Query IEP capability failed, using default cap\n");
        cap.scaling_supported = 0;
        cap.i4_deinterlace_supported = 1;
        cap.i2_deinterlace_supported = 1;
        cap.compression_noise_reduction_supported = 1;
        cap.sampling_noise_reduction_supported = 1;
        cap.hsb_enhancement_supported = 1;
        cap.cg_enhancement_supported = 1;
        cap.direct_path_supported = 1;
        cap.max_dynamic_width = 1920;
        cap.max_dynamic_height = 1088;
        cap.max_static_width = 8192;
        cap.max_static_height = 8192;
        cap.max_enhance_radius = 3;
    }

    iommu = 0;
    if (0 > ioctl(fd, IEP_GET_IOMMU_STATE, &iommu)) {
        IEP_DEB("Get iommu state failed, mismatch library and driver, disable contrast mode\n");
        g_mode = 0;
    }

    if (g_mode) {
        rga_fd = open("/dev/rga", O_RDWR);
        if (rga_fd < 0) {
           IEP_ERR("rga device open failed\n");
           abort();
        }
    }
}

iep_api::~iep_api()
{
    if (msg->lcdc_path_en) {
        iep_param_direct_path_interface_t dpi;
        dpi.enable = 0;
        config_direct_lcdc_path(&dpi);
        run_sync();
    }

    if (fd > 0) {
        close(fd);
    }

    if (rga_fd > 0) {
        close(rga_fd);
    }

    if (msg) {
        delete msg;
    }
}

int iep_api::config_yuv_enh()
{
    msg->yuv_enhance_en = 1;

    msg->sat_con_int        = 0x80;
    msg->contrast_int       = 0x80;
    // hue_angle = 0
    msg->cos_hue_int        = 0x00;
    msg->sin_hue_int        = 0x80;

    msg->yuv_enh_brightness = 0x00;

    msg->video_mode = IEP_VIDEO_MODE_NORMAL_VIDEO;

    msg->color_bar_u = 0;
    msg->color_bar_v = 0;
    msg->color_bar_y = 0;

    return 0;
}

int iep_api::config_yuv_enh(iep_param_YUV_color_enhance_t *yuv_enh)
{
    do {
        if (0 > yuv_enh_sanity_check(yuv_enh)) {
            break;
        }

        msg->yuv_enhance_en = 1;

        msg->sat_con_int        =
            (int)(yuv_enh->yuv_enh_saturation *
                  yuv_enh->yuv_enh_contrast * 128);
        msg->contrast_int       =
            (int)(yuv_enh->yuv_enh_contrast * 128);
        msg->cos_hue_int        =
            (int)(cos(yuv_enh->yuv_enh_hue_angle) * 128.0);
        msg->sin_hue_int        =
            (int)(sin(yuv_enh->yuv_enh_hue_angle) * 128.0);
        msg->yuv_enh_brightness =
            yuv_enh->yuv_enh_brightness >= 0 ?
            yuv_enh->yuv_enh_brightness : (yuv_enh->yuv_enh_brightness + 64);

        msg->video_mode  = yuv_enh->video_mode;
        msg->color_bar_y = yuv_enh->color_bar_y;
        msg->color_bar_u = yuv_enh->color_bar_u;
        msg->color_bar_v = yuv_enh->color_bar_v;

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_color_enh()
{
    //int i;

    msg->rgb_color_enhance_en = 1;

    msg->rgb_enh_coe               = 32;
    msg->rgb_contrast_enhance_mode = IEP_RGB_COLOR_ENHANCE_MODE_DETAIL_ENHANCE;
    msg->rgb_cg_en                 = 0;
    msg->enh_threshold             = 255;
    msg->enh_alpha                 = 8;
    msg->enh_radius                = 3;

    /*for (i=0; i<sizeof(cg_tab)/sizeof(unsigned int); i++) {
        msg->cg_tab[i] = cg_tab[i];
    }*/

    return 0;
}

int iep_api::config_color_enh(iep_param_RGB_color_enhance_t *rgb_enh)
{
    do {
        int i;
        unsigned int rgb_0, rgb_1, rgb_2, rgb_3;
        unsigned int cg_0, cg_1, cg_2, cg_3;
        unsigned int tab_0, tab_1, tab_2, tab_3;
        unsigned int conf_value;

        if (0 > rgb_enh_sanity_check(rgb_enh)) {
            break;
        }

        msg->rgb_color_enhance_en = 1;
        msg->rgb_enh_coe = (unsigned int)(rgb_enh->rgb_enh_coe * 32);
        msg->rgb_contrast_enhance_mode = rgb_enh->rgb_contrast_enhance_mode;
        msg->rgb_cg_en = rgb_enh->rgb_cg_en;
	msg->rgb_enhance_mode = rgb_enh->rgb_enhance_mode;

        msg->enh_threshold = rgb_enh->enh_threshold;
        msg->enh_alpha     =
            enh_alpha_table[rgb_enh->enh_alpha_base][rgb_enh->enh_alpha_num];
        msg->enh_radius    = rgb_enh->enh_radius - 1;

        if (rgb_enh->rgb_cg_en) {
            //for b
            for (i = 0; i < 64; i++) {
                rgb_0 = 4 * i;
                rgb_1 = 4 * i + 1;
                rgb_2 = 4 * i + 2;
                rgb_3 = 4 * i + 3;

                /// need modify later
                cg_0 = pow(rgb_0, rgb_enh->cg_rb);
                cg_1 = pow(rgb_1, rgb_enh->cg_rb);
                cg_2 = pow(rgb_2, rgb_enh->cg_rb);
                cg_3 = pow(rgb_3, rgb_enh->cg_rb);

                if (cg_0 > 255) cg_0 = 255;
                if (cg_1 > 255) cg_1 = 255;
                if (cg_2 > 255) cg_2 = 255;
                if (cg_3 > 255) cg_3 = 255;

                tab_0 = (unsigned int)cg_0;
                tab_1 = (unsigned int)cg_1;
                tab_2 = (unsigned int)cg_2;
                tab_3 = (unsigned int)cg_3;

                conf_value =
                    (tab_3 << 24) + (tab_2 << 16) + (tab_1 << 8) + tab_0;
                msg->cg_tab[i] = conf_value;
            }

            //for g
            for (i = 0; i < 64; i++) {
                rgb_0 = 4 * i;
                rgb_1 = 4 * i + 1;
                rgb_2 = 4 * i + 2;
                rgb_3 = 4 * i + 3;

                /// need to modify later
                cg_0 = pow(rgb_0, rgb_enh->cg_rg);
                cg_1 = pow(rgb_1, rgb_enh->cg_rg);
                cg_2 = pow(rgb_2, rgb_enh->cg_rg);
                cg_3 = pow(rgb_3, rgb_enh->cg_rg);

                if (cg_0 > 255) cg_0 = 255;
                if (cg_1 > 255) cg_1 = 255;
                if (cg_2 > 255) cg_2 = 255;
                if (cg_3 > 255) cg_3 = 255;

                tab_0 = (unsigned int)cg_0;
                tab_1 = (unsigned int)cg_1;
                tab_2 = (unsigned int)cg_2;
                tab_3 = (unsigned int)cg_3;

                conf_value =
                    (tab_3 << 24) + (tab_2 << 16) + (tab_1 << 8) + tab_0;
                msg->cg_tab[64+i] = conf_value;
            }

            //for r
            for (i = 0; i <= 63; i = i + 1) {
                rgb_0 = 4 * i;
                rgb_1 = 4 * i + 1;
                rgb_2 = 4 * i + 2;
                rgb_3 = 4 * i + 3;

                // need to modify later
                cg_0 = pow(rgb_0, rgb_enh->cg_rr);
                cg_1 = pow(rgb_1, rgb_enh->cg_rr);
                cg_2 = pow(rgb_2, rgb_enh->cg_rr);
                cg_3 = pow(rgb_3, rgb_enh->cg_rr);

                if (cg_0 > 255) cg_0 = 255;
                if (cg_1 > 255) cg_1 = 255;
                if (cg_2 > 255) cg_2 = 255;
                if (cg_3 > 255) cg_3 = 255;

                tab_0 = (unsigned int)cg_0;
                tab_1 = (unsigned int)cg_1;
                tab_2 = (unsigned int)cg_2;
                tab_3 = (unsigned int)cg_3;
                
                conf_value =
                    (tab_3 << 24) + (tab_2 << 16) + (tab_1 << 8) + tab_0;
                msg->cg_tab[2*64+i] = conf_value;
            }
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_scale()
{
    msg->scale_up_mode = IEP_SCALE_UP_MODE_CATROM;

    return 0;
}

int iep_api::config_scale(iep_param_scale *scale)
{
    do {
        if (scale == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        msg->scale_up_mode = scale->scale_up_mode;

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_yuv_denoise()
{
    /*do {
        msg->yuv_3D_denoise_en = 1;

        return 0;
    }
    while (0);*/
    IEP_ERR("NOT available in this version\n");

    return -1;
}

int iep_api::config_yuv_denoise(iep_img *src_itemp, iep_img *src_ftemp,
                                iep_img *dst_itemp, iep_img *dst_ftemp)
{
    do {
        if (0 > yuv_denoise_sanity_check(src_itemp, src_ftemp,
                                         dst_itemp, dst_ftemp)) {
            break;
        }

        memcpy(&msg->src_itemp, src_itemp, sizeof(iep_img));
        memcpy(&msg->src_ftemp, src_ftemp, sizeof(iep_img));
        memcpy(&msg->dst_itemp, dst_itemp, sizeof(iep_img));
        memcpy(&msg->dst_ftemp, dst_ftemp, sizeof(iep_img));

        if (g_mode) {
            msg->src_itemp.act_w /= 2;
            msg->src_ftemp.act_w /= 2;
            msg->dst_itemp.act_w /= 2;
            msg->dst_ftemp.act_w /= 2;
            msg->src_itemp.x_off = msg->src_itemp.act_w;
            msg->src_ftemp.x_off = msg->src_ftemp.act_w;
            msg->dst_itemp.x_off = msg->dst_itemp.act_w;
            msg->dst_ftemp.x_off = msg->dst_ftemp.act_w;
        }

        msg->yuv_3D_denoise_en = 1;

        return 0;
    } while (0);

    return -1;
}

int iep_api::config_yuv_deinterlace()
{
    msg->dein_high_fre_en    = 0;
    msg->dein_mode           = IEP_DEINTERLACE_MODE_I2O1;
    msg->field_order         = FIELD_ORDER_BOTTOM_FIRST;
    msg->dein_ei_mode        = 0;
    msg->dein_ei_sel         = 0;
    msg->dein_ei_radius      = 0;
    msg->dein_ei_smooth      = 0;
    msg->dein_high_fre_fct   = 0;

    return 0;
}

int iep_api::config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil)
{
    do {
        if (yuv_dil == NULL) {
            break;
        }

        msg->dein_high_fre_en    = yuv_dil->high_freq_en;
        msg->dein_mode           = yuv_dil->dil_mode;
        msg->field_order         = yuv_dil->field_order;
        msg->dein_ei_mode        = yuv_dil->dil_ei_mode;
        msg->dein_ei_sel         = yuv_dil->dil_ei_sel;
        msg->dein_ei_radius      = yuv_dil->dil_ei_radius;
        msg->dein_ei_smooth      = yuv_dil->dil_ei_smooth;
        msg->dein_high_fre_fct   = yuv_dil->dil_high_freq_fct;

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_yuv_dil_src_dst(iep_img *src1, iep_img *dst1)
{
    do {
        if (0 > dil_src_dst_sanity_check(src1, dst1)) {
            break;
        }

        if (src1 != NULL) {
            memcpy(&msg->src1, src1, sizeof(iep_img));
        }

        if (dst1 != NULL) {
            memcpy(&msg->dst1, dst1, sizeof(iep_img));
        }

        if (g_mode) {
            msg->src1.act_w /= 2;
            msg->dst1.act_w /= 2;
            msg->src1.x_off = msg->src1.act_w;
            msg->dst1.x_off = msg->dst1.act_w;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_yuv_deinterlace(iep_param_yuv_deinterlace_t *yuv_dil,
                                    iep_img *src1, iep_img *dst1)
{
    do {
        if (0 > deinterlace_sanity_check(yuv_dil, src1, dst1)) {
            break;
        }

        msg->dein_high_fre_en    = yuv_dil->high_freq_en;
        msg->dein_mode           = yuv_dil->dil_mode;
        msg->field_order         = yuv_dil->field_order;
        msg->dein_ei_mode        = yuv_dil->dil_ei_mode;
        msg->dein_ei_sel         = yuv_dil->dil_ei_sel;
        msg->dein_ei_radius      = yuv_dil->dil_ei_radius;
        msg->dein_ei_smooth      = yuv_dil->dil_ei_smooth;
        msg->dein_high_fre_fct   = yuv_dil->dil_high_freq_fct;

        if (src1 != NULL) {
            memcpy(&msg->src1, src1, sizeof(iep_img));
        }

        if (dst1 != NULL) {
            memcpy(&msg->dst1, dst1, sizeof(iep_img));
        }

        if (g_mode) {
            msg->src1.act_w /= 2;
            msg->dst1.act_w /= 2;
            msg->src1.x_off = msg->src1.act_w;
            msg->dst1.x_off = msg->dst1.act_w;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_color_space_convertion()
{
    msg->rgb2yuv_mode = IEP_COLOR_CONVERT_MODE_BT601_L;
    msg->yuv2rgb_mode = IEP_COLOR_CONVERT_MODE_BT601_L;

    msg->rgb2yuv_clip_en = 0;
    msg->yuv2rgb_clip_en = 0;

    msg->global_alpha_value = 0;

    msg->dither_up_en   = 1;
    msg->dither_down_en = 1;

    return 0;
}

int iep_api::config_color_space_convertion(
    iep_param_color_space_convertion_t *clr_convert)
{
    do {
        if (0 > color_space_convertion_sanity_check(clr_convert)) {
            break;
        }

        msg->rgb2yuv_mode        = clr_convert->rgb2yuv_mode;
        msg->yuv2rgb_mode        = clr_convert->yuv2rgb_mode;
        msg->rgb2yuv_clip_en     = clr_convert->rgb2yuv_input_clip;
        msg->yuv2rgb_clip_en     = clr_convert->yuv2rgb_input_clip;
        msg->global_alpha_value  = clr_convert->global_alpha_value;
        msg->dither_up_en        = clr_convert->dither_up_en;
        msg->dither_down_en      = clr_convert->dither_down_en;

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_direct_lcdc_path(iep_param_direct_path_interface_t *dpi)
{
    do {
        if (0 > direct_lcdc_path_sanity_check(dpi)) {
            break;
        }

        msg->lcdc_path_en = dpi->enable;
        msg->off_x  = dpi->off_x;
        msg->off_y  = dpi->off_y;
        msg->width  = dpi->width;
        msg->height = dpi->height;
        msg->layer  = dpi->layer;

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::config_src_dst(iep_img *src, iep_img *dst)
{
    do {
        if (0 > init_sanity_check(src, dst)) {
            break;
        }

        memcpy(&msg->src, src, sizeof(iep_img));
        memcpy(&msg->dst, dst, sizeof(iep_img));

        if (g_mode) {
            msg->src.act_w /= 2;
            msg->src.x_off = msg->src.act_w;
            msg->dst.act_w /= 2;
            msg->dst.x_off = msg->dst.act_w;
        }

        if ((src->format == IEP_FORMAT_YCbCr_420_P
            || src->format == IEP_FORMAT_YCbCr_420_SP
            || src->format == IEP_FORMAT_YCbCr_422_P
            || src->format == IEP_FORMAT_YCbCr_422_SP
            || src->format == IEP_FORMAT_YCrCb_420_P
            || src->format == IEP_FORMAT_YCrCb_420_SP
            || src->format == IEP_FORMAT_YCrCb_422_P
            || src->format == IEP_FORMAT_YCrCb_422_SP) &&
              msg->dein_mode == IEP_DEINTERLACE_MODE_DISABLE) {
            msg->dein_mode = IEP_DEINTERLACE_MODE_BYPASS;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::init(iep_img *src, iep_img *dst)
{
    do {
        if (0 > init_sanity_check(src, dst)) {
            break;
        }

        memset(msg, 0, sizeof(IEP_MSG));

        memcpy(&msg->src, src, sizeof(iep_img));
        memcpy(&msg->dst, dst, sizeof(iep_img));

        if (g_mode) {
            msg->src.act_w /= 2;
            msg->src.x_off = msg->src.act_w;
            msg->dst.act_w /= 2;
            msg->dst.x_off = msg->dst.act_w;
        }

        if ((src->format == IEP_FORMAT_YCbCr_420_P
            || src->format == IEP_FORMAT_YCbCr_420_SP
            || src->format == IEP_FORMAT_YCbCr_422_P
            || src->format == IEP_FORMAT_YCbCr_422_SP
            || src->format == IEP_FORMAT_YCrCb_420_P
            || src->format == IEP_FORMAT_YCrCb_420_SP
            || src->format == IEP_FORMAT_YCrCb_422_P
            || src->format == IEP_FORMAT_YCrCb_422_SP) &&
            msg->dein_mode == IEP_DEINTERLACE_MODE_DISABLE) {
            msg->dein_mode = IEP_DEINTERLACE_MODE_BYPASS;
        }

        return 0;
    }
    while (0);

    return -1;
}

void iep_api::recover_image(struct rga_req *req, iep_img *dst)
{
    if (iommu) {
        req->src.yrgb_addr = msg->src.mem_addr;
        req->src.uv_addr = 0;//msg->src.uv_addr;
        req->src.v_addr = 0;//msg->src.v_addr;
    } else {
        req->src.yrgb_addr = 0;
        req->src.uv_addr  = msg->src.mem_addr;
        req->src.v_addr   = 0;
    }
    req->src.vir_w = msg->src.vir_w;
    req->src.vir_h = msg->src.vir_h;
    req->src.format = RK_FORMAT_YCbCr_420_SP;
    req->src.alpha_swap |= 0;

    req->src.act_w = msg->src.act_w;
    req->src.act_h = msg->src.act_h;
    req->src.x_offset = 0;
    req->src.y_offset = 0;

    if (iommu) {
        req->dst.yrgb_addr = dst->mem_addr;
        req->dst.uv_addr  = 0;//msg->dst.uv_addr;
        req->dst.v_addr   = 0;//msg->dst.v_addr;
    } else {
        req->dst.yrgb_addr = 0;
        req->dst.uv_addr  = dst->mem_addr;
        req->dst.v_addr   = 0;
    }
    req->dst.vir_w = dst->vir_w;
    req->dst.vir_h = dst->vir_h;
    req->dst.format = RK_FORMAT_YCbCr_420_SP;
    req->clip.xmin = 0;
    req->clip.xmax = dst->vir_w - 1;
    req->clip.ymin = 0;
    req->clip.ymax = dst->vir_h - 1;
    req->dst.alpha_swap |= 0;

    req->dst.act_w = dst->act_w;
    req->dst.act_h = dst->act_h;
    req->dst.x_offset = 0;
    req->dst.y_offset = 0;

    IEP_DEB("src y %x u %x v %x, dst y %x u %x v %x\n",
            req->src.yrgb_addr, req->src.uv_addr, req->src.v_addr,
            req->dst.yrgb_addr, req->dst.uv_addr, req->dst.v_addr);

    IEP_DEB("src vir %d x %d, dst vir %d x %d\n", req->src.vir_w,
            req->src.vir_h, req->dst.vir_w, req->dst.vir_h);

    IEP_DEB("src act %d x %d, offset (%d, %d), dst act %d x %d, offset (%d, %d)\n",
            req->src.act_w, req->src.act_h, req->src.x_offset, req->src.y_offset,
            req->dst.act_w, req->dst.act_h, req->dst.x_offset, req->dst.y_offset);
}

int iep_api::run_sync()
{
    do {
        struct rga_req req;

        if (0 > ioctl(fd, IEP_SET_PARAMETER, (void *)msg)) {
            IEP_ERR("ioctl IEP_SET_PARAMETER failure\n");
            break;
        }

        if (g_mode) {
            memset(&req, 0, sizeof(struct rga_req));

            recover_image(&req, &msg->dst);

            if (iommu) {
                req.mmu_info.mmu_en = 1;
                req.mmu_info.mmu_flag = 1 | (1 << 8) | (1 << 10) | (1 << 31);
            } else {
                req.mmu_info.mmu_en = 0;
            }
            if (0 > ioctl(rga_fd, RGA_BLIT_SYNC, &req)) {
                IEP_ERR("RGA_BLIT_SYNC failed\n");
            }

            if (msg->dein_mode == IEP_DEINTERLACE_MODE_I4O2) {
                memset(&req, 0, sizeof(struct rga_req));
                recover_image(&req, &msg->dst1);
                if (iommu) {
                    req.mmu_info.mmu_en = 1;
                    req.mmu_info.mmu_flag = 1 | (1 << 8) | (1 << 10) | (1 << 31);
                } else {
                    req.mmu_info.mmu_en = 0;
                }
                if (0 > ioctl(rga_fd, RGA_BLIT_SYNC, &req)) {
                    IEP_ERR("RGA_BLIT_SYNC failed\n");
                }
            }
        }

        if (0 > ioctl(fd, IEP_GET_RESULT_SYNC, 0)) {
            IEP_ERR("%d, failure\n", pid);
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

void* iep_api::notify_func(void *param)
{
    iep_api *api = (iep_api *)param;
    int result = 0;

    fd_set rfds;
    struct timespec tv;
    int status;
    sigset_t set;

    FD_ZERO(&rfds);
    FD_SET(api->fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_nsec = 2e9;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    status = pselect(api->fd + 1, &rfds, NULL, NULL, &tv, &set);

    if (0 == status) {
        IEP_ERR("%d, timeout\n", api->pid);
        ioctl(api->fd, IEP_GET_RESULT_SYNC, NULL);
        result = -1;
    } else if (-1 == status) {
        IEP_ERR("%d, error\n", api->pid);
        ioctl(api->fd, IEP_RELEASE_CURRENT_TASK, NULL);
        result = -2;
    } else if (FD_ISSET(api->fd, &rfds)) {
        IEP_DEB("%d, success\n", api->pid);
        result = 0;
    }

    api->notify(result);

    return NULL;
}

int iep_api::run_async(iep_notify notify)
{
    do {

        if (notify != NULL) {
            this->notify = notify;
            if (0 != pthread_create(&td_notify, NULL, iep_api::notify_func, this)) {
                IEP_ERR("internal error\n");
                break;
            }
        }

        if (0 > ioctl(fd, IEP_SET_PARAMETER, msg)) {
            IEP_ERR("ioctl IEP_SET_PARAMETER failure\n");
            break;
        }

        if (0 > ioctl(fd, IEP_GET_RESULT_ASYNC, 0)) {
            IEP_ERR("%d, failure\n", pid);
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::poll() 
{
    int result = 0;

    fd_set rfds;
    struct timespec tv;
    int status;
    sigset_t set;

    FD_ZERO(&rfds);
    FD_SET(this->fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_nsec = 2e9;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    status = pselect(this->fd + 1, &rfds, NULL, NULL, &tv, &set);

    if (0 == status) {
        IEP_ERR("%d, timeout\n", this->pid);
        ioctl(this->fd, IEP_GET_RESULT_SYNC, NULL);
        result = -1;
    } else if (-1 == status) {
        IEP_ERR("%d, error\n", this->pid);
        ioctl(this->fd, IEP_RELEASE_CURRENT_TASK, NULL);
        result = -2;
    } else if (FD_ISSET(this->fd, &rfds)) {
        IEP_DEB("%d, success\n", this->pid);
        result = 0;
    }

    return result;
}

int iep_api::yuv_enh_sanity_check(iep_param_YUV_color_enhance_t *yuv_enh) {
    do {
        if (yuv_enh == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        if (yuv_enh->yuv_enh_saturation < 0 || yuv_enh->yuv_enh_saturation > 1.992) {
            IEP_ERR("Invalidate parameter, yuv_enh_saturation!\n");
            break;
        }

        if (yuv_enh->yuv_enh_contrast < 0 || yuv_enh->yuv_enh_contrast > 1.992) {
            IEP_ERR("Invalidate parameter, yuv_enh_contrast!\n");
            break;
        }

        if (yuv_enh->yuv_enh_brightness < -32 || yuv_enh->yuv_enh_brightness > 31) {
            IEP_ERR("Invalidate parameter, yuv_enh_brightness!\n");
            break;
        }

        if (yuv_enh->yuv_enh_hue_angle < -30 || yuv_enh->yuv_enh_hue_angle > 30) {
            IEP_ERR("Invalidate parameter, yuv_enh_hue_angle!\n");
            break;
        }

        if (yuv_enh->video_mode < 0 || yuv_enh->video_mode > 3) {
            IEP_ERR("Invalidate parameter, video_mode!\n");
            break;
        }

        if (yuv_enh->color_bar_y > 127 || yuv_enh->color_bar_u > 127 || yuv_enh->color_bar_v > 127) {
            IEP_ERR("Invalidate parameter, color_bar_*!\n");
            break;
        }

        if (msg->src.format >= 1 && msg->src.format <= 5 && msg->dst.format >= 1 && msg->dst.format <= 5) {
            IEP_ERR("Invalidate parameter, contradition between in/out format and yuv enhance\n");
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::rgb_enh_sanity_check(iep_param_RGB_color_enhance_t *rgb_enh)
{
    do {
        if (rgb_enh == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        if (rgb_enh->enh_alpha_base > 8 || rgb_enh->enh_alpha_base < 0
            || rgb_enh->enh_alpha_num < 0 || rgb_enh->enh_alpha_num > 24) {
            IEP_ERR("Invalidate parameter, enh_alpha_base or enh_alpha_num!\n");
            break;
        }

        if (enh_alpha_table[rgb_enh->enh_alpha_base][rgb_enh->enh_alpha_num] == -1) {
            IEP_ERR("Invalidate parameter, enh_alpha_base or enh_alpha_num!\n");
            break;
        }

        if (rgb_enh->enh_threshold > 255 || rgb_enh->enh_threshold < 0) {
            IEP_ERR("Invalidate parameter, enh_threshold!\n");
            break;
        }

        if (rgb_enh->enh_radius > 4 || rgb_enh->enh_radius < 1) {
            IEP_ERR("Invalidate parameter, enh_radius!\n");
            break;
        }

        if (rgb_enh->rgb_contrast_enhance_mode < 0 || rgb_enh->rgb_contrast_enhance_mode > 1) {
            IEP_ERR("Invalidate parameter, rgb_contrast_enhance_mode!\n");
            break;
        }

        if (rgb_enh->rgb_enhance_mode < 0 || rgb_enh->rgb_enhance_mode > 3) {
            IEP_ERR("Invalidate parameter, rgb_enhance_mode!\n");
            break;
        }

        if (rgb_enh->rgb_enh_coe > 3.96875 || rgb_enh->rgb_enh_coe < 0) {
            IEP_ERR("Invalidate parameter, rgb_enh_coe!\n");
            break;
        }

        if (msg->src.format >= 10 && msg->src.format <= 17 && msg->dst.format >= 10 && msg->dst.format <= 17) {
            IEP_ERR("Invalidate parameter, contradition between in/out format and yuv enhance\n");
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::color_space_convertion_sanity_check(iep_param_color_space_convertion_t *clr_convert)
{
    do {
        if (clr_convert == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        if (clr_convert->dither_up_en && msg->src.format != IEP_FORMAT_RGB_565) {
            IEP_ERR("Invalidate parameter, contradiction between dither up enable and source image format!\n");
            break;
        }

        if (clr_convert->dither_down_en && msg->dst.format != IEP_FORMAT_RGB_565) {
            IEP_ERR("Invalidate parameter, contradiction between dither down enable and destination image format!\n");
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

int iep_api::yuv_denoise_sanity_check(iep_img *src_itemp, iep_img *src_ftemp, iep_img *dst_itemp, iep_img *dst_ftemp)
{
    do {
        if (src_itemp == NULL || src_ftemp == NULL || dst_itemp == NULL || dst_ftemp == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }
                
        return 0;
    } while (0);
    
    return -1;
}

int iep_api::dil_src_dst_sanity_check(iep_img *src1, iep_img *dst1)
{
    do {
        switch (msg->dein_mode) {
        case IEP_DEINTERLACE_MODE_I4O2:
            if (dst1 == NULL) {
                IEP_ERR("Invalidate parameter!\n");
                goto err;
            } else if (!g_mode && (dst1->act_w != msg->dst.act_w || dst1->act_h != msg->dst.act_h)) {
                IEP_ERR("Invalidate parameter, contradiction between two destination image size!\n");
                goto err;
            }
        case IEP_DEINTERLACE_MODE_I4O1:
            if (src1 == NULL) {
                IEP_ERR("Invalidate parameter!\n");
                goto err;
            } else if (!g_mode && (src1->act_w != msg->src.act_w || src1->act_h != msg->src.act_h)) {
                IEP_ERR("Invalidate parameter, contradiction between two source image size!\n");
                goto err;
            }
        case IEP_DEINTERLACE_MODE_I2O1:
            if (msg->src.act_w > 1920) {
                goto err;
            }
            break;
        default:
            ;
        }

        return 0;
    }
    while (0);

err:
    return -1;
}

int iep_api::deinterlace_sanity_check(iep_param_yuv_deinterlace_t *yuv_dil, iep_img *src1, iep_img *dst1)
{
    do {
        if (yuv_dil == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        if (yuv_dil->dil_mode == IEP_DEINTERLACE_MODE_I4O2 && msg->lcdc_path_en) {
            IEP_ERR("Contradiction between dpi enable and deinterlace mode i4o2\n");
            break;
        }

        switch (yuv_dil->dil_mode) {
        case IEP_DEINTERLACE_MODE_I4O2:
            if (dst1 == NULL) {
                IEP_ERR("Invalidate parameter!\n");
                goto err;
            } else if (!g_mode && (dst1->act_w != msg->dst.act_w || dst1->act_h != msg->dst.act_h)) {
                IEP_ERR("Invalidate parameter, contradiction between two destination image size!\n");
                goto err;
            }
        case IEP_DEINTERLACE_MODE_I4O1:
            if (src1 == NULL) {
                IEP_ERR("Invalidate parameter!\n");
                goto err;
            } else if (!g_mode && (src1->act_w != msg->src.act_w || src1->act_h != msg->src.act_h)) {
                IEP_ERR("Invalidate parameter, contradiction between two source image size!\n");
                goto err;
            }
        case IEP_DEINTERLACE_MODE_I2O1:
            if (msg->src.act_w > 1920) {
                goto err;
            }
            break;
        default:
            ;
        }

        return 0;
    }
    while (0);

err:
    return -1;
}

int iep_api::direct_lcdc_path_sanity_check(iep_param_direct_path_interface_t *dpi)
{
    do {
        int fmt;

        if (dpi == NULL) {
            IEP_ERR("Invalid parameter\n");
            break;
        }

        if (msg->dein_mode == IEP_DEINTERLACE_MODE_I4O2) {
            IEP_ERR("Contradiction between dpi enable and deinterlace mode i4o2\n");
            break;
        }

        fmt = msg->dst.format;

        switch (fmt) {
        case IEP_FORMAT_YCbCr_422_P:
        case IEP_FORMAT_YCrCb_422_SP:
        case IEP_FORMAT_YCrCb_422_P:
        case IEP_FORMAT_YCrCb_420_SP:
        case IEP_FORMAT_YCbCr_420_P:
        case IEP_FORMAT_YCrCb_420_P:
        case IEP_FORMAT_RGBA_8888:
        case IEP_FORMAT_BGR_565:
            IEP_ERR("Contradiction between dpi and destination format\n");
            break;
        default:
            return 0;
        }
    }
    while (0);

    return -1;
}

int iep_api::init_sanity_check(iep_img *src, iep_img *dst)
{
    do {
        int scl_fct_v, scl_fct_h;
        
        if (src == NULL || dst == NULL) {
            IEP_ERR("Invalidate parameter!\n");
            break;
        }

        if ((src->format > 0x05 && src->format < 0x10) || src->format > 0x17) {
            IEP_ERR("Invalidate parameter, i/o format!\n");
            break;
        }

        if (src->act_w > 4096 || src->act_h > 8192) {
            IEP_ERR("Invalidate parameter, source image size!\n");
            break;
        }

        if (dst->act_w > 4096 || dst->act_h > 4096) {
            IEP_ERR("Invalidate parameter, destination image size!\n");
            break;
        }

        scl_fct_h = src->act_w > dst->act_w ? (src->act_w * 1000 / dst->act_w) : (dst->act_w * 1000 / src->act_w);
        scl_fct_v = src->act_h > dst->act_h ? (src->act_h * 1000 / dst->act_h) : (dst->act_h * 1000 / src->act_h);

        if (scl_fct_h > 16000 || scl_fct_v > 16000) {
            IEP_ERR("Invalidate parameter, scale factor!\n");
            break;
        }

        return 0;
    }
    while (0);

    return -1;
}

struct IEP_CAP* iep_api::query()
{
    return &cap;
}

IEP_QUERY_INTERLACE iep_api::query_interlace()
{
    if (cap.i4_deinterlace_supported) {
        return IEP_QUERY_INTERLACE_FULL_FUNC;
    } else if (cap.i2_deinterlace_supported) {
        return IEP_QUERY_INTERLACE_I2O1_ONLY;
    } else {
        return IEP_QUERY_INTERLACE_UNSUPPORTED;
    }
}

IEP_QUERY_DIMENSION iep_api::query_dimension()
{
    if (cap.max_dynamic_width > 1920) {
        return IEP_QUERY_DIMENSION_4096;
    } else {
        return IEP_QUERY_DIMENSION_1920;
    }
}

iep_interface* iep_interface::create_new() 
{
    return new iep_api();
}

void iep_interface::reclaim(iep_interface *iep_inf)
{
    if (iep_inf) {
        delete iep_inf;
        iep_inf = NULL;
    }
}

iep_interface* iep_interface_create_new()
{
    return iep_interface::create_new();
}

void iep_interface_reclaim(iep_interface *iep_inf)
{
    iep_interface::reclaim(iep_inf);
}

extern "C"
void* iep_ops_claim()
{
    return iep_interface::create_new();
}

extern "C"
int iep_ops_init(void *iep_obj, iep_img *src, iep_img *dst)
{
    return ((iep_interface*)iep_obj)->init(src, dst);
}

extern "C"
int iep_ops_init_discrete(void *iep_obj, 
                          int32_t src_act_w, int32_t src_act_h, 
                          int32_t src_x_off, int32_t src_y_off,
                          int32_t src_vir_w, int32_t src_vir_h, 
                          int32_t src_format, 
                          uint32_t src_mem_addr, uint32_t src_uv_addr, uint32_t src_v_addr,
                          int32_t dst_act_w, int32_t dst_act_h, 
                          int32_t dst_x_off, int32_t dst_y_off,
                          int32_t dst_vir_w, int32_t dst_vir_h, 
                          int32_t dst_format, 
                          uint32_t dst_mem_addr, uint32_t dst_uv_addr, uint32_t dst_v_addr)
{
    iep_img src;
    iep_img dst;

    src.act_w    = src_act_w;
    src.act_h    = src_act_h;
    src.x_off    = src_x_off;
    src.y_off    = src_y_off;
    src.vir_w    = src_vir_w;
    src.vir_h    = src_vir_h;
    src.format   = src_format;
    src.mem_addr = src_mem_addr;
    src.uv_addr  = src_uv_addr;
    src.v_addr   = src_v_addr;

    dst.act_w    = dst_act_w;
    dst.act_h    = dst_act_h;
    dst.x_off    = dst_x_off;
    dst.y_off    = dst_y_off;
    dst.vir_w    = dst_vir_w;
    dst.vir_h    = dst_vir_h;
    dst.format   = dst_format;
    dst.mem_addr = dst_mem_addr;
    dst.uv_addr  = dst_uv_addr;
    dst.v_addr   = dst_v_addr;

    ALOGE("%s, (%d, %d), (%d, %d), (%d, %d), %d, %x, %x, %x\n", __func__,
          src.act_w, src.act_h, src.x_off, src.y_off,
          src.vir_w, src.vir_h, src.format,
          src.mem_addr, src.uv_addr, src.v_addr);

    ALOGE("%s, (%d, %d), (%d, %d), (%d, %d), %d, %x, %x, %x\n", __func__,
          dst.act_w, dst.act_h, dst.x_off, dst.y_off,
          dst.vir_w, dst.vir_h, dst.format,
          dst.mem_addr, dst.uv_addr, dst.v_addr);

    return ((iep_interface*)iep_obj)->init(&src, &dst);
}

extern "C"
int iep_ops_config_src_dst(void *iep_obj, iep_img *src, iep_img *dst)
{
    return ((iep_interface*)iep_obj)->config_src_dst(src, dst);
}

extern "C"
int iep_ops_config_yuv_enh(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_yuv_enh();
}

extern "C"
int iep_ops_config_yuv_enh_param(void *iep_obj, iep_param_YUV_color_enhance_t *yuv_enh)
{
    return ((iep_interface*)iep_obj)->config_yuv_enh(yuv_enh);
}

extern "C"
int iep_ops_config_color_enh(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_color_enh();
}

extern "C"
int iep_ops_config_color_enh_param(void *iep_obj, iep_param_RGB_color_enhance_t *rgb_enh)
{
    return ((iep_interface*)iep_obj)->config_color_enh(rgb_enh);
}

extern "C"
int iep_ops_config_scale(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_scale();
}

extern "C"
int iep_ops_config_scale_param(void *iep_obj, iep_param_scale_t *scale)
{
    return ((iep_interface*)iep_obj)->config_scale(scale);
}

extern "C"
int iep_ops_config_yuv_denoise(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_yuv_denoise();
}

extern "C"
int iep_ops_config_yuv_deinterlace(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_yuv_deinterlace();
}

extern "C"
int iep_ops_config_yuv_deinterlace_param1(void *iep_obj, iep_param_yuv_deinterlace_t *yuv_dil)
{
    return ((iep_interface*)iep_obj)->config_yuv_deinterlace(yuv_dil);
}

extern "C"
int iep_ops_config_yuv_dil_src_dst(void *iep_obj, iep_img *src1, iep_img *dst1)
{
    return ((iep_interface*)iep_obj)->config_yuv_dil_src_dst(src1, dst1);
}

extern "C"
int iep_ops_config_yuv_deinterlace_param2(void *iep_obj, iep_param_yuv_deinterlace_t *yuv_dil, iep_img *src1, iep_img *dst1)
{
    return ((iep_interface*)iep_obj)->config_yuv_deinterlace(yuv_dil, src1, dst1);
}

extern "C"
int iep_ops_config_color_space_convertion(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->config_color_space_convertion();
}

extern "C"
int iep_ops_config_color_space_convertion_param(void *iep_obj, iep_param_color_space_convertion_t *clr_convert)
{
    return ((iep_interface*)iep_obj)->config_color_space_convertion(clr_convert);
}

extern "C"
int iep_ops_config_direct_lcdc_path(void *iep_obj, iep_param_direct_path_interface_t *dpi)
{
    return ((iep_interface*)iep_obj)->config_direct_lcdc_path(dpi);
}

extern "C"
int iep_ops_run_sync(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->run_sync();
}

extern "C"
int iep_ops_run_async_ncb(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->run_async(NULL);
}

extern "C"
int iep_ops_run_async(void *iep_obj, iep_notify notify)
{
    return ((iep_interface*)iep_obj)->run_async(notify);
}

extern "C"
int iep_ops_poll(void *iep_obj)
{
    return ((iep_interface*)iep_obj)->poll();
}

extern "C"
void iep_ops_reclaim(void *iep_obj)
{
    iep_interface::reclaim((iep_interface*)iep_obj);
}

struct iep_ops* alloc_iep_ops()
{
    struct iep_ops *ops = (struct iep_ops*)malloc(sizeof(struct iep_ops));

    if (ops == NULL) {
        IEP_ERR("allocate structure iep_ops failure\n");
        return NULL;
    }

    ops->claim = iep_ops_claim;
    ops->init = iep_ops_init;
    ops->config_src_dst = iep_ops_config_src_dst;
    ops->config_yuv_enh = iep_ops_config_yuv_enh;
    ops->config_yuv_enh_param = iep_ops_config_yuv_enh_param;
    ops->config_color_enh = iep_ops_config_color_enh;
    ops->config_color_enh_param = iep_ops_config_color_enh_param;
    ops->config_scale = iep_ops_config_scale;
    ops->config_scale_param = iep_ops_config_scale_param;
    ops->config_yuv_denoise = iep_ops_config_yuv_denoise;
    ops->config_yuv_deinterlace = iep_ops_config_yuv_deinterlace;
    ops->config_yuv_deinterlace_param1 = iep_ops_config_yuv_deinterlace_param1;
    ops->config_yuv_dil_src_dst = iep_ops_config_yuv_dil_src_dst;
    ops->config_yuv_deinterlace_param2 = iep_ops_config_yuv_deinterlace_param2;
    ops->config_color_space_convertion = iep_ops_config_color_space_convertion;
    ops->config_color_space_convertion_param = iep_ops_config_color_space_convertion_param;
    ops->config_direct_lcdc_path = iep_ops_config_direct_lcdc_path;
    ops->run_sync = iep_ops_run_sync;
    ops->run_async = iep_ops_run_async;
    ops->poll = iep_ops_poll;
    ops->reclaim = iep_ops_reclaim;

    return ops;
}

void free_iep_ops(struct iep_ops *ops)
{
    if (ops != NULL) {
        free(ops);
    }
}

