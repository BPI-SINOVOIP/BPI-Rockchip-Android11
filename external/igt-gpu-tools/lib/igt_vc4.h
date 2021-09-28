/*
 * Copyright © 2016 Broadcom
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

#ifndef IGT_VC4_H
#define IGT_VC4_H

uint32_t igt_vc4_get_cleared_bo(int fd, size_t size, uint32_t clearval);
int igt_vc4_create_bo(int fd, size_t size);
void *igt_vc4_mmap_bo(int fd, uint32_t handle, uint32_t size, unsigned prot);
int igt_vc4_get_param(int fd, uint32_t param, uint64_t *val);
bool igt_vc4_purgeable_bo(int fd, int handle, bool purgeable);
bool igt_vc4_is_tiled(uint64_t modifier);

void igt_vc4_set_tiling(int fd, uint32_t handle, uint64_t modifier);
uint64_t igt_vc4_get_tiling(int fd, uint32_t handle);

void vc4_fb_convert_plane_to_tiled(struct igt_fb *dst, void *dst_buf,
				     struct igt_fb *src, void *src_buf);
void vc4_fb_convert_plane_from_tiled(struct igt_fb *dst, void *dst_buf,
				       struct igt_fb *src, void *src_buf);

#endif /* IGT_VC4_H */
