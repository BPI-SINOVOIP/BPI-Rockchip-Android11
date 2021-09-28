/*
 * Copyright © 2017 Intel Corporation
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
#include "config.h"
#include "igt.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>
#include <linux/limits.h>

#define TOOLS "../tools/"

struct line_check {
	int found;
	const char *substr;
};

/**
 * Our igt_log_buffer_inspect handler. Checks the output of the
 * intel_l3_parity tool and increments line_check::found if a specific
 * substring is found.
 */
static bool check_cmd_output(const char *line, void *data)
{
	struct line_check *check = data;

	if (strstr(line, check->substr)) {
		check->found++;
	}

	return false;
}

static void assert_cmd_success(int exec_return)
{
	igt_skip_on_f(exec_return == IGT_EXIT_SKIP,
		      "intel_l3_parity not supported\n");
	igt_assert_eq(exec_return, IGT_EXIT_SUCCESS);
}

static bool chdir_to_tools_dir(void)
{
	char path[PATH_MAX];

	/* Try TOOLS relative to cwd */
	if (chdir(TOOLS) == 0)
		return true;

	/* Try TOOLS and install dir relative to test binary */
	if (readlink("/proc/self/exe", path, sizeof(path)) > 0)
		chdir(dirname(path));

	return chdir(TOOLS) == 0 || chdir("../../bin") == 0;
}

igt_main
{
	igt_skip_on_simulation();

	igt_fixture {
		char *path;

		igt_require_f(chdir_to_tools_dir(),
			      "Unable to determine the tools directory, expecting them in $cwd/" TOOLS " or $path/" TOOLS "\n");
		path = get_current_dir_name();
		igt_info("Using tools from %s\n", path);
		free(path);
	}

	igt_subtest("sysfs_l3_parity") {
		int exec_return;
		struct line_check line;

		igt_require(access("intel_l3_parity", X_OK) == 0);

		/* Sanity check that l3 parity tool is usable: Enable
		 * row,bank,subbank 0,0,0.
		 *
		 * TODO: Better way to find intel_l3_parity. This path
		 * is relative to piglit's path, when run through
		 * piglit.
		 */
		igt_system_cmd(exec_return,
			       "./intel_l3_parity -r 0 -b 0 "
			       "-s 0 -e");
		assert_cmd_success(exec_return);

		/* Disable row,bank,subbank 0,0,0. */
		igt_system_cmd(exec_return,
			       "./intel_l3_parity -r 0 -b 0 "
			       "-s 0 -d");
		assert_cmd_success(exec_return);

		/* Check that disabling was successful */
		igt_system_cmd(exec_return,
			       "./intel_l3_parity -l");
		assert_cmd_success(exec_return);
		line.substr = "Row 0, Bank 0, Subbank 0 is disabled";
		line.found = 0;
		igt_log_buffer_inspect(check_cmd_output, &line);
		igt_assert_eq(line.found, 1);

		/* Re-enable row,bank,subbank 0,0,0 */
		igt_system_cmd(exec_return,
			       "./intel_l3_parity -r 0 -b 0 "
			       "-s 0 -e");
		assert_cmd_success(exec_return);

		/* Check that re-enabling was successful:
		 * intel_l3_parity -l should now not print that Row 0,
		 * Bank 0, Subbank 0 is disabled.
		 *
		 * The previously printed line is already in the log
		 * buffer so we check for count 1.
		 */
		igt_system_cmd(exec_return,
			       "./intel_l3_parity -l");
		assert_cmd_success(exec_return);
		line.substr = "Row 0, Bank 0, Subbank 0 is disabled";
		line.found = 0;
		igt_log_buffer_inspect(check_cmd_output,
				       &line);
		igt_assert_eq(line.found, 1);
	}

	igt_subtest("tools_test") {
		igt_require(access("intel_reg", X_OK) == 0);

		igt_assert_eq(igt_system_quiet("./intel_reg read 0x4030"),
			      IGT_EXIT_SUCCESS);

		igt_assert_eq(igt_system_quiet("./intel_reg dump"),
			      IGT_EXIT_SUCCESS);
	}
}
