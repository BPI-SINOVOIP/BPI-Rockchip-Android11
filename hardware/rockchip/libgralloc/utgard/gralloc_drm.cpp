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

/**
 * @file gralloc_drm.cpp
 * 实现 gralloc_drm.h 定义的 gralloc_drm_device 的功能接口.
 */

#define LOG_TAG "GRALLOC-DRM"

#include <log/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <vector>

#include "gralloc_drm.h"
#include "gralloc_drm_priv.h"
#include "gralloc_buffer_priv.h"

#define unlikely(x) __builtin_expect(!!(x), 0)

static int32_t gralloc_drm_pid = 0;

static pthread_mutex_t bo_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Return the pid of the process.
 */
static int gralloc_drm_get_pid(void)
{
	if (unlikely(!gralloc_drm_pid))
		android_atomic_write((int32_t) getpid(), &gralloc_drm_pid);

	return gralloc_drm_pid;
}

/*
 * Create the driver for a DRM fd.
 * @param fd
 *      fd_of_drm_dev
 */
static struct gralloc_drm_drv_t *
init_drv_from_fd(int fd)
{
	struct gralloc_drm_drv_t *drv = NULL;
	drmVersionPtr version;

	/* get the kernel module name */
	version = drmGetVersion(fd);
	if (!version) {
		ALOGE("invalid DRM fd");
		return NULL;
	}

	if (version->name) {
#ifdef ENABLE_PIPE
		drv = gralloc_drm_drv_create_for_pipe(fd, version->name);
#endif

#ifdef ENABLE_INTEL
		if (!drv && !strcmp(version->name, "i915"))
			drv = gralloc_drm_drv_create_for_intel(fd);
#endif
#ifdef ENABLE_RADEON
		if (!drv && !strcmp(version->name, "radeon"))
			drv = gralloc_drm_drv_create_for_radeon(fd);
#endif
#ifdef ENABLE_ROCKCHIP
		if (!drv && !strcmp(version->name, "rockchip"))
            /* .KP : 关联 rk_drm 对 driver_of_gralloc_drm_device_t 的实现. */
			drv = gralloc_drm_drv_create_for_rockchip(fd);
#endif
#ifdef ENABLE_NOUVEAU
		if (!drv && !strcmp(version->name, "nouveau"))
			drv = gralloc_drm_drv_create_for_nouveau(fd);
#endif
	}

	if (!drv) {
		ALOGE("unsupported driver: %s", (version->name) ?
				version->name : "NULL");
	}

	drmFreeVersion(version);

	return drv;
}

/*
 * Create a DRM device object (gralloc_drm_device).
 */
struct gralloc_drm_t *gralloc_drm_create(void)
{
	char path[PROPERTY_VALUE_MAX];
	struct gralloc_drm_t *drm;

	drm = new gralloc_drm_t;
	if (!drm)
		return NULL;

	property_get("vendor.ggralloc.drm.device", path, "/dev/dri/renderD128");
	drm->fd = open(path, O_RDWR); // DP : fd_of_drm_dev
	if (drm->fd < 0) {
		ALOGE("failed to open %s", path);
		return NULL;
	}

	drm->drv = init_drv_from_fd(drm->fd);
	if (!drm->drv) {
		close(drm->fd);
		delete drm;
		return NULL;
	}

	return drm;
}

/*
 * Destroy a DRM device object.
 */
void gralloc_drm_destroy(struct gralloc_drm_t *drm)
{
	if (drm && drm->drv)
		drm->drv->destroy(drm->drv);
	close(drm->fd);
	delete drm;
}

/*
 * Get the file descriptor of a DRM device object.
 */
int gralloc_drm_get_fd(struct gralloc_drm_t *drm)
{
	return drm->fd;
}

/*
 * Validate a buffer handle and return the associated bo.
 * 某些 case 中还完成对 buffer 的 import 操作, 见对参数 'drm' 的说明.
 * .R : 隐式的语义, 不好.
 *
 * @param _handle
 *      待 check 的 buffer
 * @param drm
 *      若是 NULL, 仅仅 check '_handle' 在当前进程是否是有效的.
 *          若无效, 返回 NULL.
 *      若非 NULL, 且当前 进程 "不是" alloc 当前 buffer 的进程, 返回 register(import) 到当前进程之后的 bo.
 *          仅在 gralloc_drm_handle_register() 中这样使用.
 */
