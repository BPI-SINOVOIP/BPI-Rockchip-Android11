
/******************************************************************************
 *
 * Copyright 2019, Fuzhou Rockchip Electronics Co.Ltd. All rights reserved.
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of:
 * Fuzhou Rockchip Electronics Co.Ltd .
 *
 *
 *****************************************************************************/
/**
 * @file    uvc_hal_types.h
 *
 *****************************************************************************/
#ifndef _UVC_HAL_TYPES_H_
#define _UVC_HAL_TYPES_H_

#define _VERSION_(a,b,c) (((a)<<16)+((b)<<8)+(c))

#define UVC_HAL_VERSION           _VERSION_(1, 0x0, 1)


enum UvcCmd{
    CMD_REBOOT = 1,
    CMD_SET_CAPS,
    CMD_GET_CAPS,
    CMD_SET_BLS,
    CMD_GET_BLS,
    CMD_SET_LSC,
    CMD_GET_LSC,
    CMD_SET_CCM,
    CMD_GET_CCM,
    CMD_SET_AWB,
    CMD_GET_AWB,
    CMD_SET_AWB_WP,
    CMD_GET_AWB_WP,
    CMD_SET_AWB_CURV,
    CMD_GET_AWB_CURV,
    CMD_SET_AWB_REFGAIN,
    CMD_GET_AWB_REFGAIN,
    CMD_SET_GOC,
    CMD_GET_GOC,
    CMD_SET_CPROC,
    CMD_GET_CPROC,
    CMD_SET_DPF,
    CMD_GET_DPF,
    CMD_SET_FLT,
    CMD_GET_FLT,
    CMD_GET_SYSINFO,
    CMD_GET_SENSOR_INFO,
    CMD_GET_PROTOCOL_VER,
    CMD_SET_EXPOSURE,
    CMD_SET_MIRROR,
    CMD_SET_SENSOR_REG,
    CMD_GET_SENSOR_REG
};

enum ISP_UVC_CMD_TYPE_e{
    CMD_TYPE_SYNC = 0xF,
    CMD_TYPE_ASYNC = 0x1F
};

typedef int (*vpu_encode_jpeg_init_t)(int width,int height,int quant);
typedef void (*uvc_set_run_state_t)(bool state);
typedef void (*vpu_encode_jpeg_done_t)();
typedef bool (*uvc_get_run_state_t)();
typedef unsigned int (*uvc_get_fcc_t)();
typedef void (*uvc_get_resolution_t)(int* width, int* height);
typedef void (*uvc_buffer_write_t)(void* extra_data,
                                        unsigned long extra_size,
                                        void* data,
                                        unsigned long size,
                                        unsigned int fcc);
typedef	int (*vpu_encode_jpeg_doing_t)(
                                void* srcbuf,
                                int src_fd,
                                unsigned long src_size);
typedef void (*vpu_encode_jpeg_set_encbuf_t)(int fd, void *viraddr, unsigned long phyaddr, unsigned int size);
typedef void (*vpu_encode_jpeg_get_encbuf_t)(unsigned char** jpeg_out, unsigned int *jpeg_len);
typedef bool (*uvc_buffer_write_enable_t)();
typedef int (*uvc_main_proc_t)();
typedef void (*uvc_getMsg_t)(void *pMsg);
typedef void (*uvc_sem_signal_t)();
typedef unsigned int (*uvc_getVersion_t)();


typedef struct uvc_vpu_ops_s {
    vpu_encode_jpeg_init_t encode_init;
    vpu_encode_jpeg_done_t encode_deinit;
    vpu_encode_jpeg_doing_t encode_process;
    vpu_encode_jpeg_set_encbuf_t encode_set_buf;
    vpu_encode_jpeg_get_encbuf_t encode_get_buf;
}uvc_vpu_ops_t;

typedef struct uvc_proc_ops_s {
    uvc_set_run_state_t set_state;
    uvc_get_run_state_t get_state;
    uvc_get_fcc_t get_fcc;
    uvc_get_resolution_t get_res;
    uvc_buffer_write_t transfer_data;
    uvc_buffer_write_enable_t transfer_data_enable;
    uvc_main_proc_t  uvc_main_proc;
    uvc_getMsg_t     uvc_getMessage;
    uvc_sem_signal_t uvc_signal;
    uvc_getVersion_t uvc_getVersion;
}uvc_proc_ops_t;

