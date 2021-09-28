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
 *
 * Displayport Compliance Testing Application
 *
 * This is the userspace component of the Displayport Compliance testing
 * software required for compliance testing of the I915 Display Port driver.
 * This must be running in order to successfully complete Display Port
 * compliance testing. This app and the kernel code that accompanies it has been
 * written to satify the requirements of the Displayport Link CTS 1.2 rev1.1
 * specification from VESA. Note that this application does not support eDP
 * compliance testing.
 *
 * Compliance Testing requires several components:
 *   - A kernel build that contains the patch set for DP compliance support
 *   - A Displayport Compliance Testing appliance such as Unigraf-DPR120
 *   - This user application
 *   - A windows host machine to run the DPR test software
 *   - Root access on the DUT due to the use of sysfs utility
 *
 * Test Setup:
 * It is strongly recommended that the windows host, test appliance and DUT
 * be freshly restarted before any testing begins to ensure that any previous
 * configurations and settings will not interfere with test process. Refer to
 * the test appliance documentation for setup, software installation and
 * operation specific to that device.
 *
 * The Linux DUT must be in text (console) mode and cannot have any other
 * display manager running. You must be logged in as root to run this user app.
 * Once the user application is up and running, waiting for test requests, the
 * software on the windows host can now be used to execute the compliance tests.
 *
 * This userspace application supports following tests from the DP CTS Spec
 * Rev 1.1:
 *   - Link Training Tests: Supports tests 4.3.1.1 to 4.3.2.3
 *   - EDID Tests: Supports EDID read (4.2.2.3),EDID Read failure and corruption
 *     detection tests (4.2.2.4, 4.2.2.5, 4.2.2.6)
 *   - Video Pattern generation tests: This supports only the 24 and 18bpp color
 *     ramp test pattern (4.3.3.1).
 *
 * Connections (required):
 *   - Test Appliance connected to the external Displayport connector on the DUT
 *   - Test Appliance Monitor Out connected to Displayport connector on the
 * monitor
 *   - Test appliance connected to the Windows Host via USB
 *
 * Debugfs Files:
 * The file root for all  the debugfs file:
 * /sys/kernel/debug/dri/0/
 *
 * The specific files are as follows:
 *
 * i915_dp_test_active
 * A simple flag that indicates whether or not compliance testing is currently
 * active in the kernel. This flag is polled by userspace and once set, invokes
 * the test handler in the user app. This flag is set by the test handler in the
 * kernel after reading the registers requested by the test appliance.
 *
 * i915_dp_test_data
 * Test data is used by the kernel to pass parameters to the user app. Eg: In
 * case of EDID tests, the data that is delivered to the userspace is the video
 * mode to be set for the test.
 * In case of video pattern test, the data that is delivered to the userspace is
 * the width and height of the test pattern and the bits per color value.
 *
 * i915_dp_test_type
 * The test type variable instructs the user app as to what the requested test
 * was from the sink device. These values defined at the top of the application's
 * main implementation file must be kept in sync with the values defined in the
 * kernel's drm_dp_helper.h file.
 * This app is based on some prior work submitted in April 2015 by Todd Previte
 * (<tprevite@gmail.com>)
 *
 *
 * This tool can be run as:
 * ./intel_dp_compliance  It will wait till you start compliance suite from
 * DPR 120.
 * ./intel_dp_compliance -h  This will open the help
 * ./intel_dp_compliance -i  This will provide information about current
 * connectors/CRTCs. This can be used for debugging purpose.
 *
 * Authors:
 *    Manasi Navare <manasi.d.navare@intel.com>
 *
 * Elements of the modeset code adapted from David Herrmann's
 * DRM modeset example
 *
 */
#include "igt.h"
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "intel_dp_compliance.h"

#include <stdlib.h>
#include <signal.h>

/* User Input definitions */
#define HELP_DESCRIPTION 1

/* Debugfs file definitions */
#define INTEL_DP_TEST_TYPE_FILE		"i915_dp_test_type"
#define INTEL_DP_TEST_ACTIVE_FILE	"i915_dp_test_active"
#define INTEL_DP_TEST_DATA_FILE		"i915_dp_test_data"

/* DRM definitions - must be kept in sync with the DRM header */
#define DP_TEST_LINK_TRAINING		(1 << 0)
#define DP_TEST_LINK_VIDEO_PATTERN	(1 << 1)
#define DP_TEST_LINK_EDID_READ		(1 << 2)
#define DP_TEST_LINK_PHY_TEST_PATTERN	(1 << 3) /* DPCD >= 1.1 */

