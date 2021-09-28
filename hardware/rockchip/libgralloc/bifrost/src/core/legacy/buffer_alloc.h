/*
 * Copyright (C) 2018-2019 ARM Limited. All rights reserved.
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

#ifndef BUFFER_ALLOC_H_
#define BUFFER_ALLOC_H_

namespace legacy
{
	enum AllocBaseType
	{
		UNCOMPRESSED,
		AFBC,
		/* AN AFBC buffer with additional padding to ensure a 64-byte alignment
		 * for each row of blocks in the header */
		AFBC_PADDED,
		AFBC_WIDEBLK,
		AFBC_EXTRAWIDEBLK,
	};


	typedef struct
	{
		AllocBaseType primary_type;
		/* If is_multi_plane is true then chroma plane is implicitly extra wide block for AFBC. */
		bool is_multi_plane;
		bool is_tiled;

		bool is_afbc() const
		{
			return primary_type != UNCOMPRESSED;
		}
	} alloc_type_t;


	int get_alloc_size(uint64_t internal_format,
	                   uint64_t usage,
	                   alloc_type_t alloc_type,
	                   int old_alloc_width,
	                   int old_alloc_height,
	                   int * old_byte_stride,
	                   int * pixel_stride,
	                   size_t * size);

	void mali_gralloc_adjust_dimensions(const uint64_t internal_format,
	                                    const uint64_t usage,
	                                    const alloc_type_t type,
	                                    const uint32_t width,
	                                    const uint32_t height,
	                                    int * const internal_width,
	                                    int * const internal_height);


	void get_afbc_alignment(const int width, const int height, const alloc_type_t type,
	                        int * const w_aligned, int * const h_aligned);
}

#endif
