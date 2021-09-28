/*
 * Copyright (C) 2016 Fuzhou Rockchip lectronics Co.Ltd
 * Authors:
 *	Yakir Yang <ykk@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _FIMrga_H_
#define _FIMrga_H_

enum e_rga_buf_type {
	RGA_IMGBUF_COLOR,
	RGA_IMGBUF_GEM,
	RGA_IMGBUF_USERPTR,
};

#define RGA_PLANE_MAX_NR	3
#define RGA_MAX_CMD_NR		32
#define RGA_MAX_GEM_CMD_NR	10
#define RGA_MAX_CMD_LIST_NR     64

struct rga_image {
	unsigned int			color_mode;
	unsigned int			width;
	unsigned int			height;
	unsigned int			stride;
	unsigned int			hstride;
	unsigned int			fill_color;
	enum e_rga_buf_type		buf_type;
	unsigned int			bo[RGA_PLANE_MAX_NR];
	struct drm_rockchip_rga_userptr	user_ptr[RGA_PLANE_MAX_NR];
};

struct rga_context {
	int				fd;
	int				log;
	unsigned int			major;
	unsigned int			minor;
	struct drm_rockchip_rga_cmd	cmd[RGA_MAX_CMD_NR];
	struct drm_rockchip_rga_cmd	cmd_buf[RGA_MAX_GEM_CMD_NR];
	unsigned int			cmd_nr;
	unsigned int			cmd_buf_nr;
	unsigned int			cmdlist_nr;
};

struct rga_context *rga_init(int fd);

void rga_fini(struct rga_context *ctx);

int rga_exec(struct rga_context *ctx);

int rga_solid_fill(struct rga_context *ctx, struct rga_image *img,
		   unsigned int x, unsigned int y, unsigned int w,
		   unsigned int h);

int rga_copy(struct rga_context *ctx, struct rga_image *src,
	     struct rga_image *dst, unsigned int src_x,
	     unsigned int src_y, unsigned int dst_x, unsigned int dst_y,
	     unsigned int w, unsigned int h);

int rga_copy_with_scale(struct rga_context *ctx, struct rga_image *src,
			struct rga_image *dst, unsigned int src_x,
			unsigned int src_y, unsigned int src_w,
			unsigned int src_h, unsigned int dst_x,
			unsigned int dst_y, unsigned int dst_w,
			unsigned int dst_h);

int rga_copy_with_rotate(struct rga_context *ctx, struct rga_image *src,
			 struct rga_image *dst, unsigned int src_x,
			 unsigned int src_y, unsigned int src_w,
			 unsigned int src_h, unsigned int dst_x,
			 unsigned int dst_y, unsigned int dst_w,
			 unsigned int dst_h, unsigned int degree);

int rga_multiple_transform(struct rga_context *ctx, struct rga_image *src,
			   struct rga_image *dst, unsigned int src_x,
			   unsigned int src_y, unsigned int src_w,
			   unsigned int src_h, unsigned int dst_x,
			   unsigned int dst_y, unsigned int dst_w,
			   unsigned int dst_h, unsigned int degree,
			   unsigned int x_mirr, unsigned int y_mirr);
#endif /* _RGA_H_ */
