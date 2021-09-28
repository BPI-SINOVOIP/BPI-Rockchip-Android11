/*
 * Copyright (C) 2020 ARM Limited. All rights reserved.
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

#include "gralloc_capabilities.h"

#include <string.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>

#include "core/format_info.h"

/* Writing to runtime_caps_read is guarded by mutex caps_init_mutex. */
static pthread_mutex_t caps_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool runtime_caps_read = false;

mali_gralloc_format_caps cpu_runtime_caps;
mali_gralloc_format_caps dpu_runtime_caps;
mali_gralloc_format_caps dpu_aeu_runtime_caps;
mali_gralloc_format_caps vpu_runtime_caps;
mali_gralloc_format_caps gpu_runtime_caps;
mali_gralloc_format_caps cam_runtime_caps;

#define MALI_GRALLOC_GPU_LIB_NAME "libGLES_mali.so"
#define MALI_GRALLOC_VPU_LIB_NAME "libstagefrighthw.so"
#define MALI_GRALLOC_DPU_LIB_NAME "hwcomposer.drm.so"
#define MALI_GRALLOC_DPU_AEU_LIB_NAME "dpu_aeu_fake_caps.so"

#if defined(MALI_GRALLOC_VENDOR_VPU) && (MALI_GRALLOC_VENDOR_VPU == 1)
#define MALI_GRALLOC_VPU_LIBRARY_PATH "/vendor/lib/"
#else
#define MALI_GRALLOC_VPU_LIBRARY_PATH "/system/lib/"
#endif

static bool get_block_capabilities(const char *name, mali_gralloc_format_caps *block_caps)
{
	void *dso_handle = NULL;
	bool rval = false;

	/* Clear any error conditions */
	dlerror();

	/* Look for MALI_GRALLOC_FORMATCAPS_SYM_NAME_STR symbol in user-space drivers
	 * to determine hw format capabilities.
	 */
	dso_handle = dlopen(name, RTLD_LAZY);

	if (dso_handle)
	{
		void *sym = dlsym(dso_handle, MALI_GRALLOC_FORMATCAPS_SYM_NAME_STR);

		if (sym)
		{
			memcpy((void *)block_caps, sym, sizeof(mali_gralloc_format_caps));
			rval = true;
		}
		else
		{
			MALI_GRALLOC_LOGE("Symbol %s is not found in %s shared object",
			      MALI_GRALLOC_FORMATCAPS_SYM_NAME_STR, name);
		}
		dlclose(dso_handle);
	}
	else
	{
		// MALI_GRALLOC_LOGW("Unable to dlopen %s shared object, error = %s", name, dlerror());
	}

	return rval;
}

