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

#include "igt.h"
#include "drmtest.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

struct additional_test {
	const char *name;
	uint32_t obj_type;
	void (*prop_test)(int fd, uint32_t id, uint32_t type, drmModePropertyPtr prop,
			  uint32_t prop_id, uint64_t prop_value, bool atomic);
};

static void prepare_pipe(igt_display_t *display, enum pipe pipe, igt_output_t *output, struct igt_fb *fb)
{
	drmModeModeInfo *mode = igt_output_get_mode(output);

	igt_create_pattern_fb(display->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE, fb);

	igt_output_set_pipe(output, pipe);

	igt_plane_set_fb(igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY), fb);

	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);
}

static void cleanup_pipe(igt_display_t *display, enum pipe pipe, igt_output_t *output, struct igt_fb *fb)
{
	igt_plane_t *plane;

	for_each_plane_on_pipe(display, pipe, plane)
		igt_plane_set_fb(plane, NULL);

	igt_output_set_pipe(output, PIPE_NONE);

	igt_display_commit2(display, display->is_atomic ? COMMIT_ATOMIC : COMMIT_LEGACY);

	igt_remove_fb(display->drm_fd, fb);
}

static bool ignore_property(uint32_t obj_type, uint32_t prop_flags,
			    const char *name, bool atomic)
{
	if (prop_flags & DRM_MODE_PROP_IMMUTABLE)
		return true;

	switch (obj_type) {
	case DRM_MODE_OBJECT_CONNECTOR:
		if (atomic && !strcmp(name, "DPMS"))
			return true;
		break;
	default:
		break;
	}

	return false;
}

static void max_bpc_prop_test(int fd, uint32_t id, uint32_t type, drmModePropertyPtr prop,
			      uint32_t prop_id, uint64_t prop_value, bool atomic)
{
	drmModeAtomicReqPtr req = NULL;
	int i, ret;

	if (atomic)
		req = drmModeAtomicAlloc();

	for (i = prop->values[0]; i <= prop->values[1]; i++) {
		if (!atomic) {
			ret = drmModeObjectSetProperty(fd, id, type, prop_id, i);

			igt_assert_eq(ret, 0);
		} else {
			ret = drmModeAtomicAddProperty(req, id, prop_id, i);
			igt_assert(ret >= 0);

			ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
			igt_assert_eq(ret, 0);
		}
	}

	if (atomic)
		drmModeAtomicFree(req);
}

static const struct additional_test property_functional_test[] = {
									{"max bpc", DRM_MODE_OBJECT_CONNECTOR,
									 max_bpc_prop_test},
								 };

static bool has_additional_test_lookup(uint32_t obj_type, const char *name,
				bool atomic, int *index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(property_functional_test); i++)
		if (property_functional_test[i].obj_type == obj_type &&
		    !strcmp(name, property_functional_test[i].name)) {
			*index = i;
			return true;
		}

	return false;
}
static void test_properties(int fd, uint32_t type, uint32_t id, bool atomic)
{
	drmModeObjectPropertiesPtr props =
		drmModeObjectGetProperties(fd, id, type);
	int i, j, ret;
	drmModeAtomicReqPtr req = NULL;

	igt_assert(props);

	if (atomic)
		req = drmModeAtomicAlloc();

	for (i = 0; i < props->count_props; i++) {
		uint32_t prop_id = props->props[i];
		uint64_t prop_value = props->prop_values[i];
		drmModePropertyPtr prop = drmModeGetProperty(fd, prop_id);

		igt_assert(prop);

		if (ignore_property(type, prop->flags, prop->name, atomic)) {
			igt_debug("Ignoring property \"%s\"\n", prop->name);

			continue;
		}

		igt_debug("Testing property \"%s\"\n", prop->name);

		if (!atomic) {
			ret = drmModeObjectSetProperty(fd, id, type, prop_id, prop_value);

			igt_assert_eq(ret, 0);
		} else {
			ret = drmModeAtomicAddProperty(req, id, prop_id, prop_value);
			igt_assert(ret >= 0);

			ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_TEST_ONLY, NULL);
			igt_assert_eq(ret, 0);
		}

		if (has_additional_test_lookup(type, prop->name, atomic, &j))
			property_functional_test[j].prop_test(fd, id, type, prop, prop_id, prop_value, atomic);

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);

	if (atomic) {
		ret = drmModeAtomicCommit(fd, req, 0, NULL);
		igt_assert_eq(ret, 0);

		drmModeAtomicFree(req);
	}
}