#define DP_COMPLIANCE_TEST_TYPE_MASK	(DP_TEST_LINK_TRAINING      |	\
					 DP_TEST_LINK_VIDEO_PATTERN |	\
					 DP_TEST_LINK_EDID_READ     |	\
					 DP_TEST_LINK_PHY_TEST_PATTERN)

/* NOTE: These must be kept in sync with the definitions in intel_dp.c */
#define INTEL_DP_EDID_SHIFT_MASK	0
#define INTEL_DP_EDID_OK		(0 << INTEL_DP_EDID_SHIFT_MASK)
#define INTEL_DP_EDID_CORRUPT		(1 << INTEL_DP_EDID_SHIFT_MASK)
#define INTEL_DP_RESOLUTION_SHIFT_MASK	0
#define INTEL_DP_RESOLUTION_PREFERRED	(1 << INTEL_DP_RESOLUTION_SHIFT_MASK)
#define INTEL_DP_RESOLUTION_STANDARD	(2 << INTEL_DP_RESOLUTION_SHIFT_MASK)
#define INTEL_DP_RESOLUTION_FAILSAFE	(3 << INTEL_DP_RESOLUTION_SHIFT_MASK)
#define DP_COMPLIANCE_VIDEO_MODE_MASK	(INTEL_DP_RESOLUTION_PREFERRED	|\
					 INTEL_DP_RESOLUTION_STANDARD	|\
					 INTEL_DP_RESOLUTION_FAILSAFE)

/* Global file pointers for the sysfs files */
FILE *test_active_fp, *test_data_fp, *test_type_fp;

bool video_pattern_flag;

/* Video pattern test globals */
uint16_t hdisplay;
uint16_t vdisplay;
uint8_t bitdepth;

static int tio_fd;
struct termios saved_tio;

drmModeRes *resources;
int drm_fd, modes, gen;
uint64_t tiling = LOCAL_DRM_FORMAT_MOD_NONE;
uint32_t depth = 24, stride, bpp;
int specified_mode_num = -1, specified_disp_id = -1;
int width, height;
uint32_t test_crtc;
uint32_t test_connector_id;
enum {
	INTEL_MODE_INVALID = -1,
	INTEL_MODE_NONE = 0,
	INTEL_MODE_PREFERRED,
	INTEL_MODE_STANDARD,
	INTEL_MODE_FAILSAFE,
	INTEL_MODE_VIDEO_PATTERN_TEST
} intel_display_mode;

struct test_video_pattern {
	uint16_t hdisplay;
	uint16_t vdisplay;
	uint8_t bitdepth;
	uint32_t fb;
	uint32_t size;
	struct igt_fb fb_pattern;
	drmModeModeInfo mode;
	uint32_t *pixmap;
};

struct connector {
	uint32_t id;
	int mode_valid;
	drmModeModeInfo mode, mode_standard, mode_preferred, mode_failsafe;
	drmModeConnector *connector;
	int crtc;
	/* Standard and preferred frame buffer*/
	uint32_t fb, fb_width, fb_height, fb_size;
	uint8_t *pixmap;
	struct igt_fb fb_video_pattern;
	/* Failsafe framebuffer - note this is a 16-bit buffer */
	uint32_t failsafe_fb, failsafe_width, failsafe_height;
	uint32_t failsafe_size;
	uint8_t *failsafe_pixmap;
	struct igt_fb fb_failsafe_pattern;
	struct test_video_pattern test_pattern;
};

static void clear_test_active(void)
{
	rewind(test_active_fp);
	fprintf(test_active_fp, "%d", 0);
	fflush(test_active_fp);
}

static FILE *fopenat(int dir, const char *name, const char *mode)
{
	int fd = openat(dir, name, O_RDWR);
	return fdopen(fd, mode);
}

static void setup_debugfs_files(void)
{
	int dir = igt_debugfs_dir(drm_fd);

	test_type_fp = fopenat(dir, INTEL_DP_TEST_TYPE_FILE, "r");
	igt_require(test_type_fp);

	test_data_fp = fopenat(dir, INTEL_DP_TEST_DATA_FILE, "r");
	igt_require(test_data_fp);

	test_active_fp = fopenat(dir, INTEL_DP_TEST_ACTIVE_FILE, "w+");
	igt_require(test_active_fp);

	close(dir);

	/* Reset the active flag for safety */
	clear_test_active();
}

