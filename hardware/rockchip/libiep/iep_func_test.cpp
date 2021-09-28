#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <memory.h>

#include <sys/mman.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "cutils/log.h"

#include "iep_api.h"
#include "iep.h"
#include "../librkvpu/vpu_mem.h"

typedef enum {
    TEST_CASE_NONE,
    TEST_CASE_YUVENHANCE,
    TEST_CASE_RGBENHANCE,
    TEST_CASE_DENOISE,
    TEST_CASE_DEINTERLACE
} TEST_CASE;

typedef struct mem_region {
    int phy_src;
    int phy_reg;
    int phy_dst;

    int len_src;
    int len_reg;
    int len_dst;

    uint8_t *vir_src;
    uint8_t *vir_reg;
    uint8_t *vir_dst;

    int src_fmt;
    int src_w;
    int src_h;
    char src_url[100];

    int dst_fmt;
    int dst_w;
    int dst_h;
    char dst_url[100];

    TEST_CASE testcase;
    char cfg_url[100];
} mem_region_t;

static int64_t GetTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return((int64_t)now.tv_sec) * 1000000 + ((int64_t)now.tv_usec);
}

static int read_line(FILE *file, char *str) {
    int ch;
    int i = 0;

    while (1) {
        ch = fgetc(file);
        if (ch == EOF || feof(file)) {
            return -1;
        } else if (ch == '\n') {
            break;
        }

        str[i++] = ch;
    }

    return 0;
}