static void run_plane_property_tests(igt_display_t *display, enum pipe pipe, igt_output_t *output, bool atomic)
{
	struct igt_fb fb;
	igt_plane_t *plane;

	prepare_pipe(display, pipe, output, &fb);

	for_each_plane_on_pipe(display, pipe, plane) {
		igt_info("Testing plane properties on %s.#%d-%s (output: %s)\n",
			 kmstest_pipe_name(pipe), plane->index, kmstest_plane_type_name(plane->type), output->name);

		test_properties(display->drm_fd, DRM_MODE_OBJECT_PLANE, plane->drm_plane->plane_id, atomic);
	}

	cleanup_pipe(display, pipe, output, &fb);
}

static void run_crtc_property_tests(igt_display_t *display, enum pipe pipe, igt_output_t *output, bool atomic)
{
	struct igt_fb fb;

	prepare_pipe(display, pipe, output, &fb);

	igt_info("Testing crtc properties on %s (output: %s)\n", kmstest_pipe_name(pipe), output->name);

	test_properties(display->drm_fd, DRM_MODE_OBJECT_CRTC, display->pipes[pipe].crtc_id, atomic);

	cleanup_pipe(display, pipe, output, &fb);
}

static void run_connector_property_tests(igt_display_t *display, enum pipe pipe, igt_output_t *output, bool atomic)
{
	struct igt_fb fb;

	if (pipe != PIPE_NONE)
		prepare_pipe(display, pipe, output, &fb);

	igt_info("Testing connector properties on output %s (pipe: %s)\n", output->name, kmstest_pipe_name(pipe));

	test_properties(display->drm_fd, DRM_MODE_OBJECT_CONNECTOR, output->id, atomic);

	if (pipe != PIPE_NONE)
		cleanup_pipe(display, pipe, output, &fb);
}

static void plane_properties(igt_display_t *display, bool atomic)
{
	bool found_any = false, found;
	igt_output_t *output;
	enum pipe pipe;

	if (atomic)
		igt_skip_on(!display->is_atomic);

	for_each_pipe(display, pipe) {
		found = false;

		for_each_valid_output_on_pipe(display, pipe, output) {
			found_any = found = true;

			run_plane_property_tests(display, pipe, output, atomic);
			break;
		}
	}

	igt_skip_on(!found_any);
}

static void crtc_properties(igt_display_t *display, bool atomic)
{
	bool found_any_valid_pipe = false, found;
	enum pipe pipe;
	igt_output_t *output;

	if (atomic)
		igt_skip_on(!display->is_atomic);

	for_each_pipe(display, pipe) {
		found = false;

		for_each_valid_output_on_pipe(display, pipe, output) {
			found_any_valid_pipe = found = true;

			run_crtc_property_tests(display, pipe, output, atomic);
			break;
		}
	}

	igt_skip_on(!found_any_valid_pipe);
}

static void connector_properties(igt_display_t *display, bool atomic)
{
	int i;
	enum pipe pipe;
	igt_output_t *output;

	if (atomic)
		igt_skip_on(!display->is_atomic);

	for_each_connected_output(display, output) {
		bool found = false;

		for_each_pipe(display, pipe) {
			if (!igt_pipe_connector_valid(pipe, output))
				continue;

			found = true;
			run_connector_property_tests(display, pipe, output, atomic);
			break;
		}

		igt_assert_f(found, "Connected output should have at least 1 valid crtc\n");
	}

	for (i = 0; i < display->n_outputs; i++)
		if (!igt_output_is_connected(&display->outputs[i]))
			run_connector_property_tests(display, PIPE_NONE, &display->outputs[i], atomic);
}

