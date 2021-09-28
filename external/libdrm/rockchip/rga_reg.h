/*
 * Copyright (C) 2016 Fuzhou Electronics Co.Ltd
 * Authors:
 *	Yakir Yang <ykk@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _RGA_REG_H_
#define _RGA_REG_H_

#define MODE_CTRL			0x0100
#define SRC_INFO			0x0104
#define SRC_Y_RGB_BASE_ADDR		0x0108
#define SRC_CB_BASE_ADDR		0x010c
#define SRC_CR_BASE_ADDR		0x0110
#define SRC1_RGB_BASE_ADDR		0x0114
#define SRC_VIR_INFO			0x0118
#define SRC_ACT_INFO			0x011c
#define SRC_X_FACTOR			0x0120
#define SRC_Y_FACTOR			0x0124
#define SRC_BG_COLOR			0x0128
#define SRC_FG_COLOR			0x012c
#define SRC_TR_COLOR0			0x0130
#define SRC_TR_COLOR1			0x0134

#define DST_INFO			0x0138
#define DST_Y_RGB_BASE_ADDR		0x013c
#define DST_CB_BASE_ADDR		0x0140
#define DST_CR_BASE_ADDR		0x0144
#define DST_VIR_INFO			0x0148
#define DST_ACT_INFO			0x014c

#define ALPHA_CTRL0			0x0150
#define ALPHA_CTRL1			0x0154
#define FADING_CTRL			0x0158
#define PAT_CON				0x015c
#define ROP_CON0			0x0160
#define ROP_CON1			0x0164
#define MASK_BASE			0x0168

#define MMU_CTRL1			0x016c
#define MMU_SRC_BASE			0x0170
#define MMU_SRC1_BASE			0x0174
#define MMU_DST_BASE			0x0178
#define MMU_ELS_BASE			0x017c

enum e_rga_render_mode {
	RGA_MODE_RENDER_BITBLT = 0,
	RGA_MODE_RENDER_COLOR_PALETTE = 1,
	RGA_MODE_RENDER_RECTANGLE_FILL = 2,
	RGA_MODE_RENDER_UPDATE_PALETTE_LUT_RAM = 3,
};

enum e_rga_bitblt_mode {
	RGA_MODE_BITBLT_MODE_SRC_TO_DST = 0,
	RGA_MODE_BITBLT_MODE_SRC_SRC1_TO_DST = 1,
};

enum e_rga_cf_rop4_pat {
	RGA_MODE_CF_ROP4_SOLID = 0,
	RGA_MODE_CF_ROP4_PATTERN = 1,
};

enum e_rga_src_color_format {
	RGA_SRC_COLOR_FMT_ABGR8888 = 0,
	RGA_SRC_COLOR_FMT_XBGR8888 = 1,
	RGA_SRC_COLOR_FMT_RGB888 = 2,
	RGA_SRC_COLOR_FMT_RGB565 = 4,
	RGA_SRC_COLOR_FMT_ARGB1555 = 5,
	RGA_SRC_COLOR_FMT_ARGB4444 = 6,
	RGA_SRC_COLOR_FMT_YUV422SP = 8,
	RGA_SRC_COLOR_FMT_YUV422P = 9,
	RGA_SRC_COLOR_FMT_YUV420SP = 10,
	RGA_SRC_COLOR_FMT_YUV420P = 11,
	/* SRC_COLOR Palette */
	RGA_SRC_COLOR_FMT_CP_1BPP = 12,
	RGA_SRC_COLOR_FMT_CP_2BPP = 13,
	RGA_SRC_COLOR_FMT_CP_4BPP = 14,
	RGA_SRC_COLOR_FMT_CP_8BPP = 15,
	RGA_SRC_COLOR_FMT_MASK = 15,
};

enum e_rga_src_color_swap {
	RGA_SRC_COLOR_RB_SWAP = 1,
	RGA_SRC_COLOR_ALPHA_SWAP = 2,
	RGA_SRC_COLOR_UV_SWAP = 4,
};