static int parse_cfg_file(FILE *file, void *param) {
    char str[100];

    fseek(file, 0, SEEK_SET);
    memset(str, 0, sizeof(str));
    read_line(file, str);

    ALOGD("cfg title %s\n", str);

    if (strncmp(str, "deinterlace", strlen("deinterlace")) == 0) {
        iep_param_yuv_deinterlace_t *arg = (iep_param_yuv_deinterlace_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            if (strncmp(str, "high_freq_en", strlen("high_freq_en")) == 0) {
                sscanf(str, "high_freq_en %d", (int*)&arg->high_freq_en);
            } else if (strncmp(str, "dil_mode", strlen("dil_mode")) == 0) {
                sscanf(str, "dil_mode %d", (int*)&arg->dil_mode);
            } else if (strncmp(str, "dil_high_freq_fct", strlen("dil_high_freq_fct")) == 0) {
                sscanf(str, "dil_high_freq_fct %d", (int*)&arg->dil_high_freq_fct);
            } else if (strncmp(str, "dil_ei_mode", strlen("dil_ei_mode")) == 0) {
                sscanf(str, "dil_ei_mode %d", (int*)&arg->dil_ei_mode);
            } else if (strncmp(str, "dil_ei_smooth", strlen("dil_ei_smooth")) == 0) {
                sscanf(str, "dil_ei_smooth %d", (int*)&arg->dil_ei_smooth);
            } else if (strncmp(str, "dil_ei_sel", strlen("dil_ei_sel")) == 0) {
                sscanf(str, "dil_ei_sel %d", (int*)&arg->dil_ei_sel);
            } else if (strncmp(str, "dil_ei_radius", strlen("dil_ei_radius")) == 0) {
                sscanf(str, "dil_ei_radius %d", (int*)&arg->dil_ei_radius);
            }
        }
        while (ret == 0);
    } else if (strncmp(str, "yuv enhance", strlen("yuv enhance")) == 0) {
        iep_param_YUV_color_enhance_t *arg = (iep_param_YUV_color_enhance_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            ALOGD("parameter %s\t", str);
            if (strncmp(str, "yuv_enh_saturation", strlen("yuv_enh_saturation")) == 0) {
                sscanf(str, "yuv_enh_saturation %f", &arg->yuv_enh_saturation);
                ALOGD("value %f\n", arg->yuv_enh_saturation);
            } else if (strncmp(str, "yuv_enh_contrast", strlen("yuv_enh_contrast")) == 0) {
                sscanf(str, "yuv_enh_contrast %f", &arg->yuv_enh_contrast);
                ALOGD("value %f\n", arg->yuv_enh_contrast);
            } else if (strncmp(str, "yuv_enh_brightness", strlen("yuv_enh_brightness")) == 0) {
                sscanf(str, "yuv_enh_brightness %d", (int*)&arg->yuv_enh_brightness);
                ALOGD("value %d\n", arg->yuv_enh_brightness);
            } else if (strncmp(str, "yuv_enh_hue_angle", strlen("yuv_enh_hue_angle")) == 0) {
                sscanf(str, "yuv_enh_hue_angle %d", (int*)&arg->yuv_enh_hue_angle);
                ALOGD("value %d\n", arg->yuv_enh_hue_angle);
            } else if (strncmp(str, "video_mode", strlen("video_mode")) == 0) {
                sscanf(str, "video_mode %d", (int*)&arg->video_mode);
                ALOGD("value %d\n", arg->video_mode);
            } else if (strncmp(str, "color_bar_y", strlen("color_bar_y")) == 0) {
                sscanf(str, "color_bar_y %d", (int*)&arg->color_bar_y);
                ALOGD("value %d\n", arg->color_bar_y);
            } else if (strncmp(str, "color_bar_u", strlen("color_bar_u")) == 0) {
                sscanf(str, "color_bar_u %d", (int*)&arg->color_bar_u);
                ALOGD("value %d\n", arg->color_bar_u);
            } else if (strncmp(str, "color_bar_v", strlen("color_bar_v")) == 0) {
                sscanf(str, "color_bar_v %d", (int*)&arg->color_bar_v);
                ALOGD("value %d\n", arg->color_bar_v);
            }
        }
        while (ret == 0);
    } else if (strncmp(str, "rgb enhance", strlen("rgb enhance")) == 0) {
        iep_param_RGB_color_enhance_t *arg = (iep_param_RGB_color_enhance_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            ALOGD("parameter %s\t", str);
            if (strncmp(str, "rgb_enh_coe", strlen("rgb_enh_coe")) == 0) {
                sscanf(str, "rgb_enh_coe %f", &arg->rgb_enh_coe);
                ALOGD("value %f\n", arg->rgb_enh_coe);
            } else if (strncmp(str, "rgb_enhance_mode", strlen("rgb_enhance_mode")) == 0) {
                sscanf(str, "rgb_enhance_mode %d", (int*)&arg->rgb_enhance_mode);
                ALOGD("value %d\n", arg->rgb_enhance_mode);
            } else if (strncmp(str, "rgb_cg_en", strlen("rgb_cg_en")) == 0) {
                sscanf(str, "rgb_cg_en %d", (int*)&arg->rgb_cg_en);
                ALOGD("value %d\n", arg->rgb_cg_en);
            } else if (strncmp(str, "cg_rr", strlen("cg_rr")) == 0) {
                sscanf(str, "cg_rr %lf", &arg->cg_rr);
                ALOGD("value %f\n", arg->cg_rr);
            } else if (strncmp(str, "cg_rg", strlen("cg_rg")) == 0) {
                sscanf(str, "cg_rg %lf", &arg->cg_rg);
                ALOGD("value %f\n", arg->cg_rg);
            } else if (strncmp(str, "cg_rb", strlen("cg_rb")) == 0) {
                sscanf(str, "cg_rb %lf", &arg->cg_rb);
                ALOGD("value %f\n", arg->cg_rb);
            } else if (strncmp(str, "rgb_contrast_enhance_mode", strlen("rgb_contrast_enhance_mode")) == 0) {
                sscanf(str, "rgb_contrast_enhance_mode %d", (int*)&arg->rgb_contrast_enhance_mode);
                ALOGD("value %d\n", (int*)arg->rgb_contrast_enhance_mode);
            } else if (strncmp(str, "enh_threshold", strlen("enh_threshold")) == 0) {
                sscanf(str, "enh_threshold %d", (int*)&arg->enh_threshold);
                ALOGD("value %d\n", arg->enh_threshold);
            } else if (strncmp(str, "enh_alpha_num", strlen("enh_alpha_num")) == 0) {
                sscanf(str, "enh_alpha_num %d", (int*)&arg->enh_alpha_num);
                ALOGD("value %d\n", arg->enh_alpha_num);
            } else if (strncmp(str, "enh_alpha_base", strlen("enh_alpha_base")) == 0) {
                sscanf(str, "enh_alpha_base %d", (int*)&arg->enh_alpha_base);
                ALOGD("value %d\n", arg->enh_alpha_base);
            } else if (strncmp(str, "enh_radius", strlen("enh_radius")) == 0) {
                sscanf(str, "enh_radius %d", (int*)&arg->enh_radius);
                ALOGD("value %d\n", arg->enh_radius);
            }
        }
        while (ret == 0);
    } else if (strncmp(str, "scale", strlen("scale")) == 0) {
        iep_param_scale_t *arg = (iep_param_scale_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            if (strncmp(str, "scale_up_mode", strlen("scale_up_mode")) == 0) {
                sscanf(str, "scale_up_mode %d", (int*)&arg->scale_up_mode);
            }
        }
        while (ret == 0);
    } else if (strncmp(str, "color space convertion", strlen("color space convertion")) == 0) {
        iep_param_color_space_convertion_t *arg = (iep_param_color_space_convertion_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            if (strncmp(str, "rgb2yuv_mode", strlen("rgb2yuv_mode")) == 0) {
                sscanf(str, "rgb2yuv_mode %d", (int*)&arg->rgb2yuv_mode);
            } else if (strncmp(str, "yuv2rgb_mode", strlen("yuv2rgb_mode")) == 0) {
                sscanf(str, "yuv2rgb_mode %d", (int*)&arg->yuv2rgb_mode);
            } else if (strncmp(str, "rgb2yuv_input_clip", strlen("rgb2yuv_input_clip")) == 0) {
                sscanf(str, "rgb2yuv_input_clip %d", (int*)&arg->rgb2yuv_input_clip);
            } else if (strncmp(str, "yuv2rgb_input_clip", strlen("yuv2rgb_input_clip")) == 0) {
                sscanf(str, "yuv2rgb_input_clip %d", (int*)&arg->yuv2rgb_input_clip);
            } else if (strncmp(str, "global_alpha_value", strlen("global_alpha_value")) == 0) {
                sscanf(str, "global_alpha_value %d", (int*)&arg->global_alpha_value);
            } else if (strncmp(str, "dither_up_en", strlen("dither_up_en")) == 0) {
                sscanf(str, "dither_up_en %d", (int*)&arg->dither_up_en);
            } else if (strncmp(str, "dither_down_en", strlen("dither_down_en")) == 0) {
                sscanf(str, "dither_down_en %d", (int*)&arg->dither_down_en);
            }
        }
        while (ret == 0);
    } else if (strncmp(str, "direct lcdc path", strlen("direct lcdc path")) == 0) {
        iep_param_direct_path_interface_t *arg = (iep_param_direct_path_interface_t*)param;
        int ret;

        do {
            memset(str, 0, sizeof(str));
            ret = read_line(file, str);
            if (strncmp(str, "enable", strlen("enable")) == 0) {
                sscanf(str, "enable %d", (int*)&arg->enable);
            } else if (strncmp(str, "off_x", strlen("off_x")) == 0) {
                sscanf(str, "off_x %d", &arg->off_x);
            } else if (strncmp(str, "off_y", strlen("off_y")) == 0) {
                sscanf(str, "off_y %d", &arg->off_y);
            } else if (strncmp(str, "width", strlen("width")) == 0) {
                sscanf(str, "width %d", &arg->width);
            } else if (strncmp(str, "height", strlen("height")) == 0) {
                sscanf(str, "height %d", &arg->height);
            } else if (strncmp(str, "layer", strlen("layer")) == 0) {
                sscanf(str, "layer %d", (int*)&arg->layer);
            }
        }
        while (ret == 0);
    }

    return 0;
}

