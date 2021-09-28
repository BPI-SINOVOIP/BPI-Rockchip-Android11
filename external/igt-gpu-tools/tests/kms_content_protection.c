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
 */

#include <poll.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <libudev.h>
#include "igt.h"
#include "igt_sysfs.h"
#include "igt_kms.h"
#include "igt_kmod.h"

IGT_TEST_DESCRIPTION("Test content protection (HDCP)");

struct data {
	int drm_fd;
	igt_display_t display;
	struct igt_fb red, green;
	unsigned int cp_tests;
} data;

/* Test flags */
#define CP_DPMS					(1 << 0)
#define CP_LIC					(1 << 1)
#define CP_MEI_RELOAD				(1 << 2)
#define CP_TYPE_CHANGE				(1 << 3)
#define CP_UEVENT				(1 << 4)

#define CP_UNDESIRED				0
#define CP_DESIRED				1
#define CP_ENABLED				2

/*
 * HDCP_CONTENT_TYPE_0 can be handled on both HDCP1.4 and HDCP2.2. Where as
 * HDCP_CONTENT_TYPE_1 can be handled only through HDCP2.2.
 */
#define HDCP_CONTENT_TYPE_0				0
#define HDCP_CONTENT_TYPE_1				1

#define LIC_PERIOD_MSEC				(4 * 1000)
/* Kernel retry count=3, Max time per authentication allowed = 6Sec */
#define KERNEL_AUTH_TIME_ALLOWED_MSEC		(3 *  6 * 1000)
#define KERNEL_DISABLE_TIME_ALLOWED_MSEC	(1 * 1000)
#define FLIP_EVENT_POLLING_TIMEOUT_MSEC		1000

__u8 facsimile_srm[] = {
	0x80, 0x0, 0x0, 0x05, 0x01, 0x0, 0x0, 0x36, 0x02, 0x51, 0x1E, 0xF2,
	0x1A, 0xCD, 0xE7, 0x26, 0x97, 0xF4, 0x01, 0x97, 0x10, 0x19, 0x92, 0x53,
	0xE9, 0xF0, 0x59, 0x95, 0xA3, 0x7A, 0x3B, 0xFE, 0xE0, 0x9C, 0x76, 0xDD,
	0x83, 0xAA, 0xC2, 0x5B, 0x24, 0xB3, 0x36, 0x84, 0x94, 0x75, 0x34, 0xDB,
	0x10, 0x9E, 0x3B, 0x23, 0x13, 0xD8, 0x7A, 0xC2, 0x30, 0x79, 0x84};

static void flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
			 unsigned int tv_usec, void *_data)
{
	igt_debug("Flip event received.\n");
}

static int wait_flip_event(void)
{
	int rc;
	drmEventContext evctx;
	struct pollfd pfd;

	evctx.version = 2;
	evctx.vblank_handler = NULL;
	evctx.page_flip_handler = flip_handler;

	pfd.fd = data.drm_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	rc = poll(&pfd, 1, FLIP_EVENT_POLLING_TIMEOUT_MSEC);
	switch (rc) {
	case 0:
		igt_info("Poll timeout. 1Sec.\n");
		rc = -ETIMEDOUT;
		break;
	case 1:
		rc = drmHandleEvent(data.drm_fd, &evctx);
		igt_assert_eq(rc, 0);
		rc = 0;
		break;
	default:
		igt_info("Unexpected poll rc %d\n", rc);
		rc = -1;
		break;
	}

	return rc;
}

static bool hdcp_event(struct udev_monitor *uevent_monitor,
		       struct udev *udev, uint32_t conn_id, uint32_t prop_id)
{
	struct udev_device *dev;
	dev_t udev_devnum;
	struct stat s;
	const char *hotplug, *connector, *property;
	bool ret = false;

	dev = udev_monitor_receive_device(uevent_monitor);
	if (!dev)
		goto out;

	udev_devnum = udev_device_get_devnum(dev);
	fstat(data.display.drm_fd, &s);

	hotplug = udev_device_get_property_value(dev, "HOTPLUG");
	if (!(memcmp(&s.st_rdev, &udev_devnum, sizeof(dev_t)) == 0 &&
	    hotplug && atoi(hotplug) == 1)) {
		igt_debug("Not a Hotplug event\n");
		goto out_dev;
	}

