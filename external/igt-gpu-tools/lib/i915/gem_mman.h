/*
 * Copyright © 2007, 2011, 2013, 2014, 2019 Intel Corporation
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
 *
 */

#ifndef GEM_MMAN_H
#define GEM_MMAN_H

void *gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot);
void *gem_mmap__cpu(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);

bool gem_mmap__has_wc(int fd);
void *gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);

#ifndef I915_GEM_DOMAIN_WC
#define I915_GEM_DOMAIN_WC 0x80
#endif

void *__gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot);
void *__gem_mmap__cpu(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);
void *__gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);

int gem_munmap(void *ptr, uint64_t size);

/**
 * gem_require_mmap_wc:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether direct (i.e. cpu access path, bypassing
 * the gtt) write-combine memory mappings are available. Automatically skips
 * through igt_require() if not.
 */
#define gem_require_mmap_wc(fd) igt_require(gem_mmap__has_wc(fd))

#endif /* GEM_MMAN_H */

