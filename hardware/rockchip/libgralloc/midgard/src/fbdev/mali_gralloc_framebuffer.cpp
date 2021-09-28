/*
 * Copyright (C) 2019-2020 ARM Limited. All rights reserved.
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

#include "fbdev/mali_gralloc_framebuffer.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <system/window.h>
#include <log/log.h>
#include <cutils/atomic.h>

#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "gralloc_helper.h"
#include "1.x/gralloc_vsync.h"
#include "core/mali_gralloc_bufferaccess.h"
#include "allocator/mali_gralloc_ion.h"
#include "allocator/mali_gralloc_shared_memory.h"
#include "gralloc_buffer_priv.h"

#include <hardware/gralloc1.h>

#include <asm/types.h>

/* NOTE:
 * If your framebuffer device driver is integrated with dma_buf, you will have to
 * change this IOCTL definition to reflect your integration with the framebuffer
 * device.
 * Expected return value is a structure filled with a file descriptor
 * backing your framebuffer device memory.
 */
struct fb_dmabuf_export
{
	__u32 fd;
	__u32 flags;
};
#define FBIOGET_DMABUF _IOR('F', 0x21, struct fb_dmabuf_export)

/* Number of buffers for page flipping */
#define NUM_BUFFERS 2

enum
{
	PAGE_FLIP = 0x00000001,
};