enum e_rga_src_csc_mode {
	RGA_SRC_CSC_MODE_BT601_R0 = 0,
	RGA_SRC_CSC_MODE_BT601_R1 = 1,
	RGA_SRC_CSC_MODE_BT709_R0 = 2,
	RGA_SRC_CSC_MODE_BT709_R1 = 3,
	/*
	RGA_SRC_CSC_MODE_BYPASS = 0,
	RGA_SRC_CSC_MODE_BT601_R0 = 1,
	RGA_SRC_CSC_MODE_BT601_R1 = 2,
	RGA_SRC_CSC_MODE_BT709_R0 = 3,
	*/
};

enum e_rga_src_rot_mode {
	RGA_SRC_ROT_MODE_0_DEGREE = 0,
	RGA_SRC_ROT_MODE_90_DEGREE = 1,
	RGA_SRC_ROT_MODE_180_DEGREE = 2,
	RGA_SRC_ROT_MODE_270_DEGREE = 3,
};

enum e_rga_src_mirr_mode {
	RGA_SRC_MIRR_MODE_NO = 0,
	RGA_SRC_MIRR_MODE_X = 1,
	RGA_SRC_MIRR_MODE_Y = 2,
	RGA_SRC_MIRR_MODE_X_Y = 3,
};

enum e_rga_src_hscl_mode {
	RGA_SRC_HSCL_MODE_NO = 0,
	RGA_SRC_HSCL_MODE_DOWN = 1,
	RGA_SRC_HSCL_MODE_UP = 2,
};

enum e_rga_src_vscl_mode {
	RGA_SRC_VSCL_MODE_NO = 0,
	RGA_SRC_VSCL_MODE_DOWN = 1,
	RGA_SRC_VSCL_MODE_UP = 2,
};

enum e_rga_src_trans_enable {
	RGA_SRC_TRANS_ENABLE_R = 1,
	RGA_SRC_TRANS_ENABLE_G = 2,
	RGA_SRC_TRANS_ENABLE_B = 4,
	RGA_SRC_TRANS_ENABLE_A = 8,
};

enum e_rga_src_bic_coe_select {
	RGA_SRC_BIC_COE_SELEC_CATROM = 0,
	RGA_SRC_BIC_COE_SELEC_MITCHELL = 1,
	RGA_SRC_BIC_COE_SELEC_HERMITE = 2,
	RGA_SRC_BIC_COE_SELEC_BSPLINE = 3,
};

enum e_rga_src_yuv_ten_enable {
	RGA_SRC_YUV_TEN_DISABLE = 0,
	RGA_SRC_YUV_TEN_ENABLE = 1,
};

enum e_rga_src_yuv_ten_round_enable {
	RGA_SRC_YUV_TEN_ROUND_DISABLE = 0,
	RGA_SRC_YUV_TEN_ROUND_ENABLE = 1,
};

enum e_rga_dst_color_format {
	RGA_DST_COLOR_FMT_ABGR888 = 0,
	RGA_DST_COLOR_FMT_XBGR888 = 1,
	RGA_DST_COLOR_FMT_RGB888 = 2,
	RGA_DST_COLOR_FMT_RGB565 = 4,
	RGA_DST_COLOR_FMT_ARGB1555 = 5,
	RGA_DST_COLOR_FMT_ARGB4444 = 6,
	RGA_DST_COLOR_FMT_YUV422SP = 8,
	RGA_DST_COLOR_FMT_YUV422P = 9,
	RGA_DST_COLOR_FMT_YUV420SP = 10,
	RGA_DST_COLOR_FMT_YUV420P = 11,
	RGA_DST_COLOR_FMT_MASK = 11,
};

enum e_rga_dst_color_swap {
	RGA_DST_COLOR_RB_SWAP = 1,
	RGA_DST_COLOR_ALPHA_SWAP = 2,
	RGA_DST_COLOR_UV_SWAP = 4,
};

enum e_rga_src1_color_format {
	RGA_SRC1_COLOR_FMT_ABGR888 = 0,
	RGA_SRC1_COLOR_FMT_XBGR888 = 1,
	RGA_SRC1_COLOR_FMT_RGB888 = 2,
	RGA_SRC1_COLOR_FMT_RGB565 = 4,
	RGA_SRC1_COLOR_FMT_ARGB1555 = 5,
	RGA_SRC1_COLOR_FMT_ARGB4444 = 6,
	RGA_SRC1_COLOR_FMT_MASK	 = 6,
};

