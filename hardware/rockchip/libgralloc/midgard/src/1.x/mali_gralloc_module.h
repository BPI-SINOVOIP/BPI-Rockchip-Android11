/*
 * Copyright (C) 2016, 2018-2020 ARM Limited. All rights reserved.
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
#ifndef MALI_GRALLOC_MODULE_H_
#define MALI_GRALLOC_MODULE_H_

#include <system/window.h>
#include <linux/fb.h>
#include <pthread.h>

#if GRALLOC_VERSION_MAJOR == 1
#include <hardware/gralloc1.h>
#else
#if HIDL_COMMON_VERSION_SCALED == 100
#include <android/hardware/graphics/common/1.0/types.h>
#elif HIDL_COMMON_VERSION_SCALED == 110
#include <android/hardware/graphics/common/1.1/types.h>
#endif
#endif

#include <ion/ion_4.12.h>

#include "mali_gralloc_log.h"

typedef enum
{
	MALI_DPY_TYPE_UNKNOWN = 0,
	MALI_DPY_TYPE_CLCD,
	MALI_DPY_TYPE_HDLCD
} mali_dpy_type;

#if GRALLOC_VERSION_MAJOR == 1
extern hw_module_methods_t mali_gralloc_module_methods;

typedef struct
{
	struct hw_module_t common;
} gralloc_module_t;
#endif

struct private_module_t
{
#if GRALLOC_VERSION_MAJOR == 1
	gralloc_module_t base;
#endif

	struct private_handle_t *framebuffer;
	uint32_t flags;
	uint32_t numBuffers;
	uint32_t bufferMask;
	pthread_mutex_t lock;
	buffer_handle_t currentBuffer;
	mali_dpy_type dpy_type;

	struct fb_var_screeninfo info;
	struct fb_fix_screeninfo finfo;
	float xdpi;
	float ydpi;
	float fps;
	int swapInterval;
	uint64_t fbdev_format;

#ifdef __cplusplus
	/* Never intended to be used from C code */
	enum
	{
		// flag to indicate we'll post this buffer
		PRIV_USAGE_LOCKED_FOR_POST = 0x80000000
	};

	#define INIT_ZERO(obj) (memset(&(obj), 0, sizeof((obj))))
	private_module_t()
	{
	#if GRALLOC_VERSION_MAJOR == 1
		base.common.tag = HARDWARE_MODULE_TAG;
		/* Force incompatibility with Android Gralloc2on1 wrappers by setting an invalid version. */
		base.common.version_major = -1;
		MALI_GRALLOC_LOGE("Arm Module v1.0 (fb only)");
		base.common.version_minor = 0;
		base.common.id = GRALLOC_HARDWARE_MODULE_ID;
		base.common.name = "Graphics Memory Allocator Module";
		base.common.author = "ARM Ltd.";
		base.common.methods = &mali_gralloc_module_methods;
		base.common.dso = NULL;
		INIT_ZERO(base.common.reserved);
	#endif

		framebuffer = NULL;
		flags = 0;
		numBuffers = 0;
		bufferMask = 0;
		pthread_mutex_init(&(lock), NULL);
		currentBuffer = NULL;
		INIT_ZERO(info);
		INIT_ZERO(finfo);
		xdpi = 0.0f;
		ydpi = 0.0f;
		fps = 0.0f;
		swapInterval = 1;
	}
	#undef INIT_ZERO
#endif /* For #ifdef __cplusplus */
};

typedef struct private_module_t mali_gralloc_module;

#endif /* MALI_GRALLOC_MODULE_H_ */
