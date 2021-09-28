/*
 * Copyright (C) 2016 Fuzhou Rcockhip Electronics Co.Ltd
 * Authors:
 *	Yakir Yang <ykk@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <android/log.h>

#include <sys/mman.h>
#include <linux/stddef.h>

#include <xf86drm.h>

#include "drm_fourcc.h"
//#include "libdrm_macros.h"

#include "rockchip_drm.h"
#include "rockchip_rga.h"
#include "rga_reg.h"

#define  LOGI(...) __android_log_print(ANDROID_LOG_INFO, "libdrm", __VA_ARGS__)
#define  LOGW(...) __android_log_print(ANDROID_LOG_WARN, "libdrm", __VA_ARGS__)
#define  LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "libdrm", __VA_ARGS__)

enum rga_base_addr_reg {
	rga_dst = 0,
	rga_src
};

enum e_rga_start_pos {
	LT = 0,
	LB = 1,
	RT = 2,
	RB = 3,
};

struct rga_addr_offset {
	unsigned int y_off;
	unsigned int u_off;
	unsigned int v_off;
};

struct rga_corners_addr_offset {
	struct rga_addr_offset left_top;
	struct rga_addr_offset right_top;
	struct rga_addr_offset left_bottom;
	struct rga_addr_offset right_bottom;
};

static int rga_get_uv_factor(int drm_color_format)
{
	int ydiv = 1;

        switch (drm_color_format) {
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		ydiv = 2;
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV12_10:
	//case DRM_FORMAT_NV21_10:
		ydiv = 4;
		break;

	default:
		break;
	}

	return ydiv;
}

static int rga_get_ydiv(int drm_color_format)
{
	int ydiv = 1;

        switch (drm_color_format) {
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		ydiv = 1;
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV12_10:
	//case DRM_FORMAT_NV21_10:
		ydiv = 2;
		break;

	default:
		break;
	}

	return ydiv;
}

static int rga_get_xdiv(int drm_color_format)
{
	int xdiv = 2;

        switch (drm_color_format) {
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV12_10:
	//case DRM_FORMAT_NV21_10:
		xdiv = 1;
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		xdiv = 2;
		break;

	default:
		break;
	}

	return xdiv;
}

static int rga_get_color_swap(int drm_color_format)
{
	unsigned int swap = 0;

	switch (drm_color_format) {
		case DRM_FORMAT_RGBA8888:
		case DRM_FORMAT_RGBX8888:
		case DRM_FORMAT_RGBA5551:
		case DRM_FORMAT_RGBA4444:
		case DRM_FORMAT_RGB888:
		case DRM_FORMAT_RGB565:
			break;

		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV12_10:
			break;

		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_ABGR1555:
		case DRM_FORMAT_ABGR4444:
			swap |= RGA_SRC_COLOR_ALPHA_SWAP;

		case DRM_FORMAT_BGRA8888:
		case DRM_FORMAT_BGRX8888:
		case DRM_FORMAT_BGRA5551:
		case DRM_FORMAT_BGRA4444:
		case DRM_FORMAT_BGR888:
		case DRM_FORMAT_BGR565:
			swap |= RGA_SRC_COLOR_RB_SWAP;
			break;

		case DRM_FORMAT_YVU422:
		case DRM_FORMAT_YVU420:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV61:
		//case DRM_FORMAT_NV21_10:
			swap |= RGA_SRC_COLOR_UV_SWAP;
			break;

		default:
			printf("Unsupport input color format %d\n", drm_color_format);
			break;
	}

	return swap;
}

static int rga_get_color_format(int drm_color_format)
{
        switch (drm_color_format) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_ABGR8888:
		case DRM_FORMAT_BGRA8888:
		case DRM_FORMAT_RGBA8888:
			return RGA_SRC_COLOR_FMT_ABGR8888;

		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_XBGR8888:
		case DRM_FORMAT_RGBX8888:
		case DRM_FORMAT_BGRX8888:
			return RGA_SRC_COLOR_FMT_XBGR8888;

		case DRM_FORMAT_RGB888:
		case DRM_FORMAT_BGR888:
			return RGA_SRC_COLOR_FMT_RGB888;

		case DRM_FORMAT_RGB565:
		case DRM_FORMAT_BGR565:
			return RGA_SRC_COLOR_FMT_RGB565;

		case DRM_FORMAT_ARGB1555:
		case DRM_FORMAT_ABGR1555:
		case DRM_FORMAT_RGBA5551:
		case DRM_FORMAT_BGRA5551:
			return RGA_SRC_COLOR_FMT_ARGB1555;

		case DRM_FORMAT_ARGB4444:
		case DRM_FORMAT_ABGR4444:
		case DRM_FORMAT_RGBA4444:
		case DRM_FORMAT_BGRA4444:
			return RGA_SRC_COLOR_FMT_ARGB4444;

		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV61:
			return RGA_SRC_COLOR_FMT_YUV422SP;

		case DRM_FORMAT_YUV422:
		case DRM_FORMAT_YVU422:
			return RGA_SRC_COLOR_FMT_YUV422P;

		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_NV12_10:
			//case DRM_FORMAT_NV21_10:
			return RGA_SRC_COLOR_FMT_YUV420SP;

		case DRM_FORMAT_YUV420:
		case DRM_FORMAT_YVU420:
			return RGA_SRC_COLOR_FMT_YUV420P;

		default:
			return -EINVAL;
	};
}

int get_string_of_cmd(int index,char *buf)
{
	switch (index) {
		case MODE_CTRL		:strcpy(buf,"MODE_CTRL          ");break;
		case SRC_INFO		:strcpy(buf,"SRC_INFO           ");break;
		case SRC_Y_RGB_BASE_ADDR:strcpy(buf,"SRC_Y_RGB_BASE_ADDR");break;
		case SRC_CB_BASE_ADDR	:strcpy(buf,"SRC_CB_BASE_ADDR   ");break;
		case SRC_CR_BASE_ADDR	:strcpy(buf,"SRC_CR_BASE_ADDR   ");break;
		case SRC1_RGB_BASE_ADDR	:strcpy(buf,"SRC1_RGB_BASE_ADDR ");break;
		case SRC_VIR_INFO	:strcpy(buf,"SRC_VIR_INFO       ");break;
		case SRC_ACT_INFO	:strcpy(buf,"SRC_ACT_INFO       ");break;
		case SRC_X_FACTOR	:strcpy(buf,"SRC_X_FACTOR       ");break;
		case SRC_Y_FACTOR	:strcpy(buf,"SRC_Y_FACTOR       ");break;
		case SRC_BG_COLOR	:strcpy(buf,"SRC_BG_COLOR       ");break;
		case SRC_FG_COLOR	:strcpy(buf,"SRC_FG_COLOR       ");break;
		case SRC_TR_COLOR0	:strcpy(buf,"SRC_TR_COLOR0      ");break;
		case SRC_TR_COLOR1	:strcpy(buf,"SRC_TR_COLOR1      ");break;
		case DST_INFO		:strcpy(buf,"DST_INFO           ");break;
		case DST_Y_RGB_BASE_ADDR:strcpy(buf,"DST_Y_RGB_BASE_ADDR");break;
		case DST_CB_BASE_ADDR	:strcpy(buf,"DST_CB_BASE_ADDR   ");break;
		case DST_CR_BASE_ADDR	:strcpy(buf,"DST_CR_BASE_ADDR   ");break;
		case DST_VIR_INFO	:strcpy(buf,"DST_VIR_INFO       ");break;
		case DST_ACT_INFO	:strcpy(buf,"DST_ACT_INFO       ");break;
		case ALPHA_CTRL0	:strcpy(buf,"ALPHA_CTRL0        ");break;
		case ALPHA_CTRL1	:strcpy(buf,"ALPHA_CTRL1        ");break;
		case FADING_CTRL	:strcpy(buf,"FADING_CTRL        ");break;
		case PAT_CON		:strcpy(buf,"PAT_CON            ");break;
		case ROP_CON0		:strcpy(buf,"ROP_CON0           ");break;
		case ROP_CON1		:strcpy(buf,"ROP_CON1           ");break;
		case MASK_BASE		:strcpy(buf,"MASK_BASE          ");break;
		case MMU_CTRL1		:strcpy(buf,"MMU_CTRL1          ");break;
		case MMU_SRC_BASE	:strcpy(buf,"MMU_SRC_BASE       ");break;
		case MMU_SRC1_BASE	:strcpy(buf,"MMU_SRC1_BASE      ");break;
		case MMU_DST_BASE	:strcpy(buf,"MMU_DST_BASE       ");break;
		case MMU_ELS_BASE	:strcpy(buf,"MMU_ELS_BASE       ");break;
		case RGA_BUF_TYPE_GEMFD | SRC_Y_RGB_BASE_ADDR:
					 strcpy(buf,"SRC_Y_RGB_BASE_ADDR");break;
		case RGA_BUF_TYPE_GEMFD | DST_Y_RGB_BASE_ADDR:
					 strcpy(buf,"DST_Y_RGB_BASE_ADDR");break;
		default			:strcpy(buf,"ERROR_OFFSET       ");break;
	}
	return 0;
}

static unsigned int rga_get_scaling(unsigned int src, unsigned int dst)
{
	/*
	 * The rga hw scaling factor is a normalized inverse of the scaling factor.
	 * For example: When source width is 100 and destination width is 200
	 * (scaling of 2x), then the hw factor is NC * 100 / 200.
	 * The normalization factor (NC) is 2^16 = 0x10000.
	 */

	return (src > dst) ? ((dst << 16) / src) : ((src << 16) / dst);
}

