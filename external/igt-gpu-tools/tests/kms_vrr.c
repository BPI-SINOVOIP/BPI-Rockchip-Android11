/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "igt.h"
#include "sw_sync.h"
#include <fcntl.h>
#include <signal.h>

#define NSECS_PER_SEC (1000000000ull)

/*
 * Each test measurement step runs for ~5 seconds.
 * This gives a decent sample size + enough time for any adaptation to occur if necessary.
 */
#define TEST_DURATION_NS (5000000000ull)

enum {
	TEST_NONE = 0,
	TEST_DPMS = 1 << 0,
	TEST_SUSPEND = 1 << 1,
};

typedef struct range {
	unsigned int min;
	unsigned int max;
} range_t;

typedef struct data {
	igt_display_t display;
	int drm_fd;
	igt_fb_t fb0;
	igt_fb_t fb1;
} data_t;

typedef void (*test_t)(data_t*, enum pipe, igt_output_t*, uint32_t);

/* Converts a timespec structure to nanoseconds. */
static uint64_t timespec_to_ns(struct timespec *ts)
{
	return ts->tv_sec * NSECS_PER_SEC + ts->tv_nsec;
}

/*
 * Gets a vblank event from DRM and returns its timestamp in nanoseconds.
 * This blocks until the event is received.
 */
static uint64_t get_vblank_event_ns(data_t *data)
{
	struct drm_event_vblank ev;

	igt_set_timeout(1, "Waiting for vblank event\n");
	igt_assert_eq(read(data->drm_fd, &ev, sizeof(ev)), sizeof(ev));
	igt_reset_timeout();

	return ev.tv_sec * NSECS_PER_SEC + ev.tv_usec * 1000ull;
}

/*
 * Returns the current CLOCK_MONOTONIC time in nanoseconds.
 * The regular IGT helpers can't be used since they default to
 * CLOCK_MONOTONIC_RAW - which isn't what the kernel uses for its timestamps.
 */
static uint64_t get_time_ns(void)
{
	struct timespec ts;
	memset(&ts, 0, sizeof(ts));
	errno = 0;

	if (!clock_gettime(CLOCK_MONOTONIC, &ts))
		return timespec_to_ns(&ts);

	igt_warn("Could not read monotonic time: %s\n", strerror(errno));
	igt_fail(-errno);

	return 0;
}

/* Returns the rate duration in nanoseconds for the given refresh rate. */
static uint64_t rate_from_refresh(uint64_t refresh)
{
	return NSECS_PER_SEC / refresh;
}

/* Returns the min and max vrr range from the connector debugfs. */
static range_t get_vrr_range(data_t *data, igt_output_t *output)
{
	char buf[256];
	char *start_loc;
	int fd, res;
	range_t range;

	fd = igt_debugfs_connector_dir(data->drm_fd, output->name, O_RDONLY);
	igt_assert(fd >= 0);

	res = igt_debugfs_simple_read(fd, "vrr_range", buf, sizeof(buf));
	igt_require(res > 0);

	close(fd);

	igt_assert(start_loc = strstr(buf, "Min: "));
	igt_assert_eq(sscanf(start_loc, "Min: %u", &range.min), 1);

	igt_assert(start_loc = strstr(buf, "Max: "));
	igt_assert_eq(sscanf(start_loc, "Max: %u", &range.max), 1);

	return range;
}

/* Returns a suitable vrr test frequency. */
static uint32_t get_test_rate_ns(data_t *data, igt_output_t *output)
{
	drmModeModeInfo *mode = igt_output_get_mode(output);
	range_t range;
	uint32_t vtest;

	/*
	 * The frequency with the fastest convergence speed should be
	 * the midpoint between the current mode vfreq and the min
	 * supported vfreq.
	 */
	range = get_vrr_range(data, output);
	igt_require(mode->vrefresh > range.min);

	vtest = (mode->vrefresh - range.min) / 2 + range.min;
	igt_require(vtest < mode->vrefresh);

	return rate_from_refresh(vtest);
}

/* Returns true if an output supports VRR. */
static bool has_vrr(igt_output_t *output)
{
	return igt_output_has_prop(output, IGT_CONNECTOR_VRR_CAPABLE) &&
	       igt_output_get_prop(output, IGT_CONNECTOR_VRR_CAPABLE);
}

