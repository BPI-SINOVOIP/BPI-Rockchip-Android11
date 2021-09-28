/*
 * Copyright (C) 2016, 2018-2020 ARM Limited. All rights reserved.
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
#include <inttypes.h>
#include <hardware/hardware.h>
#include <hardware/gralloc1.h>
#include <sync/sync.h>

#include "1.x/framebuffer_device.h"
#include "allocator/mali_gralloc_ion.h"

static void mali_gralloc_getCapabilities(gralloc1_device_t *dev, uint32_t *outCount, int32_t *outCapabilities)
{
	GRALLOC_UNUSED(dev);
	GRALLOC_UNUSED(outCapabilities);
	if (outCount != NULL)
	{
		*outCount = 0;
	}
}

static gralloc1_function_pointer_t mali_gralloc_getFunction(gralloc1_device_t *dev, int32_t descriptor)
{
	GRALLOC_UNUSED(dev);
	GRALLOC_UNUSED(descriptor);
	gralloc1_function_pointer_t rval = NULL;

	return rval;
}

static int mali_gralloc_device_close(struct hw_device_t *device)
{
	gralloc1_device_t *dev = reinterpret_cast<gralloc1_device_t *>(device);
	if (dev)
	{
		delete dev;
	}

	mali_gralloc_ion_close();

	return 0;
}

int mali_gralloc_device_open(hw_module_t const *module, const char *name, hw_device_t **device)
{
	gralloc1_device_t *dev;

	GRALLOC_UNUSED(name);

	dev = new gralloc1_device_t;

	if (NULL == dev)
	{
		return -1;
	}

	/* initialize our state here */
	memset(dev, 0, sizeof(*dev));

	/* initialize the procs */
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = const_cast<hw_module_t *>(module);
	dev->common.close = mali_gralloc_device_close;

	dev->getCapabilities = mali_gralloc_getCapabilities;
	dev->getFunction = mali_gralloc_getFunction;

	*device = &dev->common;

	return 0;
}