enum HAL_BLS_MODE {
  HAL_BLS_MODE_FIXED = 0,
  HAL_BLS_MODE_AUTO = 1
};

enum HAL_BLS_WINCFG {
  HAL_BLS_WINCFG_OFF = 0,
  HAL_BLS_WINCFG_WIN1 = 1,
  HAL_BLS_WINCFG_WIN2 = 2,
  HAL_BLS_WINCFG_WIN1_2 = 3
};

typedef struct HAL_Bls_Win {
  unsigned short h_offs;
  unsigned short v_offs;
  unsigned short width;
  unsigned short height;
} __attribute__((packed)) HAL_Bls_Win_t;

struct HAL_ISP_bls_cfg_s {
  enum HAL_BLS_MODE mode;
  enum HAL_BLS_WINCFG win_cfg;
  HAL_Bls_Win_t win1;
  HAL_Bls_Win_t win2;
  unsigned char samples;
  unsigned short fixed_red;
  unsigned short fixed_greenR;
  unsigned short fixed_greenB;
  unsigned short fixed_blue;
}__attribute__((packed));

#define HAL_ISP_LSC_NAME_LEN         25
#define HAL_ISP_LSC_SIZE_TBL_LEN     8
#define HAL_ISP_LSC_MATRIX_COLOR_NUM 4
#define HAL_ISP_LSC_MATRIX_TBL_LEN   289

struct HAL_ISP_Lsc_Profile_s {
  char    LscName[HAL_ISP_LSC_NAME_LEN];

  unsigned short  LscSectors;
  unsigned short  LscNo;
  unsigned short  LscXo;
  unsigned short  LscYo;

  unsigned short  LscXSizeTbl[HAL_ISP_LSC_SIZE_TBL_LEN];
  unsigned short  LscYSizeTbl[HAL_ISP_LSC_SIZE_TBL_LEN];

  unsigned short  LscMatrix[HAL_ISP_LSC_MATRIX_COLOR_NUM][HAL_ISP_LSC_MATRIX_TBL_LEN];
}__attribute__((packed));

struct HAL_ISP_Lsc_Query_s {
  char    LscNameUp[HAL_ISP_LSC_NAME_LEN];
  char    LscNameDn[HAL_ISP_LSC_NAME_LEN];
}__attribute__((packed));

#define HAL_ISP_ILL_NAME_LEN    20
struct HAL_ISP_AWB_CCM_SET_s {
  char ill_name[HAL_ISP_ILL_NAME_LEN];
#if 0
  float coeff00;
  float coeff01;
  float coeff02;
  float coeff10;
  float coeff11;
  float coeff12;
  float coeff20;
  float coeff21;
  float coeff22;
#else
  float coeff[9];
#endif

  float ct_offset_r;
  float ct_offset_g;
  float ct_offset_b;
}__attribute__((packed));

struct HAL_ISP_AWB_CCM_GET_s {
  char name_up[HAL_ISP_ILL_NAME_LEN];
  char name_dn[HAL_ISP_ILL_NAME_LEN];
  #if 0
  float coeff00;
  float coeff01;
  float coeff02;
  float coeff10;
  float coeff11;
  float coeff12;
  float coeff20;
  float coeff21;
  float coeff22;
  #else
  float coeff[9];
  #endif
  float ct_offset_r;
  float ct_offset_g;
  float ct_offset_b;
}__attribute__((packed));

struct HAL_ISP_AWB_s {
  float r_gain;
  float gr_gain;
  float gb_gain;
  float b_gain;
  unsigned char lock_ill;
  char ill_name[HAL_ISP_ILL_NAME_LEN];
}__attribute__((packed));

