/*
 * Copyright © 2018 Intel Corporation
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
 * Displayport Display Stream Compression test
 * Until the CRC support is added this needs to be invoked with --interactive
 * to manually verify if the test pattern is seen without corruption for each
 * subtest.
 *
 * Authors:
 * Manasi Navare <manasi.d.navare@intel.com>
 *
 */
#include "igt.h"
#include "igt_sysfs.h"
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>

enum dsc_test_type
{
	test_basic_dsc_enable
};

typedef struct {
	int drm_fd;
	int debugfs_fd;
	uint32_t id;
	igt_display_t display;
	struct igt_fb fb_test_pattern;
	igt_output_t *output;
	int mode_valid;
	drmModeModeInfo *mode;
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	int crtc;
	enum pipe pipe;
	char conn_name[128];
} data_t;

bool force_dsc_en_orig;
int force_dsc_restore_fd = -1;

static inline void manual(const char *expected)
{
	igt_debug_manual_check("all", expected);
}

static bool is_dp_dsc_supported(data_t *data)
{
	char file_name[128] = {0};
	char buf[512];

	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	igt_require(igt_debugfs_simple_read(data->debugfs_fd, file_name, buf,
					    sizeof(buf)) > 0);
	igt_debugfs_read(data->drm_fd, file_name, buf);

	return strstr(buf, "DSC_Sink_Support: yes");
}

static bool is_dp_fec_supported(data_t *data)
{
	char file_name[128] = {0};
	char buf[512];

	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	igt_debugfs_read(data->drm_fd, file_name, buf);

	return strstr(buf, "FEC_Sink_Support: yes");
}

static bool is_dp_dsc_enabled(data_t *data)
{
	char file_name[128] = {0};
	char buf[512];

	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	igt_debugfs_read(data->drm_fd, file_name, buf);

	return strstr(buf, "DSC_Enabled: yes");
}

static void force_dp_dsc_enable(data_t *data)
{
	char file_name[128] = {0};
	int ret;

	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	igt_debug ("Forcing DSC enable on %s\n", data->conn_name);
	ret = igt_sysfs_write(data->debugfs_fd, file_name, "1", 1);
	igt_assert_f(ret > 0, "debugfs_write failed");
}

static bool is_force_dsc_enabled(data_t *data)
{
	char file_name[128] = {0};
	char buf[512];

	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	igt_debugfs_read(data->drm_fd, file_name, buf);

	return strstr(buf, "Force_DSC_Enable: yes");
}

static void save_force_dsc_en(data_t *data)
{
	char file_name[128] = {0};

	force_dsc_en_orig = is_force_dsc_enabled(data);
	strcpy(file_name, data->conn_name);
	strcat(file_name, "/i915_dsc_fec_support");
	force_dsc_restore_fd = openat(igt_debugfs_dir(data->drm_fd),
				      file_name, O_WRONLY);
	igt_assert(force_dsc_restore_fd >= 0);
}

static void restore_force_dsc_en(void)
{
	if (force_dsc_restore_fd < 0)
		return;

	igt_debug("Restoring DSC enable\n");
	igt_assert(write(force_dsc_restore_fd, force_dsc_en_orig ? "1" : "0", 1) == 1);

	close(force_dsc_restore_fd);
	force_dsc_restore_fd = -1;
}

static void test_cleanup(data_t *data)
{
	igt_plane_t *primary;

	if (data->output) {
		primary = igt_output_get_plane_type(data->output,
						    DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, NULL);
		igt_display_commit(&data->display);
		igt_remove_fb(data->drm_fd, &data->fb_test_pattern);
	}
}

static void kms_dp_dsc_exit_handler(int sig)
{
	restore_force_dsc_en();
}


/*
 * Re-probe connectors and do a modeset with DSC
 *
 */
