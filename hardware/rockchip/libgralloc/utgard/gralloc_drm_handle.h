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
 * 定义了表征 graphic_buffer_handle 的 类型 gralloc_drm_handle_t, 对应 arm_gralloc 中的 private_handle_t.
 */

#ifndef _GRALLOC_DRM_HANDLE_H_
#define _GRALLOC_DRM_HANDLE_H_

//#include <system/window.h>
#include <cutils/native_handle.h>
#include <system/graphics.h>
#include <hardware/gralloc.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_HWC2
#define USE_HWC2
#endif

#if 1 //RK_DRM_GRALLOC
#define NUM_FB_BUFFERS 3
#define GRALLOC_ARM_UMP_MODULE 0
#define GRALLOC_ARM_DMA_BUF_MODULE 1

#define GRALLOC_UN_USED(arg)     (arg=arg)
typedef enum
{
	MALI_YUV_NO_INFO,
	MALI_YUV_BT601_NARROW,
	MALI_YUV_BT601_WIDE,
	MALI_YUV_BT709_NARROW,
	MALI_YUV_BT709_WIDE
} mali_gralloc_yuv_info;

typedef enum
{
	MALI_DPY_TYPE_UNKNOWN = 0,
	MALI_DPY_TYPE_CLCD,
	MALI_DPY_TYPE_HDLCD
} mali_dpy_type;

enum
{
	PRIV_FLAGS_FRAMEBUFFER = 0x00000001,
	PRIV_FLAGS_USES_UMP    = 0x00000002,
	PRIV_FLAGS_USES_ION    = 0x00000004,
};

#endif

struct gralloc_drm_bo_t;

/**
 * 对应 arm_gralloc 中的 private_handle_t.
 */
struct gralloc_drm_handle_t {
    /* 基类子对象. */
	native_handle_t base;

	/* file descriptors of the underlying dma_buf. */
	int prime_fd;

#if 1 //RK_DRM_GRALLOC
#ifdef USE_HWC2
    /**
     * 用于存储和 rk 平台相关的 attributes 的 shared_memory 的 fd.
     * 由庄晓亮仿照 'share_attr_fd' 实现,
     * 对应 buffer 的具体类型是 rk_ashmem_t,
     *      具体定义在 义在 hardware/libhardware/include/hardware/gralloc.h 中.
     * 对该 buffer 的创建和访问的接口, 也定义在 gralloc_buffer_priv.h 中.
     */
	int ashmem_fd;
#endif

       // uint64_t   internal_format;
       // int        internalWidth;
       // int        internalHeight;
        int	   flags;
        int        byte_stride;
        int        size;
        int        ref;
        int        pixel_stride;

        union {
                off_t    offset;
                uint64_t padding4;
        };
	// cpu_addr out of use
	union
	{
		void   *cpu_addr;
		uint64_t padding;
	};
#ifdef USE_HWC2
	union {
		void*	 ashmem_base;
		uint64_t padding5;
	};
#endif
	mali_gralloc_yuv_info yuv_info;
#endif

	/* integers */
	int magic;

	int width;
	int height;
	int format;
	int usage;

	int name;   /* the name of the bo */
	int stride; /* the stride in bytes, byte_stride. */
	uint32_t phy_addr;
	uint32_t reserve0;
	uint32_t reserve1;
	uint32_t reserve2;

    /* 表征 'this' 在当前进程中的 buffer_object. */
	struct gralloc_drm_bo_t *data; /* pointer to struct gralloc_drm_bo_t */

	// FIXME: the attributes below should be out-of-line
	uint64_t unknown __attribute__((aligned(8)));

	int data_owner; /* owner of data (for validation) */
        // value 是 pid, buffer 被 alloc 的时候 首次有效设置.
};

/**
 * gralloc_drm_handle_t::magic 的固定取值.
 *
 * @see create_bo_handle().
 */
#define GRALLOC_DRM_HANDLE_MAGIC 0x12345678
#ifdef USE_HWC2
#define GRALLOC_DRM_HANDLE_NUM_FDS 2
#else
#define GRALLOC_DRM_HANDLE_NUM_FDS 1
#endif
#define GRALLOC_DRM_HANDLE_NUM_INTS (						\
	((sizeof(struct gralloc_drm_handle_t) - sizeof(native_handle_t))/sizeof(int))	\
	 - GRALLOC_DRM_HANDLE_NUM_FDS)
enum
{
       /* Buffer won't be allocated as AFBC */
       GRALLOC_ARM_USAGE_NO_AFBC = GRALLOC_USAGE_PRIVATE_1 | GRALLOC_USAGE_PRIVATE_2
};

static pthread_mutex_t handle_mutex = PTHREAD_MUTEX_INITIALIZER;

// .R : "buffer_handle_t" : ./include/system/window.h:60:typedef const native_handle_t* buffer_handle_t;
static inline struct gralloc_drm_handle_t *gralloc_drm_handle(buffer_handle_t _handle)
{
	struct gralloc_drm_handle_t *handle = (struct gralloc_drm_handle_t *) _handle;

	pthread_mutex_lock(&handle_mutex);
	if(handle)
	{
		handle->ref++;
	}

	if (handle && (handle->base.version != sizeof(handle->base) ||
		handle->base.numInts != GRALLOC_DRM_HANDLE_NUM_INTS ||
		handle->base.numFds != GRALLOC_DRM_HANDLE_NUM_FDS ||
		handle->magic != GRALLOC_DRM_HANDLE_MAGIC)) {
		ALOGE("invalid handle: version=%d, numInts=%d, numFds=%d, magic=%x",
				handle->base.version, handle->base.numInts,
				handle->base.numFds, handle->magic);
		ALOGE("invalid handle: right version=%zu, numInts=%zu, numFds=%d, magic=%x",
				sizeof(handle->base),GRALLOC_DRM_HANDLE_NUM_INTS,GRALLOC_DRM_HANDLE_NUM_FDS,
				GRALLOC_DRM_HANDLE_MAGIC);
		handle = NULL;
	}
	pthread_mutex_unlock(&handle_mutex);
	return handle;
}

static inline void gralloc_drm_unlock_handle(buffer_handle_t _handle)
{
	struct gralloc_drm_handle_t *handle = (struct gralloc_drm_handle_t *) _handle;
	pthread_mutex_lock(&handle_mutex);
	if(handle)
	{
		handle->ref--;
	}
	pthread_mutex_unlock(&handle_mutex);
}

#ifdef __cplusplus
}
#endif
#endif /* _GRALLOC_DRM_HANDLE_H_ */