static struct rga_corners_addr_offset
rga_get_addr_offset(struct rga_image *img, unsigned int x, unsigned int y,
		    unsigned int w, unsigned int h)
{
	struct rga_corners_addr_offset offsets;
	struct rga_addr_offset *lt, *lb, *rt, *rb;
	unsigned int x_div = 0, y_div = 0, uv_stride = 0, pixel_width = 0, uv_factor = 0;

	lt = &offsets.left_top;
	lb = &offsets.left_bottom;
	rt = &offsets.right_top;
	rb = &offsets.right_bottom;

	x_div = rga_get_xdiv(img->color_mode);
	y_div = rga_get_ydiv(img->color_mode);
	uv_factor = rga_get_uv_factor(img->color_mode);
	uv_stride = img->stride / x_div;
	pixel_width = img->stride / img->width;

	lt->y_off = y * img->stride + x * pixel_width;
	lt->u_off = img->stride * img->hstride + (y / y_div) * uv_stride + x / x_div;
	lt->v_off = lt->u_off + img->width * img->hstride / uv_factor;

	lb->y_off = lt->y_off + (h - 1) * img->stride;
	lb->u_off = lt->u_off + (h / y_div - 1) * uv_stride;
	lb->v_off = lt->v_off + (h / y_div - 1) * uv_stride;

	rt->y_off = lt->y_off + (w - 1) * pixel_width;
	rt->u_off = lt->u_off + w / x_div - 1;
	rt->v_off = lt->v_off + w / x_div - 1;

	rb->y_off = lb->y_off + (w - 1) * pixel_width;
	rb->u_off = lb->u_off + w / x_div - 1;
	rb->v_off = lb->v_off + w / x_div - 1;

	return offsets;
}

