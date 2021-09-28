/*
 * Copyright © 2014-2015 Intel Corporation
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
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Daniel Stone <daniels@collabora.com>
 */

#include "igt.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

IGT_TEST_DESCRIPTION("Tests behaviour of mass-data 'blob' properties.");

static const struct drm_mode_modeinfo test_mode_valid = {
	.clock = 1234,
	.hdisplay = 640,
	.hsync_start = 641,
	.hsync_end = 642,
	.htotal = 643,
	.hskew = 0,
	.vdisplay = 480,
	.vsync_start = 481,
	.vsync_end = 482,
	.vtotal = 483,
	.vscan = 0,
	.vrefresh = 60000,
	.flags = 0,
	.type = 0,
	.name = "FROMUSER",
};


#define ioctl_or_ret_errno(fd, ioc, ioc_data) { \
	if (drmIoctl(fd, ioc, ioc_data) != 0) \
		return errno; \
}

static void igt_require_propblob(int fd)
{
	struct drm_mode_create_blob c;
	struct drm_mode_destroy_blob d;
	uint32_t blob_data;
	c.data = (uintptr_t) &blob_data;
	c.length = sizeof(blob_data);

	igt_require(drmIoctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &c) == 0);
	d.blob_id = c.blob_id;
	igt_require(drmIoctl(fd, DRM_IOCTL_MODE_DESTROYPROPBLOB, &d) == 0);
}

static int
validate_prop(int fd, uint32_t prop_id)
{
	struct drm_mode_get_blob get;
	struct drm_mode_modeinfo ret_mode;

	get.blob_id = prop_id;
	get.length = 0;
	get.data = (uintptr_t) 0;
	ioctl_or_ret_errno(fd, DRM_IOCTL_MODE_GETPROPBLOB, &get);

	if (get.length != sizeof(test_mode_valid))
		return ENOMEM;

	get.data = (uintptr_t) &ret_mode;
	ioctl_or_ret_errno(fd, DRM_IOCTL_MODE_GETPROPBLOB, &get);

	if (memcmp(&ret_mode, &test_mode_valid, sizeof(test_mode_valid)) != 0)
		return EINVAL;

	return 0;
}

static uint32_t
create_prop(int fd)
{
	struct drm_mode_create_blob create;

	create.length = sizeof(test_mode_valid);
	create.data = (uintptr_t) &test_mode_valid;

	do_ioctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &create);
	igt_assert_neq_u32(create.blob_id, 0);

	return create.blob_id;
}

static int
destroy_prop(int fd, uint32_t prop_id)
{
	struct drm_mode_destroy_blob destroy;

	destroy.blob_id = prop_id;
	ioctl_or_ret_errno(fd, DRM_IOCTL_MODE_DESTROYPROPBLOB, &destroy);

	return 0;
}

static void
test_validate(int fd)
{
	struct drm_mode_create_blob create;
	struct drm_mode_get_blob get;
	char too_small[32];
	uint32_t prop_id;

	memset(too_small, 0, sizeof(too_small));

	/* Outlandish size. */
	create.length = 0x10000;
	create.data = (uintptr_t) &too_small;
	do_ioctl_err(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &create, EFAULT);

	/* Outlandish address. */
	create.length = sizeof(test_mode_valid);
	create.data = (uintptr_t) 0xdeadbeee;
	do_ioctl_err(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &create, EFAULT);

	/* When we pass an incorrect size, the kernel should correct us. */
	prop_id = create_prop(fd);
	get.blob_id = prop_id;
	get.length = sizeof(too_small);
	get.data = (uintptr_t) too_small;
	do_ioctl(fd, DRM_IOCTL_MODE_GETPROPBLOB, &get);
	igt_assert_eq_u32(get.length, sizeof(test_mode_valid));

	get.blob_id = prop_id;
	get.data = (uintptr_t) 0xdeadbeee;
	do_ioctl_err(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &create, EFAULT);
}

static void
test_lifetime(int fd)
{
	int fd2;
	uint32_t prop_id, prop_id2;

	fd2 = drm_open_driver(DRIVER_ANY);
	igt_assert_fd(fd2);

	/* Ensure clients can see properties created by other clients. */
	prop_id = create_prop(fd);
	igt_assert_eq(validate_prop(fd, prop_id), 0);
	igt_assert_eq(validate_prop(fd2, prop_id), 0);

	/* ... but can't destroy them. */
	igt_assert_eq(destroy_prop(fd2, prop_id), EPERM);

	/* Make sure properties can't be accessed once explicitly destroyed. */
	prop_id2 = create_prop(fd2);
	igt_assert_eq(validate_prop(fd2, prop_id2), 0);
	igt_assert_eq(destroy_prop(fd2, prop_id2), 0);
	igt_assert_eq(validate_prop(fd2, prop_id2), ENOENT);
	igt_assert_eq(validate_prop(fd, prop_id2), ENOENT);

	/* Make sure properties are cleaned up on client exit. */
	prop_id2 = create_prop(fd2);
	igt_assert_eq(validate_prop(fd, prop_id2), 0);
	igt_assert_eq(close(fd2), 0);
	igt_assert_eq(validate_prop(fd, prop_id2), ENOENT);

	igt_assert_eq(validate_prop(fd, prop_id), 0);
	igt_assert_eq(destroy_prop(fd, prop_id), 0);
	igt_assert_eq(validate_prop(fd, prop_id), ENOENT);
}

