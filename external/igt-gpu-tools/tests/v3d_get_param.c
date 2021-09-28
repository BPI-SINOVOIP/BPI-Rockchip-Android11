/*
 * Copyright © 2017 Broadcom
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

#include "igt.h"
#include "igt_v3d.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "v3d_drm.h"
#include "igt_v3d.h"

igt_main
{
	int fd;

	igt_fixture
		fd = drm_open_driver(DRIVER_V3D);

	igt_subtest("base-params") {
		enum drm_v3d_param last_base_param =
			DRM_V3D_PARAM_V3D_CORE0_IDENT2;

		uint32_t results[last_base_param];

		for (int i = 0; i < ARRAY_SIZE(results); i++)
			results[i] = igt_v3d_get_param(fd, i);

		/* HUB TVER field */
		igt_assert_lte(3, results[DRM_V3D_PARAM_V3D_HUB_IDENT1] & 0xf);

		/* CORE's ident, has VER field but also an ID. */
		igt_assert_eq(results[DRM_V3D_PARAM_V3D_CORE0_IDENT0] & 0xffffff,
			      0x443356 /* "V3D" */);
	}

	igt_subtest("get-bad-param") {
		struct drm_v3d_get_param get = {
			.param = 0xd0d0d0d0,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_GET_PARAM, &get, EINVAL);
	}

	igt_subtest("get-bad-flags") {
		struct drm_v3d_get_param get = {
			.param = DRM_V3D_PARAM_V3D_HUB_IDENT1,
			.pad = 1,
		};
		do_ioctl_err(fd, DRM_IOCTL_V3D_GET_PARAM, &get, EINVAL);
	}

	igt_fixture
		close(fd);
}