/* Toggles variable refresh rate on the pipe. */
static void set_vrr_on_pipe(data_t *data, enum pipe pipe, bool enabled)
{
	igt_pipe_set_prop_value(&data->display, pipe, IGT_CRTC_VRR_ENABLED,
				enabled);
	igt_display_commit_atomic(&data->display, 0, NULL);
}

/* Prepare the display for testing on the given pipe. */
static void prepare_test(data_t *data, igt_output_t *output, enum pipe pipe)
{
	drmModeModeInfo mode = *igt_output_get_mode(output);
	igt_plane_t *primary;
	cairo_t *cr;

	/* Reset output */
	igt_display_reset(&data->display);
	igt_output_set_pipe(output, pipe);

	/* Prepare resources */
	igt_create_color_fb(data->drm_fd, mode.hdisplay, mode.vdisplay,
			    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			    0.50, 0.50, 0.50, &data->fb0);

	igt_create_color_fb(data->drm_fd, mode.hdisplay, mode.vdisplay,
			    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			    0.50, 0.50, 0.50, &data->fb1);

	cr = igt_get_cairo_ctx(data->drm_fd, &data->fb0);

	igt_paint_color(cr, 0, 0, mode.hdisplay / 10, mode.vdisplay / 10,
			1.00, 0.00, 0.00);

	igt_put_cairo_ctx(data->drm_fd, &data->fb0, cr);

	/* Take care of any required modesetting before the test begins. */
	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, &data->fb0);

	igt_display_commit_atomic(&data->display,
				  DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
}

/* Waits for the vblank interval. Returns the vblank timestamp in ns. */
static uint64_t
wait_for_vblank(data_t *data, enum pipe pipe)
{
	drmVBlank vbl = {};

	vbl.request.type = kmstest_get_vbl_flag(pipe);
	vbl.request.type |= DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
	vbl.request.sequence = 1;
	drmWaitVBlank(data->drm_fd, &vbl);

	return get_vblank_event_ns(data);
}

/* Performs an asynchronous non-blocking page-flip on a pipe. */
static int
do_flip(data_t *data, enum pipe pipe_id, igt_fb_t *fb)
{
	igt_pipe_t *pipe = &data->display.pipes[pipe_id];
	int ret;

	igt_set_timeout(1, "Scheduling page flip\n");

	/*
	 * Only the legacy flip ioctl supports async flips.
	 * It's also non-blocking, but returns -EBUSY if flipping too fast.
	 * 2x monitor tests will need async flips in the atomic API.
	 */
	do {
		ret = drmModePageFlip(data->drm_fd, pipe->crtc_id,
				      fb->fb_id,
				      DRM_MODE_PAGE_FLIP_EVENT |
				      DRM_MODE_PAGE_FLIP_ASYNC,
				      data);
	} while (ret == -EBUSY);

	igt_assert_eq(ret, 0);
	igt_reset_timeout();

	return 0;
}

/*
 * Flips at the given rate and measures against the expected value.
 * Returns the pass rate as a percentage from 0 - 100.
 *
 * The VRR API is quite flexible in terms of definition - the driver
 * can arbitrarily restrict the bounds further than the absolute
 * min and max range. But VRR is really about extending the flip
 * to prevent stuttering or to match a source content rate.
 *
 * The only way to "present" at a fixed rate like userspace in a vendor
 * neutral manner is to do it with async flips. This avoids the need
 * to wait for next vblank and it should eventually converge at the
 * desired rate.
 */
