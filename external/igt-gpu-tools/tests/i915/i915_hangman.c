/*
 * Copyright © 2014 Intel Corporation
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
 *    Mika Kuoppala <mika.kuoppala@intel.com>
 *    Oscar Mateo <oscar.mateo@intel.com>
 *
 */

#include "igt.h"
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "igt_sysfs.h"
#include "igt_debugfs.h"

#ifndef I915_PARAM_CMD_PARSER_VERSION
#define I915_PARAM_CMD_PARSER_VERSION       28
#endif

static int device = -1;
static int sysfs = -1;

static bool has_error_state(int dir)
{
	bool result;
	int fd;

	fd = openat(dir, "error", O_RDONLY);
	if (fd < 0)
		return false;

	if (read(fd, &result, sizeof(result)) < 0)
		result = false;
	else
		result = true;

	close(fd);
	return result;
}

static void assert_entry(const char *s, bool expect)
{
	char *error;

	error = igt_sysfs_get(sysfs, "error");
	igt_assert(error);

	igt_assert_f(!!strcasecmp(error, s) != expect,
		     "contents of error: '%s' (expected %s '%s')\n",
		     error, expect ? "": "not", s);

	free(error);
}

static void assert_error_state_clear(void)
{
	assert_entry("no error state collected", true);
}

static void assert_error_state_collected(void)
{
	assert_entry("no error state collected", false);
}

static void clear_error_state(void)
{
	igt_sysfs_write(sysfs, "error", "", 1);
}

static void test_error_state_basic(void)
{
	int fd;

	clear_error_state();
	assert_error_state_clear();

	/* Manually trigger a hang by request a reset */
	fd = igt_debugfs_open(device, "i915_wedged", O_WRONLY);
	igt_ignore_warn(write(fd, "1\n", 2));
	close(fd);

	assert_error_state_collected();

	clear_error_state();
	assert_error_state_clear();
}

static FILE *open_error(void)
{
	int fd;

	fd = openat(sysfs, "error", O_RDONLY);
	if (fd < 0)
		return NULL;

	return fdopen(fd, "r");
}

static bool uses_cmd_parser(void)
{
	int parser_version = 0;
	drm_i915_getparam_t gp;

	gp.param = I915_PARAM_CMD_PARSER_VERSION;
	gp.value = &parser_version;
	drmIoctl(device, DRM_IOCTL_I915_GETPARAM, &gp);

	return parser_version > 0;
}

static void check_error_state(const char *expected_ring_name,
			      uint64_t expected_offset,
			      const uint32_t *batch)
{
	bool cmd_parser = uses_cmd_parser();
	FILE *file = open_error();
	char *line = NULL;
	size_t line_size = 0;
	bool found = false;

	igt_debug("%s(expected ring name=%s, expected offset=%"PRIx64")\n",
		  __func__, expected_ring_name, expected_offset);
	igt_debugfs_dump(device, "i915_error_state");

	igt_assert(getline(&line, &line_size, file) != -1);
	igt_assert(strcasecmp(line, "No error state collected"));

	while (getline(&line, &line_size, file) > 0) {
		char *dashes;
		uint32_t gtt_offset_upper, gtt_offset_lower;
		int matched;

		dashes = strstr(line, "---");
		if (!dashes)
			continue;

		matched = sscanf(dashes, "--- gtt_offset = 0x%08x %08x\n",
				 &gtt_offset_upper, &gtt_offset_lower);
		if (matched) {
			char expected_line[128];
			uint64_t gtt_offset;
			int i;

			strncpy(expected_line, line, dashes - line);
			expected_line[dashes - line - 1] = '\0';
			igt_assert(strstr(expected_line, expected_ring_name));

			gtt_offset = gtt_offset_upper;
			if (matched == 2) {
				gtt_offset <<= 32;
				gtt_offset |= gtt_offset_lower;
			}
			if (!cmd_parser)
				igt_assert_eq_u64(gtt_offset, expected_offset);

			for (i = 0; i < 1024; i++) {
				igt_assert(getline(&line, &line_size, file) > 0);
				if (line[0] == ':' || line[0] == '~')
					break;

				snprintf(expected_line, sizeof(expected_line),
					 "%08x :  %08x",
					 4*i, batch[i]);
				igt_assert(strstr(line, expected_line));
			}

			found = true;
			break;
		}
	}

	free(line);
	fclose(file);

	clear_error_state();

	igt_assert(found);
}

static void test_error_state_capture(unsigned ring_id,
				     const char *ring_name)
{
	uint32_t *batch;
	igt_hang_t hang;
	uint64_t offset;

	clear_error_state();

	hang = igt_hang_ctx(device, 0, ring_id, HANG_ALLOW_CAPTURE);
	offset = hang.spin->obj[IGT_SPIN_BATCH].offset;

	batch = gem_mmap__cpu(device, hang.spin->handle, 0, 4096, PROT_READ);
	gem_set_domain(device, hang.spin->handle, I915_GEM_DOMAIN_CPU, 0);

	igt_post_hang_ring(device, hang);

	check_error_state(ring_name, offset, batch);
	munmap(batch, 4096);
}

/* This test covers the case where we end up in an uninitialised area of the
 * ppgtt and keep executing through it. This is particularly relevant if 48b
 * ppgtt is enabled because the ppgtt is massively bigger compared to the 32b
 * case and it takes a lot more time to wrap, so the acthd can potentially keep
 * increasing for a long time
 */
static void hangcheck_unterminated(void)
{
	/* timeout needs to be greater than ~5*hangcheck */
	int64_t timeout_ns = 100ull * NSEC_PER_SEC; /* 100 seconds */
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 gem_exec;
	uint32_t handle;

	igt_require(gem_uses_full_ppgtt(device));
	igt_require_hang_ring(device, 0);

	handle = gem_create(device, 4096);

	memset(&gem_exec, 0, sizeof(gem_exec));
	gem_exec.handle = handle;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)&gem_exec;
	execbuf.buffer_count = 1;

	gem_execbuf(device, &execbuf);
	if (gem_wait(device, handle, &timeout_ns) != 0) {
		/* need to manually trigger an hang to clean before failing */
		igt_force_gpu_reset(device);
		igt_assert_f(0, "unterminated batch did not trigger an hang!");
	}
}

igt_main
{
	const struct intel_execution_engine2 *e;
	igt_hang_t hang = {};

	igt_skip_on_simulation();

	igt_fixture {
		device = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(device);

		hang = igt_allow_hang(device, 0, HANG_ALLOW_CAPTURE);

		sysfs = igt_sysfs_open(device);
		igt_assert(sysfs != -1);

		igt_require(has_error_state(sysfs));
	}

	igt_subtest("error-state-basic")
		test_error_state_basic();

	__for_each_physical_engine(device, e)
		igt_subtest_f("error-state-capture-%s", e->name)
			test_error_state_capture(e->flags, e->name);

	igt_subtest("hangcheck-unterminated")
		hangcheck_unterminated();

	igt_fixture {
		igt_disallow_hang(device, hang);
	}
}
