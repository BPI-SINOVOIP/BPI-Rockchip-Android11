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

#include "igt.h"
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "vc4_drm.h"
#include "vc4_packet.h"

igt_main
{
	int fd;

	igt_fixture
		fd = drm_open_driver(DRIVER_VC4);

	igt_subtest("bad-color-write") {
		uint32_t size = 4096;
		/* A single row will be a page. */
		uint32_t width = 1024;
		uint32_t height = size / (width * 4);
		uint32_t handle = 0xd0d0d0d0; /* Don't actually make a BO */
		struct drm_vc4_submit_cl submit = {
			.color_write = {
				.hindex = 0,
				.bits = VC4_SET_FIELD(VC4_RENDER_CONFIG_FORMAT_RGBA8888,
						      VC4_RENDER_CONFIG_FORMAT),
			},

			.color_read = { .hindex = ~0 },
			.zs_read = { .hindex = ~0 },
			.zs_write = { .hindex = ~0 },
			.msaa_color_write = { .hindex = ~0 },
			.msaa_zs_write = { .hindex = ~0 },

			.bo_handles = (uint64_t)(uintptr_t)&handle,
			.bo_handle_count = 1,
			.width = width,
			.height = height,
			.max_x_tile = ALIGN(width, 64) / 64 - 1,
			.max_y_tile = ALIGN(height, 64) / 64 - 1,
			.clear_color = { 0xcccccccc, 0xcccccccc },
			.flags = VC4_SUBMIT_CL_USE_CLEAR_COLOR,
		};

		igt_assert_eq_u32(width * height * 4, size);

		do_ioctl_err(fd, DRM_IOCTL_VC4_SUBMIT_CL, &submit, EINVAL);
	}

	igt_fixture
		close(fd);
}