static struct rga_addr_offset *
rga_lookup_draw_pos(struct rga_corners_addr_offset *offsets,
		    enum e_rga_src_rot_mode rotate_mode,
		    enum e_rga_src_mirr_mode mirr_mode)
{
	static enum e_rga_start_pos rot_mir_point_matrix[4][4] = {
		{ LT, RT, LB, RB, },
		{ RT, LT, RB, LB, },
		{ RB, LB, RT, LT, },
		{ LB, RB, LT, RT, },
	};

	if (offsets == NULL)
		return NULL;

	switch (rot_mir_point_matrix[rotate_mode][mirr_mode]) {
	case LT:
		return &offsets->left_top;
	case LB:
		return &offsets->left_bottom;
	case RT:
		return &offsets->right_top;
	case RB:
		return &offsets->right_bottom;
	};

	return NULL;
}

/*
 * rga_add_cmd - set given command and value to user side command buffer.
 *
 * @ctx: a pointer to rga_context structure.
 * @cmd: command data.
 * @value: value data.
 */
static int rga_add_cmd(struct rga_context *ctx, unsigned long cmd,
			unsigned long value)
{
	char buf[25];
	const char *fmt;

	if (ctx->log & 1) {
		get_string_of_cmd(cmd, buf);
		fprintf(stderr,"%s:0x%x:0x%x\n",buf,cmd,value);
		LOGI("%s:%8x:  0x%x\n",buf,cmd,value);
	}

	switch (cmd & ~(RGA_IMGBUF_USERPTR)) {
	case SRC_Y_RGB_BASE_ADDR:
	case SRC_CB_BASE_ADDR:
	case SRC_CR_BASE_ADDR:
	case SRC1_RGB_BASE_ADDR:
	case DST_Y_RGB_BASE_ADDR:
	case DST_CB_BASE_ADDR:
	case DST_CR_BASE_ADDR:
		if (ctx->cmd_buf_nr >= RGA_MAX_GEM_CMD_NR) {
			fprintf(stderr, "Overflow cmd_gem size.\n");
			return -EINVAL;
		}

		ctx->cmd_buf[ctx->cmd_buf_nr].offset = cmd;
		ctx->cmd_buf[ctx->cmd_buf_nr].data = value;
		ctx->cmd_buf_nr++;

		break;
	default:
		if (ctx->cmd_nr >= RGA_MAX_CMD_NR) {
			fprintf(stderr, "Overflow cmd size.\n");
			return -EINVAL;
		}

		ctx->cmd[ctx->cmd_nr].offset = cmd;
		ctx->cmd[ctx->cmd_nr].data = value;
		ctx->cmd_nr++;

		break;
	}

	return 0;
}