#define HAL_ISP_AWBFADE2PARM_LEN  6
struct HAL_ISP_AWB_White_Point_Set_s {
  unsigned short win_h_offs;
  unsigned short win_v_offs;
  unsigned short win_width;
  unsigned short win_height;
  unsigned char awb_mode;
  #if 1//awb_v11
  float afFade[HAL_ISP_AWBFADE2PARM_LEN];
  float afmaxCSum_br[HAL_ISP_AWBFADE2PARM_LEN];
  float afmaxCSum_sr[HAL_ISP_AWBFADE2PARM_LEN];
  float afminC_br[HAL_ISP_AWBFADE2PARM_LEN];
  float afMaxY_br[HAL_ISP_AWBFADE2PARM_LEN];
  float afMinY_br[HAL_ISP_AWBFADE2PARM_LEN];
  float afminC_sr[HAL_ISP_AWBFADE2PARM_LEN];
  float afMaxY_sr[HAL_ISP_AWBFADE2PARM_LEN];
  float afMinY_sr[HAL_ISP_AWBFADE2PARM_LEN];
  float afRefCb[HAL_ISP_AWBFADE2PARM_LEN];
  float afRefCr[HAL_ISP_AWBFADE2PARM_LEN];
#else//awb_v10
    float afFade[HAL_ISP_AWBFADE2PARM_LEN];
    float afCbMinRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afCrMinRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afMaxCSumRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afCbMinRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afCrMinRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afMaxCSumRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afMinCRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afMinCRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afMaxYRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afMaxYRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afMinYMaxGRegionMax[HAL_ISP_AWBFADE2PARM_LEN];
    float afMinYMaxGRegionMin[HAL_ISP_AWBFADE2PARM_LEN];
    float afRefCb[HAL_ISP_AWBFADE2PARM_LEN];
    float afRefCr[HAL_ISP_AWBFADE2PARM_LEN];
#endif
  float fRgProjIndoorMin;
  float fRgProjOutdoorMin;
  float fRgProjMax;
  float fRgProjMaxSky;
  float fRgProjALimit;
  float fRgProjAWeight;
  float fRgProjYellowLimitEnable;
  float fRgProjYellowLimit;
  float fRgProjIllToCwfEnable;
  float fRgProjIllToCwf;
  float fRgProjIllToCwfWeight;
  float fRegionSize;
  float fRegionSizeInc;
  float fRegionSizeDec;

  unsigned int cnt;
  unsigned char mean_y;
  unsigned char mean_cb;
  unsigned char mean_cr;
  unsigned short mean_r;
  unsigned short mean_b;
  unsigned short mean_g;
}__attribute__((packed));


struct HAL_ISP_AWB_White_Point_Get_s {
  unsigned short win_h_offs;
  unsigned short win_v_offs;
  unsigned short win_width;
  unsigned short win_height;
  unsigned char awb_mode;
  unsigned int cnt;
  unsigned char mean_y;
  unsigned char mean_cb;
  unsigned char mean_cr;
  unsigned short mean_r;
  unsigned short mean_b;
  unsigned short mean_g;

  unsigned char RefCr;
  unsigned char RefCb;
  unsigned char MinY;
  unsigned char MaxY;
  unsigned char MinC;
  unsigned char MaxCSum;

  float RgProjection;
  float RegionSize;
  float Rg_clipped;
  float Rg_unclipped;
  float Bg_clipped;
  float Bg_unclipped;
}__attribute__((packed));


#define HAL_ISP_CURVE_NAME_LEN    20
#define HAL_ISP_AWBCLIPPARM_LEN   16
struct HAL_ISP_AWB_Curve_s {
  float f_N0_Rg;
  float f_N0_Bg;
  float f_d;
  float Kfactor;

  float afRg1[HAL_ISP_AWBCLIPPARM_LEN];
  float afMaxDist1[HAL_ISP_AWBCLIPPARM_LEN];
  float afRg2[HAL_ISP_AWBCLIPPARM_LEN];
  float afMaxDist2[HAL_ISP_AWBCLIPPARM_LEN];
  float afGlobalFade1[HAL_ISP_AWBCLIPPARM_LEN];
  float afGlobalGainDistance1[HAL_ISP_AWBCLIPPARM_LEN];
  float afGlobalFade2[HAL_ISP_AWBCLIPPARM_LEN];
  float afGlobalGainDistance2[HAL_ISP_AWBCLIPPARM_LEN];
}__attribute__((packed));

struct HAL_ISP_AWB_RefGain_s {
  char ill_name[HAL_ISP_ILL_NAME_LEN];
  float refRGain;
  float refGrGain;
  float refGbGain;
  float refBGain;
}__attribute__((packed));

#define HAL_ISP_GOC_SCENE_NAME_LEN    20
//#ifdef RKISP_V12
#define HAL_ISP_GOC_GAMMA_NUM    34
//#else
//#define HAL_ISP_GOC_GAMMA_NUM    17
//#endif
enum HAL_ISP_GOC_WDR_STATUS {
  HAL_ISP_GOC_NORMAL,
  HAL_ISP_GOC_WDR_ON
};