static int init_frame_buffer_locked(struct private_module_t *module)
{
	if (module->framebuffer)
	{
		return 0; // Nothing to do, already initialized
	}

	char const *const device_template[] = { "/dev/graphics/fb%u", "/dev/fb%u", NULL };

	int fd = -1;
	int i = 0;
	char name[64];

	while ((fd == -1) && device_template[i])
	{
		snprintf(name, 64, device_template[i], 0);
		fd = open(name, O_RDWR, 0);
		i++;
	}

	if (fd < 0)
	{
		return -errno;
	}

	struct fb_fix_screeninfo finfo;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
	{
		return -errno;
	}

	struct fb_var_screeninfo info;

	if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
	{
		return -errno;
	}

	info.reserved[0] = 0;
	info.reserved[1] = 0;
	info.reserved[2] = 0;
	info.xoffset = 0;
	info.yoffset = 0;
	info.activate = FB_ACTIVATE_NOW;

#if GRALLOC_FB_BPP == 16
	/*
	 * Explicitly request 5/6/5
	 */
	info.bits_per_pixel = 16;
	info.red.offset = 11;
	info.red.length = 5;
	info.green.offset = 5;
	info.green.length = 6;
	info.blue.offset = 0;
	info.blue.length = 5;
	info.transp.offset = 0;
	info.transp.length = 0;
#elif GRALLOC_FB_BPP == 32
	/*
	 * Explicitly request 8/8/8
	 */
	info.bits_per_pixel = 32;
	info.red.offset = 16;
	info.red.length = 8;
	info.green.offset = 8;
	info.green.length = 8;
	info.blue.offset = 0;
	info.blue.length = 8;
	info.transp.offset = 0;
	info.transp.length = 0;
#else
#error "Invalid framebuffer bit depth"
#endif

	/*
	 * Request NUM_BUFFERS screens (at lest 2 for page flipping)
	 */
	info.yres_virtual = info.yres * NUM_BUFFERS;

	uint32_t flags = PAGE_FLIP;

	if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1)
	{
		info.yres_virtual = info.yres;
		flags &= ~PAGE_FLIP;
		MALI_GRALLOC_LOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported fd: %d", fd);
	}

	if (info.yres_virtual < info.yres * 2)
	{
		// we need at least 2 for page-flipping
		info.yres_virtual = info.yres;
		flags &= ~PAGE_FLIP;
		MALI_GRALLOC_LOGW("page flipping not supported (yres_virtual=%d, requested=%d)", info.yres_virtual, info.yres * 2);
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
	{
		return -errno;
	}

	int refreshRate = 0;

	if (info.pixclock > 0)
	{
		refreshRate =
		    1000000000000000LLU / (uint64_t(info.upper_margin + info.lower_margin + info.yres + info.hsync_len) *
		                           (info.left_margin + info.right_margin + info.xres + info.vsync_len) * info.pixclock);
	}
	else
	{
		MALI_GRALLOC_LOGW("fbdev pixclock is zero for fd: %d", fd);
	}

	if (refreshRate == 0)
	{
		refreshRate = 60 * 1000; // 60 Hz
	}

	if (int(info.width) <= 0 || int(info.height) <= 0)
	{
		// the driver doesn't return that information
		// default to 160 dpi
		info.width = ((info.xres * 25.4f) / 160.0f + 0.5f);
		info.height = ((info.yres * 25.4f) / 160.0f + 0.5f);
	}

	float xdpi = (info.xres * 25.4f) / info.width;
	float ydpi = (info.yres * 25.4f) / info.height;
	float fps = refreshRate / 1000.0f;

	MALI_GRALLOC_LOGI("using (fd=%d)\n"
	     "id           = %s\n"
	     "xres         = %d px\n"
	     "yres         = %d px\n"
	     "xres_virtual = %d px\n"
	     "yres_virtual = %d px\n"
	     "bpp          = %d\n"
	     "r            = %2u:%u\n"
	     "g            = %2u:%u\n"
	     "b            = %2u:%u\n",
	     fd, finfo.id, info.xres, info.yres, info.xres_virtual, info.yres_virtual, info.bits_per_pixel, info.red.offset,
	     info.red.length, info.green.offset, info.green.length, info.blue.offset, info.blue.length);

	MALI_GRALLOC_LOGI("width        = %d mm (%f dpi)\n"
	     "height       = %d mm (%f dpi)\n"
	     "refresh rate = %.2f Hz\n",
	     info.width, xdpi, info.height, ydpi, fps);

	if (0 == strncmp(finfo.id, "CLCD FB", 7))
	{
		module->dpy_type = MALI_DPY_TYPE_CLCD;
	}
	else if (0 == strncmp(finfo.id, "ARM Mali HDLCD", 14))
	{
		module->dpy_type = MALI_DPY_TYPE_HDLCD;
	}
	else if (0 == strncmp(finfo.id, "ARM HDLCD Control", 16))
	{
		module->dpy_type = MALI_DPY_TYPE_HDLCD;
	}
	else
	{
		module->dpy_type = MALI_DPY_TYPE_UNKNOWN;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
	{
		return -errno;
	}

	if (finfo.smem_len <= 0)
	{
		return -errno;
	}

	module->flags = flags;
	module->info = info;
	module->finfo = finfo;
	module->xdpi = xdpi;
	module->ydpi = ydpi;
	module->fps = fps;
	module->swapInterval = 1;

	/*
	 * map the framebuffer
	 */
	size_t fbSize = round_up_to_page_size(finfo.line_length * info.yres_virtual);
	void *vaddr = mmap(0, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (vaddr == MAP_FAILED)
	{
		MALI_GRALLOC_LOGE("Error mapping the framebuffer (%s)", strerror(errno));
		return -errno;
	}

	memset(vaddr, 0, fbSize);
	// Create a "fake" buffer object for the entire frame buffer memory, and store it in the module
	module->framebuffer = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER, fbSize, vaddr,
	                                           static_cast<uint64_t>(GRALLOC_USAGE_HW_FB),
	                                           static_cast<uint64_t>(GRALLOC_USAGE_HW_FB), dup(fd), 0,
	                                           finfo.line_length, info.xres_virtual, info.yres_virtual,
	                                           module->fbdev_format);

	module->numBuffers = info.yres_virtual / info.yres;
	module->bufferMask = 0;

	return 0;
}

int mali_gralloc_fb_module_init(struct private_module_t *module)
{
	pthread_mutex_lock(&module->lock);
	int err = init_frame_buffer_locked(module);
	pthread_mutex_unlock(&module->lock);
	return err;
}

static int fb_alloc_framebuffer_dmabuf(private_module_t *m, private_handle_t *hnd)
{
	struct fb_dmabuf_export fb_dma_buf;
	int res;
	res = ioctl(m->framebuffer->fd, FBIOGET_DMABUF, &fb_dma_buf);

	if (res == 0)
	{
		hnd->share_fd = fb_dma_buf.fd;
		return 0;
	}
	else
	{
		MALI_GRALLOC_LOGI("FBIOGET_DMABUF ioctl failed(%d). See gralloc_priv.h and the integration manual for vendor framebuffer "
		     "integration",
		     res);
		return -1;
	}
}

static int fb_alloc_from_ion_module(mali_gralloc_module *m, int width, int height, int byte_stride, size_t buffer_size,
                                    uint64_t consumer_usage, uint64_t producer_usage, buffer_handle_t *pHandle)
{
	buffer_descriptor_t fb_buffer_descriptor;
	gralloc_buffer_descriptor_t gralloc_buffer_descriptor[1];
	bool shared = false;
	int err = 0;

	fb_buffer_descriptor.width = width;
	fb_buffer_descriptor.height = height;
	fb_buffer_descriptor.size = buffer_size;
	fb_buffer_descriptor.old_alloc_width = width;
	fb_buffer_descriptor.old_alloc_height = height;
	fb_buffer_descriptor.old_byte_stride = byte_stride;
	fb_buffer_descriptor.pixel_stride = width;

	memset(fb_buffer_descriptor.plane_info, 0, sizeof(fb_buffer_descriptor.plane_info));
	fb_buffer_descriptor.plane_info[0].alloc_width = width;
	fb_buffer_descriptor.plane_info[0].alloc_height = height;
	fb_buffer_descriptor.plane_info[0].byte_stride = byte_stride;
	fb_buffer_descriptor.plane_info[0].offset = 0;

	fb_buffer_descriptor.old_internal_format = m->fbdev_format;
	fb_buffer_descriptor.alloc_format = m->fbdev_format;
	fb_buffer_descriptor.consumer_usage = consumer_usage;
	fb_buffer_descriptor.producer_usage = producer_usage;
	fb_buffer_descriptor.layer_count = 1;

	gralloc_buffer_descriptor[0] = (gralloc_buffer_descriptor_t)(&fb_buffer_descriptor);

	err = mali_gralloc_ion_allocate(gralloc_buffer_descriptor, 1, pHandle, &shared);

	return err;
}

static int fb_alloc_framebuffer_locked(mali_gralloc_module *m, uint64_t consumer_usage, uint64_t producer_usage,
                                       buffer_handle_t *pHandle, int *stride, int *byte_stride)
{
	// allocate the framebuffer
	if (m->framebuffer == NULL)
	{
		// initialize the framebuffer, the framebuffer is mapped once and forever.
		int err = init_frame_buffer_locked(m);

		if (err < 0)
		{
			return err;
		}
	}

	uint32_t bufferMask = m->bufferMask;
	const uint32_t numBuffers = m->numBuffers;
	/* framebufferSize is used for allocating the handle to the framebuffer and refers
	 *                 to the size of the actual framebuffer.
	 * alignedFramebufferSize is used for allocating a possible internal buffer and
	 *                        thus need to consider internal alignment requirements. */
	const size_t framebufferSize = m->finfo.line_length * m->info.yres;
	const size_t alignedFramebufferSize = GRALLOC_ALIGN(m->finfo.line_length, 64) * m->info.yres;

	*stride = m->info.xres;

	if (numBuffers == 1)
	{
		// If we have only one buffer, we never use page-flipping. Instead,
		// we return a regular buffer which will be memcpy'ed to the main
		// screen when post is called.
		uint64_t newConsumerUsage = (consumer_usage & ~(static_cast<uint64_t>(GRALLOC_USAGE_HW_FB)));
		uint64_t newProducerUsage = (producer_usage & ~(static_cast<uint64_t>(GRALLOC_USAGE_HW_FB))) |
		                                               static_cast<uint64_t>(GRALLOC_USAGE_HW_2D);
		MALI_GRALLOC_LOGW("fallback to single buffering. Virtual Y-res too small %d", m->info.yres);
		*byte_stride = GRALLOC_ALIGN(m->finfo.line_length, 64);
		return fb_alloc_from_ion_module(m, m->info.xres, m->info.yres, *byte_stride,
		                                alignedFramebufferSize, newConsumerUsage, newProducerUsage, pHandle);
	}

	if (bufferMask >= ((1LU << numBuffers) - 1))
	{
		// We ran out of buffers, reset bufferMask
		bufferMask = 0;
		m->bufferMask = 0;
	}

	uintptr_t framebufferVaddr = (uintptr_t)m->framebuffer->base;

	// find a free slot
	for (uint32_t i = 0; i < numBuffers; i++)
	{
		if ((bufferMask & (1LU << i)) == 0)
		{
			m->bufferMask |= (1LU << i);
			break;
		}

		framebufferVaddr += framebufferSize;
	}

	// The entire framebuffer memory is already mapped, now create a buffer object for parts of this memory
	private_handle_t *hnd = new private_handle_t(
	    private_handle_t::PRIV_FLAGS_FRAMEBUFFER, framebufferSize, (void *)framebufferVaddr, consumer_usage,
	    producer_usage, dup(m->framebuffer->fd), (framebufferVaddr - (uintptr_t)m->framebuffer->base),
	    m->finfo.line_length, m->info.xres, m->info.yres, m->fbdev_format);

	/*
	 * Perform allocator specific actions. If these fail we fall back to a regular buffer
	 * which will be memcpy'ed to the main screen when fb_post is called.
	 */
	if (fb_alloc_framebuffer_dmabuf(m, hnd) == -1)
	{
		delete hnd;
		uint64_t newConsumerUsage = (consumer_usage & ~(static_cast<uint64_t>(GRALLOC_USAGE_HW_FB)));
		uint64_t newProducerUsage = (producer_usage & ~(static_cast<uint64_t>(GRALLOC_USAGE_HW_FB))) |
		                                               static_cast<uint64_t>(GRALLOC_USAGE_HW_2D);
		MALI_GRALLOC_LOGE("Fallback to single buffering. Unable to map framebuffer memory to handle:%p", hnd);
		*byte_stride = GRALLOC_ALIGN(m->finfo.line_length, 64);
		return fb_alloc_from_ion_module(m, m->info.xres, m->info.yres, *byte_stride,
		                                alignedFramebufferSize, newConsumerUsage, newProducerUsage, pHandle);
	}

	*pHandle = hnd;
	*byte_stride = m->finfo.line_length;

	return 0;
}

int32_t mali_gralloc_fb_allocate(private_module_t * const module,
                                 const buffer_descriptor_t * const bufDescriptor,
                                 buffer_handle_t * const outBuffers)
{
	uint64_t format = bufDescriptor->hal_format;

/* Some display controllers expect the framebuffer to be in BGRX format, hence we force the format to avoid colour swap issues. */
#if GRALLOC_FB_SWAP_RED_BLUE == 1
#if GRALLOC_FB_BPP == 16
	format = HAL_PIXEL_FORMAT_RGB_565;
#elif GRALLOC_FB_BPP == 32
	if ((bufDescriptor->producer_usage & GRALLOC_USAGE_SW_WRITE_MASK ||
	    bufDescriptor->consumer_usage & GRALLOC_USAGE_SW_READ_MASK) &&
	    format != HAL_PIXEL_FORMAT_BGRA_8888)
	{
		MALI_GRALLOC_LOGE("Format unsuitable for both framebuffer usage and CPU access. Failing allocation.");
		return -1;
	}
	format = HAL_PIXEL_FORMAT_BGRA_8888;
#else
#error "Invalid framebuffer bit depth"
#endif
#endif

	int byte_stride, pixel_stride;
	pthread_mutex_lock(&module->lock);
	const int status = fb_alloc_framebuffer_locked(module, bufDescriptor->consumer_usage,
	                                               bufDescriptor->producer_usage,
	                                               outBuffers, &pixel_stride, &byte_stride);
	pthread_mutex_unlock(&module->lock);
	if (status < 0)
	{
		return status;
	}
	else
	{
		private_handle_t *hnd = (private_handle_t *)*outBuffers;

		/* Allocate a meta-data buffer for framebuffer too. fbhal
		 * ones wont need it but for hwc they will.
		 */
		hnd->attr_size = GRALLOC_ALIGN(sizeof(attr_region), PAGE_SIZE);
		std::tie(hnd->share_attr_fd, hnd->attr_base) =
		    gralloc_shared_memory_allocate("gralloc_shared_attr", hnd->attr_size);
		if (hnd->share_attr_fd < 0 || hnd->attr_base == MAP_FAILED)
		{
			MALI_GRALLOC_LOGW("Failed to allocate shared memory for framebuffer: %s", strerror(errno));
		}
		else
		{
			new(hnd->attr_base) attr_region();
		}

		hnd->req_format = format;
		hnd->yuv_info = MALI_YUV_BT601_NARROW;
		hnd->internal_format = format;
		hnd->alloc_format = format;
		hnd->byte_stride = byte_stride;
		hnd->width = bufDescriptor->width;
		hnd->height = bufDescriptor->height;
		hnd->stride = pixel_stride;
		hnd->internalWidth = bufDescriptor->width;
		hnd->internalHeight = bufDescriptor->height;
		hnd->layer_count = 1;
		return 0;
	}
}