int rga_dump_context(struct rga_context ctx)
{
    int i = 0;
    char buf[25];
    fprintf(stderr,"********************frame start************************\n"
                   "fd=%d,major=%d,minor=%d\n"
                   "cmd_nr=%d,cmd=%p\n"
                   "cmd_buf_nr=%d,cmd_buf=%p\n"
                   "cmdlist_nr=%d\n",
                    ctx.fd,ctx.major,ctx.minor,ctx.cmd_nr,ctx.cmd,
                    ctx.cmd_buf_nr,ctx.cmd_buf,ctx.cmdlist_nr);

    fprintf(stderr,"\n---------------cmd_nr=%d,cmd=%p:\n",ctx.cmd_nr,ctx.cmd);
    for(i = 0; i < ctx.cmd_nr; i ++) {
        get_string_of_cmd(ctx.cmd[i].offset,buf);
        fprintf(stderr,"%s:0x%x:[0x%x]\n",
                                       buf,ctx.cmd[i].offset,ctx.cmd[i].data);
    }

    fprintf(stderr,"\ncmd_buf_nr=%d,cmd_buf=%p\n",ctx.cmd_buf_nr,ctx.cmd_buf);
    for(i = 0; i < ctx.cmd_buf_nr; i ++) {
        get_string_of_cmd(ctx.cmd_buf[i].offset,buf);
        fprintf(stderr,"%s:0x%x:[0x%x]\n",
                               buf,ctx.cmd_buf[i].offset,ctx.cmd_buf[i].data);
    }
    fprintf(stderr,"*******************frame end*************************\n");
    return 0;
}

int rga_src_color_is_yuv(int format)
{
	int ret = 0;
	switch (format) {
		case RGA_SRC_COLOR_FMT_YUV422SP:
		case RGA_SRC_COLOR_FMT_YUV422P:
		case RGA_SRC_COLOR_FMT_YUV420SP:
		case RGA_SRC_COLOR_FMT_YUV420P:
			ret = 1;
			break;

		default:
			break;
	}

	return ret;
}

int rga_dst_color_is_yuv(int format)
{
	int ret = 0;
	switch (format) {
		case RGA_DST_COLOR_FMT_YUV422SP:
		case RGA_DST_COLOR_FMT_YUV422P:
		case RGA_DST_COLOR_FMT_YUV420SP:
		case RGA_DST_COLOR_FMT_YUV420P:
			ret = 1;
			break;

		default:
			break;
	}

	return ret;
}
/*
 * rga_add_base_addr - helper function to set dst/src base address register.
 *
 * @ctx: a pointer to rga_context structure.
 * @img: a pointer to the dst/src rga_image structure.
 * @reg: the register that should be set.
 */
static void rga_add_base_addr(struct rga_context *ctx, struct rga_image *img,
			      enum rga_base_addr_reg reg)
{
	const unsigned long cmd = (reg == rga_dst) ?
		DST_Y_RGB_BASE_ADDR : SRC_Y_RGB_BASE_ADDR;

	if (img->buf_type == RGA_IMGBUF_USERPTR) {
		fprintf(stderr, "Can't support userptr now!\n");
		return;
	} else {
		rga_add_cmd(ctx, cmd | RGA_BUF_TYPE_GEMFD, img->bo[0]);
	}
}

/*
 * rga_reset - reset rga hardware.
 *
 * @ctx: a pointer to rga_context structure.
 *
 */
static void rga_reset(struct rga_context *ctx)
{
	ctx->cmd_nr = 0;
	ctx->cmd_buf_nr = 0;
}

/*
 * rga_flush - submit all commands and values in user side command buffer
 *		to command queue aware of rga dma.
 *
 * @ctx: a pointer to rga_context structure.
 *
 * This function should be called after all commands and values to user
 * side command buffer are set. It submits that buffer to the kernel side driver.
 */
static int rga_flush(struct rga_context *ctx)
{
	int ret;
	struct drm_rockchip_rga_set_cmdlist cmdlist = {0};

	if (ctx->cmd_nr == 0 && ctx->cmd_buf_nr == 0)
		return -1;

	if (ctx->cmdlist_nr >= RGA_MAX_CMD_LIST_NR) {
		fprintf(stderr, "Overflow cmdlist.\n");
		return -EINVAL;
	}

	//rga_dump_context(*ctx);

	cmdlist.cmd = (uint64_t)(uintptr_t)&ctx->cmd[0];
	cmdlist.cmd_buf = (uint64_t)(uintptr_t)&ctx->cmd_buf[0];
	cmdlist.cmd_nr = ctx->cmd_nr;
	cmdlist.cmd_buf_nr = ctx->cmd_buf_nr;

	ctx->cmd_nr = 0;
	ctx->cmd_buf_nr = 0;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_ROCKCHIP_RGA_SET_CMDLIST, &cmdlist);
	if (ret < 0) {
		fprintf(stderr, "failed to set cmdlist.\n");
		return ret;
	}

	ctx->cmdlist_nr++;

	return ret;
}