	connector = udev_device_get_property_value(dev, "CONNECTOR");
	if (!(memcmp(&s.st_rdev, &udev_devnum, sizeof(dev_t)) == 0 &&
	    connector && atoi(connector) == conn_id)) {
		igt_debug("Not for connector id: %u\n", conn_id);
		goto out_dev;
	}

	property = udev_device_get_property_value(dev, "PROPERTY");
	if (!(memcmp(&s.st_rdev, &udev_devnum, sizeof(dev_t)) == 0 &&
	    property && atoi(property) == prop_id)) {
		igt_debug("Not for property id: %u\n", prop_id);
		goto out_dev;
	}
	ret = true;

out_dev:
	udev_device_unref(dev);
out:
	return ret;
}

static void hdcp_udev_fini(struct udev_monitor *uevent_monitor,
			   struct udev *udev)
{
	if (uevent_monitor)
		udev_monitor_unref(uevent_monitor);
	if (udev)
		udev_unref(udev);
}

static int hdcp_udev_init(struct udev_monitor *uevent_monitor,
			  struct udev *udev)
{
	int ret = -EINVAL;

	udev = udev_new();
	if (!udev) {
		igt_info("failed to create udev object\n");
		goto out;
	}

	uevent_monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (!uevent_monitor) {
		igt_info("failed to create udev event monitor\n");
		goto out;
	}

	ret = udev_monitor_filter_add_match_subsystem_devtype(uevent_monitor,
							      "drm",
							      "drm_minor");
	if (ret < 0) {
		igt_info("failed to filter for drm events\n");
		goto out;
	}

	ret = udev_monitor_enable_receiving(uevent_monitor);
	if (ret < 0) {
		igt_info("failed to enable udev event reception\n");
		goto out;
	}

	return udev_monitor_get_fd(uevent_monitor);

out:
	hdcp_udev_fini(uevent_monitor, udev);
	return ret;
}

#define MAX_EVENTS	10
static bool wait_for_hdcp_event(uint32_t conn_id, uint32_t prop_id,
				uint32_t timeout_mSec)
{

	struct udev_monitor *uevent_monitor = NULL;
	struct udev *udev = NULL;
	int udev_fd, epoll_fd;
	struct epoll_event event, events[MAX_EVENTS];
	bool ret = false;

	udev_fd = hdcp_udev_init(uevent_monitor, udev);
	if (udev_fd < 0)
		return false;

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		igt_info("Failed to create epoll fd. %d\n", epoll_fd);
		goto out_ep_create;
	}

	event.events = EPOLLIN | EPOLLERR;
	event.data.fd = 0;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udev_fd, &event)) {
		igt_info("failed to fd into epoll\n");
		goto out_ep_ctl;
	}

	if (epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_mSec))
		ret = hdcp_event(uevent_monitor, udev, conn_id, prop_id);

out_ep_ctl:
	if (close(epoll_fd))
		igt_info("failed to close the epoll fd\n");
out_ep_create:
	hdcp_udev_fini(uevent_monitor, udev);
	return ret;
}

static bool
wait_for_prop_value(igt_output_t *output, uint64_t expected,
		    uint32_t timeout_mSec)
{
	uint64_t val;
	int i;

	if (data.cp_tests & CP_UEVENT && expected != CP_UNDESIRED) {
		igt_assert_f(wait_for_hdcp_event(output->id,
			     output->props[IGT_CONNECTOR_CONTENT_PROTECTION],
			     timeout_mSec), "uevent is not received");

		val = igt_output_get_prop(output,
					  IGT_CONNECTOR_CONTENT_PROTECTION);
		if (val == expected)
			return true;
	} else {
		for (i = 0; i < timeout_mSec; i++) {
			val = igt_output_get_prop(output,
						  IGT_CONNECTOR_CONTENT_PROTECTION);
			if (val == expected)
				return true;
			usleep(1000);
		}
	}

	igt_info("prop_value mismatch %" PRId64 " != %" PRId64 "\n",
		 val, expected);

	return false;
}