enum e_rga_src1_color_swap {
	RGA_SRC1_COLOR_RB_SWAP = 1,
	RGA_SRC1_COLOR_ALPHA_SWAP = 2,
};

enum e_rga_dst_dither_down_mode {
	RGA_DST_DITHER_MODE_888_TO_666 = 0,
	RGA_DST_DITHER_MODE_888_TO_565 = 1,
	RGA_DST_DITHER_MODE_888_TO_555 = 2,
	RGA_DST_DITHER_MODE_888_TO_444 = 3,
};

enum e_rga_dst_csc_mode {
	RGA_DST_CSC_MODE_BYPASS = 0,
	RGA_DST_CSC_MODE_BT601_R0 = 1,
	RGA_DST_CSC_MODE_BT601_R1 = 2,
	RGA_DST_CSC_MODE_BT709_R0 = 3,
};

enum e_rga_alpha_rop_mode {
	RGA_ALPHA_ROP_MODE_2 = 0,
	RGA_ALPHA_ROP_MODE_3 = 1,
	RGA_ALPHA_ROP_MODE_4 = 2,
};

enum e_rga_alpha_rop_select {
	RGA_ALPHA_SELECT_ALPHA = 0,
	RGA_ALPHA_SELECT_ROP = 1,
};


union rga_mode_ctrl {
	unsigned int val;
	struct {
		/* [0:2] */
		enum e_rga_render_mode	render:3;
		/* [3:6] */
		enum e_rga_bitblt_mode	bitblt:1;
		enum e_rga_cf_rop4_pat	cf_rop4_pat:1;
		unsigned int		alpha_zero_key:1;
		unsigned int		gradient_sat:1;
		/* [7:31] */
		unsigned int		reserved:25;
	} data;
};

union rga_src_info {
	unsigned int val;
	struct {
		/* [0:3] */
		enum e_rga_src_color_format		format:4;
		/* [4:7] */
		enum e_rga_src_color_swap		swap:3;
		unsigned int				cp_endian:1;
		/* [8:17] */
		enum e_rga_src_csc_mode			csc_mode:2;
		enum e_rga_src_rot_mode			rot_mode:2;
		enum e_rga_src_mirr_mode		mir_mode:2;
		enum e_rga_src_hscl_mode		hscl_mode:2;
		enum e_rga_src_vscl_mode		vscl_mode:2;
		/* [18:22] */
		unsigned int				trans_mode:1;
		enum e_rga_src_trans_enable		trans_enable:4;
		/* [23:25] */
		unsigned int				dither_up_en:1;
		enum e_rga_src_bic_coe_select		bic_coe_sel:2;
		/* [26] */
		unsigned int				reserved:1;
		/* [27:28] */
		enum e_rga_src_yuv_ten_enable		yuv_ten_en:1;
		enum e_rga_src_yuv_ten_round_enable	yuv_ten_round_en:1;
		/* [29:31]*/
		unsigned int				reserved1:3;
	} data;
};

union rga_src_vir_info {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	vir_width:15;
		unsigned int	reserved:1;
		/* [16:25] */
		unsigned int	vir_stride:10;
		/* [26:31] */
		unsigned int	reserved1:6;
	} data;
};

union rga_src_act_info {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	act_width:13;
		unsigned int	reserved:3;
		/* [16:31] */
		unsigned int	act_height:13;
		unsigned int	reserved1:3;
	} data;
};

union rga_src_x_factor {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	down_scale_factor:16;
		/* [16:31] */
		unsigned int	up_scale_factor:16;
	} data;
};

union rga_src_y_factor {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	down_scale_factor:16;
		/* [16:31] */
		unsigned int	up_scale_factor:16;
	} data;
};

/* Alpha / Red / Green / Blue */
union rga_src_cp_gr_color {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	gradient_x:16;
		/* [16:31] */
		unsigned int	gradient_y:16;
	} data;
};

union rga_src_transparency_color0 {
	unsigned int val;
	struct {
		/* [0:7] */
		unsigned int	trans_rmin:8;
		/* [8:15] */
		unsigned int	trans_gmin:8;
		/* [16:23] */
		unsigned int	trans_bmin:8;
		/* [24:31] */
		unsigned int	trans_amin:8;
	} data;
};