static unsigned long get_test_type(void)
{
	unsigned long test_type;
	int ret;

	if (!test_type_fp)
		fprintf(stderr, "Invalid test_type file\n");
	rewind(test_type_fp);
	ret = fscanf(test_type_fp, "%lx", &test_type);
	if (ret < 1 || test_type <= 0) {
		igt_warn("test_type read failed - %lx\n", test_type);
		return 0;
	}

	return test_type;
}

static unsigned long get_test_edid_data(void)
{
	unsigned long test_data;
	int ret;

	if (!test_data_fp)
		fprintf(stderr, "Invalid test_data file\r\n");

	rewind(test_data_fp);
	ret = fscanf(test_data_fp, "%lx", &test_data);
	if (ret < 1 || test_data <= 0) {
		igt_warn("test_data read failed - %lx\r\n", test_data);
		return 0;
	}

	return test_data;
}

static void get_test_videopattern_data(void)
{
	int count = 0;
	uint16_t video_pattern_value[3];
	char video_pattern_attribute[15];
	int ret;

	if (!test_data_fp)
		fprintf(stderr, "Invalid test_data file\n");

	rewind(test_data_fp);
	while (!feof(test_data_fp) && count < 3) {
		ret = fscanf(test_data_fp, "%s %u\n", video_pattern_attribute,
		       (unsigned int *)&video_pattern_value[count++]);
		if (ret < 2) {
			igt_warn("test_data read failed\n");
			return;
		}
	}

	hdisplay = video_pattern_value[0];
	vdisplay = video_pattern_value[1];
	bitdepth = video_pattern_value[2];
	igt_info("Hdisplay = %d\n", hdisplay);
	igt_info("Vdisplay = %d\n", vdisplay);
	igt_info("BitDepth = %u\n", bitdepth);

}

static int process_test_request(int test_type)
{
	int mode;
	unsigned long test_data_edid;
	bool valid = false;
	switch (test_type) {
	case DP_TEST_LINK_VIDEO_PATTERN:
		video_pattern_flag = true;
		get_test_videopattern_data();
		mode = INTEL_MODE_VIDEO_PATTERN_TEST;
		valid = true;
		break;
	case DP_TEST_LINK_EDID_READ:
		test_data_edid = get_test_edid_data();
		mode = (test_data_edid & DP_COMPLIANCE_VIDEO_MODE_MASK) >>
			INTEL_DP_RESOLUTION_SHIFT_MASK;
		valid = true;
		break;
	default:
		/* Unknown test type */
		fprintf(stderr, "Invalid test request, ignored.\n");
		break;
	}

	if (valid)
		return update_display(mode, true);

	return -1;
}

static void dump_connectors_fd(int drmfd)
{
	int i, j;

	drmModeRes *mode_resources = drmModeGetResources(drmfd);

	if (!mode_resources) {
		igt_warn("drmModeGetResources failed: %s\n", strerror(errno));
		return;
	}

	igt_info("Connectors:\n");
	igt_info("id\tencoder\tstatus\t\ttype\tsize (mm)\tmodes\n");
	for (i = 0; i < mode_resources->count_connectors; i++) {
		drmModeConnector *connector;

		connector = drmModeGetConnectorCurrent(drmfd,
						       mode_resources->connectors[i]);
		if (!connector) {
			igt_warn("Could not get connector %i: %s\n",
				 mode_resources->connectors[i], strerror(errno));
			continue;
		}

		igt_info("%d\t%d\t%s\t%s\t%dx%d\t\t%d\n",
			 connector->connector_id,
			 connector->encoder_id,
			 kmstest_connector_status_str(connector->connection),
			 kmstest_connector_type_str(connector->connector_type),
			 connector->mmWidth,
			 connector->mmHeight,
			 connector->count_modes);

		if (!connector->count_modes)
			continue;

		igt_info("  Modes:\n");
		igt_info("  name refresh (Hz) hdisp hss hse htot vdisp ""vss vse vtot flags type clock\n");
		for (j = 0; j < connector->count_modes; j++) {
			igt_info("[%d]", j);
			kmstest_dump_mode(&connector->modes[j]);
		}

		drmModeFreeConnector(connector);
	}
	igt_info("\n");

	drmModeFreeResources(mode_resources);
}

