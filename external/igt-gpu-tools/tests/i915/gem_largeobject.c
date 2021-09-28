/*
 * Copyright © 2008 Intel Corporation
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
 * Authors:
 *    Jesse Barnes <jbarnes@virtuousgeek.org>
 *
 */

#include "igt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "drm.h"

/* Should take 64 pages to store the page pointers on 64 bit */
#define OBJ_SIZE (128 * 1024 * 1024)

unsigned char *data;

static void
test_large_object(int fd)
{
	struct drm_i915_gem_create create;
	struct drm_i915_gem_pin pin;
	uint32_t obj_size;
	char *ptr;

	memset(&create, 0, sizeof(create));
	memset(&pin, 0, sizeof(pin));

	if (gem_aperture_size(fd)*3/4 < OBJ_SIZE/2)
		obj_size = OBJ_SIZE / 4;
	else if (gem_aperture_size(fd)*3/4 < OBJ_SIZE)
		obj_size = OBJ_SIZE / 2;
	else
		obj_size = OBJ_SIZE;
	create.size = obj_size;
	igt_info("obj size %i\n", obj_size);

	igt_assert(ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) == 0);

	/* prefault */
	ptr = gem_mmap__gtt(fd, create.handle, obj_size,
			    PROT_WRITE | PROT_READ);
	*ptr = 0;

	gem_write(fd, create.handle, 0, data, obj_size);

	/* kernel should clean this up for us */
}

igt_simple_main
{
	int fd;

	igt_skip_on_simulation();

	data = malloc(OBJ_SIZE);
	igt_assert(data);

	fd = drm_open_driver(DRIVER_INTEL);

	test_large_object(fd);

	free(data);
}