enum HAL_ISP_GOC_CFG_MODE {
  HAL_ISP_GOC_CFG_MODE_LOGARITHMIC = 1,
  HAL_ISP_GOC_CFG_MODE_EQUIDISTANT
};

struct HAL_ISP_GOC_s {
  char scene_name[HAL_ISP_GOC_SCENE_NAME_LEN];
  enum HAL_ISP_GOC_WDR_STATUS wdr_status;
  enum HAL_ISP_GOC_CFG_MODE cfg_mode;
  unsigned short gamma_y[HAL_ISP_GOC_GAMMA_NUM];
}__attribute__((packed));

enum HAL_ISP_CPROC_MODE {
  HAL_ISP_CPROC_PREVIEW,
  HAL_ISP_CPROC_CAPTURE,
  HAL_ISP_CPROC_VIDEO
};

struct HAL_ISP_CPROC_s {
  enum HAL_ISP_CPROC_MODE mode;
  float cproc_contrast;
  float cproc_hue;
  float cproc_saturation;
  char cproc_brightness;
}__attribute__((packed));


#define HAL_ISP_ADPF_DPF_NAME_LEN  20
#define HAL_ISP_ADPF_DPF_NLL_COEFF_LEN  17
struct HAL_ISP_ADPF_DPF_s {
  char  dpf_name[HAL_ISP_ADPF_DPF_NAME_LEN];
  unsigned char dpf_enable;
  unsigned char nll_segment;
  unsigned short nll_coeff[HAL_ISP_ADPF_DPF_NLL_COEFF_LEN];
  unsigned short sigma_green;
  unsigned short sigma_redblue;
  float gradient;
  float offset;
  float fRed;
  float fGreenR;
  float fGreenB;
  float fBlue;
}__attribute__((packed));

#define HAL_ISP_FLT_CURVE_NUM 5
#define HAL_ISP_FLT_NAME_LEN  20
struct HAL_ISP_FLT_Denoise_Curve_s {
  unsigned char denoise_gain[HAL_ISP_FLT_CURVE_NUM];
  unsigned char denoise_level[HAL_ISP_FLT_CURVE_NUM];
}__attribute__((packed));

struct HAL_ISP_FLT_Sharp_Curve_s {
  unsigned char sharp_gain[HAL_ISP_FLT_CURVE_NUM];
  unsigned char sharp_level[HAL_ISP_FLT_CURVE_NUM];
}__attribute__((packed));

struct HAL_ISP_FLT_Level_Conf_s {
  unsigned char grn_stage1;
  unsigned char chr_h_mode;
  unsigned char chr_v_mode;
  #if 1
  unsigned int thresh_bl0;
  unsigned int thresh_bl1;
  unsigned int thresh_sh0;
  unsigned int thresh_sh1;
  unsigned int fac_sh1;
  unsigned int fac_sh0;
  unsigned int fac_mid;
  unsigned int fac_bl0;
  unsigned int fac_bl1;
  #else
  float thresh_bl0;
  float thresh_bl1;
  float thresh_sh0;
  float thresh_sh1;
  float fac_sh1;
  float fac_sh0;
  float fac_mid;
  float fac_bl0;
  float fac_bl1;
  #endif
}__attribute__((packed));

struct HAL_ISP_FLT_Set_s {
  char  filter_name[HAL_ISP_FLT_NAME_LEN];
  unsigned char scene_mode;
  unsigned char filter_enable;
  struct HAL_ISP_FLT_Denoise_Curve_s denoise;
  struct HAL_ISP_FLT_Sharp_Curve_s sharp;
  unsigned char level_conf_enable;
  unsigned char level;
  struct HAL_ISP_FLT_Level_Conf_s level_conf;
}__attribute__((packed));

struct HAL_ISP_FLT_Get_ParamIn_s {
  unsigned char scene;
  unsigned char level;
}__attribute__((packed));

struct HAL_ISP_FLT_Get_s {
  char  filter_name[HAL_ISP_FLT_NAME_LEN];
  unsigned char filter_enable;
  struct HAL_ISP_FLT_Denoise_Curve_s denoise;
  struct HAL_ISP_FLT_Sharp_Curve_s sharp;
  unsigned char level_conf_enable;
  unsigned char is_level_exit;
  struct HAL_ISP_FLT_Level_Conf_s level_conf;
}__attribute__((packed));

