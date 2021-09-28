/*
 * Copyright © 2015 Intel Corporation
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
 * Authors: Paulo Zanoni <paulo.r.zanoni@intel.com>
 *
 */

#include "igt.h"
#include "igt_psr.h"
#include "igt_sysfs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


IGT_TEST_DESCRIPTION("Test the relationship between fbcon and the frontbuffer "
		     "tracking infrastructure.");

#define MAX_CONNECTORS 32

static bool do_wait_user = false;

struct drm_info {
	int fd;
	int debugfs_fd;
	drmModeResPtr res;
	drmModeConnectorPtr connectors[MAX_CONNECTORS];
};

static void wait_user(const char *msg)
{
	if (!do_wait_user)
		return;

	igt_info("%s Press enter...\n", msg);
	while (getchar() != '\n')
		;
}

static void setup_drm(struct drm_info *drm)
{
	int i;

	if (drm->fd >= 0)
		return;

	drm->fd = drm_open_driver_master(DRIVER_INTEL);

	drm->res = drmModeGetResources(drm->fd);
	igt_require(drm->res);
	igt_assert(drm->res->count_connectors <= MAX_CONNECTORS);

	for (i = 0; i < drm->res->count_connectors; i++)
		drm->connectors[i] = drmModeGetConnectorCurrent(drm->fd,
						drm->res->connectors[i]);

	kmstest_set_vt_graphics_mode();
}

static void teardown_drm(struct drm_info *drm)
{
	int i;

	kmstest_restore_vt_mode();

	for (i = 0; i < drm->res->count_connectors; i++)
		drmModeFreeConnector(drm->connectors[i]);

	drmModeFreeResources(drm->res);
	igt_assert(close(drm->fd) == 0);
	drm->fd = -1;
}

static bool fbc_supported_on_chipset(int debugfs_fd)
{
	char buf[128];
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_fbc_status",
				      buf, sizeof(buf));
	if (ret < 0)
		return false;

	return !strstr(buf, "FBC unsupported on this chipset\n");
}

static bool connector_can_fbc(drmModeConnectorPtr connector)
{
	return true;
}

static void fbc_print_status(int debugfs_fd)
{
	static char buf[128];

	igt_debugfs_simple_read(debugfs_fd, "i915_fbc_status", buf,
				sizeof(buf));
	igt_debug("FBC status: %s\n", buf);
}

static bool fbc_is_enabled(int debugfs_fd)
{
	char buf[128];

	igt_debugfs_simple_read(debugfs_fd, "i915_fbc_status", buf,
				sizeof(buf));
	return strstr(buf, "FBC enabled\n");
}

static bool fbc_wait_until_enabled(int debugfs_fd)
{
	bool r = igt_wait(fbc_is_enabled(debugfs_fd), 5000, 1);
	fbc_print_status(debugfs_fd);
	return r;
}

static bool fbc_wait_until_update(int debugfs)
{
	/*
	 * FBC is not expected to be enabled because fbcon do not uses a tiled
	 * framebuffer so a fence can not be setup on the framebuffer and FBC
	 * code requires a fence to accurate track frontbuffer modifications
	 * (what maybe is not necessary anymore as we now have
	 * intel_fbc_invalidate()/flush()).
	 *
	 * If one day fbcon starts to use a tiled framebuffer we would need to
	 * check the 'Compressing' status as in each blink it would be disabled.
	 */
	return !fbc_wait_until_enabled(debugfs);
}

typedef bool (*connector_possible_fn)(drmModeConnectorPtr connector);

static void set_mode_for_one_screen(struct drm_info *drm, struct igt_fb *fb,
				    connector_possible_fn connector_possible)
{
	int i, rc;
	uint32_t crtc_id;
	drmModeModeInfoPtr mode;
	uint32_t buffer_id;
	drmModeConnectorPtr c = NULL;

	for (i = 0; i < drm->res->count_connectors; i++) {
		c = drm->connectors[i];

		if (c->connection == DRM_MODE_CONNECTED && c->count_modes &&
		    connector_possible(c)) {
			mode = &c->modes[0];
			break;
		}
	}
	igt_require_f(i < drm->res->count_connectors,
		      "No connector available\n");

	crtc_id = kmstest_find_crtc_for_connector(drm->fd, drm->res, c, 0);

	buffer_id = igt_create_fb(drm->fd, mode->hdisplay, mode->vdisplay,
				  DRM_FORMAT_XRGB8888,
				  LOCAL_I915_FORMAT_MOD_X_TILED, fb);
	igt_draw_fill_fb(drm->fd, fb, 0xFF);

	igt_info("Setting %dx%d mode for %s connector\n",
		 mode->hdisplay, mode->vdisplay,
		 kmstest_connector_type_str(c->connector_type));

	rc = drmModeSetCrtc(drm->fd, crtc_id, buffer_id, 0, 0,
			    &c->connector_id, 1, mode);
	igt_assert_eq(rc, 0);
}