static void
commit_display_and_wait_for_flip(enum igt_commit_style s)
{
	int ret;
	uint32_t flag;

	if (s == COMMIT_ATOMIC) {
		flag = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_ALLOW_MODESET;
		igt_display_commit_atomic(&data.display, flag, NULL);

		ret = wait_flip_event();
		igt_assert_f(!ret, "wait_flip_event failed. %d\n", ret);
	} else {
		igt_display_commit2(&data.display, s);

		/* Wait for 50mSec */
		usleep(50 * 1000);
	}
}

static void modeset_with_fb(const enum pipe pipe, igt_output_t *output,
			    enum igt_commit_style s)
{
	igt_display_t *display = &data.display;
	drmModeModeInfo mode;
	igt_plane_t *primary;

	igt_assert(kmstest_get_connector_default_mode(
			display->drm_fd, output->config.connector, &mode));

	igt_output_override_mode(output, &mode);
	igt_output_set_pipe(output, pipe);

	igt_create_color_fb(display->drm_fd, mode.hdisplay, mode.vdisplay,
			    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			    1.f, 0.f, 0.f, &data.red);
	igt_create_color_fb(display->drm_fd, mode.hdisplay, mode.vdisplay,
			    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			    0.f, 1.f, 0.f, &data.green);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	igt_display_commit2(display, s);
	igt_plane_set_fb(primary, &data.red);

	/* Wait for Flip completion before starting the HDCP authentication */
	commit_display_and_wait_for_flip(s);
}

static bool test_cp_enable(igt_output_t *output, enum igt_commit_style s,
			   int content_type, bool type_change)
{
	igt_display_t *display = &data.display;
	igt_plane_t *primary;
	bool ret;

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	if (!type_change)
		igt_output_set_prop_value(output,
					  IGT_CONNECTOR_CONTENT_PROTECTION,
					  CP_DESIRED);

	if (output->props[IGT_CONNECTOR_HDCP_CONTENT_TYPE])
		igt_output_set_prop_value(output,
					  IGT_CONNECTOR_HDCP_CONTENT_TYPE,
					  content_type);
	igt_display_commit2(display, s);

	ret = wait_for_prop_value(output, CP_ENABLED,
				  KERNEL_AUTH_TIME_ALLOWED_MSEC);
	if (ret) {
		igt_plane_set_fb(primary, &data.green);
		igt_display_commit2(display, s);
	}

	return ret;
}

static void test_cp_disable(igt_output_t *output, enum igt_commit_style s)
{
	igt_display_t *display = &data.display;
	igt_plane_t *primary;
	bool ret;

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	/*
	 * Even on HDCP enable failed scenario, IGT should exit leaving the
	 * "content protection" at "UNDESIRED".
	 */
	igt_output_set_prop_value(output, IGT_CONNECTOR_CONTENT_PROTECTION,
				  CP_UNDESIRED);
	igt_plane_set_fb(primary, &data.red);
	igt_display_commit2(display, s);

	/* Wait for HDCP to be disabled, before crtc off */
	ret = wait_for_prop_value(output, CP_UNDESIRED,
				  KERNEL_DISABLE_TIME_ALLOWED_MSEC);
	igt_assert_f(ret, "Content Protection not cleared\n");
}

static void test_cp_enable_with_retry(igt_output_t *output,
				      enum igt_commit_style s, int retry,
				      int content_type, bool expect_failure,
				      bool type_change)
{
	int retry_orig = retry;
	bool ret;

	do {
		if (!type_change || retry_orig != retry)
			test_cp_disable(output, s);

		ret = test_cp_enable(output, s, content_type, type_change);

		if (!ret && --retry)
			igt_debug("Retry (%d/2) ...\n", 3 - retry);
	} while (retry && !ret);

	if (!ret)
		test_cp_disable(output, s);

	if (expect_failure)
		igt_assert_f(!ret,
			     "CP Enabled. Though it is expected to fail\n");
	else
		igt_assert_f(ret, "Content Protection not enabled\n");

}

static bool igt_pipe_is_free(igt_display_t *display, enum pipe pipe)
{
	int i;

	for (i = 0; i < display->n_outputs; i++)
		if (display->outputs[i].pending_pipe == pipe)
			return false;

	return true;
}