static struct gralloc_drm_bo_t *validate_handle(buffer_handle_t _handle,
		struct gralloc_drm_t *drm)
{
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
		return NULL;

	/* the buffer handle is passed to a new process */
    // 若 当前 进程 "不是" alloc 当前 buffer 的进程, 即 "_handle" 来自 remote 进程, 则...
	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		struct gralloc_drm_bo_t *bo;

		/* check only */
		if (!drm)
		{
		    gralloc_drm_unlock_handle(_handle);
			return NULL;
		}

		ALOGD_IF(RK_DRM_GRALLOC_DEBUG,"handle: name=%d pfd=%d\n", handle->name,handle->prime_fd);
		/* create the struct gralloc_drm_bo_t locally */
		if (handle->name || handle->prime_fd >= 0)
        {
            /* 通过当前的 driver_of_gralloc_drm_device_t, create the struct gralloc_drm_bo_t locally */
			bo = drm->drv->alloc(drm->drv, handle); // .trick : "alloc" : 这里实现将完成 import 操作.
        }
		else /* an invalid handle */
			bo = NULL;
		if (bo) {
			bo->drm = drm;
			bo->imported = 1;
			bo->handle = handle;
			bo->refcount = 0;
		}

		handle->data_owner = gralloc_drm_get_pid();
		handle->data = bo;
	}
	gralloc_drm_unlock_handle(_handle);
	return handle->data;
}

/*
 * Register a buffer handle.
 */
int gralloc_drm_handle_register(buffer_handle_t handle, struct gralloc_drm_t *drm)
{
    struct gralloc_drm_bo_t *bo;

    pthread_mutex_lock(&bo_mutex);
    bo = validate_handle(handle, drm);
    if (!bo) {
	pthread_mutex_unlock(&bo_mutex);
        return -EINVAL;
    }

    //If handle is modified,then we need update bo->handle.
    if(bo->imported==1 && (unsigned long)bo->handle != (unsigned long)handle)
    {
        ALOGD_IF(RK_DRM_GRALLOC_DEBUG, "%s: update bo->handle=%p ==> handle=%p",__FUNCTION__,bo->handle,handle);
        bo->handle = (struct gralloc_drm_handle_t *)handle;
    }

    bo->refcount++;
    pthread_mutex_unlock(&bo_mutex);

	return 0;
}

/*
 * Unregister a buffer handle.  It is no-op for handles created locally.
 */
int gralloc_drm_handle_unregister(buffer_handle_t handle)
{
	struct gralloc_drm_bo_t *bo;

	bo = validate_handle(handle, NULL);
	if (!bo)
		return -EINVAL;

    //If handle is modified,then we need update bo->handle.
    if(bo->imported==1 && (unsigned long)bo->handle != (unsigned long)handle)
    {
        ALOGD_IF(RK_DRM_GRALLOC_DEBUG, "%s: update bo->handle=%p ==> handle=%p",__FUNCTION__,bo->handle,handle);
        bo->handle = (struct gralloc_drm_handle_t *)handle;
    }

    gralloc_drm_bo_decref(bo);

	return 0;
}

/*
 * Create a buffer handle.
 */
static struct gralloc_drm_handle_t *create_bo_handle(int width,
		int height, int format, int usage)
{
	struct gralloc_drm_handle_t *handle;

	handle = new gralloc_drm_handle_t;
	if (!handle)
		return NULL;

	handle->base.version = sizeof(handle->base);
	handle->base.numInts = GRALLOC_DRM_HANDLE_NUM_INTS;
	handle->base.numFds = GRALLOC_DRM_HANDLE_NUM_FDS;

	handle->magic = GRALLOC_DRM_HANDLE_MAGIC;
	handle->width = width;
	handle->height = height;
	handle->format = format;
	handle->usage = usage;
	handle->prime_fd = -1;
#if RK_DRM_GRALLOC
#ifdef USE_HWC2
	handle->ashmem_fd = -1;
#endif
	handle->yuv_info = MALI_YUV_NO_INFO;
	handle->phy_addr = 0;
#endif
	ALOGD_IF(RK_DRM_GRALLOC_DEBUG,"create_bo_handle handle: version=%d, numInts=%d, numFds=%d, magic=%x",
		handle->base.version, handle->base.numInts,
		handle->base.numFds, handle->magic);

	return handle;
}