/**
 * rga_init - create a new rga context and get hardware version.
 *
 * fd: a file descriptor to an opened drm device.
 */
struct rga_context *rga_init(int fd)
{
	struct drm_rockchip_rga_get_ver ver;
	struct rga_context *ctx;
	int ret;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		fprintf(stderr, "failed to allocate context.\n");
		return NULL;
	}

	ctx->fd = fd;

	ret = drmIoctl(fd, DRM_IOCTL_ROCKCHIP_RGA_GET_VER, &ver);
	if (ret < 0) {
		fprintf(stderr, "failed to get version.\n");
		free(ctx);
		return NULL;
	}

	ctx->major = ver.major;
	ctx->minor = ver.minor;

	return ctx;
}

void rga_fini(struct rga_context *ctx)
{
	if (ctx)
		free(ctx);
}

/**
 * rga_exec - start the dma to process all commands summited by rga_flush().
 *
 * @ctx: a pointer to rga_context structure.
 */
int rga_exec(struct rga_context *ctx)
{
	struct drm_rockchip_rga_exec exec;
	int ret;

	if (ctx->cmdlist_nr == 0)
		return -EINVAL;

	exec.async = 0;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_ROCKCHIP_RGA_EXEC, &exec);
	if (ret < 0) {
		fprintf(stderr, "failed to execute.\n");
		return ret;
	}

	ctx->cmdlist_nr = 0;

	return ret;
}

/**
 * rga_solid_fill - fill given buffer with given color data.
 *
 * @ctx: a pointer to rga_context structure.
 * @img: a pointer to rga_image structure including image and buffer
 *	information.
 * @x: x start position to buffer filled with given color data.
 * @y: y start position to buffer filled with given color data.
 * @w: width value to buffer filled with given color data.
 * @h: height value to buffer filled with given color data.
 */
int rga_solid_fill(struct rga_context *ctx, struct rga_image *img,
		   unsigned int x, unsigned int y, unsigned int w,
		   unsigned int h)
{
	union rga_mode_ctrl mode;
	union rga_dst_info dst_info;
	union rga_dst_vir_info dst_vir_info;
	union rga_dst_act_info dst_act_info;

	struct rga_corners_addr_offset offsets;

	if (x + w > img->width)
		w = img->width - x;
	if (y + h > img->height)
		h = img->height - y;

	/* Init the operation registers to zero */
	mode.val = 0;
	dst_info.val = 0;
	dst_act_info.val = 0;
	dst_vir_info.val = 0;

	/*
	 * Configure the RGA operation mode registers:
	 *   Bitblt Mode,
	 *   SRC + DST=> DST,
	 *   Solid color fill,
	 *   Gradient status is not-clip,
	 */
	mode.data.gradient_sat = 1;
	mode.data.render = RGA_MODE_RENDER_BITBLT;
	mode.data.render = RGA_MODE_RENDER_RECTANGLE_FILL;
	mode.data.cf_rop4_pat = RGA_MODE_CF_ROP4_SOLID,
	mode.data.bitblt = RGA_MODE_BITBLT_MODE_SRC_TO_DST;

	rga_add_cmd(ctx, MODE_CTRL, mode.val);


	/*
	 * Translate the DRM color format to RGA color format
	 */
	dst_info.data.format = rga_get_color_format(img->color_mode);
	dst_info.data.swap   = rga_get_color_swap(img->color_mode);
	dst_info.data.csc_mode = RGA_DST_CSC_MODE_BT601_R0;

	if (dst_info.data.format == RGA_DST_COLOR_FMT_YUV422SP ||
	    dst_info.data.format == RGA_DST_COLOR_FMT_YUV422P ||
	    dst_info.data.format == RGA_DST_COLOR_FMT_YUV420SP ||
	    dst_info.data.format == RGA_DST_COLOR_FMT_YUV420P)
		dst_info.data.csc_mode = RGA_DST_CSC_MODE_BT601_R0;

	rga_add_cmd(ctx, DST_INFO, dst_info.val);


	/*
	 * Configure the target color to foreground color.
	 */
	rga_add_cmd(ctx, SRC_FG_COLOR, img->fill_color);


	/*
	 * Cacluate the framebuffer virtual strides and active size,
	 * note that the step of vir_stride is 4 byte words
	 */
	dst_vir_info.data.vir_stride = img->stride >> 2;
	dst_act_info.data.act_height = h - 1;
	dst_act_info.data.act_width = w - 1;

	rga_add_cmd(ctx, DST_VIR_INFO, dst_vir_info.val);
	rga_add_cmd(ctx, DST_ACT_INFO, dst_act_info.val);


	/*
	 * Configure the dest framebuffer base address with pixel offset.
	 */
	offsets = rga_get_addr_offset(img, x, y, w, h);

	rga_add_cmd(ctx, DST_Y_RGB_BASE_ADDR, offsets.left_top.y_off);
	rga_add_cmd(ctx, DST_CB_BASE_ADDR, offsets.left_top.u_off);
	rga_add_cmd(ctx, DST_CR_BASE_ADDR, offsets.left_top.v_off);

	rga_add_base_addr(ctx, img, rga_dst);


	/* Start to flush RGA device */
	rga_flush(ctx);

	return 0;
}