static void test_invalid_properties(int fd,
				    uint32_t id1,
				    uint32_t type1,
				    uint32_t id2,
				    uint32_t type2,
				    bool atomic)
{
	drmModeObjectPropertiesPtr props1 =
		drmModeObjectGetProperties(fd, id1, type1);
	drmModeObjectPropertiesPtr props2 =
		drmModeObjectGetProperties(fd, id2, type2);

	int i, j, ret;
	drmModeAtomicReqPtr req;

	igt_assert(props1 && props2);

	for (i = 0; i < props2->count_props; i++) {
		uint32_t prop_id = props2->props[i];
		uint64_t prop_value = props2->prop_values[i];
		drmModePropertyPtr prop = drmModeGetProperty(fd, prop_id);
		bool found = false;

		igt_assert(prop);

		for (j = 0; j < props1->count_props; j++)
			if (props1->props[j] == prop_id) {
				found = true;
				break;
			}

		if (found)
			continue;

		igt_debug("Testing property \"%s\" on [%x:%u]\n", prop->name, type1, id1);

		if (!atomic) {
			ret = drmModeObjectSetProperty(fd, id1, type1, prop_id, prop_value);

			igt_assert_eq(ret, -EINVAL);
		} else {
			req = drmModeAtomicAlloc();
			igt_assert(req);

			ret = drmModeAtomicAddProperty(req, id1, prop_id, prop_value);
			igt_assert(ret >= 0);

			ret = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
			igt_assert_eq(ret, -ENOENT);

			drmModeAtomicFree(req);
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props1);
	drmModeFreeObjectProperties(props2);
}
static void test_object_invalid_properties(igt_display_t *display,
					   uint32_t id, uint32_t type, bool atomic)
{
	igt_output_t *output;
	igt_plane_t *plane;
	enum pipe pipe;
	int i;

	for_each_pipe(display, pipe)
		test_invalid_properties(display->drm_fd, id, type, display->pipes[pipe].crtc_id, DRM_MODE_OBJECT_CRTC, atomic);

	for_each_pipe(display, pipe)
		for_each_plane_on_pipe(display, pipe, plane)
			test_invalid_properties(display->drm_fd, id, type, plane->drm_plane->plane_id, DRM_MODE_OBJECT_PLANE, atomic);

	for (i = 0, output = &display->outputs[0]; i < display->n_outputs; output = &display->outputs[++i])
		test_invalid_properties(display->drm_fd, id, type, output->id, DRM_MODE_OBJECT_CONNECTOR, atomic);
}

static void validate_range_prop(const struct drm_mode_get_property *prop,
				uint64_t value)
{
	const uint64_t *values = from_user_pointer(prop->values_ptr);
	bool is_unsigned = prop->flags & DRM_MODE_PROP_RANGE;
	bool immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;

	igt_assert_eq(prop->count_values, 2);
	igt_assert_eq(prop->count_enum_blobs, 0);
	igt_assert(values[0] != values[1] || immutable);

	if (is_unsigned) {
		igt_assert_lte_u64(values[0], values[1]);
		igt_assert_lte_u64(values[0], value);
		igt_assert_lte_u64(value, values[1]);
	} else {
		igt_assert_lte_s64(values[0], values[1]);
		igt_assert_lte_s64(values[0], value);
		igt_assert_lte_s64(value, values[1]);
	}

}

static void validate_enums(const struct drm_mode_get_property *prop)
{
	const uint64_t *values = from_user_pointer(prop->values_ptr);
	const struct drm_mode_property_enum *enums =
		from_user_pointer(prop->enum_blob_ptr);

	for (int i = 0; i < prop->count_enum_blobs; i++) {
		int name_len = strnlen(enums[i].name,
				       sizeof(enums[i].name));

		igt_assert_lte(1, name_len);
		igt_assert_lte(name_len, sizeof(enums[i].name) - 1);

		/* no idea why we have this duplicated */
		igt_assert_eq_u64(values[i], enums[i].value);
	}
}

static void validate_enum_prop(const struct drm_mode_get_property *prop,
			       uint64_t value)
{
	const uint64_t *values = from_user_pointer(prop->values_ptr);
	bool immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
	int i;

	igt_assert_lte(1, prop->count_values);
	igt_assert_eq(prop->count_enum_blobs, prop->count_values);
	igt_assert(prop->count_values != 1 || immutable);

	for (i = 0; i < prop->count_values; i++) {
		if (value == values[i])
			break;
	}
	igt_assert(i != prop->count_values);

	validate_enums(prop);
}

static void validate_bitmask_prop(const struct drm_mode_get_property *prop,
				  uint64_t value)
{
	const uint64_t *values = from_user_pointer(prop->values_ptr);
	bool immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
	uint64_t mask = 0;

	igt_assert_lte(1, prop->count_values);
	igt_assert_eq(prop->count_enum_blobs, prop->count_values);
	igt_assert(prop->count_values != 1 || immutable);

	for (int i = 0; i < prop->count_values; i++) {
		igt_assert_lte_u64(values[i], 63);
		mask |= 1ULL << values[i];
	}

	igt_assert_eq_u64(value & ~mask, 0);
	igt_assert_neq_u64(value & mask, 0);

	validate_enums(prop);
}

static void validate_blob_prop(int fd,
			       const struct drm_mode_get_property *prop,
			       uint64_t value)
{
	struct drm_mode_get_blob blob;

	/*
	 * Despite what libdrm makes you believe, we never supply
	 * additional information for BLOB properties, only for enums
	 * and bitmasks
	 */
	igt_assert_eq(prop->count_values, 0);
	igt_assert_eq(prop->count_enum_blobs, 0);

	igt_assert_lte_u64(value, 0xffffffff);

	/*
	 * Immutable blob properties can have value==0.
	 * Happens for example with the "EDID" property
	 * when there is nothing hooked up to the connector.
	 */

	if (!value)
		return;

	memset(&blob, 0, sizeof(blob));
	blob.blob_id = value;

	do_ioctl(fd, DRM_IOCTL_MODE_GETPROPBLOB, &blob);
}

static void validate_object_prop(int fd,
				 const struct drm_mode_get_property *prop,
				 uint64_t value)
{
	const uint64_t *values = from_user_pointer(prop->values_ptr);
	bool immutable = prop->flags & DRM_MODE_PROP_IMMUTABLE;
	struct drm_mode_crtc crtc;
	struct drm_mode_fb_cmd fb;

	igt_assert_eq(prop->count_values, 1);
	igt_assert_eq(prop->count_enum_blobs, 0);

	igt_assert_lte_u64(value, 0xffffffff);
	igt_assert(!immutable || value != 0);

	switch (values[0]) {
	case DRM_MODE_OBJECT_CRTC:
		if (!value)
			break;
		memset(&crtc, 0, sizeof(crtc));
		crtc.crtc_id = value;
		do_ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc);
		break;
	case DRM_MODE_OBJECT_FB:
		if (!value)
			break;
		memset(&fb, 0, sizeof(fb));
		fb.fb_id = value;
		do_ioctl(fd, DRM_IOCTL_MODE_GETFB, &fb);
		break;
	default:
		/* These are the only types we have so far */
		igt_assert(0);
	}
}

