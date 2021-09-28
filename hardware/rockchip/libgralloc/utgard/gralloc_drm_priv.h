/*
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * 定义和 gralloc_drm_device 内部具体实现有关的类型 等.
 */

#ifndef _GRALLOC_DRM_PRIV_H_
#define _GRALLOC_DRM_PRIV_H_

#include <pthread.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "gralloc_drm_handle.h"

#define GET_VPU_INTO_FROM_HEAD      0 //zxl: 0:get vpu info from head of handle base  1:get vpu info from end of handle base

#ifdef __cplusplus
extern "C" {
#endif

/**
 * .DP : gralloc_drm_device_t, gralloc_drm_t
 * 封装了对 drm_dev 的 fd, 以及 抽象了 功能接口.
 */
struct gralloc_drm_t {
	/* initialized by gralloc_drm_create */
	int fd; // fd_of_drm_dev

    /* 指向 'this' driver_of_gralloc_drm_device_t 实例. */
	struct gralloc_drm_drv_t *drv;
};

/**
 * .DP : drm_gralloc_module_t
 * drm_gralloc 对 gralloc_module_t 的具体实现, 即 drm_gralloc_module_t.
 */
struct drm_module_t {
	gralloc_module_t base;

	pthread_mutex_t mutex;

    /** gralloc_drm_device. */
	struct gralloc_drm_t *drm;
#ifdef __cplusplus
	/* default constructor */
	drm_module_t();
#endif

#if RK_DRM_GRALLOC
	volatile int32_t refcount;
#endif
};

/*
 * .DP : driver_of_gralloc_drm_device_t
 * 定义了对 gralloc_drm_bo_t 实例的一系列操作方法.
 */
struct gralloc_drm_drv_t {
	/* destroy the driver */
	void (*destroy)(struct gralloc_drm_drv_t *drv);

    /* allocate or import a bo
     * .KP : "alloc" 同时有 import 的隐式的语义, 这不好.
     */
	struct gralloc_drm_bo_t *(*alloc)(struct gralloc_drm_drv_t *drv,
			                  struct gralloc_drm_handle_t *handle);

	/* free a bo */
	void (*free)(struct gralloc_drm_drv_t *drv,
		     struct gralloc_drm_bo_t *bo);

	/* map a bo for CPU access */
	int (*map)(struct gralloc_drm_drv_t *drv,
		   struct gralloc_drm_bo_t *bo,
		   int x, int y, int w, int h, int enable_write, void **addr);

	/* unmap a bo */
	void (*unmap)(struct gralloc_drm_drv_t *drv,
		      struct gralloc_drm_bo_t *bo);

	/* query component offsets, strides and handles for a format */
	void (*resolve_format)(struct gralloc_drm_drv_t *drv,
		     struct gralloc_drm_bo_t *bo,
		     uint32_t *pitches, uint32_t *offsets, uint32_t *handles);
};

/**
 * .DP : gralloc_drm_buffer_obj_t, gralloc_drm_bo_t.
 */
struct gralloc_drm_bo_t {
	struct gralloc_drm_t *drm;
	struct gralloc_drm_handle_t *handle;

	int imported;  /* the handle is from a remote proces when true */
	int fb_handle; /* the GEM handle of the bo */
	int fb_id;     /* the fb id */

    /**
     * 当前 bo 实例被 lock 的次数.
     */
	int lock_count;
    /**
     * 对当前 bo 实例的所有 lock 操作的 usage 的 bits_or.
     */
	int locked_for;

	unsigned int refcount;
};

void drm_gem_rockchip_free(struct gralloc_drm_drv_t *drv,        struct gralloc_drm_bo_t *bo);

struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_pipe(int fd, const char *name);
struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_intel(int fd);
struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_radeon(int fd);
/**
 * 创建并返回 driver_of_rk_drm_device.
 * @param fd
 *      fd_of_drm_dev
 */
struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_rockchip(int fd);
struct gralloc_drm_drv_t *gralloc_drm_drv_create_for_nouveau(int fd);

#ifdef __cplusplus
}
#endif
#endif /* _GRALLOC_DRM_PRIV_H_ */