#define HAL_ISP_STORE_PATH_LEN         32
enum HAL_ISP_CAP_FORMAT {
  HAL_ISP_FMT_YUV420 = 0x18,
  HAL_ISP_FMT_YUV422 = 0x1E,
  HAL_ISP_FMT_RAW10 = 0x2B,
  HAL_ISP_FMT_RAW12 = 0x2C
};

enum HAL_ISP_CAP_RESULT {
  HAL_ISP_CAP_FINISH,
  HAL_ISP_CAP_RUNNING
};

enum HAL_ISP_AE_MODE {
  HAL_ISP_AE_MANUAL,
  HAL_ISP_AE_AUTO
};

struct HAL_ISP_Cap_Req_s {
  unsigned char cap_id;
  char  store_path[HAL_ISP_STORE_PATH_LEN];
  enum HAL_ISP_CAP_FORMAT cap_format;
  unsigned char cap_num;
  unsigned short cap_height;
  unsigned short cap_width;
  enum HAL_ISP_AE_MODE ae_mode;
  unsigned char exp_time_h;
  unsigned char exp_time_l;
  unsigned char exp_gain_h;
  unsigned char exp_gain_l;
  unsigned short af_code;
}__attribute__((packed));

struct HAL_ISP_Cap_Result_s {
  unsigned char cap_id;
  enum HAL_ISP_CAP_RESULT result;
}__attribute__((packed));


#define HAL_ISP_SYS_INFO_LEN         32
#define HAL_ISP_SENSOR_RESOLUTION_NUM         8

struct HAL_ISP_Sensor_Reso_s {
  unsigned short width;
  unsigned short height;
}__attribute__((packed));

struct HAL_ISP_OTP_Info_s {
  unsigned char awb_otp:1;
  unsigned char lsc_otp:1;
}__attribute__((packed));

struct HAL_ISP_Sys_Info_s {
  char  platform[HAL_ISP_SYS_INFO_LEN];
  char  sensor[HAL_ISP_SYS_INFO_LEN];
  char  module[HAL_ISP_SYS_INFO_LEN];
  char  lens[HAL_ISP_SYS_INFO_LEN];
  char  iq_name[HAL_ISP_SYS_INFO_LEN*2];
  unsigned char otp_flag;
  unsigned int otp_r_value;
  unsigned int otp_gr_value;
  unsigned int otp_gb_value;
  unsigned int otp_b_value;
  unsigned char max_exp_time_h;
  unsigned char max_exp_time_l;
  unsigned char max_exp_gain_h;
  unsigned char max_exp_gain_l;
  unsigned char reso_num;
  struct HAL_ISP_Sensor_Reso_s reso[HAL_ISP_SENSOR_RESOLUTION_NUM];
  unsigned char sensor_fmt;
  unsigned char bayer_pattern;
}__attribute__((packed));

struct HAL_ISP_Sensor_Mirror_s {
  unsigned char horizontal_mirror:1;
  unsigned char vertical_mirror:1;
}__attribute__((packed));

struct HAL_ISP_Sensor_Info_s {
  unsigned char exp_time_h;
  unsigned char exp_time_l;
  unsigned char exp_gain_h;
  unsigned char exp_gain_l;
  unsigned char mirror_info;
  unsigned short frame_length_lines;
  unsigned short line_length_pck;
  unsigned int vt_pix_clk_freq_hz;
  unsigned char binning;
  unsigned char black_white_mode;
}__attribute__((packed));

struct HAL_ISP_Sensor_Exposure_s {
  enum HAL_ISP_AE_MODE ae_mode;
  unsigned char exp_time_h;
  unsigned char exp_time_l;
  unsigned char exp_gain_h;
  unsigned char exp_gain_l;
}__attribute__((packed));

struct HAL_ISP_Sensor_Reg_s {
  unsigned char reg_addr_len;
  unsigned short reg_addr;
  unsigned char reg_data_len;
  unsigned short reg_data;
}__attribute__((packed));

#define HAL_ISP_IQ_PATH_LEN    32
struct HAL_ISP_Reboot_Req_s {
  unsigned char reboot;
  char  iq_path[HAL_ISP_IQ_PATH_LEN];
}__attribute__((packed));

struct HAL_ISP_Protocol_Ver_s {
    unsigned char major_ver;
    unsigned char minor_ver;
    unsigned int magicCode;
}__attribute__((packed));

#endif