static void validate_property(int fd,
			      const struct drm_mode_get_property *prop,
			      uint64_t value, bool atomic)
{
	uint32_t flags = prop->flags;
	uint32_t legacy_type = flags & DRM_MODE_PROP_LEGACY_TYPE;
	uint32_t ext_type = flags & DRM_MODE_PROP_EXTENDED_TYPE;

	igt_assert_eq((flags & ~(DRM_MODE_PROP_LEGACY_TYPE |
				 DRM_MODE_PROP_EXTENDED_TYPE |
				 DRM_MODE_PROP_IMMUTABLE |
				 DRM_MODE_PROP_ATOMIC)), 0);

	igt_assert(atomic ||
		   (flags & DRM_MODE_PROP_ATOMIC) == 0);

	igt_assert_neq(!legacy_type, !ext_type);

	igt_assert(legacy_type == 0 ||
		   is_power_of_two(legacy_type));

	switch (legacy_type) {
	case DRM_MODE_PROP_RANGE:
		validate_range_prop(prop, value);
		break;
	case DRM_MODE_PROP_ENUM:
		validate_enum_prop(prop, value);
		break;
	case DRM_MODE_PROP_BITMASK:
		validate_bitmask_prop(prop, value);
		break;
	case DRM_MODE_PROP_BLOB:
		validate_blob_prop(fd, prop, value);
		break;
	default:
		igt_assert_eq(legacy_type, 0);
	}

	switch (ext_type) {
	case DRM_MODE_PROP_OBJECT:
		validate_object_prop(fd, prop, value);
		break;
	case DRM_MODE_PROP_SIGNED_RANGE:
		validate_range_prop(prop, value);
		break;
	default:
		igt_assert_eq(ext_type, 0);
	}
}

