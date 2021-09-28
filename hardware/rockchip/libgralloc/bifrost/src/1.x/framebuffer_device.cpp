/*
 * Copyright (C) 2010-2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This file implements the framebuffer device API in legacy HAL Gralloc
 * and is therefore only used in Gralloc 1.0.
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <system/window.h>
#include <log/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/fb.h>

#include <hardware/gralloc1.h>

#include "1.x/mali_gralloc_module.h"
#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_helper.h"
#include "1.x/gralloc_vsync.h"
#include "core/mali_gralloc_bufferaccess.h"
#include "fbdev/mali_gralloc_framebuffer.h"
#include "allocator/mali_gralloc_ion.h"
#include "gralloc_buffer_priv.h"

#define STANDARD_LINUX_SCREEN

static int fb_set_swap_interval(struct framebuffer_device_t *dev, int interval)
{
	if (interval < dev->minSwapInterval)
	{
		interval = dev->minSwapInterval;
	}
	else if (interval > dev->maxSwapInterval)
	{
		interval = dev->maxSwapInterval;
	}

	private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);
	m->swapInterval = interval;

	if (0 == interval)
	{
		gralloc_vsync_disable(dev);
	}
	else
	{
		gralloc_vsync_enable(dev);
	}

	return 0;
}

static int fb_post(struct framebuffer_device_t *dev, buffer_handle_t buffer)
{
	if (private_handle_t::validate(buffer) < 0)
	{
		return -EINVAL;
	}

	private_handle_t const *hnd = reinterpret_cast<private_handle_t const *>(buffer);
	private_module_t *m = reinterpret_cast<private_module_t *>(dev->common.module);

	if (m->currentBuffer)
	{
		mali_gralloc_unlock(m->currentBuffer);
		m->currentBuffer = 0;
	}

	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
	{
#if GRALLOC_USE_LEGACY_LOCK != 1
		/* Buffer is being locked for non-cpu usage, hence no need to populate a valid cpu address parameter */
		mali_gralloc_lock(buffer, private_module_t::PRIV_USAGE_LOCKED_FOR_POST,
		                             0, 0, 0, 0, NULL);
#else
		mali_gralloc_lock(buffer, private_module_t::PRIV_USAGE_LOCKED_FOR_POST, -1, -1, -1, -1, NULL);
#endif
		int interrupt;
		m->info.activate = FB_ACTIVATE_VBL;
		m->info.yoffset = hnd->offset / m->finfo.line_length;

#ifdef STANDARD_LINUX_SCREEN

		if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1)
		{
			MALI_GRALLOC_LOGE("FBIOPAN_DISPLAY failed for fd: %d", m->framebuffer->fd);
			mali_gralloc_unlock(buffer);
			return -errno;
		}

#else /*Standard Android way*/

		if (ioctl(m->framebuffer->fd, FBIOPUT_VSCREENINFO, &m->info) == -1)
		{
			MALI_GRALLOC_LOGE("FBIOPUT_VSCREENINFO failed for fd: %d", m->framebuffer->fd);
			mali_gralloc_unlock(m, buffer);
			return -errno;
		}

#endif

		if (0 != gralloc_wait_for_vsync(dev))
		{
			MALI_GRALLOC_LOGE("Gralloc wait for vsync failed for fd: %d", m->framebuffer->fd);
			mali_gralloc_unlock(buffer);
			return -errno;
		}

		m->currentBuffer = buffer;
	}
	else
	{
		void *fb_vaddr;
		void *buffer_vaddr;

#if GRALLOC_USE_LEGACY_LOCK != 1
		mali_gralloc_lock(m->framebuffer, GRALLOC_USAGE_SW_WRITE_RARELY, 0, 0, 0, 0, &fb_vaddr);
		mali_gralloc_lock(buffer, GRALLOC_USAGE_SW_READ_RARELY, 0, 0, 0, 0, &buffer_vaddr);
#else
		mali_gralloc_lock(m->framebuffer, GRALLOC_USAGE_SW_WRITE_RARELY, -1, -1, -1, -1, &fb_vaddr);
		mali_gralloc_lock(buffer, GRALLOC_USAGE_SW_READ_RARELY, -1, -1, -1, -1, &buffer_vaddr);
#endif
		// If buffer's alignment match framebuffer alignment we can do a direct copy.
		// If not we must fallback to do an aligned copy of each line.
		if (hnd->byte_stride == (int)m->finfo.line_length)
		{
			memcpy(fb_vaddr, buffer_vaddr, m->finfo.line_length * m->info.yres);
		}
		else
		{
			uintptr_t fb_offset = 0;
			uintptr_t buffer_offset = 0;
			unsigned int i;

			for (i = 0; i < m->info.yres; i++)
			{
				memcpy((void *)((uintptr_t)fb_vaddr + fb_offset), (void *)((uintptr_t)buffer_vaddr + buffer_offset),
				       m->finfo.line_length);

				fb_offset += m->finfo.line_length;
				buffer_offset += hnd->byte_stride;
			}
		}

		mali_gralloc_unlock(buffer);
		mali_gralloc_unlock(m->framebuffer);
	}

	return 0;
}