static void test_cp_lic(igt_output_t *output)
{
	bool ret;

	/* Wait for 4Secs (min 2 cycles of Link Integrity Check) */
	ret = wait_for_prop_value(output, CP_DESIRED, LIC_PERIOD_MSEC);
	igt_assert_f(!ret, "Content Protection LIC Failed\n");
}

static bool write_srm_as_fw(const __u8 *srm, int len)
{
	int fd, ret, total = 0;

	fd = open("/lib/firmware/display_hdcp_srm.bin",
		  O_WRONLY | O_CREAT, S_IRWXU);
	do {
		ret = write(fd, srm + total, len - total);
		if (ret < 0)
			ret = -errno;
		if (ret == -EINTR || ret == -EAGAIN)
			continue;
		if (ret <= 0)
			break;
		total += ret;
	} while (total != len);
	close(fd);

	return total < len ? false : true;
}

static void test_content_protection_on_output(igt_output_t *output,
					      enum igt_commit_style s,
					      int content_type)
{
	igt_display_t *display = &data.display;
	igt_plane_t *primary;
	enum pipe pipe;
	bool ret;

	for_each_pipe(display, pipe) {
		if (!igt_pipe_connector_valid(pipe, output))
			continue;

		/*
		 * If previous subtest of connector failed, pipe
		 * attached to that connector is not released.
		 * Because of that we have to choose the non
		 * attached pipe for this subtest.
		 */
		if (!igt_pipe_is_free(display, pipe))
			continue;

		modeset_with_fb(pipe, output, s);
		test_cp_enable_with_retry(output, s, 3, content_type, false,
					  false);

		if (data.cp_tests & CP_TYPE_CHANGE) {
			/* Type 1 -> Type 0 */
			test_cp_enable_with_retry(output, s, 3,
						  HDCP_CONTENT_TYPE_0, false,
						  true);
			/* Type 0 -> Type 1 */
			test_cp_enable_with_retry(output, s, 3,
						  content_type, false,
						  true);
		}

		if (data.cp_tests & CP_MEI_RELOAD) {
			igt_assert_f(!igt_kmod_unload("mei_hdcp", 0),
				     "mei_hdcp unload failed");

			/* Expected to fail */
			test_cp_enable_with_retry(output, s, 3,
						  content_type, true, false);

			igt_assert_f(!igt_kmod_load("mei_hdcp", NULL),
				     "mei_hdcp load failed");

			/* Expected to pass */
			test_cp_enable_with_retry(output, s, 3,
						  content_type, false, false);
		}

		if (data.cp_tests & CP_LIC)
			test_cp_lic(output);

		if (data.cp_tests & CP_DPMS) {
			igt_pipe_set_prop_value(display, pipe,
						IGT_CRTC_ACTIVE, 0);
			igt_display_commit2(display, s);

			igt_pipe_set_prop_value(display, pipe,
						IGT_CRTC_ACTIVE, 1);
			igt_display_commit2(display, s);

			ret = wait_for_prop_value(output, CP_ENABLED,
						  KERNEL_AUTH_TIME_ALLOWED_MSEC);
			if (!ret)
				test_cp_enable_with_retry(output, s, 2,
							  content_type, false,
							  false);
		}

		test_cp_disable(output, s);
		primary = igt_output_get_plane_type(output,
						    DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);

		/*
		 * Testing a output with a pipe is enough for HDCP
		 * testing. No ROI in testing the connector with other
		 * pipes. So Break the loop on pipe.
		 */
		break;
	}
}

static void __debugfs_read(int fd, const char *param, char *buf, int len)
{
	len = igt_debugfs_simple_read(fd, param, buf, len);
	if (len < 0)
		igt_assert_eq(len, -ENODEV);
}

#define debugfs_read(fd, p, arr) __debugfs_read(fd, p, arr, sizeof(arr))

#define MAX_SINK_HDCP_CAP_BUF_LEN	5000

static bool sink_hdcp_capable(igt_output_t *output)
{
	char buf[MAX_SINK_HDCP_CAP_BUF_LEN];
	int fd;

	fd = igt_debugfs_connector_dir(data.drm_fd, output->name, O_RDONLY);
	if (fd < 0)
		return false;

	debugfs_read(fd, "i915_hdcp_sink_capability", buf);
	close(fd);

	igt_debug("Sink capability: %s\n", buf);

	return strstr(buf, "HDCP1.4");
}