static void* iep_process_thread(void *param) 
{
    int i, cnt = 0;

    mem_region_t *mr = (mem_region_t*)param;

    uint32_t phy_src, phy_reg, phy_dst;
    int len_src, len_reg, len_dst;
    uint8_t *vir_reg, *vir_src, *vir_dst;
    iep_img src;
    iep_img dst;

    int datalen = 0;

    len_reg = mr->len_reg;
    len_src = mr->len_src;
    len_dst = mr->len_dst;

    phy_reg = mr->phy_reg;
    phy_src = mr->phy_src;
    phy_dst = mr->phy_dst;

    vir_reg = mr->vir_reg;
    vir_src = mr->vir_src;
    vir_dst = mr->vir_dst;

    iep_interface *api = iep_interface::create_new();

    FILE *srcfile = fopen(mr->src_url, "rb");
    FILE *dstfile = fopen(mr->dst_url, "wb");

    switch (mr->src_fmt) {
    case IEP_FORMAT_ABGR_8888:
    case IEP_FORMAT_ARGB_8888:
    case IEP_FORMAT_BGRA_8888:
    case IEP_FORMAT_RGBA_8888:
        datalen = mr->src_w * mr->src_h * 4;
        break;
    case IEP_FORMAT_BGR_565:
    case IEP_FORMAT_RGB_565:
    case IEP_FORMAT_YCbCr_422_P:
    case IEP_FORMAT_YCbCr_422_SP:
    case IEP_FORMAT_YCrCb_422_P:
    case IEP_FORMAT_YCrCb_422_SP:
        datalen = mr->src_w * mr->src_h * 2;
        src.v_addr = phy_src + mr->src_w * mr->src_h + mr->src_w * mr->src_h / 2;
        break;
    case IEP_FORMAT_YCbCr_420_P:
    case IEP_FORMAT_YCbCr_420_SP:
    case IEP_FORMAT_YCrCb_420_P:
    case IEP_FORMAT_YCrCb_420_SP:
        datalen = mr->src_w * mr->src_h * 3 / 2;
        src.v_addr = phy_src + mr->src_w * mr->src_h + mr->src_w * mr->src_h / 4;
        break;
    default:
        ;
    }

    ALOGD("%s %d\n", __func__, __LINE__);

    fread(vir_src, 1, datalen, srcfile);

    int64_t intime = GetTime();
    
    src.act_w = mr->src_w;
    src.act_h = mr->src_h;
    src.x_off = 0;
    src.y_off = 0;
    src.vir_w = mr->src_w;
    src.vir_h = mr->src_h;
    src.format = mr->src_fmt;
    src.mem_addr = phy_src;
    src.uv_addr  = phy_src + mr->src_w * mr->src_h;

    switch (mr->dst_fmt) {
    case IEP_FORMAT_ABGR_8888:
    case IEP_FORMAT_ARGB_8888:
    case IEP_FORMAT_BGRA_8888:
    case IEP_FORMAT_RGBA_8888:
        datalen = mr->dst_w * mr->dst_h * 4;
        break;
    case IEP_FORMAT_BGR_565:
    case IEP_FORMAT_RGB_565:
    case IEP_FORMAT_YCbCr_422_P:
    case IEP_FORMAT_YCbCr_422_SP:
    case IEP_FORMAT_YCrCb_422_P:
    case IEP_FORMAT_YCrCb_422_SP:
        datalen = mr->dst_w * mr->dst_h * 2;
        dst.v_addr = phy_dst + mr->dst_w * mr->dst_h + mr->dst_w * mr->dst_h / 2;
        break;
    case IEP_FORMAT_YCbCr_420_P:
    case IEP_FORMAT_YCbCr_420_SP:
    case IEP_FORMAT_YCrCb_420_P:
    case IEP_FORMAT_YCrCb_420_SP:
        datalen = mr->dst_w * mr->dst_h * 3 / 2;
        dst.v_addr = phy_dst + mr->dst_w * mr->dst_h + mr->dst_w * mr->dst_h / 4;
        break;
    default:
        ;
    }

    dst.act_w = mr->dst_w;
    dst.act_h = mr->dst_h;
    dst.x_off = 0;
    dst.y_off = 0;
    dst.vir_w = mr->dst_w;
    dst.vir_h = mr->dst_h;
    dst.format = mr->dst_fmt;
    dst.mem_addr = phy_dst;
    dst.uv_addr = phy_dst + mr->dst_w * mr->dst_h;

    api->init(&src, &dst);

    switch (mr->testcase) {
    case TEST_CASE_DENOISE:
        {
            FILE *cfgfile = fopen(mr->cfg_url, "r");

            if (cfgfile == NULL) {
                api->config_yuv_denoise();
            } else {
                ;
            }
        }
        break;
    case TEST_CASE_YUVENHANCE:
        {
            FILE *cfgfile = fopen(mr->cfg_url, "r");

            if (cfgfile == NULL) {
                api->config_yuv_enh();
            } else {
                iep_param_YUV_color_enhance_t yuvparam;
                parse_cfg_file(cfgfile, &yuvparam);
                api->config_yuv_enh(&yuvparam);
                fclose(cfgfile);
            }
        }
        break;
    case TEST_CASE_RGBENHANCE:
        {
            FILE *cfgfile = fopen(mr->cfg_url, "r");

            if (cfgfile == NULL) {
                api->config_color_enh();
            } else {
                iep_param_RGB_color_enhance_t rgbparam;
                parse_cfg_file(cfgfile, &rgbparam);
                api->config_color_enh(&rgbparam);
                fclose(cfgfile);
            }
        }
        break;
    case TEST_CASE_DEINTERLACE:
        {
            iep_img src1;
            iep_img dst1;
            iep_param_yuv_deinterlace_t yuv_dil;
            
            fread(vir_src + datalen, 1, datalen, srcfile);
    
            src1.act_w = mr->src_w;
            src1.act_h = mr->src_h;
            src1.x_off = 0;
            src1.y_off = 0;
            src1.vir_w = mr->src_w;
            src1.vir_h = mr->src_h;
            src1.format = mr->src_fmt;
            src1.mem_addr = phy_src + datalen;
            src1.uv_addr  = phy_src + datalen + mr->src_w * mr->src_h;

            dst1.act_w = mr->dst_w;
            dst1.act_h = mr->dst_h;
            dst1.x_off = 0;
            dst1.y_off = 0;
            dst1.vir_w = mr->dst_w;
            dst1.vir_h = mr->dst_h;
            dst1.format = mr->dst_fmt;
            dst1.mem_addr = phy_dst + datalen;
            dst1.uv_addr = phy_dst + datalen + mr->dst_w * mr->dst_h;

            FILE *cfgfile = fopen(mr->cfg_url, "r");

            if (cfgfile == NULL) {
                yuv_dil.high_freq_en = 1;
                yuv_dil.dil_mode = IEP_DEINTERLACE_MODE_I4O1;
                yuv_dil.field_order = FIELD_ORDER_BOTTOM_FIRST;
                yuv_dil.dil_ei_mode = 0;
                yuv_dil.dil_ei_smooth = 0;
                yuv_dil.dil_ei_sel = 0;
                yuv_dil.dil_ei_radius = 0;
            } else {
                parse_cfg_file(cfgfile, &yuv_dil);
                fclose(cfgfile);
            }
            
            api->config_yuv_deinterlace(&yuv_dil, &src1, &dst1);
        }
        break;
    default:
        ;
    }

#if 0
    iep_param_direct_path_interface_t dpi;

    dpi.enable = 1;
    dpi.off_x = 0;
    dpi.off_y = 0;
    dpi.width = mr->dst_w;
    dpi.height = mr->dst_h;
    dpi.layer = 1;
    
    if (0 > api->config_direct_lcdc_path(&dpi)) {
        ALOGE("Failure to Configure DIRECT LCDC PATH\n");
    }
#endif

    if (0 == api->run_sync()) {
        ALOGD("%d success\n", getpid());
    } else {
        ALOGE("%d failure\n", getpid());
    }

    ALOGD("%s consume %lld\n", __func__, GetTime() - intime);

    fwrite(vir_dst, 1, datalen, dstfile);

    fclose(srcfile);
    fclose(dstfile);

    iep_interface::reclaim(api);

    return NULL;
}