static void dump_crtcs_fd(int drmfd)
{
	int i;
	drmModeRes *mode_resources;

	mode_resources = drmModeGetResources(drmfd);
	if (!mode_resources) {
		igt_warn("drmModeGetResources failed: %s\n", strerror(errno));
		return;
	}

	igt_info("CRTCs:\n");
	igt_info("id\tfb\tpos\tsize\n");
	for (i = 0; i < mode_resources->count_crtcs; i++) {
		drmModeCrtc *crtc;

		crtc = drmModeGetCrtc(drmfd, mode_resources->crtcs[i]);
		if (!crtc) {
			igt_warn("Could not get crtc %i: %s\n", mode_resources->crtcs[i], strerror(errno));
			continue;
		}
		igt_info("%d\t%d\t(%d,%d)\t(%dx%d)\n",
			 crtc->crtc_id,
			 crtc->buffer_id,
			 crtc->x,
			 crtc->y,
			 crtc->width,
			 crtc->height);

		kmstest_dump_mode(&crtc->mode);

		drmModeFreeCrtc(crtc);
	}
	igt_info("\n");

	drmModeFreeResources(mode_resources);
}

static void dump_info(void)
{
	dump_connectors_fd(drm_fd);
	dump_crtcs_fd(drm_fd);
}

