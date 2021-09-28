/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt_amd.h"
#include "igt.h"
#include <amdgpu_drm.h>

uint32_t igt_amd_create_bo(int fd, uint64_t size)
{
	union drm_amdgpu_gem_create create;

	memset(&create, 0, sizeof(create));
	create.in.bo_size = size;
	create.in.alignment = 256;
	create.in.domains = AMDGPU_GEM_DOMAIN_VRAM;
	create.in.domain_flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED
				 | AMDGPU_GEM_CREATE_VRAM_CLEARED;

	do_ioctl(fd, DRM_IOCTL_AMDGPU_GEM_CREATE, &create);
	igt_assert(create.out.handle);

	return create.out.handle;
}

void *igt_amd_mmap_bo(int fd, uint32_t handle, uint64_t size, int prot)
{
	union drm_amdgpu_gem_mmap map;
	void *ptr;

	memset(&map, 0, sizeof(map));
	map.in.handle = handle;

	do_ioctl(fd, DRM_IOCTL_AMDGPU_GEM_MMAP, &map);

	ptr = mmap(0, size, prot, MAP_SHARED, fd, map.out.addr_ptr);
	return ptr == MAP_FAILED ? NULL : ptr;
}
