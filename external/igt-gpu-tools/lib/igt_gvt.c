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

#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include "igt_gvt.h"
#include "igt_sysfs.h"
#include "igt_kmod.h"
#include "drmtest.h"

/**
 * SECTION:igt_gvt
 * @short_description: Graphics virtualization technology library
 * @title: GVT
 * @include: igt_gvt.h
 */

static bool is_gvt_enabled(void)
{
	bool enabled = false;
	int dir, fd;

	fd = __drm_open_driver(DRIVER_INTEL);
	dir = igt_sysfs_open_parameters(fd);
	if (dir < 0)
		return false;

	enabled = igt_sysfs_get_boolean(dir, "enable_gvt");

	close(dir);
	close(fd);

	return enabled;

}

bool igt_gvt_load_module(void)
{
	if (is_gvt_enabled())
		return true;

	if (igt_i915_driver_unload())
		return false;

	if (igt_i915_driver_load("enable_gvt=1"))
		return false;

	return is_gvt_enabled();
}

void igt_gvt_unload_module(void)
{
	if (!is_gvt_enabled())
		return;

	igt_i915_driver_unload();

	igt_i915_driver_load(NULL);

	igt_assert(!is_gvt_enabled());
}