int main(int argc, char **argv)
{
    int i;
    pthread_t td;
    int ch;

    int pmem_phy_head;
    uint8_t *pmem_vir_head;

    VPUMemLinear_t vpumem; 
    mem_region_t mr;

    mr.src_fmt = IEP_FORMAT_YCbCr_420_SP;
    mr.src_w = 640;
    mr.src_h = 480;

    mr.dst_fmt = IEP_FORMAT_YCbCr_420_SP;
    mr.dst_w = 640;
    mr.dst_h = 480;

    /// get options
    opterr = 0;
    while ((ch = getopt(argc, argv, "f:w:h:c:F:W:H:C:t:x:")) != -1) {
        switch (ch) {
        case 'w':
            mr.src_w = atoi(optarg);
            break;
        case 'h':
            mr.src_h = atoi(optarg);
            break;
        case 'c':
            if (strcmp(optarg, "argb8888") == 0) {
                mr.src_fmt = IEP_FORMAT_ARGB_8888;
            } else if (strcmp(optarg, "abgr8888") == 0) {
                mr.src_fmt = IEP_FORMAT_ABGR_8888;
            } else if (strcmp(optarg, "rgba8888") == 0) {
                mr.src_fmt = IEP_FORMAT_BGRA_8888;
            } else if (strcmp(optarg, "bgra8888") == 0) {
                mr.src_fmt = IEP_FORMAT_BGRA_8888;
            } else if (strcmp(optarg, "rgb565") == 0) {
                mr.src_fmt = IEP_FORMAT_RGB_565;
            } else if (strcmp(optarg, "bgr565") == 0) {
                mr.src_fmt = IEP_FORMAT_BGR_565;
            } else if (strcmp(optarg, "yuv422sp") == 0) {
                mr.src_fmt = IEP_FORMAT_YCbCr_422_SP;
            } else if (strcmp(optarg, "yuv422p") == 0) {
                mr.src_fmt = IEP_FORMAT_YCbCr_422_P;
            } else if (strcmp(optarg, "yuv420sp") == 0) {
                mr.src_fmt = IEP_FORMAT_YCbCr_420_SP;
            } else if (strcmp(optarg, "yuv420p") == 0) {
                mr.src_fmt = IEP_FORMAT_YCbCr_420_P;
            } else if (strcmp(optarg, "yvu422sp") == 0) {
                mr.src_fmt = IEP_FORMAT_YCrCb_422_SP;
            } else if (strcmp(optarg, "yvu422p") == 0) {
                mr.src_fmt = IEP_FORMAT_YCrCb_422_P;
            } else if (strcmp(optarg, "yvu420sp") == 0) {
                mr.src_fmt = IEP_FORMAT_YCrCb_420_SP;
            } else if (strcmp(optarg, "yvu420p") == 0) {
                mr.src_fmt = IEP_FORMAT_YCrCb_420_P;
            }
            break;
        case 'W':
            mr.dst_w = atoi(optarg);
            break;
        case 'H':
            mr.dst_h = atoi(optarg);
            break;
        case 'C':
            if (strcmp(optarg, "argb8888") == 0) {
                mr.dst_fmt = IEP_FORMAT_ARGB_8888;
            } else if (strcmp(optarg, "abgr8888") == 0) {
                mr.dst_fmt = IEP_FORMAT_ABGR_8888;
            } else if (strcmp(optarg, "rgba8888") == 0) {
                mr.dst_fmt = IEP_FORMAT_BGRA_8888;
            } else if (strcmp(optarg, "bgra8888") == 0) {
                mr.dst_fmt = IEP_FORMAT_BGRA_8888;
            } else if (strcmp(optarg, "rgb565") == 0) {
                mr.dst_fmt = IEP_FORMAT_RGB_565;
            } else if (strcmp(optarg, "bgr565") == 0) {
                mr.dst_fmt = IEP_FORMAT_BGR_565;
            } else if (strcmp(optarg, "yuv422sp") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCbCr_422_SP;
            } else if (strcmp(optarg, "yuv422p") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCbCr_422_P;
            } else if (strcmp(optarg, "yuv420sp") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCbCr_420_SP;
            } else if (strcmp(optarg, "yuv420p") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCbCr_420_P;
            } else if (strcmp(optarg, "yvu422sp") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCrCb_422_SP;
            } else if (strcmp(optarg, "yvu422p") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCrCb_422_P;
            } else if (strcmp(optarg, "yvu420sp") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCrCb_420_SP;
            } else if (strcmp(optarg, "yvu420p") == 0) {
                mr.dst_fmt = IEP_FORMAT_YCrCb_420_P;
            }
            break;
        case 'f':
            ALOGD("input filename: %s\n", optarg);
            strcpy(mr.src_url, optarg);
            break;
        case 'F':
            ALOGD("output filename: %s\n", optarg);
            strcpy(mr.dst_url, optarg);
            break;
        case 't':
            if (strcmp(optarg, "denoise") == 0) {
                mr.testcase = TEST_CASE_DENOISE;
            } else if (strcmp(optarg, "yuvenhance") == 0) {
                mr.testcase = TEST_CASE_YUVENHANCE;
            } else if (strcmp(optarg, "rgbenhance") == 0) {
                mr.testcase = TEST_CASE_RGBENHANCE;
            } else if (strcmp(optarg, "deinterlace") == 0) {
                mr.testcase = TEST_CASE_DEINTERLACE;
            } else {
                mr.testcase = TEST_CASE_NONE;
            }
            break;
        case 'x':
            ALOGD("configure filename: %s\n", optarg);
            strcpy(mr.cfg_url, optarg);
            break;
        default:
            ;
        }
    }


    /// allocate in/out memory and initialize memory address
    VPUMallocLinear(&vpumem, 12<<20);

    pmem_phy_head = vpumem.phy_addr;//region.offset;
    pmem_vir_head = (uint8_t*)vpumem.vir_addr;

    mr.len_reg = 0x100;
    mr.len_src = 5<<20;
    mr.len_dst = 5<<20;

    mr.phy_reg = vpumem.phy_addr;
    pmem_phy_head += mr.len_reg;
    mr.phy_src = pmem_phy_head;
    pmem_phy_head += mr.len_src;
    mr.phy_dst = pmem_phy_head;
    pmem_phy_head += mr.len_dst;

    mr.vir_reg = (uint8_t*)vpumem.vir_addr;
    pmem_vir_head += mr.len_reg;
    mr.vir_src = pmem_vir_head;
    pmem_vir_head += mr.len_src;
    mr.vir_dst = pmem_vir_head;
    pmem_vir_head += mr.len_dst;

    pthread_create(&td, NULL, iep_process_thread, &mr);

    pthread_join(td, NULL);

    VPUFreeLinear(&vpumem);

    return 0;
}