static void update_display(data_t *data, enum dsc_test_type test_type)
{
	igt_plane_t *primary;
	data->mode = igt_output_get_mode(data->output);
	data->connector = data->output->config.connector;

	if (data->connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort &&
	    data->pipe == PIPE_A) {
		igt_debug("DSC not supported on Pipe A on external DP\n");
		return;
	}

	/* Disable the output first */
	igt_output_set_pipe(data->output, PIPE_NONE);
	igt_display_commit(&data->display);

	if (test_type == test_basic_dsc_enable) {
		bool enabled;

		igt_debug("DSC is supported on %s\n", data->conn_name);
		save_force_dsc_en(data);
		force_dp_dsc_enable(data);

		igt_output_set_pipe(data->output, data->pipe);
		igt_create_pattern_fb(data->drm_fd, data->mode->hdisplay,
				      data->mode->vdisplay,
				      DRM_FORMAT_XRGB8888,
				      LOCAL_DRM_FORMAT_MOD_NONE,
				      &data->fb_test_pattern);
		primary = igt_output_get_plane_type(data->output,
						    DRM_PLANE_TYPE_PRIMARY);

		/* Now set the output to the desired mode */
		igt_plane_set_fb(primary, &data->fb_test_pattern);
		igt_display_commit(&data->display);

		/*
		 * Until we have CRC check support, manually check if RGB test
		 * pattern has no corruption.
		 */
		manual("RGB test pattern without corruption");

		enabled = is_dp_dsc_enabled(data);
		restore_force_dsc_en();

		igt_assert_f(enabled,
			     "Default DSC enable failed on Connector: %s Pipe: %s\n",
			     data->conn_name,
			     kmstest_pipe_name(data->pipe));
	} else {
		igt_assert(!"Unknown test type\n");
	}
}

static void run_test(data_t *data, igt_output_t *output,
		     enum dsc_test_type test_type)
{
	enum pipe pipe;

	for_each_pipe(&data->display, pipe) {

		if (igt_pipe_connector_valid(pipe, output)) {
			data->pipe = pipe;
			data->output = output;
			update_display(data, test_type);
			test_cleanup(data);
		}
	}
}

igt_main
{
	data_t data = {};
	igt_output_t *output;
	drmModeRes *res;
	drmModeConnector *connector;
	int i, test_conn_cnt, test_cnt;
	int tests[] = {DRM_MODE_CONNECTOR_eDP, DRM_MODE_CONNECTOR_DisplayPort};

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_ANY);
		data.debugfs_fd = igt_debugfs_dir(data.drm_fd);
		kmstest_set_vt_graphics_mode();
		igt_install_exit_handler(kms_dp_dsc_exit_handler);
		igt_display_require(&data.display, data.drm_fd);
		igt_require(res = drmModeGetResources(data.drm_fd));
	}

	for (test_cnt = 0; test_cnt < ARRAY_SIZE(tests); test_cnt++) {

		igt_subtest_f("basic-dsc-enable-%s",
			      kmstest_connector_type_str(tests[test_cnt])) {
			test_conn_cnt = 0;
			for (i = 0; i < res->count_connectors; i++) {
				connector = drmModeGetConnectorCurrent(data.drm_fd,
								       res->connectors[i]);
				if (connector->connection != DRM_MODE_CONNECTED ||
				    connector->connector_type !=
				    tests[test_cnt])
					continue;
				output = igt_output_from_connector(&data.display, connector);
				sprintf(data.conn_name, "%s-%d",
					kmstest_connector_type_str(connector->connector_type),
					connector->connector_type_id);
				if(!is_dp_dsc_supported(&data)) {
					igt_debug("DSC not supported on connector %s \n",
						  data.conn_name);
					continue;
				}
				if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort &&
				    !is_dp_fec_supported(&data)) {
					igt_debug("DSC cannot be enabled without FEC on %s\n",
						  data.conn_name);
					continue;
				}
				test_conn_cnt++;
				run_test(&data, output, test_basic_dsc_enable);
			}
			igt_skip_on(test_conn_cnt == 0);
		}
	}

	igt_fixture {
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(data.debugfs_fd);
		close(data.drm_fd);
		igt_display_fini(&data.display);
	}
}
