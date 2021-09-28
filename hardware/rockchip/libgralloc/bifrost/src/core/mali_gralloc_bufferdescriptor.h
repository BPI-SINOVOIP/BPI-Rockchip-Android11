/*
 * Copyright (C) 2016-2020 Arm Limited. All rights reserved.
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

#ifndef MALI_GRALLOC_BUFFERDESCRIPTOR_H_
#define MALI_GRALLOC_BUFFERDESCRIPTOR_H_

#include "mali_gralloc_buffer.h"
#include "mali_gralloc_formats.h"
#include <string>

typedef uint64_t gralloc_buffer_descriptor_t;

/* A buffer_descriptor contains the requested parameters for the buffer
 * as well as the calculated parameters that are passed to the allocator.
 */
struct buffer_descriptor_t
{
	/* For validation. */
	uint32_t signature;

	/* Requested parameters from IAllocator. */
	uint32_t width;
	uint32_t height;
	uint64_t producer_usage;
	uint64_t consumer_usage;
	uint64_t hal_format;
	uint32_t layer_count;
	mali_gralloc_format_type format_type;
	std::string name;
	uint64_t reserved_size;

	/*
	 * Calculated values that will be passed to the allocator in order to
	 * allocate the buffer.
	 */
	size_t size;
	int pixel_stride;
	uint64_t alloc_format;
	plane_info_t plane_info[MAX_PLANES];

	/* Calculated values only used for GRALLOC_USE_LEGACY_CALCS */
	int old_byte_stride;
	int old_alloc_width;
	int old_alloc_height;
	uint64_t old_internal_format;

	buffer_descriptor_t() :
	    signature(0),
	    width(0),
	    height(0),
	    producer_usage(0),
	    consumer_usage(0),
	    hal_format(0),
	    layer_count(0),
	    format_type(MALI_GRALLOC_FORMAT_TYPE_USAGE),
	    name("Unnamed"),
	    reserved_size(0),
	    size(0),
	    pixel_stride(0),
	    alloc_format(0),
	    old_byte_stride(0),
	    old_alloc_width(0),
	    old_alloc_height(0),
	    old_internal_format(0)
	{
		memset(plane_info, 0, sizeof(plane_info_t) * MAX_PLANES);
	}
};

#endif /* MALI_GRALLOC_BUFFERDESCRIPTOR_H_ */