int rga_multiple_transform(struct rga_context *ctx, struct rga_image *src,
			   struct rga_image *dst, unsigned int src_x,
			   unsigned int src_y, unsigned int src_w,
			   unsigned int src_h, unsigned int dst_x,
			   unsigned int dst_y, unsigned int dst_w,
			   unsigned int dst_h, unsigned int degree,
			   unsigned int x_mirr, unsigned int y_mirr)
{
	union rga_mode_ctrl mode;
	union rga_src_info src_info;
	union rga_dst_info dst_info;
	union rga_src_x_factor x_factor;
	union rga_src_y_factor y_factor;
	union rga_src_vir_info src_vir_info;
	union rga_src_act_info src_act_info;
	union rga_dst_vir_info dst_vir_info;
	union rga_dst_act_info dst_act_info;

	struct rga_addr_offset *dst_offset;
	struct rga_corners_addr_offset offsets;
	struct rga_corners_addr_offset src_offsets;

	unsigned int scale_dst_w, scale_dst_h;

	if (degree != 0 && degree != 90 && degree != 180 && degree != 270) {
		fprintf(stderr, "invalid rotate degree.\n");
		rga_reset(ctx);
		return -EINVAL;
	}

	if (src_w < 32 || src_h < 34 || dst_w < 32 || dst_h < 34) {
		fprintf(stderr, "invalid src/dst width or height.\n");
		rga_reset(ctx);
		return -EINVAL;
	}

	if (src_x + src_w > src->width)
		src_w = src->width - src_x;
	if (src_y + src_h > src->height)
		src_h = src->height - src_y;

	if (dst_x + dst_w > dst->width)
		dst_w = dst->width - dst_x;
	if (dst_y + dst_h > dst->height)
		dst_h = dst->height - dst_y;

	if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		rga_reset(ctx);
		return -EINVAL;
	}

	/* Init RGA registers values to zero */
	mode.val = 0;
	x_factor.val = 0;
	y_factor.val = 0;
	src_info.val = 0;
	dst_info.val = 0;
	src_vir_info.val = 0;
	dst_vir_info.val = 0;
	src_act_info.val = 0;
	dst_act_info.val = 0;

	/*
	 * Configure the RGA operation mode registers:
	 *   Bitblt Mode,
	 *   SRC => DST,
	 *   Gradient status is not-clip,
	 */
	mode.data.gradient_sat = 1;
	mode.data.render = RGA_MODE_RENDER_BITBLT;
	mode.data.bitblt = RGA_MODE_BITBLT_MODE_SRC_TO_DST;
	rga_add_cmd(ctx, MODE_CTRL, mode.val);

	/*
	 * Translate the DRM color format to RGA color format, and
	 * configure the actual rotate / mirr mode.
	 */
	src_info.data.format   = rga_get_color_format(src->color_mode);
	dst_info.data.format   = rga_get_color_format(dst->color_mode);
	src_info.data.swap     = rga_get_color_swap(src->color_mode);
	dst_info.data.swap     = rga_get_color_swap(dst->color_mode);

	if (src->color_mode == DRM_FORMAT_NV12_10) {
		src_info.data.yuv_ten_en = RGA_SRC_YUV_TEN_ENABLE;
		src_info.data.yuv_ten_round_en = RGA_SRC_YUV_TEN_ROUND_ENABLE;
	}

	switch (degree) {
	case 90:
		src_info.data.rot_mode = RGA_SRC_ROT_MODE_90_DEGREE;
		break;
	case 180:
		src_info.data.rot_mode = RGA_SRC_ROT_MODE_180_DEGREE;
		break;
	case 270:
		src_info.data.rot_mode = RGA_SRC_ROT_MODE_270_DEGREE;
		break;
	default:
		src_info.data.rot_mode = RGA_SRC_ROT_MODE_0_DEGREE;
		break;
	}

	if (x_mirr)
		src_info.data.mir_mode |= RGA_SRC_MIRR_MODE_X;
	if (y_mirr)
		src_info.data.mir_mode |= RGA_SRC_MIRR_MODE_Y;

	/*
	 * Cacluate the up/down scaling mode/factor.
	 *
	 * RGA used to scale the picture first, and then rotate second,
	 * so we need to swap the w/h when rotate degree is 90/270.
	 */
	if (src_info.data.rot_mode == RGA_SRC_ROT_MODE_90_DEGREE ||
	    src_info.data.rot_mode == RGA_SRC_ROT_MODE_270_DEGREE) {
		if (ctx->major == 0 || ctx->minor == 0) {
			if (dst_w == src_h)
				src_h -= 8;
			if (abs(src_w - dst_h) < 16)
				src_w -= 16;
		}

		scale_dst_h = dst_w;
		scale_dst_w = dst_h;
	} else {
		scale_dst_w = dst_w;
		scale_dst_h = dst_h;
	}

	if (src_w == scale_dst_w) {
		src_info.data.hscl_mode = RGA_SRC_HSCL_MODE_NO;
		x_factor.val = 0;
		if (src->color_mode == DRM_FORMAT_NV12_10)
		    src_info.data.hscl_mode = RGA_SRC_HSCL_MODE_DOWN | RGA_SRC_HSCL_MODE_UP;
	} else if(src_w > scale_dst_w) {
		src_info.data.hscl_mode = RGA_SRC_HSCL_MODE_DOWN;
		x_factor.data.down_scale_factor = rga_get_scaling(src_w, scale_dst_w) + 1;
	} else {
		src_info.data.hscl_mode = RGA_SRC_HSCL_MODE_UP;
		x_factor.data.up_scale_factor = rga_get_scaling(src_w - 1, scale_dst_w - 1);
	}

	if (src_h == scale_dst_h) {
		src_info.data.vscl_mode = RGA_SRC_VSCL_MODE_NO;
		y_factor.val = 0;
		if (src->color_mode == DRM_FORMAT_NV12_10)
		    src_info.data.vscl_mode = RGA_SRC_VSCL_MODE_DOWN | RGA_SRC_VSCL_MODE_UP;
	} else if(src_h > scale_dst_h) {
		src_info.data.vscl_mode = RGA_SRC_VSCL_MODE_DOWN;
		y_factor.data.down_scale_factor = rga_get_scaling(src_h, scale_dst_h) + 1;
	} else {
		src_info.data.vscl_mode = RGA_SRC_VSCL_MODE_UP;
		y_factor.data.up_scale_factor = rga_get_scaling(src_h - 1, scale_dst_h - 1);
	}

	rga_add_cmd(ctx, SRC_X_FACTOR, x_factor.val);
	rga_add_cmd(ctx, SRC_Y_FACTOR, y_factor.val);

	if (rga_src_color_is_yuv(src_info.data.format)
			&& rga_dst_color_is_yuv(dst_info.data.format)) {
		src_info.data.csc_mode = RGA_SRC_CSC_MODE_BT601_R0;
		dst_info.data.csc_mode = RGA_SRC_CSC_MODE_BT601_R0;
	}

	if (rga_src_color_is_yuv(src_info.data.format)
			&& !rga_dst_color_is_yuv(dst_info.data.format))
		src_info.data.csc_mode = RGA_SRC_CSC_MODE_BT601_R1;

	if (!rga_src_color_is_yuv(src_info.data.format)
			&& rga_dst_color_is_yuv(dst_info.data.format))
		dst_info.data.csc_mode = RGA_SRC_CSC_MODE_BT601_R1;

	rga_add_cmd(ctx, SRC_INFO, src_info.val);
	rga_add_cmd(ctx, DST_INFO, dst_info.val);


	/*
	 * Cacluate the framebuffer virtual strides and active size,
	 * note that the step of vir_stride / vir_width is 4 byte words
	 */
	src_vir_info.data.vir_stride = 0x3ff;//src->stride >> 2;
	src_vir_info.data.vir_width = src->stride >> 2;

	src_act_info.data.act_height = src_h - 1;
	src_act_info.data.act_width = src_w - 1;

	dst_vir_info.data.vir_stride = dst->stride >> 2;
	dst_act_info.data.act_height = dst_h - 1;
	dst_act_info.data.act_width = dst_w - 1;

	rga_add_cmd(ctx, SRC_VIR_INFO, src_vir_info.val);
	rga_add_cmd(ctx, SRC_ACT_INFO, src_act_info.val);

	rga_add_cmd(ctx, DST_VIR_INFO, dst_vir_info.val);
	rga_add_cmd(ctx, DST_ACT_INFO, dst_act_info.val);


	/*
	 * Cacluate the source framebuffer base address with offset pixel.
	 */
	src_offsets = rga_get_addr_offset(src, src_x, src_y, src_w, src_h);

	rga_add_cmd(ctx, SRC_Y_RGB_BASE_ADDR, src_offsets.left_top.y_off);
	rga_add_cmd(ctx, SRC_CB_BASE_ADDR, src_offsets.left_top.u_off);
	rga_add_cmd(ctx, SRC_CR_BASE_ADDR, src_offsets.left_top.v_off);

	rga_add_base_addr(ctx, src, rga_src);


	/*
	 * Configure the dest framebuffer base address with pixel offset.
	 */
	offsets = rga_get_addr_offset(dst, dst_x, dst_y, dst_w, dst_h);
	dst_offset = rga_lookup_draw_pos(&offsets, src_info.data.rot_mode,
					 src_info.data.mir_mode);

	rga_add_cmd(ctx, DST_Y_RGB_BASE_ADDR, dst_offset->y_off);
	rga_add_cmd(ctx, DST_CB_BASE_ADDR, dst_offset->u_off);
	rga_add_cmd(ctx, DST_CR_BASE_ADDR, dst_offset->v_off);

	rga_add_base_addr(ctx, dst, rga_dst);


	/* Start to flush RGA device */
	rga_flush(ctx);

	return 0;
}

