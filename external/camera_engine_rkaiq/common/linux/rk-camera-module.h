/* SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR MIT) */
/*
 * Rockchip module information
 * Copyright (C) 2018-2019 Rockchip Electronics Co., Ltd.
 */

#ifndef _UAPI_RKMODULE_CAMERA_H
#define _UAPI_RKMODULE_CAMERA_H

#include <linux/types.h>

#define RKMODULE_NAME_LEN       32
#define RKMODULE_LSCDATA_LEN        441

#define RKMODULE_CAMERA_MODULE_INDEX    "rockchip,camera-module-index"
#define RKMODULE_CAMERA_MODULE_FACING   "rockchip,camera-module-facing"
#define RKMODULE_CAMERA_MODULE_NAME "rockchip,camera-module-name"
#define RKMODULE_CAMERA_LENS_NAME   "rockchip,camera-module-lens-name"

#define RKMODULE_GET_MODULE_INFO    \
    _IOR('V', BASE_VIDIOC_PRIVATE + 0, struct rkmodule_inf)

#define RKMODULE_AWB_CFG    \
    _IOW('V', BASE_VIDIOC_PRIVATE + 1, struct rkmodule_awb_cfg)

#define RKMODULE_AF_CFG \
    _IOW('V', BASE_VIDIOC_PRIVATE + 2, struct rkmodule_af_cfg)

#define RKMODULE_LSC_CFG    \
    _IOW('V', BASE_VIDIOC_PRIVATE + 3, struct rkmodule_lsc_cfg)

#define RKMODULE_GET_HDR_CFG    \
    _IOR('V', BASE_VIDIOC_PRIVATE + 4, struct rkmodule_hdr_cfg)

#define RKMODULE_SET_HDR_CFG    \
    _IOW('V', BASE_VIDIOC_PRIVATE + 5, struct rkmodule_hdr_cfg)

#define RKMODULE_SET_CONVERSION_GAIN    \
    _IOW('V', BASE_VIDIOC_PRIVATE + 6, __u32)

#define RKMODULE_GET_LVDS_CFG   \
    _IOR('V', BASE_VIDIOC_PRIVATE + 7, struct rkmodule_lvds_cfg)

#define RKMODULE_SET_DPCC_CFG   \
    _IOW('V', BASE_VIDIOC_PRIVATE + 8, struct rkmodule_dpcc_cfg)

#define RKMODULE_GET_NR_SWITCH_THRESHOLD    \
            _IOR('V', BASE_VIDIOC_PRIVATE + 9, struct rkmodule_nr_switch_threshold)

/**
 * struct rkmodule_base_inf - module base information
 *
 */
struct rkmodule_base_inf {
    char sensor[RKMODULE_NAME_LEN];
    char module[RKMODULE_NAME_LEN];
    char lens[RKMODULE_NAME_LEN];
} __attribute__ ((packed));

/**
 * struct rkmodule_fac_inf - module factory information
 *
 */
struct rkmodule_fac_inf {
    __u32 flag;

    char module[RKMODULE_NAME_LEN];
    char lens[RKMODULE_NAME_LEN];
    __u32 year;
    __u32 month;
    __u32 day;
} __attribute__ ((packed));

/**
 * struct rkmodule_awb_inf - module awb information
 *
 */
struct rkmodule_awb_inf {
    __u32 flag;

    __u32 r_value;
    __u32 b_value;
    __u32 gr_value;
    __u32 gb_value;

    __u32 golden_r_value;
    __u32 golden_b_value;
    __u32 golden_gr_value;
    __u32 golden_gb_value;
} __attribute__ ((packed));

/**
 * struct rkmodule_lsc_inf - module lsc information
 *
 */
struct rkmodule_lsc_inf {
    __u32 flag;

    __u16 lsc_w;
    __u16 lsc_h;
    __u16 decimal_bits;

    __u16 lsc_r[RKMODULE_LSCDATA_LEN];
    __u16 lsc_b[RKMODULE_LSCDATA_LEN];
    __u16 lsc_gr[RKMODULE_LSCDATA_LEN];
    __u16 lsc_gb[RKMODULE_LSCDATA_LEN];
} __attribute__ ((packed));

/**
 * struct rkmodule_af_inf - module af information
 *
 */