static int setup_framebuffers(struct connector *dp_conn)
{

	dp_conn->fb = igt_create_fb(drm_fd,
				    dp_conn->fb_width, dp_conn->fb_height,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    &dp_conn->fb_video_pattern);
	igt_assert(dp_conn->fb);

	/* Map the mapping of GEM object into the virtual address space */
	dp_conn->pixmap = gem_mmap__gtt(drm_fd,
					dp_conn->fb_video_pattern.gem_handle,
					dp_conn->fb_video_pattern.size,
					PROT_READ | PROT_WRITE);
	if (dp_conn->pixmap == NULL)
		return -1;

	gem_set_domain(drm_fd, dp_conn->fb_video_pattern.gem_handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	dp_conn->fb_size = dp_conn->fb_video_pattern.size;

	/* After filling the device memory with 0s it needs to be unmapped */
	memset(dp_conn->pixmap, 0, dp_conn->fb_size);
	munmap(dp_conn->pixmap, dp_conn->fb_size);

	return 0;
}

static int setup_failsafe_framebuffer(struct connector *dp_conn)
{

	dp_conn->failsafe_fb = igt_create_fb(drm_fd,
					     dp_conn->failsafe_width,
					     dp_conn->failsafe_height,
					     DRM_FORMAT_XRGB8888,
					     LOCAL_DRM_FORMAT_MOD_NONE,
					     &dp_conn->fb_failsafe_pattern);
	igt_assert(dp_conn->failsafe_fb);

	/* Map the mapping of GEM object into the virtual address space */
	dp_conn->failsafe_pixmap = gem_mmap__gtt(drm_fd,
						 dp_conn->fb_failsafe_pattern.gem_handle,
						 dp_conn->fb_failsafe_pattern.size,
						 PROT_READ | PROT_WRITE);
	if (dp_conn->failsafe_pixmap == NULL)
		return -1;

	gem_set_domain(drm_fd, dp_conn->fb_failsafe_pattern.gem_handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	dp_conn->failsafe_size = dp_conn->fb_failsafe_pattern.size;

	/* After filling the device framebuffer the mapped memory needs to be freed */
	memset(dp_conn->failsafe_pixmap, 0, dp_conn->failsafe_size);
	munmap(dp_conn->failsafe_pixmap, dp_conn->failsafe_size);

	return 0;

}

static int setup_video_pattern_framebuffer(struct connector *dp_conn)
{
	uint32_t  video_width, video_height;

	video_width = dp_conn->test_pattern.hdisplay;
	video_height = dp_conn->test_pattern.vdisplay;
	/*
	 * Display WA1172: Gen10 To pass the color data unaffected set either
	 * per-pixel alpha or Plane alpha to 0xff. Use ARGB8888 and set alpha to 0xff.
	 */
	dp_conn->test_pattern.fb = igt_create_fb(drm_fd,
						 video_width, video_height,
						 gen == 10 ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888,
						 LOCAL_DRM_FORMAT_MOD_NONE,
						 &dp_conn->test_pattern.fb_pattern);
	igt_assert(dp_conn->test_pattern.fb);

	/* Map the mapping of GEM object into the virtual address space */
	dp_conn->test_pattern.pixmap = gem_mmap__gtt(drm_fd,
						     dp_conn->test_pattern.fb_pattern.gem_handle,
						     dp_conn->test_pattern.fb_pattern.size,
						     PROT_READ | PROT_WRITE);
	if (dp_conn->test_pattern.pixmap == NULL)
		return -1;

	gem_set_domain(drm_fd, dp_conn->test_pattern.fb_pattern.gem_handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	dp_conn->test_pattern.size = dp_conn->test_pattern.fb_pattern.size;

	memset(dp_conn->test_pattern.pixmap, 0, dp_conn->test_pattern.size);
	return 0;

}

static int fill_framebuffer(struct connector *dp_conn)
{
	uint32_t tile_height, tile_width, video_width, video_height;
	uint32_t *red_ptr, *green_ptr, *blue_ptr, *white_ptr, *src_ptr, *dst_ptr;
	int x, y;
	int32_t pixel_val;
	uint8_t alpha;

	video_width = dp_conn->test_pattern.hdisplay;
	video_height = dp_conn->test_pattern.vdisplay;

	tile_height = 64;
	tile_width = 1 <<  (dp_conn->test_pattern.bitdepth);

	red_ptr = dp_conn->test_pattern.pixmap;
	green_ptr = red_ptr + (video_width * tile_height);
	blue_ptr = green_ptr + (video_width * tile_height);
	white_ptr = blue_ptr + (video_width * tile_height);
	x = 0;

	/* Fill the frame buffer with video pattern from CTS 3.1.5 */
	while (x < video_width) {
		for (pixel_val = 0; pixel_val < 256;
		     pixel_val = pixel_val + (256 / tile_width)) {
			alpha = gen == 10 ? 0xff : 0;
			red_ptr[x] = alpha << 24 | pixel_val << 16;
			green_ptr[x] = alpha << 24 | pixel_val << 8;
			blue_ptr[x] = alpha << 24 | pixel_val << 0;
			white_ptr[x] = alpha << 24 | red_ptr[x] | green_ptr[x] |
				       blue_ptr[x];
			if (++x >= video_width)
				break;
		}
	}
	for (y = 0; y < video_height; y++) {
		if (y == 0 || y == 64 || y == 128 || y == 192)
			continue;
		switch ((y / tile_height) % 4) {
		case 0:
			src_ptr = red_ptr;
			break;
		case 1:
			src_ptr = green_ptr;
			break;
		case 2:
			src_ptr = blue_ptr;
			break;
		case 3:
			src_ptr = white_ptr;
			break;
		}
		dst_ptr = dp_conn->test_pattern.pixmap + (y * video_width);
		memcpy(dst_ptr, src_ptr, (video_width * 4));
	}
	munmap(dp_conn->test_pattern.pixmap,
	       dp_conn->test_pattern.size);
	return 0;
}

static int set_test_mode(struct connector *dp_conn)
{
	int ret = 0;
	int i;
	bool found_std = false, found_fs = false;
	drmModeConnector *c = dp_conn->connector;

	/* Ignore any disconnected devices */
	if (c->connection != DRM_MODE_CONNECTED) {
		igt_warn("Connector %u disconnected\n", c->connector_id);
		return -ENOENT;
	}
	igt_info("Connector setup:\n");
	/* Setup preferred mode - should be mode[0] in the list */
	dp_conn->mode_preferred = c->modes[0];
	dp_conn->fb_width = c->modes[0].hdisplay;
	dp_conn->fb_height = c->modes[0].vdisplay;

	dp_conn->test_pattern.mode = c->modes[0];
	dp_conn->test_pattern.mode.hdisplay = c->modes[0].hdisplay;
	dp_conn->test_pattern.mode.vdisplay = c->modes[0].vdisplay;

	igt_info("Preferred mode (mode 0) for connector %u is %ux%u\n",
		 dp_conn->id, c->modes[0].hdisplay, c->modes[0].vdisplay);
	fflush(stdin);

	for (i = 1; i < c->count_modes; i++) {
		/* Standard mode is 800x600@60 */
		if (c->modes[i].hdisplay == 800 &&
		    c->modes[i].vdisplay == 600 &&
		    c->modes[i].vrefresh == 60 &&
		    found_std == false) {
			dp_conn->mode_standard = c->modes[i];
			igt_info("Standard mode (%d) for connector %u is %ux%u\n",
				 i,
				 c->connector_id,
				 c->modes[i].hdisplay,
				 c->modes[i].vdisplay);
			found_std = true;
		}
		/* Failsafe mode is 640x480@60 */
		if (c->modes[i].hdisplay == 640 &&
		    c->modes[i].vdisplay == 480 &&
		    c->modes[i].vrefresh == 60 &&
		    found_fs == false) {
			dp_conn->mode_failsafe = c->modes[i];
			dp_conn->failsafe_width = c->modes[i].hdisplay;
			dp_conn->failsafe_height = c->modes[i].vdisplay;
			igt_info("Failsafe mode (%d) for connector %u is %ux%u\n",
				 i,
				 c->connector_id,
				 c->modes[i].hdisplay,
				 c->modes[i].vdisplay);
			found_fs = true;
		}
	}

	ret = setup_framebuffers(dp_conn);
	if (ret) {
		igt_warn("Creating framebuffer for connector %u failed (%d)\n",
			 c->connector_id, ret);
		return ret;
	}

	if (found_fs) {
		ret = setup_failsafe_framebuffer(dp_conn);
		if (ret) {
			igt_warn("Creating failsafe framebuffer for connector %u failed (%d)\n",
				 c->connector_id, ret);
			return ret;
		}
	}

	if (video_pattern_flag) {
		dp_conn->test_pattern.hdisplay = hdisplay;
		dp_conn->test_pattern.vdisplay = vdisplay;
		dp_conn->test_pattern.bitdepth = bitdepth;

		ret = setup_video_pattern_framebuffer(dp_conn);
		if (ret) {
			igt_warn("Creating framebuffer for connector %u failed (%d)\n",
				 c->connector_id, ret);
			return ret;
		}

		ret = fill_framebuffer(dp_conn);
		if (ret) {
			igt_warn("Filling framebuffer for connector %u failed (%d)\n",
				 c->connector_id, ret);
			return ret;
		}
	}

	return ret;
}

static int set_video(int mode, struct connector *test_connector)
{
	drmModeModeInfo *requested_mode;
	uint32_t required_fb_id;
	struct igt_fb required_fb;
	int ret = 0;

	switch (mode) {
	case INTEL_MODE_NONE:
		igt_info("NONE\n");
		ret = drmModeSetCrtc(drm_fd, test_connector->crtc,
				     -1, 0, 0, NULL, 0, NULL);
		goto out;
	case INTEL_MODE_PREFERRED:
		igt_info("PREFERRED\n");
		requested_mode =  &test_connector->mode_preferred;
		required_fb_id = test_connector->fb;
		required_fb = test_connector->fb_video_pattern;
		break;
	case INTEL_MODE_STANDARD:
		igt_info("STANDARD\n");
		requested_mode =  &test_connector->mode_standard;
		required_fb_id = test_connector->fb;
		required_fb = test_connector->fb_video_pattern;
		break;
	case INTEL_MODE_FAILSAFE:
		igt_info("FAILSAFE\n");
		requested_mode =  &test_connector->mode_failsafe;
		required_fb_id = test_connector->failsafe_fb;
		required_fb = test_connector->fb_failsafe_pattern;
		break;
	case INTEL_MODE_VIDEO_PATTERN_TEST:
		igt_info("VIDEO PATTERN TEST\n");
		requested_mode = &test_connector->test_pattern.mode;
		required_fb_id = test_connector->test_pattern.fb;
		required_fb = test_connector->test_pattern.fb_pattern;
		break;
	case INTEL_MODE_INVALID:
	default:
		igt_warn("INVALID! (%08x) Mode set aborted!\n", mode);
		return -1;
	}

	igt_info("CRTC(%u):", test_connector->crtc);
	kmstest_dump_mode(requested_mode);
	ret = drmModeSetCrtc(drm_fd, test_connector->crtc, required_fb_id, 0, 0,
			     &test_connector->id, 1, requested_mode);
	if (ret) {
		igt_warn("Failed to set mode (%dx%d@%dHz): %s\n",
			 requested_mode->hdisplay, requested_mode->vdisplay,
			 requested_mode->vrefresh, strerror(errno));
		igt_remove_fb(drm_fd, &required_fb);

	}
	/* Keep the pattern on output lines for 1 sec for DPR-120 to detect it */
	sleep(1);

out:
	if (ret) {
		igt_warn("Failed to set CRTC for connector %u\n",
			 test_connector->id);
	}

	return ret;
}

static int
set_default_mode(struct connector *c, bool set_mode)
{
	unsigned int fb_id = 0;
	struct igt_fb fb_info;
	int ret = 0;

	if (!set_mode) {
		ret = drmModeSetCrtc(drm_fd, c->crtc, 0, 0, 0,
				     NULL, 0, NULL);
		if (ret)
			igt_warn("Failed to unset mode");
		return ret;
	}

	c->mode = c->connector->modes[0];

	width = c->mode.hdisplay;
	height = c->mode.vdisplay;

	fb_id = igt_create_pattern_fb(drm_fd, width, height,
				      DRM_FORMAT_XRGB8888,
				      tiling, &fb_info);

	igt_info("CRTC(%u):[%d]", c->crtc, 0);
	kmstest_dump_mode(&c->mode);
	drmModeSetCrtc(drm_fd, c->crtc, -1, 0, 0, NULL, 0, NULL);
	ret = drmModeSetCrtc(drm_fd, c->crtc, fb_id, 0, 0,
			     &c->id, 1, &c->mode);
	if (ret) {
		igt_warn("Failed to set mode (%dx%d@%dHz): %s\n",
			 width, height, c->mode.vrefresh, strerror(errno));
		igt_remove_fb(drm_fd, &fb_info);

	}

	return ret;
}

static uint32_t find_crtc_for_connector(drmModeConnector *c)
{
	drmModeEncoder *e;
	uint32_t possible_crtcs;
	int i, j;

	for (i = 0; i < c->count_encoders; i++) {
		e = drmModeGetEncoder(drm_fd, c->encoders[i]);
		possible_crtcs = e->possible_crtcs;
		drmModeFreeEncoder(e);

		for (j = 0; possible_crtcs >> j; j++)
			if (possible_crtcs & (1 << j))
				return resources->crtcs[j];
	}
	return 0;
}

/*
 * Re-probe connectors and do a modeset based on test request or
 * in case of a hotplug uevent.
 *
 * @mode: Video mode requested by the test
 * @is_compliance_test: 1: If it is a compliance test
 *                      0: If it is a hotplug event
 *
 * Returns:
 * 0: On Success
 * -1: On failure
 */
int update_display(int mode, bool is_compliance_test)
{
	struct connector *connectors, *conn;
	int cnt, ret = 0;
	bool set_mode;

	resources = drmModeGetResources(drm_fd);
	if (!resources) {
		igt_warn("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}

	connectors = calloc(resources->count_connectors,
			    sizeof(struct connector));
	if (!connectors)
		return -1;

	/* Find any connected displays */
	for (cnt = 0; cnt < resources->count_connectors; cnt++) {
		drmModeConnector *c;

		conn = &connectors[cnt];
		conn->id = resources->connectors[cnt];
		c = drmModeGetConnector(drm_fd, conn->id);
		if (c->connector_type == DRM_MODE_CONNECTOR_DisplayPort &&
		    c->connection == DRM_MODE_CONNECTED) {
			test_connector_id = c->connector_id;
			conn->connector = c;
			conn->crtc = find_crtc_for_connector(c);
			test_crtc = conn->crtc;
			set_mode = true;
			break;
		} else if (c->connector_id == test_connector_id &&
			   c->connection == DRM_MODE_DISCONNECTED) {
			conn->connector = c;
			conn->crtc = test_crtc;
			set_mode = false;
			break;
		}
	}

	if (cnt == resources->count_connectors) {
		ret = -1;
		goto err;
	}

	if (is_compliance_test) {
		set_test_mode(conn);
		ret = set_video(INTEL_MODE_NONE, conn);
		ret = set_video(mode, conn);

	} else
		ret = set_default_mode(conn, set_mode);

 err:
	drmModeFreeConnector(conn->connector);
	/* Error condition if no connected displays */
	free(connectors);
	drmModeFreeResources(resources);
	return ret;
}

static const char optstr[] = "hi";

static void __attribute__((noreturn)) usage(char *name, char opt)
{
	igt_info("usage: %s [-hi]\n", name);
	igt_info("\t-i\tdump info\n");
	igt_info("\tDefault is to respond to DPR-120 tests\n");
	exit((opt != 'h') ? -1 : 0);
}

static void cleanup_debugfs(void)
{
	fclose(test_active_fp);
	fclose(test_data_fp);
	fclose(test_type_fp);
}

static void __attribute__((noreturn)) cleanup_and_exit(int ret)
{
	cleanup_debugfs();
	close(drm_fd);
	igt_info("Compliance testing application exiting\n");
	exit(ret);
}

static void cleanup_test(void)
{
	video_pattern_flag = false;
	hdisplay = 0;
	vdisplay = 0;
	bitdepth = 0;
}

static void read_test_request(void)
{
	unsigned long test_type;

	test_type = get_test_type();
	process_test_request(test_type);
	cleanup_test();
	clear_test_active();
}

static gboolean test_handler(GIOChannel *source, GIOCondition condition,
			     gpointer data)
{
	unsigned long test_active;
	int ret;

	rewind(test_active_fp);

	ret = fscanf(test_active_fp, "%lx", &test_active);
	if (ret < 1)
		return FALSE;

	if (test_active)
		read_test_request();
	return TRUE;
}

static gboolean input_event(GIOChannel *source, GIOCondition condition,
				gpointer data)
{
	gchar buf[2];
	gsize count;

	count = read(g_io_channel_unix_get_fd(source), buf, sizeof(buf));
	if (buf[0] == 'q' && (count == 1 || buf[1] == '\n')) {
		cleanup_and_exit(0);
	}

	return TRUE;
}

static void enter_exec_path(char **argv)
{
	char *exec_path = NULL;
	char *pos = NULL;
	short len_path = 0;
	int ret;

	len_path = strlen(argv[0]);
	exec_path = (char *) malloc(len_path);

	memcpy(exec_path, argv[0], len_path);
	pos = strrchr(exec_path, '/');
	if (pos != NULL)
		*(pos+1) = '\0';

	ret = chdir(exec_path);
	igt_assert_eq(ret, 0);
	free(exec_path);
}

static void restore_termio_mode(int sig)
{
	tcsetattr(tio_fd, TCSANOW, &saved_tio);
	close(tio_fd);
}

static void set_termio_mode(void)
{
	struct termios tio;

	/* don't attempt to set terminal attributes if not in the foreground
	 * process group
	 */
	if (getpgrp() != tcgetpgrp(STDOUT_FILENO))
		return;

	tio_fd = dup(STDIN_FILENO);
	tcgetattr(tio_fd, &saved_tio);
	igt_install_exit_handler(restore_termio_mode);
	tio = saved_tio;
	tio.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(tio_fd, TCSANOW, &tio);
}

int main(int argc, char **argv)
{
	int c;
	int ret = 0;
	GIOChannel *stdinchannel, *testactive_channel;
	GMainLoop *mainloop;
	bool opt_dump_info = false;
	struct option long_opts[] = {
		{"help-description", 0, 0, HELP_DESCRIPTION},
		{"help", 0, 0, 'h'},
	};

	igt_skip_on_simulation();

	enter_exec_path(argv);

	while ((c = getopt_long(argc, argv, optstr, long_opts, NULL)) != -1) {
		switch (c) {
		case 'i':
			opt_dump_info = true;
			break;
		case HELP_DESCRIPTION:
			igt_info("DP Compliance Test Suite using DPR-120\n");
			igt_info("EDID tests\n");
			igt_info("Video Pattern Generation tests\n");
			exit(0);
			break;
		default:
			/* fall through */
		case 'h':
			usage(argv[0], c);
			break;
		}
	}

	set_termio_mode();

	drm_fd = drm_open_driver(DRIVER_ANY);
	gen = intel_gen(intel_get_drm_devid(drm_fd));

	kmstest_set_vt_graphics_mode();
	setup_debugfs_files();
	cleanup_test();

	if (opt_dump_info) {
		dump_info();
		goto out_close;
	}

	/* Get the DP connector ID and CRTC */
	if (update_display(0, false)) {
		igt_warn("Failed to set default mode\n");
		ret = -1;
		goto out_close;
	}

	mainloop = g_main_loop_new(NULL, FALSE);
	if (!mainloop) {
		igt_warn("Failed to create GMainLoop\n");
		ret = -1;
		goto out_close;
	}

	if (!intel_dp_compliance_setup_hotplug()) {
		igt_warn("Failed to initialize hotplug support\n");
		goto out_mainloop;
	}

	testactive_channel = g_io_channel_unix_new(fileno(test_active_fp));
	if (!testactive_channel) {
		igt_warn("Failed to create test_active GIOChannel\n");
		goto out_close;
	}

	ret = g_io_add_watch(testactive_channel, G_IO_IN | G_IO_ERR,
			     test_handler, NULL);
	if (ret < 0) {
		igt_warn("Failed to add watch on udev GIOChannel\n");
		goto out_close;
	}

	stdinchannel = g_io_channel_unix_new(0);
	if (!stdinchannel) {
		igt_warn("Failed to create stdin GIOChannel\n");
		goto out_hotplug;
	}

	ret = g_io_add_watch(stdinchannel, G_IO_IN | G_IO_ERR, input_event,
			     NULL);
	if (ret < 0) {
		igt_warn("Failed to add watch on stdin GIOChannel\n");
		goto out_stdio;
	}

	ret = 0;

	igt_info("*************DP Compliance Testing using DPR-120*************\n");
	igt_info("Waiting for test request......\n");

	g_main_loop_run(mainloop);

out_stdio:
	g_io_channel_shutdown(stdinchannel, TRUE, NULL);
out_hotplug:
	intel_dp_compliance_cleanup_hotplug();
out_mainloop:
	g_main_loop_unref(mainloop);
out_close:
	cleanup_debugfs();
	close(drm_fd);

	igt_assert_eq(ret, 0);

	igt_info("Compliance testing application exiting\n");
	igt_exit();
}