static bool connector_can_psr(drmModeConnectorPtr connector)
{
	return (connector->connector_type == DRM_MODE_CONNECTOR_eDP);
}

static void psr_print_status(int debugfs_fd)
{
	static char buf[PSR_STATUS_MAX_LEN];

	igt_debugfs_simple_read(debugfs_fd, "i915_edp_psr_status", buf,
				sizeof(buf));
	igt_debug("PSR status: %s\n", buf);
}

static bool psr_wait_until_enabled(int debugfs_fd)
{
	bool r = psr_wait_entry(debugfs_fd, PSR_MODE_1);

	psr_print_status(debugfs_fd);
	return r;
}

static bool psr_supported_on_chipset(int debugfs_fd)
{
	return psr_sink_support(debugfs_fd, PSR_MODE_1);
}

static bool psr_wait_until_update(int debugfs_fd)
{
	return psr_long_wait_update(debugfs_fd, PSR_MODE_1);
}

static void disable_features(int debugfs_fd)
{
	igt_set_module_param_int("enable_fbc", 0);
	psr_disable(debugfs_fd);
}

static inline void fbc_modparam_enable(int debugfs_fd)
{
	igt_set_module_param_int("enable_fbc", 1);
}

static inline void psr_debugfs_enable(int debugfs_fd)
{
	psr_enable(debugfs_fd, PSR_MODE_1);
}

struct feature {
	bool (*supported_on_chipset)(int debugfs_fd);
	bool (*wait_until_enabled)(int debugfs_fd);
	bool (*wait_until_update)(int debugfs_fd);
	bool (*connector_possible_fn)(drmModeConnectorPtr connector);
	void (*enable)(int debugfs_fd);
} fbc = {
	.supported_on_chipset = fbc_supported_on_chipset,
	.wait_until_enabled = fbc_wait_until_enabled,
	.wait_until_update = fbc_wait_until_update,
	.connector_possible_fn = connector_can_fbc,
	.enable = fbc_modparam_enable,
}, psr = {
	.supported_on_chipset = psr_supported_on_chipset,
	.wait_until_enabled = psr_wait_until_enabled,
	.wait_until_update = psr_wait_until_update,
	.connector_possible_fn = connector_can_psr,
	.enable = psr_debugfs_enable,
};

static void subtest(struct drm_info *drm, struct feature *feature, bool suspend)
{
	struct igt_fb fb;

	setup_drm(drm);

	igt_require(feature->supported_on_chipset(drm->debugfs_fd));

	disable_features(drm->debugfs_fd);
	feature->enable(drm->debugfs_fd);

	kmstest_unset_all_crtcs(drm->fd, drm->res);
	wait_user("Modes unset.");
	igt_assert(!feature->wait_until_enabled(drm->debugfs_fd));

	set_mode_for_one_screen(drm, &fb, feature->connector_possible_fn);
	wait_user("Screen set.");
	igt_assert(feature->wait_until_enabled(drm->debugfs_fd));

	if (suspend) {
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);
		sleep(5);
		igt_assert(feature->wait_until_enabled(drm->debugfs_fd));
	}

	igt_remove_fb(drm->fd, &fb);
	teardown_drm(drm);

	/* Wait for fbcon to restore itself. */
	sleep(3);

	wait_user("Back to fbcon.");
	igt_assert(feature->wait_until_update(drm->debugfs_fd));

	if (suspend) {
		igt_system_suspend_autoresume(SUSPEND_STATE_MEM,
					      SUSPEND_TEST_NONE);
		sleep(5);
		igt_assert(feature->wait_until_update(drm->debugfs_fd));
	}
}

static void setup_environment(struct drm_info *drm)
{
	int drm_fd;

	drm_fd = drm_open_driver_master(DRIVER_INTEL);
	igt_require(drm_fd >= 0);
	drm->debugfs_fd = igt_debugfs_dir(drm_fd);
	igt_require(drm->debugfs_fd >= 0);
	igt_assert(close(drm_fd) == 0);

	/*
	 * igt_main()->igt_subtest_init_parse_opts()->common_init() disables the
	 * fbcon bind, so to test it is necessary enable it again
	 */
	bind_fbcon(true);
	fbcon_blink_enable(true);
}

static void teardown_environment(struct drm_info *drm)
{
	if (drm->fd >= 0)
		teardown_drm(drm);

	close(drm->debugfs_fd);
}

igt_main
{
	struct drm_info drm = { .fd = -1 };

	igt_fixture
		setup_environment(&drm);

	igt_subtest("fbc")
		subtest(&drm, &fbc, false);
	igt_subtest("psr")
		subtest(&drm, &psr, false);
	igt_subtest("fbc-suspend")
		subtest(&drm, &fbc, true);
	igt_subtest("psr-suspend")
		subtest(&drm, &psr, true);

	igt_fixture
		teardown_environment(&drm);
}