void get_ip_capabilities(void)
{
	/* Ensure capability setting is not interrupted by other
	 * allocations during start-up.
	 */
	pthread_mutex_lock(&caps_init_mutex);

	if (runtime_caps_read)
	{
		goto already_init;
	}

	sanitize_formats();

	memset((void *)&cpu_runtime_caps, 0, sizeof(cpu_runtime_caps));
	memset((void *)&dpu_runtime_caps, 0, sizeof(dpu_runtime_caps));
	memset((void *)&dpu_aeu_runtime_caps, 0, sizeof(dpu_aeu_runtime_caps));
	memset((void *)&vpu_runtime_caps, 0, sizeof(vpu_runtime_caps));
	memset((void *)&gpu_runtime_caps, 0, sizeof(gpu_runtime_caps));
	memset((void *)&cam_runtime_caps, 0, sizeof(cam_runtime_caps));

	/* Determine CPU IP capabilities */
	cpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
	cpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102;
	cpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA16161616;

	/* Determine DPU IP capabilities */
	if (!get_block_capabilities(MALI_GRALLOC_DPU_LIBRARY_PATH MALI_GRALLOC_DPU_LIB_NAME, &dpu_runtime_caps))
	{
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ;
#if MALI_DISPLAY_VERSION >= 500
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE;
#if MALI_DISPLAY_VERSION >= 550
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
#endif
#endif
#if MALI_DISPLAY_VERSION == 71
		/* Cetus adds support for wide block and tiled headers. */
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS;
		dpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102;

		/* DPU Architecture v1.0 specification, chapter 5.16 - AFBC encoding process */
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;
		dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102;
#endif
	}

	/* Determine DPU AEU IP capabilities */
	if (!get_block_capabilities(MALI_GRALLOC_DPU_AEU_LIBRARY_PATH MALI_GRALLOC_DPU_AEU_LIB_NAME, &dpu_aeu_runtime_caps))
	{
		if (dpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS)
		{
			/* Based on DPU caps when DPU supports AFBC tiled headers. */
			dpu_aeu_runtime_caps.caps_mask = dpu_runtime_caps.caps_mask;
			dpu_aeu_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;
			dpu_aeu_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ;
			dpu_aeu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;
		}
	}

	/* Determine GPU IP capabilities */
	if (access(MALI_GRALLOC_GPU_LIBRARY_PATH1 MALI_GRALLOC_GPU_LIB_NAME, R_OK) == 0)
	{
		get_block_capabilities(MALI_GRALLOC_GPU_LIBRARY_PATH1 MALI_GRALLOC_GPU_LIB_NAME, &gpu_runtime_caps);
	}
	else if (access(MALI_GRALLOC_GPU_LIBRARY_PATH2 MALI_GRALLOC_GPU_LIB_NAME, R_OK) == 0)
	{
		get_block_capabilities(MALI_GRALLOC_GPU_LIBRARY_PATH2 MALI_GRALLOC_GPU_LIB_NAME, &gpu_runtime_caps);
	}

	if ((gpu_runtime_caps.caps_mask & MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT) == 0)
	{
#if defined(MALI_GPU_SUPPORT_AFBC_BASIC) && (MALI_GPU_SUPPORT_AFBC_BASIC == 1)
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA1010102;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_PIXFMT_RGBA16161616;

#if defined(MALI_GPU_SUPPORT_AFBC_YUV_WRITE) && (MALI_GPU_SUPPORT_AFBC_YUV_WRITE == 1)
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;
#endif

#if defined(MALI_GPU_SUPPORT_AFBC_SPLITBLK) && (MALI_GPU_SUPPORT_AFBC_SPLITBLK == 1)
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
#endif

#if defined(MALI_GPU_SUPPORT_AFBC_WIDEBLK) && (MALI_GPU_SUPPORT_AFBC_WIDEBLK == 1)
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;
#endif

#if defined(MALI_GPU_SUPPORT_AFBC_TILED_HEADERS) && (MALI_GPU_SUPPORT_AFBC_TILED_HEADERS == 1)
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_SPLITBLK;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WIDEBLK;
		gpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS;
#endif
#endif /* defined(MALI_GPU_SUPPORT_AFBC_BASIC) && (MALI_GPU_SUPPORT_AFBC_BASIC == 1) */
	}

	/* Determine VPU IP capabilities */
	if (!get_block_capabilities(MALI_GRALLOC_VPU_LIBRARY_PATH MALI_GRALLOC_VPU_LIB_NAME, &vpu_runtime_caps))
	{
#if MALI_VIDEO_VERSION == 500 || MALI_VIDEO_VERSION == 550
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;
#endif

#if MALI_VIDEO_VERSION == 61
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_BASIC;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_WRITE_NON_SPARSE;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_TILED_HEADERS;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_READ;
		vpu_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_AFBC_YUV_WRITE;
#endif
	}

/* Build specific capability changes */
#if defined(GRALLOC_ARM_NO_EXTERNAL_AFBC) && (GRALLOC_ARM_NO_EXTERNAL_AFBC == 1)
	{
		dpu_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		gpu_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		vpu_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
		cam_runtime_caps.caps_mask &= ~MALI_GRALLOC_FORMAT_CAPABILITY_AFBCENABLE_MASK;
	}
#endif

#if defined(GRALLOC_CAMERA_WRITE_RAW16) && GRALLOC_CAMERA_WRITE_RAW16
		cam_runtime_caps.caps_mask |= MALI_GRALLOC_FORMAT_CAPABILITY_OPTIONS_PRESENT;
#endif

	runtime_caps_read = true;

already_init:
	pthread_mutex_unlock(&caps_init_mutex);

	MALI_GRALLOC_LOGV("GPU format capabilities 0x%" PRIx64, gpu_runtime_caps.caps_mask);
	MALI_GRALLOC_LOGV("DPU format capabilities 0x%" PRIx64, dpu_runtime_caps.caps_mask);
	MALI_GRALLOC_LOGV("VPU format capabilities 0x%" PRIx64, vpu_runtime_caps.caps_mask);
	MALI_GRALLOC_LOGV("CAM format capabilities 0x%" PRIx64, cam_runtime_caps.caps_mask);
}


/* This is used by the unit tests to get the capabilities for each IP. */
extern "C" {
	void mali_gralloc_get_caps(struct mali_gralloc_format_caps *gpu_caps,
	                           struct mali_gralloc_format_caps *vpu_caps,
	                           struct mali_gralloc_format_caps *dpu_caps,
	                           struct mali_gralloc_format_caps *dpu_aeu_caps,
	                           struct mali_gralloc_format_caps *cam_caps)
	{
		get_ip_capabilities();

		memcpy(gpu_caps, (void *)&gpu_runtime_caps, sizeof(*gpu_caps));
		memcpy(vpu_caps, (void *)&vpu_runtime_caps, sizeof(*vpu_caps));
		memcpy(dpu_caps, (void *)&dpu_runtime_caps, sizeof(*dpu_caps));
		memcpy(dpu_aeu_caps, (void *)&dpu_aeu_runtime_caps, sizeof(*dpu_aeu_caps));
		memcpy(cam_caps, (void *)&cam_runtime_caps, sizeof(*cam_caps));
	}
}