union rga_src_transparency_color1 {
	unsigned int val;
	struct {
		/* [0:7] */
		unsigned int	trans_rmax:8;
		/* [8:15] */
		unsigned int	trans_gmax:8;
		/* [16:23] */
		unsigned int	trans_bmax:8;
		/* [24:31] */
		unsigned int	trans_amax:8;
	} data;
};

union rga_dst_info {
	unsigned int val;
	struct {
		/* [0:3] */
		enum e_rga_dst_color_format	format:4;
		/* [4:6] */
		enum e_rga_dst_color_swap	swap:3;
		/* [7:9] */
		enum e_rga_src1_color_format	src1_format:3;
		/* [10:11] */
		enum e_rga_src1_color_swap	src1_swap:2;
		/* [12:15] */
		unsigned int			dither_up_en:1;
		unsigned int			dither_down_en:1;
		enum e_rga_dst_dither_down_mode	dither_down_mode:2;
		/* [16:18] */
		enum e_rga_dst_csc_mode		csc_mode:2;
		unsigned int			csc_clip:1;
		/* [19:31] */
		unsigned int			reserved:13;
	} data;
};

union rga_dst_vir_info {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	vir_stride:15;
		unsigned int	reserved:1;
		/* [16:31] */
		unsigned int	src1_vir_stride:15;
		unsigned int	reserved1:1;
	} data;
};

union rga_dst_act_info {
	unsigned int val;
	struct {
		/* [0:15] */
		unsigned int	act_width:12;
		unsigned int	reserved:4;
		/* [16:31] */
		unsigned int	act_height:12;
		unsigned int	reserved1:4;
	} data;
};

union rga_alpha_ctrl0 {
	unsigned int val;
	struct {
		/* [0:3] */
		unsigned int			rop_en:1;
		enum e_rga_alpha_rop_select	rop_select:1;
		enum e_rga_alpha_rop_mode	rop_mode:2;
		/* [4:11] */
		unsigned int			src_fading_val:8;
		/* [12:20] */
		unsigned int			dst_fading_val:8;
		unsigned int			mask_endian:1;
		/* [21:31] */
		unsigned int			reserved:11;
	} data;
};

union rga_alpha_ctrl1 {
	unsigned int val;
	struct {
		/* [0:1] */
		unsigned int	dst_color_m0:1;
		unsigned int	src_color_m0:1;
		/* [2:7] */
		unsigned int	dst_factor_m0:3;
		unsigned int	src_factor_m0:3;
		/* [8:9] */
		unsigned int	dst_alpha_cal_m0:1;
		unsigned int	src_alpha_cal_m0:1;
		/* [10:13] */
		unsigned int	dst_blend_m0:2;
		unsigned int	src_blend_m0:2;
		/* [14:15] */
		unsigned int	dst_alpha_m0:1;
		unsigned int	src_alpha_m0:1;
		/* [16:21] */
		unsigned int	dst_factor_m1:3;
		unsigned int	src_factor_m1:3;
		/* [22:23] */
		unsigned int	dst_alpha_cal_m1:1;
		unsigned int	src_alpha_cal_m1:1;
		/* [24:27] */
		unsigned int	dst_blend_m1:2;
		unsigned int	src_blend_m1:2;
		/* [28:29] */
		unsigned int	dst_alpha_m1:1;
		unsigned int	src_alpha_m1:1;
		/* [30:31] */
		unsigned int	reserved:2;
	} data;
};

union rga_fading_ctrl {
	unsigned int val;
	struct {
		/* [0:7] */
		unsigned int	fading_offset_r:8;
		/* [8:15] */
		unsigned int	fading_offset_g:8;
		/* [16:23] */
		unsigned int	fading_offset_b:8;
		/* [24:31] */
		unsigned int	fading_en:1;
		unsigned int	reserved:7;
	} data;
};

union rga_pat_con {
	unsigned int val;
	struct {
		/* [0:7] */
		unsigned int	width:8;
		/* [8:15] */
		unsigned int	height:8;
		/* [16:23] */
		unsigned int	offset_x:8;
		/* [24:31] */
		unsigned int	offset_y:8;
	} data;
};
#endif