/**
 * rga_copy_with_rorate - copy contents in source buffer to destination buffer
 *	rotate properly.
 *
 * @ctx: a pointer to rga_context structure.
 * @src: a pointer to rga_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to rga_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @src_w: width value to source buffer.
 * @src_h: height value to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @dst_w: width value to destination buffer.
 * @dst_h: height value to destination buffer.
 * @degree: rotate degree (0, 90, 180, 270)
 */
int rga_copy_with_rotate(struct rga_context *ctx, struct rga_image *src,
			 struct rga_image *dst, unsigned int src_x,
			 unsigned int src_y, unsigned int src_w,
			 unsigned int src_h, unsigned int dst_x,
			 unsigned int dst_y, unsigned int dst_w,
			 unsigned int dst_h, unsigned int degree)
{
	if (degree != 0 && degree != 90 && degree != 180 && degree != 270) {
		fprintf(stderr, "invalid rotate degree %d.\n", degree);
		return -EINVAL;
	}

	return rga_multiple_transform(ctx, src, dst, src_x, src_y, src_w,
				      src_h, dst_x, dst_y, dst_w, dst_h,
				      degree, 0, 0);
}

/**
 * rga_copy_with_scale - copy contents in source buffer to destination buffer
 *	scaling up or down properly.
 *
 * @ctx: a pointer to rga_context structure.
 * @src: a pointer to rga_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to rga_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @src_w: width value to source buffer.
 * @src_h: height value to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @dst_w: width value to destination buffer.
 * @dst_h: height value to destination buffer.
 */