static uint32_t
flip_and_measure(data_t *data, igt_output_t *output, enum pipe pipe,
		 uint64_t rate_ns, uint64_t duration_ns)
{
	uint64_t start_ns, last_vblank_ns;
	uint32_t total_flip = 0, total_pass = 0;
	bool front = false;

	/* Align with the vblank region to speed up convergence. */
	last_vblank_ns = wait_for_vblank(data, pipe);
	start_ns = get_time_ns();

	for (;;) {
		uint64_t now_ns, vblank_ns, wait_ns, target_ns;
		int64_t diff_ns;

		front = !front;
		do_flip(data, pipe, front ? &data->fb1 : &data->fb0);

		vblank_ns = get_vblank_event_ns(data);
		diff_ns = rate_ns - (vblank_ns - last_vblank_ns);
		last_vblank_ns = vblank_ns;

		total_flip += 1;

		/*
		 * Check if the difference between the two flip timestamps
		 * was within the required threshold from the expected rate.
		 *
		 * A ~50us threshold is arbitrary, but it's roughly the
		 * difference between 144Hz and 143Hz which should give this
		 * enough accuracy for most use cases.
		 */
		if (llabs(diff_ns) < 50000ll)
			total_pass += 1;

		now_ns = get_time_ns();
		if (now_ns - start_ns > duration_ns)
			break;

		/*
		 * Burn CPU until next timestamp, sleeping isn't accurate enough.
		 * It's worth noting that the target timestamp is based on absolute
		 * timestamp rather than a delta to avoid accumulation errors.
		 */
		diff_ns = now_ns - start_ns;
		wait_ns = ((diff_ns + rate_ns - 1) / rate_ns) * rate_ns;
		target_ns = start_ns + wait_ns - 10;

		while (get_time_ns() < target_ns);
	}

	igt_info("Completed %u flips, %u were in threshold for %luns.\n",
		 total_flip, total_pass, rate_ns);

	return total_flip ? ((total_pass * 100) / total_flip) : 0;
}

/* Basic VRR flip functionality test - enable, measure, disable, measure */
static void
test_basic(data_t *data, enum pipe pipe, igt_output_t *output, uint32_t flags)
{
	uint64_t rate;
	uint32_t result;

	rate = get_test_rate_ns(data, output);

	prepare_test(data, output, pipe);

	set_vrr_on_pipe(data, pipe, 1);

	/*
	 * Do a short run with VRR, but don't check the result.
	 * This is to make sure we were actually in the middle of
	 * active flipping before doing the DPMS/suspend steps.
	 */
	flip_and_measure(data, output, pipe, rate, 250000000ull);

	if (flags & TEST_DPMS) {
		kmstest_set_connector_dpms(output->display->drm_fd,
					   output->config.connector,
					   DRM_MODE_DPMS_OFF);
		kmstest_set_connector_dpms(output->display->drm_fd,
					   output->config.connector,
					   DRM_MODE_DPMS_ON);
	}

	if (flags & TEST_SUSPEND)
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);

	result = flip_and_measure(data, output, pipe, rate, TEST_DURATION_NS);

	set_vrr_on_pipe(data, pipe, 0);

	/* This check is delayed until after VRR is disabled so it isn't
	 * left enabled if the test fails. */
	igt_assert_f(result > 75,
		     "Target VRR on threshold not reached, result was %u%%\n",
		     result);

	result = flip_and_measure(data, output, pipe, rate, TEST_DURATION_NS);

	igt_assert_f(result < 10,
		     "Target VRR off threshold exceeded, result was %u%%\n",
		     result);

	igt_remove_fb(data->drm_fd, &data->fb1);
	igt_remove_fb(data->drm_fd, &data->fb0);
}

/* Runs tests on outputs that are VRR capable. */
static void
run_vrr_test(data_t *data, test_t test, uint32_t flags)
{
	igt_output_t *output;
	bool found = false;

	for_each_connected_output(&data->display, output) {
		enum pipe pipe;

		if (!has_vrr(output))
			continue;

		for_each_pipe(&data->display, pipe)
			if (igt_pipe_connector_valid(pipe, output)) {
				test(data, pipe, output, flags);
				found = true;
				break;
			}
	}

	if (!found)
		igt_skip("No vrr capable outputs found.\n");
}

igt_main
{
	data_t data = { 0 };

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);

		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
		igt_display_require_output(&data.display);
	}

	igt_subtest("flip-basic")
		run_vrr_test(&data, test_basic, 0);

	igt_subtest("flip-dpms")
		run_vrr_test(&data, test_basic, TEST_DPMS);

	igt_subtest("flip-suspend")
		run_vrr_test(&data, test_basic, TEST_SUSPEND);

	igt_fixture {
		igt_display_fini(&data.display);
	}
}