static bool sink_hdcp2_capable(igt_output_t *output)
{
	char buf[MAX_SINK_HDCP_CAP_BUF_LEN];
	int fd;

	fd = igt_debugfs_connector_dir(data.drm_fd, output->name, O_RDONLY);
	if (fd < 0)
		return false;

	debugfs_read(fd, "i915_hdcp_sink_capability", buf);
	close(fd);

	igt_debug("Sink capability: %s\n", buf);

	return strstr(buf, "HDCP2.2");
}

static void
test_content_protection(enum igt_commit_style s, int content_type)
{
	igt_display_t *display = &data.display;
	igt_output_t *output;
	int valid_tests = 0;

	if (data.cp_tests & CP_MEI_RELOAD)
		igt_require_f(igt_kmod_is_loaded("mei_hdcp"),
			      "mei_hdcp module is not loaded\n");

	for_each_connected_output(display, output) {
		if (!output->props[IGT_CONNECTOR_CONTENT_PROTECTION])
			continue;

		if (!output->props[IGT_CONNECTOR_HDCP_CONTENT_TYPE] &&
		    content_type)
			continue;

		igt_info("CP Test execution on %s\n", output->name);

		if (content_type && !sink_hdcp2_capable(output)) {
			igt_info("\tSkip %s (Sink has no HDCP2.2 support)\n",
				 output->name);
			continue;
		} else if (!sink_hdcp_capable(output)) {
			igt_info("\tSkip %s (Sink has no HDCP support)\n",
				 output->name);
			continue;
		}

		test_content_protection_on_output(output, s, content_type);
		valid_tests++;
	}

	igt_require_f(valid_tests, "No connector found with HDCP capability\n");
}

igt_main
{
	igt_fixture {
		igt_skip_on_simulation();

		data.drm_fd = drm_open_driver(DRIVER_ANY);

		igt_display_require(&data.display, data.drm_fd);
	}

	igt_subtest("legacy") {
		data.cp_tests = 0;
		test_content_protection(COMMIT_LEGACY, HDCP_CONTENT_TYPE_0);
	}

	igt_subtest("atomic") {
		igt_require(data.display.is_atomic);
		data.cp_tests = 0;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_0);
	}

	igt_subtest("atomic-dpms") {
		igt_require(data.display.is_atomic);
		data.cp_tests = CP_DPMS;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_0);
	}

	igt_subtest("LIC") {
		igt_require(data.display.is_atomic);
		data.cp_tests = CP_LIC;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_0);
	}

	igt_subtest("type1") {
		igt_require(data.display.is_atomic);
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_1);
	}

	igt_subtest("mei_interface") {
		igt_require(data.display.is_atomic);
		data.cp_tests = CP_MEI_RELOAD;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_1);
	}

	igt_subtest("content_type_change") {
		igt_require(data.display.is_atomic);
		data.cp_tests = CP_TYPE_CHANGE;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_1);
	}

	igt_subtest("uevent") {
		igt_require(data.display.is_atomic);
		data.cp_tests = CP_UEVENT;
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_0);
	}

	/*
	 *  Testing the revocation check through SRM needs a HDCP sink with
	 *  programmable Ksvs or we need a uAPI from kernel to read the
	 *  connected HDCP sink's Ksv. With that we would be able to add that
	 *  Ksv into a SRM and send in for revocation check. Since we dont have
	 *  either of these options, we test SRM writing from userspace and
	 *  validation of the same at kernel. Something is better than nothing.
	 */
	igt_subtest("srm") {
		bool ret;

		igt_require(data.display.is_atomic);
		data.cp_tests = 0;
		ret = write_srm_as_fw((const __u8 *)facsimile_srm,
				      sizeof(facsimile_srm));
		igt_assert_f(ret, "SRM update failed");
		test_content_protection(COMMIT_ATOMIC, HDCP_CONTENT_TYPE_0);
	}

	igt_fixture
		igt_display_fini(&data.display);
}