int rga_copy_with_scale(struct rga_context *ctx, struct rga_image *src,
			struct rga_image *dst, unsigned int src_x,
			unsigned int src_y, unsigned int src_w,
			unsigned int src_h, unsigned int dst_x,
			unsigned int dst_y, unsigned int dst_w,
			unsigned int dst_h)
{

	return rga_multiple_transform(ctx, src, dst, src_x, src_y, src_w,
				      src_h, dst_x, dst_y, dst_w, dst_h,
				      0, 0, 0);
}

/**
 * rga_copy - copy contents in source buffer to destination buffer.
 *
 * @ctx: a pointer to rga_context structure.
 * @src: a pointer to rga_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to rga_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @w: width value to source and destination buffers.
 * @h: height value to source and destination buffers.
 */
int rga_copy(struct rga_context *ctx, struct rga_image *src,
	     struct rga_image *dst, unsigned int src_x, unsigned int src_y,
	     unsigned int dst_x, unsigned dst_y, unsigned int w,
	     unsigned int h)
{
	unsigned int src_w = 0, src_h = 0, dst_w = 0, dst_h = 0;

	src_w = w;
	src_h = h;
	if (src_x + src_w > src->width)
		src_w = src->width - src_x;
	if (src_y + src_h > src->height)
		src_h = src->height - src_y;

	dst_w = w;
	dst_h = h;
	if (dst_x + dst_w > dst->width)
		dst_w = dst->width - dst_x;
	if (dst_y + dst_h > dst->height)
		dst_h = dst->height - dst_y;

	w = (src_w < dst_w) ? src_w : dst_w;
	h = (src_h < dst_h) ? src_h : dst_h;

	if (w <= 0 || h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		rga_reset(ctx);
		return -EINVAL;
	}

	return rga_multiple_transform(ctx, src, dst, src_x, src_y, src_w,
				      src_h, dst_x, dst_y, dst_w, dst_h,
				      0, 0, 0);
}