/*
 * Create a bo.
 */
struct gralloc_drm_bo_t *gralloc_drm_bo_create(struct gralloc_drm_t *drm,
		int width, int height, int format, int usage)
{
	struct gralloc_drm_bo_t *bo;
	struct gralloc_drm_handle_t *handle;

	handle = create_bo_handle(width, height, format, usage);
	if (!handle)
	{
	    ALOGE("zxl create_bo_handle failed");
		return NULL;
    }

	bo = drm->drv->alloc(drm->drv, handle); // "alloc" : drm_gem_rockchip_alloc(), 这里实现 allocate.
	if (!bo) {
		delete handle;
		return NULL;
	}

	bo->drm = drm;
	bo->imported = 0;
	bo->handle = handle;
	bo->fb_id = 0;
	bo->refcount = 1;

	handle->data_owner = gralloc_drm_get_pid();
	handle->data = bo;
	handle->ref = 0;

	return bo;
}

/*
 * Destroy a bo.
 */
static void gralloc_drm_bo_destroy(struct gralloc_drm_bo_t *bo)
{
	struct gralloc_drm_handle_t *handle = bo->handle;
	int imported = bo->imported;

	/* gralloc still has a reference */
	if (bo->refcount)
		return;

	//bo->drm->drv->free(bo->drm->drv, bo);
	drm_gem_rockchip_free(NULL, bo);
	if (imported) {
		handle->data_owner = 0;
		handle->data = 0;
	}
	else {
	    if(!handle->ref)
		{
		    delete handle;
		}
		else
		    ALOGE("zxl:gralloc_drm_bo_destroy handle ref=%d",handle->ref);
	}
}

/*
 * Decrease refcount, if no refs anymore then destroy.
 */
void gralloc_drm_bo_decref(struct gralloc_drm_bo_t *bo)
{
	pthread_mutex_lock(&bo_mutex);
	if (!--bo->refcount)
		gralloc_drm_bo_destroy(bo);
	pthread_mutex_unlock(&bo_mutex);
}

/*
 * Return the bo of a registered handle.
 */
struct gralloc_drm_bo_t *gralloc_drm_bo_from_handle(buffer_handle_t handle)
{
	struct gralloc_drm_bo_t *bo;

	pthread_mutex_lock(&bo_mutex);
	bo = validate_handle(handle, NULL);
	if (bo)
		bo->refcount++;
	pthread_mutex_unlock(&bo_mutex);

    //If handle is modified,then we need update bo->handle.
    if(bo->imported==1 && (unsigned long)bo->handle != (unsigned long)handle)
    {
        ALOGD_IF(RK_DRM_GRALLOC_DEBUG, "%s: update bo->handle=%p ==> handle=%p",__FUNCTION__,bo->handle,handle);
        bo->handle = (struct gralloc_drm_handle_t *)handle;
    }

	return bo;
}

int gralloc_drm_free_bo_from_handle(buffer_handle_t handle)
{
	struct gralloc_drm_bo_t *bo;

	bo = validate_handle(handle, NULL);
	if (!bo)
		return -EINVAL;

    //If handle is modified,then we need update bo->handle.
    if(bo->imported==1 && (unsigned long)bo->handle != (unsigned long)handle)
    {
        ALOGD_IF(RK_DRM_GRALLOC_DEBUG, "%s: update bo->handle=%p ==> handle=%p",__FUNCTION__,bo->handle,handle);
        bo->handle = (struct gralloc_drm_handle_t *)handle;
    }

	gralloc_drm_bo_decref(bo);

	return 0;
}