struct rkmodule_af_inf {
    __u32 flag;

    __u32 vcm_start;
    __u32 vcm_end;
    __u32 vcm_dir;
} __attribute__ ((packed));

/**
 * struct rkmodule_inf - module information
 *
 */
struct rkmodule_inf {
    struct rkmodule_base_inf base;
    struct rkmodule_fac_inf fac;
    struct rkmodule_awb_inf awb;
    struct rkmodule_lsc_inf lsc;
    struct rkmodule_af_inf af;
} __attribute__ ((packed));

/**
 * struct rkmodule_awb_inf - module awb information
 *
 */
struct rkmodule_awb_cfg {
    __u32 enable;
    __u32 golden_r_value;
    __u32 golden_b_value;
    __u32 golden_gr_value;
    __u32 golden_gb_value;
} __attribute__ ((packed));

/**
 * struct rkmodule_af_cfg
 *
 */
struct rkmodule_af_cfg {
    __u32 enable;
    __u32 vcm_start;
    __u32 vcm_end;
    __u32 vcm_dir;
} __attribute__ ((packed));

/**
 * struct rkmodule_lsc_cfg
 *
 */
struct rkmodule_lsc_cfg {
    __u32 enable;
} __attribute__ ((packed));

/**
 * NO_HDR: linear mode
 * HDR_X2: hdr two frame or line mode
 * HDR_X3: hdr three or line mode
 */
enum rkmodule_hdr_mode {
    NO_HDR = 0,
    HDR_X2 = 5,
    HDR_X3 = 6,
};

/**
 * HDR_NORMAL_VC: hdr frame with diff virtual channels
 * HDR_LINE_CNT: hdr frame with line counter
 * HDR_ID_CODE: hdr frame with identification code
 */
enum hdr_esp_mode {
    HDR_NORMAL_VC = 0,
    HDR_LINE_CNT,
    HDR_ID_CODE,
};

/**
 * lcnt: line counter
 *     padnum: the pixels of padding row
 *     padpix: the payload of padding
 * idcd: identification code
 *     efpix: identification code of Effective line
 *     obpix: identification code of OB line
 */
struct rkmodule_hdr_esp {
    enum hdr_esp_mode mode;
    union {
        struct {
            __u32 padnum;
            __u32 padpix;
        } lcnt;
        struct {
            __u32 efpix;
            __u32 obpix;
        } idcd;
    } val;
};

struct rkmodule_hdr_cfg {
    __u32 hdr_mode;
    struct rkmodule_hdr_esp esp;
} __attribute__ ((packed));

/* sensor lvds sync code
 * sav: start of active video codes
 * eav: end of active video codes
 */
struct rkmodule_sync_code {
    __u16 sav;
    __u16 eav;
};

/* sensor lvds difference sync code mode
 * LS_FIRST: valid line ls-le or sav-eav
 *     invalid line fs-fe or sav-eav
 * FS_FIRST: valid line fs-le
 *     invalid line ls-fe
 * ls: line start
 * le: line end
 * fs: frame start
 * fe: frame end
 */
enum rkmodule_lvds_mode {
    LS_FIRST = 0,
    FS_FIRST = 1,
};

/* struct rkmodule_lvds_cfg
 * act: valid line sync code
 * blk: invalid line sync code
 */
struct rkmodule_lvds_cfg {
    enum rkmodule_lvds_mode mode;
    struct rkmodule_sync_code act;
    struct rkmodule_sync_code blk;
} __attribute__ ((packed));

struct rkmodule_dpcc_cfg {
    __u32 enable;
    __u32 cur_single_dpcc;
    __u32 cur_multiple_dpcc;
    __u32 total_dpcc;
} __attribute__ ((packed));

/**
* nr switch by gain
* direct: 0 -> up_thres LNR to HNR, 1 -> up_thres HNR to LNR
* up_thres: threshold of nr change from low gain to high gain
* down_thres: threshold of nr change from high gain to low gain;
* div_coeff: Coefficients converted from float to int
*/
struct rkmodule_nr_switch_threshold {
    __u32 direct;
    __u32 up_thres;
    __u32 down_thres;
    __u32 div_coeff;
} __attribute__ ((packed));

#endif /* _UAPI_RKMODULE_CAMERA_H */