static void validate_prop(int fd, uint32_t prop_id, uint64_t value, bool atomic)
{
	struct drm_mode_get_property prop;
	struct drm_mode_property_enum *enums = NULL;
	uint64_t *values = NULL;

	memset(&prop, 0, sizeof(prop));
	prop.prop_id = prop_id;

	do_ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop);

	if (prop.count_values) {
		values = calloc(prop.count_values, sizeof(values[0]));
		igt_assert(values);
		memset(values, 0x5c, sizeof(values[0])*prop.count_values);
		prop.values_ptr = to_user_pointer(values);
	}

	if (prop.count_enum_blobs) {
		enums = calloc(prop.count_enum_blobs, sizeof(enums[0]));
		memset(enums, 0x5c, sizeof(enums[0])*prop.count_enum_blobs);
		igt_assert(enums);
		prop.enum_blob_ptr = to_user_pointer(enums);
	}

	do_ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop);

	for (int i = 0; i < prop.count_values; i++)
		igt_assert_neq_u64(values[i], 0x5c5c5c5c5c5c5c5cULL);

	for (int i = 0; i < prop.count_enum_blobs; i++)
		igt_assert_neq_u64(enums[i].value, 0x5c5c5c5c5c5c5c5cULL);

	validate_property(fd, &prop, value, atomic);

	free(values);
	free(enums);
}

static void validate_props(int fd, uint32_t obj_type, uint32_t obj_id, bool atomic)
{
	struct drm_mode_obj_get_properties properties;
	uint32_t *props = NULL;
	uint64_t *values = NULL;
	uint32_t count;

	memset(&properties, 0, sizeof(properties));
	properties.obj_type = obj_type;
	properties.obj_id = obj_id;

	do_ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &properties);

	count = properties.count_props;

	if (count) {
		props = calloc(count, sizeof(props[0]));
		memset(props, 0x5c, sizeof(props[0])*count);
		igt_assert(props);
		properties.props_ptr = to_user_pointer(props);

		values = calloc(count, sizeof(values[0]));
		memset(values, 0x5c, sizeof(values[0])*count);
		igt_assert(values);
		properties.prop_values_ptr = to_user_pointer(values);
	}

	do_ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &properties);

	igt_assert(properties.count_props == count);

	for (int i = 0; i < count; i++)
		validate_prop(fd, props[i], values[i], atomic);

	free(values);
	free(props);
}

