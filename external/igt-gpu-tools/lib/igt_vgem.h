/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef IGT_VGEM_H
#define IGT_VGEM_H

#include <stdint.h>
#include <stdbool.h>

struct vgem_bo {
	uint32_t handle;
	uint32_t width, height;
	uint32_t bpp, pitch;
	uint64_t size;
};

int __vgem_create(int fd, struct vgem_bo *bo);
void vgem_create(int fd, struct vgem_bo *bo);

void *__vgem_mmap(int fd, struct vgem_bo *bo, unsigned prot);
void *vgem_mmap(int fd, struct vgem_bo *bo, unsigned prot);

bool vgem_has_fences(int fd);
bool vgem_fence_has_flag(int fd, unsigned flags);
uint32_t vgem_fence_attach(int fd, struct vgem_bo *bo, unsigned flags);
#define VGEM_FENCE_WRITE 0x1
#define WIP_VGEM_FENCE_NOTIMEOUT 0x2
int __vgem_fence_signal(int fd, uint32_t fence);
void vgem_fence_signal(int fd, uint32_t fence);

#endif /* IGT_VGEM_H */