static void
test_multiple(int fd)
{
	uint32_t prop_ids[5];
	int fd2;
	int i;

	fd2 = drm_open_driver(DRIVER_ANY);
	igt_assert_fd(fd2);

	/* Ensure destroying multiple properties explicitly works as needed. */
	for (i = 0; i < ARRAY_SIZE(prop_ids); i++) {
		prop_ids[i] = create_prop(fd2);
		igt_assert_eq(validate_prop(fd, prop_ids[i]), 0);
		igt_assert_eq(validate_prop(fd2, prop_ids[i]), 0);
	}
	for (i = 0; i < ARRAY_SIZE(prop_ids); i++) {
		igt_assert_eq(destroy_prop(fd2, prop_ids[i]), 0);
		igt_assert_eq(validate_prop(fd2, prop_ids[i]), ENOENT);
	}
	igt_assert_eq(close(fd2), 0);

	fd2 = drm_open_driver(DRIVER_ANY);
	igt_assert_fd(fd2);

	/* Ensure that multiple properties get cleaned up on fd close. */
	for (i = 0; i < ARRAY_SIZE(prop_ids); i++) {
		prop_ids[i] = create_prop(fd2);
		igt_assert_eq(validate_prop(fd, prop_ids[i]), 0);
		igt_assert_eq(validate_prop(fd2, prop_ids[i]), 0);
	}
	igt_assert_eq(close(fd2), 0);

	for (i = 0; i < ARRAY_SIZE(prop_ids); i++)
		igt_assert_eq(validate_prop(fd, prop_ids[i]), ENOENT);
}

static void
test_core(int fd)
{
	uint32_t prop_id;

	/* The first hurdle. */
	prop_id = create_prop(fd);
	igt_assert_eq(validate_prop(fd, prop_id), 0);
	igt_assert_eq(destroy_prop(fd, prop_id), 0);

	/* Look up some invalid property IDs. They should fail. */
	igt_assert_eq(validate_prop(fd, 0xffffffff), ENOENT);
	igt_assert_eq(validate_prop(fd, 0), ENOENT);
}

static void
test_basic(int fd)
{
	uint32_t prop_id;

	/* A very simple gating test to ensure property support exists. */
	prop_id = create_prop(fd);
	igt_assert_eq(destroy_prop(fd, prop_id), 0);
}

static void prop_tests(int fd)
{
	struct drm_mode_obj_get_properties get_props = {};
	struct drm_mode_obj_set_property set_prop = {};
	uint64_t prop, prop_val, blob_id;

	igt_fixture
		blob_id = create_prop(fd);

	get_props.props_ptr = (uintptr_t) &prop;
	get_props.prop_values_ptr = (uintptr_t) &prop_val;
	get_props.count_props = 1;
	get_props.obj_id = blob_id;

	igt_subtest("invalid-get-prop-any") {
		get_props.obj_type = 0; /* DRM_MODE_OBJECT_ANY */

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES,
				    &get_props) == -1 && errno == EINVAL);
	}

	igt_subtest("invalid-get-prop") {
		get_props.obj_type = DRM_MODE_OBJECT_BLOB;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES,
				    &get_props) == -1 && errno == EINVAL);
	}

	set_prop.value = 0;
	set_prop.prop_id = 1;
	set_prop.obj_id = blob_id;

	igt_subtest("invalid-set-prop-any") {
		set_prop.obj_type = 0; /* DRM_MODE_OBJECT_ANY */

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_SETPROPERTY,
				    &set_prop) == -1 && errno == EINVAL);
	}

	igt_subtest("invalid-set-prop") {
		set_prop.obj_type = DRM_MODE_OBJECT_BLOB;

		igt_assert(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_SETPROPERTY,
				    &set_prop) == -1 && errno == EINVAL);
	}

	igt_fixture
		destroy_prop(fd, blob_id);
}

igt_main
{
	int fd;

	igt_fixture {
		fd = drm_open_driver(DRIVER_ANY);
		igt_require(fd >= 0);
		igt_require_propblob(fd);
	}

	igt_subtest("basic")
		test_basic(fd);

	igt_subtest("blob-prop-core")
		test_core(fd);

	igt_subtest("blob-prop-validate")
		test_validate(fd);

	igt_subtest("blob-prop-lifetime")
		test_lifetime(fd);

	igt_subtest("blob-multiple")
		test_multiple(fd);

	prop_tests(fd);

	igt_fixture
		close(fd);
}