static int fb_close(struct hw_device_t *device)
{
	framebuffer_device_t *dev = reinterpret_cast<framebuffer_device_t *>(device);

	if (dev)
	{
		free(dev);
	}

	return 0;
}

int framebuffer_device_open(hw_module_t const *module, const char *name, hw_device_t **device)
{
	int status = -EINVAL;

	GRALLOC_UNUSED(name);

	gralloc1_device_t *gralloc_device;

#if DISABLE_FRAMEBUFFER_HAL == 1
	MALI_GRALLOC_LOGE("Framebuffer HAL not support/disabled %s",
#ifdef MALI_DISPLAY_VERSION
	     "with MALI display enable");
#else
	     "");
#endif
	return -ENODEV;
#endif
	status = gralloc1_open(module, &gralloc_device);
	if (status < 0)
	{
		return status;
	}

	private_module_t *m = (private_module_t *)module;

	/* Populate frame buffer pixel format */
#if GRALLOC_FB_BPP == 16
	m->fbdev_format = MALI_GRALLOC_FORMAT_INTERNAL_RGB_565;
#elif GRALLOC_FB_BPP == 32
	m->fbdev_format = MALI_GRALLOC_FORMAT_INTERNAL_BGRA_8888;
#else
#error "Invalid framebuffer bit depth"
#endif

	status = mali_gralloc_fb_module_init(m);

	/* malloc is used instead of 'new' to instantiate the struct framebuffer_device_t
	 * C++11 spec specifies that if a class/struct has a const member,default constructor
	 * is deleted. So, if 'new' is used to instantiate the class/struct, it will throw
	 * error complaining about deleted constructor. Even if the struct is wrapped in a class
	 * it will still try to use the base class constructor to initialize the members, resulting
	 * in error 'deleted constructor'.
	 * This leaves two options
	 * Option 1: initialize the const members at the instantiation time. With {value1, value2 ..}
	 * Which relies on the order of the members, and if members are reordered or a new member is introduced
	 * it will end up assiging wrong value to members. Designated assignment as well has been removed in C++11
	 * Option 2: use malloc instead of 'new' to allocate the class/struct and initialize the members in code.
	 * This is the only maintainable option available.
	 */

	framebuffer_device_t *dev = reinterpret_cast<framebuffer_device_t *>(malloc(sizeof(framebuffer_device_t)));

	/* if either or both of init_frame_buffer() and malloc failed */
	if ((status < 0) || (!dev))
	{
		gralloc1_close(gralloc_device);
		(!dev) ? (void)(status = -ENOMEM) : free(dev);
		return status;
	}

	memset(dev, 0, sizeof(*dev));

	/* initialize the procs */
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = const_cast<hw_module_t *>(module);
	dev->common.close = fb_close;
	dev->setSwapInterval = fb_set_swap_interval;
	dev->post = fb_post;
	dev->setUpdateRect = 0;

	int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
	const_cast<uint32_t &>(dev->flags) = 0;
	const_cast<uint32_t &>(dev->width) = m->info.xres;
	const_cast<uint32_t &>(dev->height) = m->info.yres;
	const_cast<int &>(dev->stride) = stride;
	const_cast<int &>(dev->format) = m->fbdev_format;
	const_cast<float &>(dev->xdpi) = m->xdpi;
	const_cast<float &>(dev->ydpi) = m->ydpi;
	const_cast<float &>(dev->fps) = m->fps;
	const_cast<int &>(dev->minSwapInterval) = 0;
	const_cast<int &>(dev->maxSwapInterval) = 1;
	*device = &dev->common;

	gralloc_vsync_enable(dev);

	return status;
}
