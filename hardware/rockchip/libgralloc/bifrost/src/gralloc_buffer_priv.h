/*
 * Copyright (C) 2014-2018, 2020 Arm Limited. All rights reserved.
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

#ifndef GRALLOC_BUFFER_PRIV_H_
#define GRALLOC_BUFFER_PRIV_H_

#include "mali_gralloc_buffer.h"
#include "mali_gralloc_formats.h"
#include "gralloc_helper.h"
#include <utils/Log.h>
#include <errno.h>
#include <string.h>
#include "mali_gralloc_private_interface_types.h"

// private gralloc buffer manipulation API

/* This structure needs to have the same layout for all architectures and compilers. */
struct attr_region
{
	/* Rectangle to be cropped from the full frame (Origin in top-left corner!) */
	int32_t crop_top;
	int32_t crop_left;
	int32_t crop_height;
	int32_t crop_width;
	android_dataspace_t dataspace;

#ifdef __cplusplus
	attr_region()
	    : crop_top(-1)
	    , crop_left(-1)
	    , crop_height(-1)
	    , crop_width(-1)
	    , dataspace(HAL_DATASPACE_UNKNOWN)
	{
	}
#endif
};

static_assert(sizeof(android_dataspace_t) == 4, "Unexpected size");
/*
 * The purpose of this assert is to ensure 32-bit and 64-bit ABIs have a consistent view
 * of the memory. The assert shouldn't contain any sizeof(), as sizeof() is ABI-dependent.
 */
static_assert(sizeof(struct attr_region) == (5 * 4), "Unexpected size");

typedef struct attr_region attr_region;

struct buffer_descriptor_t;

/*
 * Map the attribute storage area before attempting to
 * read/write from it.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_map(struct private_handle_t *hnd, int readwrite)
{
	int rval = -1;
	int prot_flags = PROT_READ;

	if (!hnd)
	{
		goto out;
	}

	if (hnd->imapper_version >= 400)
	{
		MALI_GRALLOC_LOGE("Legacy attribute region not supported on Gralloc v4+");
		goto out;
	}

	if (hnd->share_attr_fd < 0)
	{
		MALI_GRALLOC_LOGE("Shared attribute region not available to be mapped");
		goto out;
	}

	if (readwrite)
	{
		prot_flags |= PROT_WRITE;
	}

	hnd->attr_base = mmap(NULL, hnd->attr_size, prot_flags, MAP_SHARED, hnd->share_attr_fd, 0);

	if (hnd->attr_base == MAP_FAILED)
	{
		MALI_GRALLOC_LOGE("Failed to mmap shared attribute region err=%s", strerror(errno));
		goto out;
	}

	rval = 0;

out:
	return rval;
}

/*
 * Unmap the attribute storage area when done with it.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_unmap(struct private_handle_t *hnd)
{
	int rval = -1;

	if (!hnd)
	{
		goto out;
	}

	if (hnd->imapper_version >= 400)
	{
		MALI_GRALLOC_LOGE("Legacy attribute region not supported on Gralloc v4+");
		goto out;
	}

	if (hnd->attr_base != MAP_FAILED)
	{
		if (munmap(hnd->attr_base, hnd->attr_size) == 0)
		{
			hnd->attr_base = MAP_FAILED;
			rval = 0;
		}
	}

out:
	return rval;
}

/*
 * Read or write an attribute from/to the storage area.
 *
 * Return 0 on success.
 */
static inline int gralloc_buffer_attr_write(struct private_handle_t *hnd, buf_attr attr, int *val)
{
	int rval = -1;

	if (!hnd || !val || attr >= GRALLOC_ARM_BUFFER_ATTR_LAST)
	{
		goto out;
	}

	if (hnd->imapper_version >= 400)
	{
		MALI_GRALLOC_LOGE("Legacy attribute region not supported on Gralloc v4+");
		goto out;
	}

	if (hnd->attr_base != MAP_FAILED)
	{
		attr_region *region = (attr_region *)hnd->attr_base;

		switch (attr)
		{
		case GRALLOC_ARM_BUFFER_ATTR_CROP_RECT:
			memcpy(&region->crop_top, val, sizeof(int) * 4);
			rval = 0;
			break;

		case GRALLOC_ARM_BUFFER_ATTR_DATASPACE:
			region->dataspace = *((android_dataspace_t *)val);
			rval = 0;
			break;
		}
	}

out:
	return rval;
}

static inline int gralloc_buffer_attr_read(struct private_handle_t *hnd, buf_attr attr, int *val)
{
	int rval = -1;

	if (!hnd || !val || attr >= GRALLOC_ARM_BUFFER_ATTR_LAST)
	{
		goto out;
	}

	if (hnd->imapper_version >= 400)
	{
		MALI_GRALLOC_LOGE("Legacy attribute region not supported on Gralloc v4+");
		goto out;
	}

	if (hnd->attr_base != MAP_FAILED)
	{
		attr_region *region = (attr_region *)hnd->attr_base;

		switch (attr)
		{
		case GRALLOC_ARM_BUFFER_ATTR_CROP_RECT:
			memcpy(val, &region->crop_top, sizeof(int) * 4);
			rval = 0;
			break;

		case GRALLOC_ARM_BUFFER_ATTR_DATASPACE:
			*val = region->dataspace;
			rval = 0;
			break;
		}
	}
out:
	return rval;
}

#endif /* GRALLOC_BUFFER_PRIV_H_ */
