/*
 * Copyright (C) 2016-2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
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
#include <errno.h>

#include <hardware/hardware.h>
#include <hardware/fb.h>

#include "1.x/mali_gralloc_module.h"
#include "mali_gralloc_buffer.h"
#include "1.x/mali_gralloc_public_interface.h"
#include "1.x/framebuffer_device.h"

static int mali_gralloc_module_device_open(const hw_module_t *module, const char *name,
                                           hw_device_t **device)
{
	int status = -EINVAL;

	/* Gralloc 1.x is not supported.
	 * GPUCORE-21547: Remove all Gralloc 1.x code from the codebase.*/
	if (!strncmp(name, GRALLOC_HARDWARE_MODULE_ID, MALI_GRALLOC_HARDWARE_MAX_STR_LEN))
	{
		status = mali_gralloc_device_open(module, name, device);
	}
	else if (!strncmp(name, GRALLOC_HARDWARE_FB0, MALI_GRALLOC_HARDWARE_MAX_STR_LEN))
	{
		status = framebuffer_device_open(module, name, device);
	}

	return status;
}

struct hw_module_methods_t mali_gralloc_module_methods = { mali_gralloc_module_device_open };

/* Export the private module as HAL_MODULE_INFO_SYM symbol as required by Gralloc v1.0 */
struct private_module_t HAL_MODULE_INFO_SYM;