/*
 * Get the buffer handle and stride of a bo.
 * @param stride
 *      to return stride in bytes.
 */
buffer_handle_t gralloc_drm_bo_get_handle(struct gralloc_drm_bo_t *bo, int *stride)
{
	if (stride)
		*stride = bo->handle->stride;
	return &bo->handle->base;
}

/*
 * Query YUV component offsets for a buffer handle
 */
void gralloc_drm_resolve_format(buffer_handle_t _handle,
	uint32_t *pitches, uint32_t *offsets, uint32_t *handles)
{
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);
	struct gralloc_drm_bo_t *bo = handle->data;
	struct gralloc_drm_t *drm = bo->drm;

	/* if handle exists and driver implements resolve_format */
	if (handle && drm->drv->resolve_format)
		drm->drv->resolve_format(drm->drv, bo,
			pitches, offsets, handles);
    gralloc_drm_unlock_handle(_handle);
}

/*
 * Lock a bo.  XXX thread-safety?
 */
int gralloc_drm_bo_lock(struct gralloc_drm_bo_t *bo,
		int usage, int x, int y, int w, int h,
		void **addr)
{
	if ((bo->handle->usage & usage) != usage) {
		/* make FB special for testing software renderer with */

		if (!(bo->handle->usage & GRALLOC_USAGE_HW_FB)
		&&  !(bo->handle->usage & GRALLOC_USAGE_HW_TEXTURE)) {
			ALOGE("bo.usage:0x%X/usage:0x%X is not GRALLOC_USAGE_HW_FB or GRALLOC_USAGE_HW_TEXTURE"
				,bo->handle->usage,usage);
			//zxl: remove this limit for fail of cts as follow:
			//		run cts -o -a arm64-v8a --skip-all-system-status-check  -m CtsRenderscriptTestCases -t android.renderscript.cts.AllocationCreateAllocationsTest#testMultipleIoReceive_USAGE_IO_INPUT
//			return -EINVAL;
		}
	}

	/* allow multiple locks with compatible usages */
	if (bo->lock_count && (bo->locked_for & usage) != usage)
		return -EINVAL;

	usage |= bo->locked_for;

	if (usage & (GRALLOC_USAGE_SW_WRITE_MASK |
		     GRALLOC_USAGE_SW_READ_MASK)) {
		/* the driver is supposed to wait for the bo */
		int write = !!(usage & GRALLOC_USAGE_SW_WRITE_MASK);
		int err = bo->drm->drv->map(bo->drm->drv, bo,
				x, y, w, h, write, addr);
		if (err)
			return err;
	}
	else {
		/* kernel handles the synchronization here */
	}

	bo->lock_count++;
	bo->locked_for |= usage;

	return 0;
}

/*
 * Unlock a bo.
 */
void gralloc_drm_bo_unlock(struct gralloc_drm_bo_t *bo)
{
	int mapped = bo->locked_for &
		(GRALLOC_USAGE_SW_WRITE_MASK | GRALLOC_USAGE_SW_READ_MASK);

	if (!bo->lock_count)
		return;

	if (mapped)
		bo->drm->drv->unmap(bo->drm->drv, bo);

	bo->lock_count--;
	if (!bo->lock_count)
		bo->locked_for = 0;
}

#ifdef USE_HWC2
int gralloc_drm_handle_get_rk_ashmem(buffer_handle_t _handle, struct rk_ashmem_t* rk_ashmem)
    // "rk_ashmem_t" : 定义在 hardware/libhardware/include/hardware/gralloc.h 中
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}

	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get prime fd before register buffer.");
	} else {
		if (rk_ashmem) {
				ret = 0;
				if(gralloc_rk_ashmem_map(handle, 0) >= 0) {
					 if (gralloc_rk_ashmem_read(handle, rk_ashmem) < 0) {
						 ALOGE("%s: gralloc_rk_ashmem_read fail",__FUNCTION__);
						 ret = -EINVAL;
					 }
					 gralloc_rk_ashmem_unmap(handle);
				} else {
					ALOGE("%s: gralloc_rk_ashmem_map fail",__FUNCTION__);
					ret = -EINVAL;
				}
		} else  {
				ALOGE("%s: rk_ashmem is null",__FUNCTION__);
				ret = -EINVAL;
		}

	}

    gralloc_drm_unlock_handle(_handle);
	return ret;
}

