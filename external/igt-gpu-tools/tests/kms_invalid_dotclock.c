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
 *
 */

#include "igt.h"
#include <stdbool.h>

IGT_TEST_DESCRIPTION("Make sure all modesets are rejected when the requested dotclock is too high");

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_output_t *output;
	drmModeResPtr res;
	int max_dotclock;
} data_t;

static bool has_scaling_mode_prop(data_t *data)
{
	return kmstest_get_property(data->drm_fd,
				    data->output->id,
				    DRM_MODE_OBJECT_CONNECTOR,
				    "scaling mode",
				    NULL, NULL, NULL);
}

static int
test_output(data_t *data)
{
	igt_output_t *output = data->output;
	drmModeModeInfo mode;
	struct igt_fb fb;
	int i;

	/*
	 * FIXME When we have a fixed mode, the kernel will ignore
	 * the user timings apart from hdisplay/vdisplay. Should
	 * fix the kernel to at least make sure the requested
	 * refresh rate as specified by the user timings will
	 * roughly match the user will get. For now skip the
	 * test on  any connector with a fixed mode.
	 */
	if (has_scaling_mode_prop(data))
		return 0;

	/*
	 * FIXME test every mode we have to be more
	 * sure everything is really getting rejected?
	 */
	mode = *igt_output_get_mode(output);
	mode.clock = data->max_dotclock + 1;

	igt_create_fb(data->drm_fd,
		      mode.hdisplay, mode.vdisplay,
		      DRM_FORMAT_XRGB8888,
		      LOCAL_DRM_FORMAT_MOD_NONE,
		      &fb);

	for (i = 0; i < data->res->count_crtcs; i++) {
		int ret;

		igt_info("Checking pipe %c connector %s with mode %s\n",
			 'A'+i, output->name, mode.name);

		ret = drmModeSetCrtc(data->drm_fd, data->res->crtcs[i],
				     fb.fb_id, 0, 0,
				     &output->id, 1, &mode);
		igt_assert_lt(ret, 0);
	}

	igt_remove_fb(data->drm_fd, &fb);

	return 1;
}

static void test(data_t *data)
{
	int valid_connectors = 0;

	for_each_connected_output(&data->display, data->output) {
		valid_connectors += test_output(data);
	}

	igt_require_f(valid_connectors, "No suitable connectors found\n");
}

static int i915_max_dotclock(data_t *data)
{
	char buf[4096];
	char *s;
	int max_dotclock = 0;

	igt_debugfs_read(data->drm_fd, "i915_frequency_info", buf);
	s = strstr(buf, "Max pixel clock frequency:");
	igt_assert(s);
	igt_assert_eq(sscanf(s, "Max pixel clock frequency: %d kHz", &max_dotclock), 1);

	/* 100 Mhz to 5 GHz seem like reasonable values to expect */
	igt_assert_lt(max_dotclock, 5000000);
	igt_assert_lt(100000, max_dotclock);

	return max_dotclock;
}

static data_t data;

igt_simple_main
{
	igt_skip_on_simulation();

	data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
	igt_require_intel(data.drm_fd);

	kmstest_set_vt_graphics_mode();

	igt_display_require(&data.display, data.drm_fd);
	data.res = drmModeGetResources(data.drm_fd);
	igt_assert(data.res);

	kmstest_unset_all_crtcs(data.drm_fd, data.res);

	data.max_dotclock = i915_max_dotclock(&data);
	igt_info("Max dotclock: %d kHz\n", data.max_dotclock);

	test(&data);

	igt_display_fini(&data.display);
	igt_reset_connectors();
	drmModeFreeResources(data.res);
}