static void expect_no_props(int fd, uint32_t obj_type, uint32_t obj_id)
{
	struct drm_mode_obj_get_properties properties;

	memset(&properties, 0, sizeof(properties));
	properties.obj_type = obj_type;
	properties.obj_id = obj_id;

	igt_assert_neq(drmIoctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &properties), 0);
}

static void get_prop_sanity(igt_display_t *display, bool atomic)
{
	int fd = display->drm_fd;
	drmModePlaneResPtr plane_res;
	drmModeResPtr res;

	res = drmModeGetResources(fd);
	plane_res = drmModeGetPlaneResources(fd);

	for (int i = 0; i < plane_res->count_planes; i++) {
		validate_props(fd, DRM_MODE_OBJECT_PLANE,
			       plane_res->planes[i], atomic);
	}

	for (int i = 0; i < res->count_crtcs; i++) {
		validate_props(fd, DRM_MODE_OBJECT_CRTC,
			       res->crtcs[i], atomic);
	}

	for (int i = 0; i < res->count_connectors; i++) {
		validate_props(fd, DRM_MODE_OBJECT_CONNECTOR,
			       res->connectors[i], atomic);
	}

	for (int i = 0; i < res->count_encoders; i++) {
		expect_no_props(fd, DRM_MODE_OBJECT_ENCODER,
				res->encoders[i]);
	}

	drmModeFreePlaneResources(plane_res);
	drmModeFreeResources(res);
}

static void invalid_properties(igt_display_t *display, bool atomic)
{
	igt_output_t *output;
	igt_plane_t *plane;
	enum pipe pipe;
	int i;

	if (atomic)
		igt_skip_on(!display->is_atomic);

	for_each_pipe(display, pipe)
		test_object_invalid_properties(display, display->pipes[pipe].crtc_id, DRM_MODE_OBJECT_CRTC, atomic);

	for_each_pipe(display, pipe)
		for_each_plane_on_pipe(display, pipe, plane)
			test_object_invalid_properties(display, plane->drm_plane->plane_id, DRM_MODE_OBJECT_PLANE, atomic);

	for (i = 0, output = &display->outputs[0]; i < display->n_outputs; output = &display->outputs[++i])
		test_object_invalid_properties(display, output->id, DRM_MODE_OBJECT_CONNECTOR, atomic);
}

igt_main
{
	igt_display_t display;

	igt_skip_on_simulation();

	igt_fixture {
		display.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&display, display.drm_fd);
	}

	igt_subtest("plane-properties-legacy")
		plane_properties(&display, false);

	igt_subtest("plane-properties-atomic")
		plane_properties(&display, true);

	igt_subtest("crtc-properties-legacy")
		crtc_properties(&display, false);

	igt_subtest("crtc-properties-atomic")
		crtc_properties(&display, true);

	igt_subtest("connector-properties-legacy")
		connector_properties(&display, false);

	igt_subtest("connector-properties-atomic")
		connector_properties(&display, true);

	igt_subtest("invalid-properties-legacy")
		invalid_properties(&display, false);

	igt_subtest("invalid-properties-atomic")
		invalid_properties(&display, true);

	igt_subtest("get_properties-sanity-atomic") {
		igt_skip_on(!display.is_atomic);
		get_prop_sanity(&display, true);
	}

	igt_subtest("get_properties-sanity-non-atomic") {
		if (display.is_atomic)
			igt_assert_eq(drmSetClientCap(display.drm_fd, DRM_CLIENT_CAP_ATOMIC, 0), 0);

		get_prop_sanity(&display, false);

		if (display.is_atomic)
			igt_assert_eq(drmSetClientCap(display.drm_fd, DRM_CLIENT_CAP_ATOMIC, 1), 0);
	}

	igt_fixture {
		igt_display_fini(&display);
	}
}