int gralloc_drm_handle_set_rk_ashmem(buffer_handle_t _handle, struct rk_ashmem_t* rk_ashmem)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get prime fd before register buffer.");
	} else {
		if (rk_ashmem) {
				ret = 0;
				if(gralloc_rk_ashmem_map(handle, 1) >= 0) {
					 if (gralloc_rk_ashmem_write(handle, rk_ashmem) < 0) {
						 ALOGE("%s: gralloc_rk_ashmem_write fail",__FUNCTION__);
						 return -EINVAL;
					 }
					 gralloc_rk_ashmem_unmap(handle);
				} else {
					ALOGE("%s: gralloc_rk_ashmem_map fail",__FUNCTION__);
					ret = -EINVAL;
				}
		} else  {
				ALOGE("%s: rk_ashmem is null",__FUNCTION__);
				ret = -EINVAL;
		}

	}

    gralloc_drm_unlock_handle(_handle);
	return ret;
}
#endif

int gralloc_drm_handle_get_phy_addr(buffer_handle_t _handle, uint32_t *phy_addr)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get prime fd before register buffer.");
	} else {
		ret = 0;
		*phy_addr = handle->phy_addr;
	}

    gralloc_drm_unlock_handle(_handle);
	return ret;
}

int gralloc_drm_handle_get_prime_fd(buffer_handle_t _handle, int *fd)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}

	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get prime fd before register buffer.");
	} else {
		ret = 0;
		*fd = handle->prime_fd;
	}

    gralloc_drm_unlock_handle(_handle);
	return ret;
}

int gralloc_drm_handle_get_attributes(buffer_handle_t _handle, void *attrs)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);
	std::vector<int> *attributes = (std::vector<int> *)attrs;

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}

	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get attributes before register buffer.");
	} else {
		ret = 0;
		attributes->clear();
		attributes->push_back(handle->width);
		attributes->push_back(handle->height);
		attributes->push_back(handle->pixel_stride);
		attributes->push_back(handle->format);
		attributes->push_back(handle->size);
		attributes->push_back(handle->stride);
	}
    gralloc_drm_unlock_handle(_handle);
	return ret;
}

#if 0
int gralloc_drm_handle_get_internal_format(buffer_handle_t _handle, uint64_t *internal_format)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
		return -EINVAL;

	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get attributes before register buffer.");
	} else {
		ret = 0;
		*internal_format = handle->internal_format;
	}
    gralloc_drm_unlock_handle(_handle);
	return ret;
}
#endif

int gralloc_drm_handle_get_width(buffer_handle_t _handle, int *width)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*width = handle->width;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}

int gralloc_drm_handle_get_height(buffer_handle_t _handle, int *height)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*height = handle->height;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}

int gralloc_drm_handle_get_stride(buffer_handle_t _handle, int *stride)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*stride = handle->pixel_stride;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}

int gralloc_drm_handle_get_byte_stride(buffer_handle_t _handle, int *byte_stride)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*byte_stride = handle->stride;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}

int gralloc_drm_handle_get_format(buffer_handle_t _handle, int *format)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*format = handle->format;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}

int gralloc_drm_handle_get_size(buffer_handle_t _handle, int *size)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*size = handle->size;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}


int gralloc_drm_handle_get_usage(buffer_handle_t _handle, int *usage)
{
	int ret = 0;
	struct gralloc_drm_handle_t *handle = gralloc_drm_handle(_handle);

	if (!handle)
	{
		gralloc_drm_unlock_handle(_handle);
		return -EINVAL;
	}


	if (unlikely(handle->data_owner != gralloc_drm_pid)) {
		ret = -EPERM;
		ALOGE("handle get usage before register buffer.");
	} else {
		ret = 0;
		*usage = handle->usage;
	}
    gralloc_drm_unlock_handle(_handle);

	return ret;
}
