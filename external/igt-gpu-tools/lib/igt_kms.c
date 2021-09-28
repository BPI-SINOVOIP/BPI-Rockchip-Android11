/*
 * Copyright © 2013 Intel Corporation
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
 * 	Daniel Vetter <daniel.vetter@ffwll.ch>
 * 	Damien Lespiau <damien.lespiau@intel.com>
 */

#include "config.h"

#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#ifdef HAVE_LINUX_KD_H
#include <linux/kd.h>
#elif HAVE_SYS_KD_H
#include <sys/kd.h>
#endif

#if !defined(ANDROID)
#include <libudev.h>
#endif

#include <poll.h>
#include <errno.h>
#include <time.h>

#include <i915_drm.h>

#include "drmtest.h"
#include "igt_kms.h"
#include "igt_aux.h"
#include "igt_edid.h"
#include "intel_chipset.h"
#include "igt_debugfs.h"
#include "igt_device.h"
#include "igt_sysfs.h"
#include "sw_sync.h"

/**
 * SECTION:igt_kms
 * @short_description: Kernel modesetting support library
 * @title: KMS
 * @include: igt.h
 *
 * This library provides support to enumerate and set modeset configurations.
 *
 * There are two parts in this library: First the low level helper function
 * which directly build on top of raw ioctls or the interfaces provided by
 * libdrm. Those functions all have a kmstest_ prefix.
 *
 * The second part is a high-level library to manage modeset configurations
 * which abstracts away some of the low-level details like the difference
 * between legacy and universal plane support for setting cursors or in the
 * future the difference between legacy and atomic commit. These high-level
 * functions have all igt_ prefixes. This part is still very much work in
 * progress and so also lacks a bit documentation for the individual functions.
 *
 * Note that this library's header pulls in the [i-g-t framebuffer](igt-gpu-tools-i-g-t-framebuffer.html)
 * library as a dependency.
 */

/* list of connectors that need resetting on exit */
#define MAX_CONNECTORS 32
static char *forced_connectors[MAX_CONNECTORS + 1];
static int forced_connectors_device[MAX_CONNECTORS + 1];

/**
 * igt_kms_get_base_edid:
 *
 * Get the base edid block, which includes the following modes:
 *
 *  - 1920x1080 60Hz
 *  - 1280x720 60Hz
 *  - 1024x768 60Hz
 *  - 800x600 60Hz
 *  - 640x480 60Hz
 *
 * Returns: a basic edid block
 */
const struct edid *igt_kms_get_base_edid(void)
{
	static struct edid edid;
	drmModeModeInfo mode = {};

	mode.clock = 148500;
	mode.hdisplay = 1920;
	mode.hsync_start = 2008;
	mode.hsync_end = 2052;
	mode.htotal = 2200;
	mode.vdisplay = 1080;
	mode.vsync_start = 1084;
	mode.vsync_end = 1089;
	mode.vtotal = 1125;
	mode.vrefresh = 60;

	edid_init_with_mode(&edid, &mode);
	edid_update_checksum(&edid);

	return &edid;
}

/**
 * igt_kms_get_alt_edid:
 *
 * Get an alternate edid block, which includes the following modes:
 *
 *  - 1400x1050 60Hz
 *  - 1920x1080 60Hz
 *  - 1280x720 60Hz
 *  - 1024x768 60Hz
 *  - 800x600 60Hz
 *  - 640x480 60Hz
 *
 * Returns: an alternate edid block
 */
const struct edid *igt_kms_get_alt_edid(void)
{
	static struct edid edid;
	drmModeModeInfo mode = {};

	mode.clock = 101000;
	mode.hdisplay = 1400;
	mode.hsync_start = 1448;
	mode.hsync_end = 1480;
	mode.htotal = 1560;
	mode.vdisplay = 1050;
	mode.vsync_start = 1053;
	mode.vsync_end = 1057;
	mode.vtotal = 1080;
	mode.vrefresh = 60;

	edid_init_with_mode(&edid, &mode);
	edid_update_checksum(&edid);

	return &edid;
}

#define AUDIO_EDID_SIZE (2 * EDID_BLOCK_SIZE)

static const struct edid *
generate_audio_edid(unsigned char raw_edid[static AUDIO_EDID_SIZE],
		    bool with_vsdb, struct cea_sad *sad,
		    struct cea_speaker_alloc *speaker_alloc)
{
	struct edid *edid;
	struct edid_ext *edid_ext;
	struct edid_cea *edid_cea;
	char *cea_data;
	struct edid_cea_data_block *block;
	const struct cea_vsdb *vsdb;
	size_t cea_data_size, vsdb_size;

	/* Create a new EDID from the base IGT EDID, and add an
	 * extension that advertises audio support. */
	edid = (struct edid *) raw_edid;
	memcpy(edid, igt_kms_get_base_edid(), sizeof(struct edid));
	edid->extensions_len = 1;
	edid_ext = &edid->extensions[0];
	edid_cea = &edid_ext->data.cea;
	cea_data = edid_cea->data;
	cea_data_size = 0;

	/* Short Audio Descriptor block */
	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_sad(block, sad, 1);

	/* A Vendor Specific Data block is needed for HDMI audio */
	if (with_vsdb) {
		block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
		vsdb = cea_vsdb_get_hdmi_default(&vsdb_size);
		cea_data_size += edid_cea_data_block_set_vsdb(block, vsdb,
							      vsdb_size);
	}

	/* Speaker Allocation Data block */
	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_speaker_alloc(block,
							       speaker_alloc);

	assert(cea_data_size <= sizeof(edid_cea->data));

	edid_ext_set_cea(edid_ext, cea_data_size, 0, EDID_CEA_BASIC_AUDIO);

	edid_update_checksum(edid);

	return edid;
}

const struct edid *igt_kms_get_hdmi_audio_edid(void)
{
	int channels;
	uint8_t sampling_rates, sample_sizes;
	static unsigned char raw_edid[AUDIO_EDID_SIZE] = {0};
	struct cea_sad sad = {0};
	struct cea_speaker_alloc speaker_alloc = {0};

	/* Initialize the Short Audio Descriptor for PCM */
	channels = 2;
	sampling_rates = CEA_SAD_SAMPLING_RATE_32KHZ |
			 CEA_SAD_SAMPLING_RATE_44KHZ |
			 CEA_SAD_SAMPLING_RATE_48KHZ;
	sample_sizes = CEA_SAD_SAMPLE_SIZE_16 |
		       CEA_SAD_SAMPLE_SIZE_20 |
		       CEA_SAD_SAMPLE_SIZE_24;
	cea_sad_init_pcm(&sad, channels, sampling_rates, sample_sizes);

	/* Initialize the Speaker Allocation Data */
	speaker_alloc.speakers = CEA_SPEAKER_FRONT_LEFT_RIGHT_CENTER;

	return generate_audio_edid(raw_edid, true, &sad, &speaker_alloc);
}

const struct edid *igt_kms_get_dp_audio_edid(void)
{
	int channels;
	uint8_t sampling_rates, sample_sizes;
	static unsigned char raw_edid[AUDIO_EDID_SIZE] = {0};
	struct cea_sad sad = {0};
	struct cea_speaker_alloc speaker_alloc = {0};

	/* Initialize the Short Audio Descriptor for PCM */
	channels = 2;
	sampling_rates = CEA_SAD_SAMPLING_RATE_32KHZ |
			 CEA_SAD_SAMPLING_RATE_44KHZ |
			 CEA_SAD_SAMPLING_RATE_48KHZ;
	sample_sizes = CEA_SAD_SAMPLE_SIZE_16 |
		       CEA_SAD_SAMPLE_SIZE_20 |
		       CEA_SAD_SAMPLE_SIZE_24;
	cea_sad_init_pcm(&sad, channels, sampling_rates, sample_sizes);

	/* Initialize the Speaker Allocation Data */
	speaker_alloc.speakers = CEA_SPEAKER_FRONT_LEFT_RIGHT_CENTER;

	return generate_audio_edid(raw_edid, false, &sad, &speaker_alloc);
}

static const uint8_t edid_4k_svds[] = {
	32 | CEA_SVD_NATIVE, /* 1080p @ 24Hz (native) */
	5,                   /* 1080i @ 60Hz */
	20,                  /* 1080i @ 50Hz */
	4,                   /* 720p @ 60Hz */
	19,                  /* 720p @ 50Hz */
};

const struct edid *igt_kms_get_4k_edid(void)
{
	static unsigned char raw_edid[256] = {0};
	struct edid *edid;
	struct edid_ext *edid_ext;
	struct edid_cea *edid_cea;
	char *cea_data;
	struct edid_cea_data_block *block;
	/* We'll add 6 extension fields to the HDMI VSDB. */
	char raw_hdmi[HDMI_VSDB_MIN_SIZE + 6] = {0};
	struct hdmi_vsdb *hdmi;
	size_t cea_data_size = 0;

	/* Create a new EDID from the base IGT EDID, and add an
	 * extension that advertises 4K support. */
	edid = (struct edid *) raw_edid;
	memcpy(edid, igt_kms_get_base_edid(), sizeof(struct edid));
	edid->extensions_len = 1;
	edid_ext = &edid->extensions[0];
	edid_cea = &edid_ext->data.cea;
	cea_data = edid_cea->data;

	/* Short Video Descriptor */
	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_svd(block, edid_4k_svds,
						     sizeof(edid_4k_svds));

	/* Vendor-Specific Data Block */
	hdmi = (struct hdmi_vsdb *) raw_hdmi;
	hdmi->src_phy_addr[0] = 0x10;
	hdmi->src_phy_addr[1] = 0x00;
	/* 6 extension fields */
	hdmi->flags1 = 0;
	hdmi->max_tdms_clock = 0;
	hdmi->flags2 = HDMI_VSDB_VIDEO_PRESENT;
	hdmi->data[0] = 0x00; /* HDMI video flags */
	hdmi->data[1] = 1 << 5; /* 1 VIC entry, 0 3D entries */
	hdmi->data[2] = 0x01; /* 2160p, specified as short descriptor */

	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_hdmi_vsdb(block, hdmi,
							   sizeof(raw_hdmi));

	assert(cea_data_size <= sizeof(edid_cea->data));

	edid_ext_set_cea(edid_ext, cea_data_size, 0, 0);

	edid_update_checksum(edid);

	return edid;
}

const struct edid *igt_kms_get_3d_edid(void)
{
	static unsigned char raw_edid[256] = {0};
	struct edid *edid;
	struct edid_ext *edid_ext;
	struct edid_cea *edid_cea;
	char *cea_data;
	struct edid_cea_data_block *block;
	/* We'll add 5 extension fields to the HDMI VSDB. */
	char raw_hdmi[HDMI_VSDB_MIN_SIZE + 5] = {0};
	struct hdmi_vsdb *hdmi;
	size_t cea_data_size = 0;

	/* Create a new EDID from the base IGT EDID, and add an
	 * extension that advertises 3D support. */
	edid = (struct edid *) raw_edid;
	memcpy(edid, igt_kms_get_base_edid(), sizeof(struct edid));
	edid->extensions_len = 1;
	edid_ext = &edid->extensions[0];
	edid_cea = &edid_ext->data.cea;
	cea_data = edid_cea->data;

	/* Short Video Descriptor */
	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_svd(block, edid_4k_svds,
						     sizeof(edid_4k_svds));

	/* Vendor-Specific Data Block */
	hdmi = (struct hdmi_vsdb *) raw_hdmi;
	hdmi->src_phy_addr[0] = 0x10;
	hdmi->src_phy_addr[1] = 0x00;
	/* 5 extension fields */
	hdmi->flags1 = 0;
	hdmi->max_tdms_clock = 0;
	hdmi->flags2 = HDMI_VSDB_VIDEO_PRESENT;
	hdmi->data[0] = HDMI_VSDB_VIDEO_3D_PRESENT; /* HDMI video flags */
	hdmi->data[1] = 0; /* 0 VIC entries, 0 3D entries */

	block = (struct edid_cea_data_block *) &cea_data[cea_data_size];
	cea_data_size += edid_cea_data_block_set_hdmi_vsdb(block, hdmi,
							   sizeof(raw_hdmi));

	assert(cea_data_size <= sizeof(edid_cea->data));

	edid_ext_set_cea(edid_ext, cea_data_size, 0, 0);

	edid_update_checksum(edid);

	return edid;
}

const char * const igt_plane_prop_names[IGT_NUM_PLANE_PROPS] = {
	[IGT_PLANE_SRC_X] = "SRC_X",
	[IGT_PLANE_SRC_Y] = "SRC_Y",
	[IGT_PLANE_SRC_W] = "SRC_W",
	[IGT_PLANE_SRC_H] = "SRC_H",
	[IGT_PLANE_CRTC_X] = "CRTC_X",
	[IGT_PLANE_CRTC_Y] = "CRTC_Y",
	[IGT_PLANE_CRTC_W] = "CRTC_W",
	[IGT_PLANE_CRTC_H] = "CRTC_H",
	[IGT_PLANE_FB_ID] = "FB_ID",
	[IGT_PLANE_CRTC_ID] = "CRTC_ID",
	[IGT_PLANE_IN_FENCE_FD] = "IN_FENCE_FD",
	[IGT_PLANE_TYPE] = "type",
	[IGT_PLANE_ROTATION] = "rotation",
	[IGT_PLANE_IN_FORMATS] = "IN_FORMATS",
	[IGT_PLANE_COLOR_ENCODING] = "COLOR_ENCODING",
	[IGT_PLANE_COLOR_RANGE] = "COLOR_RANGE",
	[IGT_PLANE_PIXEL_BLEND_MODE] = "pixel blend mode",
	[IGT_PLANE_ALPHA] = "alpha",
	[IGT_PLANE_ZPOS] = "zpos",
};

const char * const igt_crtc_prop_names[IGT_NUM_CRTC_PROPS] = {
	[IGT_CRTC_BACKGROUND] = "background_color",
	[IGT_CRTC_CTM] = "CTM",
	[IGT_CRTC_GAMMA_LUT] = "GAMMA_LUT",
	[IGT_CRTC_GAMMA_LUT_SIZE] = "GAMMA_LUT_SIZE",
	[IGT_CRTC_DEGAMMA_LUT] = "DEGAMMA_LUT",
	[IGT_CRTC_DEGAMMA_LUT_SIZE] = "DEGAMMA_LUT_SIZE",
	[IGT_CRTC_MODE_ID] = "MODE_ID",
	[IGT_CRTC_ACTIVE] = "ACTIVE",
	[IGT_CRTC_OUT_FENCE_PTR] = "OUT_FENCE_PTR",
	[IGT_CRTC_VRR_ENABLED] = "VRR_ENABLED",
};

const char * const igt_connector_prop_names[IGT_NUM_CONNECTOR_PROPS] = {
	[IGT_CONNECTOR_SCALING_MODE] = "scaling mode",
	[IGT_CONNECTOR_CRTC_ID] = "CRTC_ID",
	[IGT_CONNECTOR_DPMS] = "DPMS",
	[IGT_CONNECTOR_BROADCAST_RGB] = "Broadcast RGB",
	[IGT_CONNECTOR_CONTENT_PROTECTION] = "Content Protection",
	[IGT_CONNECTOR_VRR_CAPABLE] = "vrr_capable",
	[IGT_CONNECTOR_HDCP_CONTENT_TYPE] = "HDCP Content Type",
	[IGT_CONNECTOR_LINK_STATUS] = "link-status",
};

/*
 * Retrieve all the properies specified in props_name and store them into
 * plane->props.
 */
static void
igt_fill_plane_props(igt_display_t *display, igt_plane_t *plane,
		     int num_props, const char * const prop_names[])
{
	drmModeObjectPropertiesPtr props;
	int i, j, fd;

	fd = display->drm_fd;

	props = drmModeObjectGetProperties(fd, plane->drm_plane->plane_id, DRM_MODE_OBJECT_PLANE);
	igt_assert(props);

	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop =
			drmModeGetProperty(fd, props->props[i]);

		for (j = 0; j < num_props; j++) {
			if (strcmp(prop->name, prop_names[j]) != 0)
				continue;

			plane->props[j] = props->props[i];
			break;
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
}

/*
 * Retrieve all the properies specified in props_name and store them into
 * config->atomic_props_crtc and config->atomic_props_connector.
 */
static void
igt_atomic_fill_connector_props(igt_display_t *display, igt_output_t *output,
			int num_connector_props, const char * const conn_prop_names[])
{
	drmModeObjectPropertiesPtr props;
	int i, j, fd;

	fd = display->drm_fd;

	props = drmModeObjectGetProperties(fd, output->config.connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
	igt_assert(props);

	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop =
			drmModeGetProperty(fd, props->props[i]);

		for (j = 0; j < num_connector_props; j++) {
			if (strcmp(prop->name, conn_prop_names[j]) != 0)
				continue;

			output->props[j] = props->props[i];
			break;
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
}

static void
igt_fill_pipe_props(igt_display_t *display, igt_pipe_t *pipe,
		    int num_crtc_props, const char * const crtc_prop_names[])
{
	drmModeObjectPropertiesPtr props;
	int i, j, fd;

	fd = display->drm_fd;

	props = drmModeObjectGetProperties(fd, pipe->crtc_id, DRM_MODE_OBJECT_CRTC);
	igt_assert(props);

	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop =
			drmModeGetProperty(fd, props->props[i]);

		for (j = 0; j < num_crtc_props; j++) {
			if (strcmp(prop->name, crtc_prop_names[j]) != 0)
				continue;

			pipe->props[j] = props->props[i];
			break;
		}

		drmModeFreeProperty(prop);
	}

	drmModeFreeObjectProperties(props);
}

/**
 * kmstest_pipe_name:
 * @pipe: display pipe
 *
 * Returns: String representing @pipe, e.g. "A".
 */
const char *kmstest_pipe_name(enum pipe pipe)
{
	static const char str[] = "A\0B\0C\0D\0E\0F";

	_Static_assert(sizeof(str) == IGT_MAX_PIPES * 2,
		       "Missing pipe name");

	if (pipe == PIPE_NONE)
		return "None";

	if (pipe >= IGT_MAX_PIPES)
		return "invalid";

	return str + (pipe * 2);
}

/**
 * kmstest_pipe_to_index:
 *@pipe: display pipe in string format
 *
 * Returns: index to corresponding pipe
 */
int kmstest_pipe_to_index(char pipe)
{
	int r = pipe - 'A';

	if (r < 0 || r >= IGT_MAX_PIPES)
		return -EINVAL;

	return r;
}

/**
 * kmstest_plane_type_name:
 * @plane_type: display plane type
 *
 * Returns: String representing @plane_type, e.g. "overlay".
 */
const char *kmstest_plane_type_name(int plane_type)
{
	static const char * const names[] = {
		[DRM_PLANE_TYPE_OVERLAY] = "overlay",
		[DRM_PLANE_TYPE_PRIMARY] = "primary",
		[DRM_PLANE_TYPE_CURSOR] = "cursor",
	};

	igt_assert(plane_type < ARRAY_SIZE(names) && names[plane_type]);

	return names[plane_type];
}

struct type_name {
	int type;
	const char *name;
};

static const char *find_type_name(const struct type_name *names, int type)
{
	for (; names->name; names++) {
		if (names->type == type)
			return names->name;
	}

	return "(invalid)";
}

static const struct type_name encoder_type_names[] = {
	{ DRM_MODE_ENCODER_NONE, "none" },
	{ DRM_MODE_ENCODER_DAC, "DAC" },
	{ DRM_MODE_ENCODER_TMDS, "TMDS" },
	{ DRM_MODE_ENCODER_LVDS, "LVDS" },
	{ DRM_MODE_ENCODER_TVDAC, "TVDAC" },
	{ DRM_MODE_ENCODER_VIRTUAL, "Virtual" },
	{ DRM_MODE_ENCODER_DSI, "DSI" },
	{ DRM_MODE_ENCODER_DPMST, "DP MST" },
	{}
};

/**
 * kmstest_encoder_type_str:
 * @type: DRM_MODE_ENCODER_* enumeration value
 *
 * Returns: A string representing the drm encoder @type.
 */
const char *kmstest_encoder_type_str(int type)
{
	return find_type_name(encoder_type_names, type);
}

static const struct type_name connector_status_names[] = {
	{ DRM_MODE_CONNECTED, "connected" },
	{ DRM_MODE_DISCONNECTED, "disconnected" },
	{ DRM_MODE_UNKNOWNCONNECTION, "unknown" },
	{}
};

/**
 * kmstest_connector_status_str:
 * @status: DRM_MODE_* connector status value
 *
 * Returns: A string representing the drm connector status @status.
 */
const char *kmstest_connector_status_str(int status)
{
	return find_type_name(connector_status_names, status);
}

static const struct type_name connector_type_names[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "Unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "Composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "SVIDEO" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "Component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
	{ DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
	{ DRM_MODE_CONNECTOR_DPI, "DPI" },
	{}
};

/**
 * kmstest_connector_type_str:
 * @type: DRM_MODE_CONNECTOR_* enumeration value
 *
 * Returns: A string representing the drm connector @type.
 */
const char *kmstest_connector_type_str(int type)
{
	return find_type_name(connector_type_names, type);
}

static const char *mode_stereo_name(const drmModeModeInfo *mode)
{
	switch (mode->flags & DRM_MODE_FLAG_3D_MASK) {
	case DRM_MODE_FLAG_3D_FRAME_PACKING:
		return "FP";
	case DRM_MODE_FLAG_3D_FIELD_ALTERNATIVE:
		return "FA";
	case DRM_MODE_FLAG_3D_LINE_ALTERNATIVE:
		return "LA";
	case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_FULL:
		return "SBSF";
	case DRM_MODE_FLAG_3D_L_DEPTH:
		return "LD";
	case DRM_MODE_FLAG_3D_L_DEPTH_GFX_GFX_DEPTH:
		return "LDGFX";
	case DRM_MODE_FLAG_3D_TOP_AND_BOTTOM:
		return "TB";
	case DRM_MODE_FLAG_3D_SIDE_BY_SIDE_HALF:
		return "SBSH";
	default:
		return NULL;
	}
}

static const char *mode_picture_aspect_name(const drmModeModeInfo *mode)
{
	switch (mode->flags & DRM_MODE_FLAG_PIC_AR_MASK) {
	case DRM_MODE_FLAG_PIC_AR_NONE:
		return NULL;
	case DRM_MODE_FLAG_PIC_AR_4_3:
		return "4:3";
	case DRM_MODE_FLAG_PIC_AR_16_9:
		return "16:9";
	case DRM_MODE_FLAG_PIC_AR_64_27:
		return "64:27";
	case DRM_MODE_FLAG_PIC_AR_256_135:
		return "256:135";
	default:
		return "invalid";
	}
}

/**
 * kmstest_dump_mode:
 * @mode: libdrm mode structure
 *
 * Prints @mode to stdout in a human-readable form.
 */
void kmstest_dump_mode(drmModeModeInfo *mode)
{
	const char *stereo = mode_stereo_name(mode);
	const char *aspect = mode_picture_aspect_name(mode);

	igt_info("  %s %d %d %d %d %d %d %d %d %d 0x%x 0x%x %d%s%s%s%s%s%s\n",
		 mode->name, mode->vrefresh,
		 mode->hdisplay, mode->hsync_start,
		 mode->hsync_end, mode->htotal,
		 mode->vdisplay, mode->vsync_start,
		 mode->vsync_end, mode->vtotal,
		 mode->flags, mode->type, mode->clock,
		 stereo ? " (3D:" : "",
		 stereo ? stereo : "", stereo ? ")" : "",
		 aspect ? " (PAR:" : "",
		 aspect ? aspect : "", aspect ? ")" : "");
}

/**
 * kmstest_get_pipe_from_crtc_id:
 * @fd: DRM fd
 * @crtc_id: DRM CRTC id
 *
 * Returns: The crtc index for the given DRM CRTC ID @crtc_id. The crtc index
 * is the equivalent of the pipe id.  This value maps directly to an enum pipe
 * value used in other helper functions.  Returns 0 if the index could not be
 * determined.
 */

int kmstest_get_pipe_from_crtc_id(int fd, int crtc_id)
{
	drmModeRes *res;
	drmModeCrtc *crtc;
	int i, cur_id;

	res = drmModeGetResources(fd);
	igt_assert(res);

	for (i = 0; i < res->count_crtcs; i++) {
		crtc = drmModeGetCrtc(fd, res->crtcs[i]);
		igt_assert(crtc);
		cur_id = crtc->crtc_id;
		drmModeFreeCrtc(crtc);
		if (cur_id == crtc_id)
			break;
	}

	igt_assert(i < res->count_crtcs);

	drmModeFreeResources(res);

	return i;
}

/**
 * kmstest_find_crtc_for_connector:
 * @fd: DRM fd
 * @res: libdrm resources pointer
 * @connector: libdrm connector pointer
 * @crtc_blacklist_idx_mask: a mask of CRTC indexes that we can't return
 *
 * Returns: the CRTC ID for a CRTC that fits the connector, otherwise it asserts
 * false and never returns. The blacklist mask can be used in case you have
 * CRTCs that are already in use by other connectors.
 */
uint32_t kmstest_find_crtc_for_connector(int fd, drmModeRes *res,
					 drmModeConnector *connector,
					 uint32_t crtc_blacklist_idx_mask)
{
	drmModeEncoder *e;
	uint32_t possible_crtcs;
	int i, j;

	for (i = 0; i < connector->count_encoders; i++) {
		e = drmModeGetEncoder(fd, connector->encoders[i]);
		possible_crtcs = e->possible_crtcs & ~crtc_blacklist_idx_mask;
		drmModeFreeEncoder(e);

		for (j = 0; possible_crtcs >> j; j++)
			if (possible_crtcs & (1 << j))
				return res->crtcs[j];
	}

	igt_assert(false);
}

/**
 * kmstest_dumb_create:
 * @fd: open drm file descriptor
 * @width: width of the buffer in pixels
 * @height: height of the buffer in pixels
 * @bpp: bytes per pixel of the buffer
 * @stride: Pointer which receives the dumb bo's stride, can be NULL.
 * @size: Pointer which receives the dumb bo's size, can be NULL.
 *
 * This wraps the CREATE_DUMB ioctl, which allocates a new dumb buffer object
 * for the specified dimensions.
 *
 * Returns: The file-private handle of the created buffer object
 */
uint32_t kmstest_dumb_create(int fd, int width, int height, int bpp,
			     unsigned *stride, uint64_t *size)
{
	struct drm_mode_create_dumb create;

	memset(&create, 0, sizeof(create));
	create.width = width;
	create.height = height;
	create.bpp = bpp;

	create.handle = 0;
	do_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	igt_assert(create.handle);
	igt_assert(create.size >= (uint64_t) width * height * bpp / 8);

	if (stride)
		*stride = create.pitch;

	if (size)
		*size = create.size;

	return create.handle;
}

/**
 * kmstest_dumb_map_buffer:
 * @fd: Opened drm file descriptor
 * @handle: Offset in the file referred to by fd
 * @size: Length of the mapping, must be greater than 0
 * @prot: Describes the memory protection of the mapping
 * Returns: A pointer representing the start of the virtual mapping
 */
void *kmstest_dumb_map_buffer(int fd, uint32_t handle, uint64_t size,
			      unsigned prot)
{
	struct drm_mode_map_dumb arg = {};
	void *ptr;

	arg.handle = handle;

	do_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);

	ptr = mmap(NULL, size, prot, MAP_SHARED, fd, arg.offset);
	igt_assert(ptr != MAP_FAILED);

	return ptr;
}

static int __kmstest_dumb_destroy(int fd, uint32_t handle)
{
	struct drm_mode_destroy_dumb arg = { handle };
	int err = 0;

	if (drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg))
		err = -errno;

	errno = 0;
	return err;
}

/**
 * kmstest_dumb_destroy:
 * @fd: Opened drm file descriptor
 * @handle: Offset in the file referred to by fd
 */
void kmstest_dumb_destroy(int fd, uint32_t handle)
{
	igt_assert_eq(__kmstest_dumb_destroy(fd, handle), 0);
}

#if !defined(ANDROID)
/*
 * Returns: the previous mode, or KD_GRAPHICS if no /dev/tty0 was
 * found and nothing was done.
 */
static signed long set_vt_mode(unsigned long mode)
{
	int fd;
	unsigned long prev_mode;
	static const char TTY0[] = "/dev/tty0";

	if (access(TTY0, F_OK)) {
		/* errno message should be "No such file". Do not
		   hardcode but ask strerror() in the very unlikely
		   case something else happened. */
		igt_debug("VT: %s: %s, cannot change its mode\n",
			  TTY0, strerror(errno));
		return KD_GRAPHICS;
	}

	fd = open(TTY0, O_RDONLY);
	if (fd < 0)
		return -errno;

	prev_mode = 0;
	if (drmIoctl(fd, KDGETMODE, &prev_mode))
		goto err;
	if (drmIoctl(fd, KDSETMODE, (void *)mode))
		goto err;

	close(fd);

	return prev_mode;
err:
	close(fd);

	return -errno;
}
#endif

static unsigned long orig_vt_mode = -1UL;

/**
 * kmstest_restore_vt_mode:
 *
 * Restore the VT mode in use before #kmstest_set_vt_graphics_mode was called.
 */
void kmstest_restore_vt_mode(void)
{
#if !defined(ANDROID)
	long ret;

	if (orig_vt_mode != -1UL) {
		ret = set_vt_mode(orig_vt_mode);

		igt_assert(ret >= 0);
		igt_debug("VT: original mode 0x%lx restored\n", orig_vt_mode);
		orig_vt_mode = -1UL;
	}
#endif
}

/**
 * kmstest_set_vt_graphics_mode:
 *
 * Sets the controlling VT (if available) into graphics/raw mode and installs
 * an igt exit handler to set the VT back to text mode on exit. Use
 * #kmstest_restore_vt_mode to restore the previous VT mode manually.
 *
 * All kms tests must call this function to make sure that the fbcon doesn't
 * interfere by e.g. blanking the screen.
 */
void kmstest_set_vt_graphics_mode(void)
{
#if !defined(ANDROID)
	long ret;

	igt_install_exit_handler((igt_exit_handler_t) kmstest_restore_vt_mode);

	ret = set_vt_mode(KD_GRAPHICS);

	igt_assert(ret >= 0);
	orig_vt_mode = ret;

	igt_debug("VT: graphics mode set (mode was 0x%lx)\n", ret);
#endif
}


static void reset_connectors_at_exit(int sig)
{
	igt_reset_connectors();
}

/**
 * kmstest_force_connector:
 * @fd: drm file descriptor
 * @connector: connector
 * @state: state to force on @connector
 *
 * Force the specified state on the specified connector.
 *
 * Returns: true on success
 */
bool kmstest_force_connector(int drm_fd, drmModeConnector *connector,
			     enum kmstest_force_connector_state state)
{
	char *path, **tmp;
	const char *value;
	drmModeConnector *temp;
	uint32_t devid;
	int len, dir, idx;

#if defined(USE_INTEL)
	if (is_i915_device(drm_fd)) {
		devid = intel_get_drm_devid(drm_fd);

		/*
		 * forcing hdmi or dp connectors on HSW and BDW doesn't
		 * currently work, so fail early to allow the test to skip if
		 * required
		 */
		if ((connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
		     connector->connector_type == DRM_MODE_CONNECTOR_HDMIB ||
		     connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort)
		    && (IS_HASWELL(devid) || IS_BROADWELL(devid)))
			return false;
	}
#endif

	switch (state) {
	case FORCE_CONNECTOR_ON:
		value = "on";
		break;
	case FORCE_CONNECTOR_DIGITAL:
		value = "on-digital";
		break;
	case FORCE_CONNECTOR_OFF:
		value = "off";
		break;

	default:
	case FORCE_CONNECTOR_UNSPECIFIED:
		value = "detect";
		break;
	}

	dir = igt_sysfs_open(drm_fd);
	if (dir < 0)
		return false;

	idx = igt_device_get_card_index(drm_fd);
	if (idx < 0 || idx > 63)
		return false;

	if (asprintf(&path, "card%d-%s-%d/status",
		     idx,
		     kmstest_connector_type_str(connector->connector_type),
		     connector->connector_type_id) < 0) {
		close(dir);
		return false;
	}

	if (!igt_sysfs_set(dir, path, value)) {
		close(dir);
		return false;
	}

	for (len = 0, tmp = forced_connectors; *tmp; tmp++) {
		/* check the connector is not already present */
		if (strcmp(*tmp, path) == 0) {
			len = -1;
			break;
		}
		len++;
	}

	if (len != -1 && len < MAX_CONNECTORS) {
		forced_connectors[len] = path;
		forced_connectors_device[len] = dir;
	}

	if (len >= MAX_CONNECTORS)
		igt_warn("Connector limit reached, %s will not be reset\n",
			 path);

	igt_debug("Connector %s is now forced %s\n", path, value);
	igt_debug("Current forced connectors:\n");
	tmp = forced_connectors;
	while (*tmp) {
		igt_debug("\t%s\n", *tmp);
		tmp++;
	}

	igt_install_exit_handler(reset_connectors_at_exit);

	/* To allow callers to always use GetConnectorCurrent we need to force a
	 * redetection here. */
	temp = drmModeGetConnector(drm_fd, connector->connector_id);
	drmModeFreeConnector(temp);

	return true;
}

/**
 * kmstest_force_edid:
 * @drm_fd: drm file descriptor
 * @connector: connector to set @edid on
 * @edid: An EDID data block
 *
 * Set the EDID data on @connector to @edid. See also #igt_kms_get_base_edid.
 *
 * If @edid is NULL, the forced EDID will be removed.
 */
void kmstest_force_edid(int drm_fd, drmModeConnector *connector,
			const struct edid *edid)
{
	char *path;
	int debugfs_fd, ret;
	drmModeConnector *temp;

	igt_assert_neq(asprintf(&path, "%s-%d/edid_override", kmstest_connector_type_str(connector->connector_type), connector->connector_type_id),
		       -1);
	debugfs_fd = igt_debugfs_open(drm_fd, path, O_WRONLY | O_TRUNC);
	free(path);

	igt_require(debugfs_fd != -1);

	if (edid == NULL)
		ret = write(debugfs_fd, "reset", 5);
	else
		ret = write(debugfs_fd, edid,
			    edid_get_size(edid));
	close(debugfs_fd);

	/* To allow callers to always use GetConnectorCurrent we need to force a
	 * redetection here. */
	temp = drmModeGetConnector(drm_fd, connector->connector_id);
	drmModeFreeConnector(temp);

	igt_assert(ret != -1);
}

/**
 * kmstest_get_connector_default_mode:
 * @drm_fd: DRM fd
 * @connector: libdrm connector
 * @mode: libdrm mode
 *
 * Retrieves the default mode for @connector and stores it in @mode.
 *
 * Returns: true on success, false on failure
 */
bool kmstest_get_connector_default_mode(int drm_fd, drmModeConnector *connector,
					drmModeModeInfo *mode)
{
	int i;

	if (!connector->count_modes) {
		igt_warn("no modes for connector %d\n",
			 connector->connector_id);
		return false;
	}

	for (i = 0; i < connector->count_modes; i++) {
		if (i == 0 ||
		    connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
			*mode = connector->modes[i];
			if (mode->type & DRM_MODE_TYPE_PREFERRED)
				break;
		}
	}

	return true;
}

static void
_kmstest_connector_config_crtc_mask(int drm_fd,
				    drmModeConnector *connector,
				    struct kmstest_connector_config *config)
{
	int i;

	config->valid_crtc_idx_mask = 0;

	/* Now get a compatible encoder */
	for (i = 0; i < connector->count_encoders; i++) {
		drmModeEncoder *encoder = drmModeGetEncoder(drm_fd,
							    connector->encoders[i]);

		if (!encoder) {
			igt_warn("could not get encoder %d: %s\n",
				 connector->encoders[i],
				 strerror(errno));

			continue;
		}

		config->valid_crtc_idx_mask |= encoder->possible_crtcs;
		drmModeFreeEncoder(encoder);
	}
}

static drmModeEncoder *
_kmstest_connector_config_find_encoder(int drm_fd, drmModeConnector *connector, enum pipe pipe)
{
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		drmModeEncoder *encoder = drmModeGetEncoder(drm_fd, connector->encoders[i]);

		if (!encoder) {
			igt_warn("could not get encoder %d: %s\n",
				 connector->encoders[i],
				 strerror(errno));

			continue;
		}

		if (encoder->possible_crtcs & (1 << pipe))
			return encoder;

		drmModeFreeEncoder(encoder);
	}

	igt_assert(false);
	return NULL;
}

/**
 * _kmstest_connector_config:
 * @drm_fd: DRM fd
 * @connector_id: DRM connector id
 * @crtc_idx_mask: mask of allowed DRM CRTC indices
 * @config: structure filled with the possible configuration
 * @probe: whether to fully re-probe mode list or not
 *
 * This tries to find a suitable configuration for the given connector and CRTC
 * constraint and fills it into @config.
 */
static bool _kmstest_connector_config(int drm_fd, uint32_t connector_id,
				      unsigned long crtc_idx_mask,
				      struct kmstest_connector_config *config,
				      bool probe)
{
	drmModeRes *resources;
	drmModeConnector *connector;

	config->pipe = PIPE_NONE;

	resources = drmModeGetResources(drm_fd);
	if (!resources) {
		igt_warn("drmModeGetResources failed");
		goto err1;
	}

	/* First, find the connector & mode */
	if (probe)
		connector = drmModeGetConnector(drm_fd, connector_id);
	else
		connector = drmModeGetConnectorCurrent(drm_fd, connector_id);

	if (!connector)
		goto err2;

	if (connector->connector_id != connector_id) {
		igt_warn("connector id doesn't match (%d != %d)\n",
			 connector->connector_id, connector_id);
		goto err3;
	}

	/*
	 * Find given CRTC if crtc_id != 0 or else the first CRTC not in use.
	 * In both cases find the first compatible encoder and skip the CRTC
	 * if there is non such.
	 */
	_kmstest_connector_config_crtc_mask(drm_fd, connector, config);

	if (!connector->count_modes)
		memset(&config->default_mode, 0, sizeof(config->default_mode));
	else if (!kmstest_get_connector_default_mode(drm_fd, connector,
						     &config->default_mode))
		goto err3;

	config->connector = connector;

	crtc_idx_mask &= config->valid_crtc_idx_mask;
	if (!crtc_idx_mask)
		/* Keep config->connector */
		goto err2;

	config->pipe = ffs(crtc_idx_mask) - 1;

	config->encoder = _kmstest_connector_config_find_encoder(drm_fd, connector, config->pipe);
	config->crtc = drmModeGetCrtc(drm_fd, resources->crtcs[config->pipe]);

	if (connector->connection != DRM_MODE_CONNECTED)
		goto err2;

	if (!connector->count_modes) {
		if (probe)
			igt_warn("connector %d/%s-%d has no modes\n", connector_id,
				kmstest_connector_type_str(connector->connector_type),
				connector->connector_type_id);
		goto err2;
	}

	drmModeFreeResources(resources);
	return true;
err3:
	drmModeFreeConnector(connector);
err2:
	drmModeFreeResources(resources);
err1:
	return false;
}

/**
 * kmstest_get_connector_config:
 * @drm_fd: DRM fd
 * @connector_id: DRM connector id
 * @crtc_idx_mask: mask of allowed DRM CRTC indices
 * @config: structure filled with the possible configuration
 *
 * This tries to find a suitable configuration for the given connector and CRTC
 * constraint and fills it into @config.
 */
bool kmstest_get_connector_config(int drm_fd, uint32_t connector_id,
				  unsigned long crtc_idx_mask,
				  struct kmstest_connector_config *config)
{
	return _kmstest_connector_config(drm_fd, connector_id, crtc_idx_mask,
					 config, 0);
}

/**
 * kmstest_probe_connector_config:
 * @drm_fd: DRM fd
 * @connector_id: DRM connector id
 * @crtc_idx_mask: mask of allowed DRM CRTC indices
 * @config: structure filled with the possible configuration
 *
 * This tries to find a suitable configuration for the given connector and CRTC
 * constraint and fills it into @config, fully probing the connector in the
 * process.
 */
bool kmstest_probe_connector_config(int drm_fd, uint32_t connector_id,
				    unsigned long crtc_idx_mask,
				    struct kmstest_connector_config *config)
{
	return _kmstest_connector_config(drm_fd, connector_id, crtc_idx_mask,
					 config, 1);
}

/**
 * kmstest_free_connector_config:
 * @config: connector configuration structure
 *
 * Free any resources in @config allocated in kmstest_get_connector_config().
 */
void kmstest_free_connector_config(struct kmstest_connector_config *config)
{
	drmModeFreeCrtc(config->crtc);
	config->crtc = NULL;

	drmModeFreeEncoder(config->encoder);
	config->encoder = NULL;

	drmModeFreeConnector(config->connector);
	config->connector = NULL;
}

/**
 * kmstest_set_connector_dpms:
 * @fd: DRM fd
 * @connector: libdrm connector
 * @mode: DRM DPMS value
 *
 * This function sets the DPMS setting of @connector to @mode.
 */
void kmstest_set_connector_dpms(int fd, drmModeConnector *connector, int mode)
{
	int i, dpms = 0;
	bool found_it = false;

	for (i = 0; i < connector->count_props; i++) {
		struct drm_mode_get_property prop = {
			.prop_id = connector->props[i],
		};

		if (drmIoctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop))
			continue;

		if (strcmp(prop.name, "DPMS"))
			continue;

		dpms = prop.prop_id;
		found_it = true;
		break;
	}
	igt_assert_f(found_it, "DPMS property not found on %d\n",
		     connector->connector_id);

	igt_assert(drmModeConnectorSetProperty(fd, connector->connector_id,
					       dpms, mode) == 0);
}

/**
 * kmstest_get_property:
 * @drm_fd: drm file descriptor
 * @object_id: object whose properties we're going to get
 * @object_type: type of obj_id (DRM_MODE_OBJECT_*)
 * @name: name of the property we're going to get
 * @prop_id: if not NULL, returns the property id
 * @value: if not NULL, returns the property value
 * @prop: if not NULL, returns the property, and the caller will have to free
 *        it manually.
 *
 * Finds a property with the given name on the given object.
 *
 * Returns: true in case we found something.
 */
bool
kmstest_get_property(int drm_fd, uint32_t object_id, uint32_t object_type,
		     const char *name, uint32_t *prop_id /* out */,
		     uint64_t *value /* out */,
		     drmModePropertyPtr *prop /* out */)
{
	drmModeObjectPropertiesPtr proplist;
	drmModePropertyPtr _prop;
	bool found = false;
	int i;

	proplist = drmModeObjectGetProperties(drm_fd, object_id, object_type);
	for (i = 0; i < proplist->count_props; i++) {
		_prop = drmModeGetProperty(drm_fd, proplist->props[i]);
		if (!_prop)
			continue;

		if (strcmp(_prop->name, name) == 0) {
			found = true;
			if (prop_id)
				*prop_id = proplist->props[i];
			if (value)
				*value = proplist->prop_values[i];
			if (prop)
				*prop = _prop;
			else
				drmModeFreeProperty(_prop);

			break;
		}
		drmModeFreeProperty(_prop);
	}

	drmModeFreeObjectProperties(proplist);
	return found;
}

/**
 * kmstest_unset_all_crtcs:
 * @drm_fd: the DRM fd
 * @resources: libdrm resources pointer
 *
 * Disables all the screens.
 */
void kmstest_unset_all_crtcs(int drm_fd, drmModeResPtr resources)
{
	int i, rc;

	for (i = 0; i < resources->count_crtcs; i++) {
		rc = drmModeSetCrtc(drm_fd, resources->crtcs[i], 0, 0, 0, NULL,
				    0, NULL);
		igt_assert(rc == 0);
	}
}

/**
 * kmstest_get_crtc_idx:
 * @res: the libdrm resources
 * @crtc_id: the CRTC id
 *
 * Get the CRTC index based on its ID. This is useful since a few places of
 * libdrm deal with CRTC masks.
 */
int kmstest_get_crtc_idx(drmModeRes *res, uint32_t crtc_id)
{
	int i;

	for (i = 0; i < res->count_crtcs; i++)
		if (res->crtcs[i] == crtc_id)
			return i;

	igt_assert(false);
}

static inline uint32_t pipe_select(int pipe)
{
	if (pipe > 1)
		return pipe << DRM_VBLANK_HIGH_CRTC_SHIFT;
	else if (pipe > 0)
		return DRM_VBLANK_SECONDARY;
	else
		return 0;
}

/**
 * kmstest_get_vblank:
 * @fd: Opened drm file descriptor
 * @pipe: Display pipe
 * @flags: Flags passed to drm_ioctl_wait_vblank
 *
 * Blocks or request a signal when a specified vblank event occurs
 *
 * Returns 0 on success or non-zero unsigned integer otherwise
 */
unsigned int kmstest_get_vblank(int fd, int pipe, unsigned int flags)
{
	union drm_wait_vblank vbl;

	memset(&vbl, 0, sizeof(vbl));
	vbl.request.type = DRM_VBLANK_RELATIVE | pipe_select(pipe) | flags;
	if (drmIoctl(fd, DRM_IOCTL_WAIT_VBLANK, &vbl))
		return 0;

	return vbl.reply.sequence;
}

/**
 * kmstest_wait_for_pageflip:
 * @fd: Opened drm file descriptor
 *
 * Blocks until pageflip is completed
 *
 */
void kmstest_wait_for_pageflip(int fd)
{
	drmEventContext evctx = { .version = 2 };
	struct timeval timeout = { .tv_sec = 0, .tv_usec = 50000 };
	fd_set fds;
	int ret;

	/* Wait for pageflip completion, then consume event on fd */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	do {
		errno = 0;
		ret = select(fd + 1, &fds, NULL, NULL, &timeout);
	} while (ret < 0 && errno == EINTR);

	igt_fail_on_f(ret == 0,
		     "Exceeded timeout (50ms) while waiting for a pageflip\n");

	igt_assert_f(ret == 1,
		     "Waiting for pageflip failed with %d from select(drmfd)\n",
		     ret);

	igt_assert(drmHandleEvent(fd, &evctx) == 0);
}

static void get_plane(char *str, int type, struct kmstest_plane *plane)
{
	int ret;
	char buf[256];

	plane->type = type;
	ret = sscanf(str + 12, "%d%*c %*s %[^n]s",
		     &plane->id,
		     buf);
	igt_assert_eq(ret, 2);

	ret = sscanf(buf + 9, "%4d%*c%4d%*c", &plane->pos_x, &plane->pos_y);
	igt_assert_eq(ret, 2);

	ret = sscanf(buf + 30, "%4d%*c%4d%*c", &plane->width, &plane->height);
	igt_assert_eq(ret, 2);
}

static int parse_planes(FILE *fid, struct kmstest_plane *planes)
{
	char tmp[256];
	int n_planes;

	n_planes = 0;
	while (fgets(tmp, 256, fid) != NULL) {
		if (strstr(tmp, "type=PRI") != NULL) {
			if (planes) {
				get_plane(tmp, DRM_PLANE_TYPE_PRIMARY, &planes[n_planes]);
				planes[n_planes].index = n_planes;
			}
			n_planes++;
		} else if (strstr(tmp, "type=OVL") != NULL) {
			if (planes) {
				get_plane(tmp, DRM_PLANE_TYPE_OVERLAY, &planes[n_planes]);
				planes[n_planes].index = n_planes;
			}
			n_planes++;
		} else if (strstr(tmp, "type=CUR") != NULL) {
			if (planes) {
				get_plane(tmp, DRM_PLANE_TYPE_CURSOR, &planes[n_planes]);
				planes[n_planes].index = n_planes;
			}
			n_planes++;
			break;
		}
	}

	return n_planes;
}

static void parse_crtc(char *info, struct kmstest_crtc *crtc)
{
	char buf[256];
	int ret;
	char pipe;

	ret = sscanf(info + 4, "%d%*c %*s %c%*c %*s %s%*c",
		     &crtc->id, &pipe, buf);
	igt_assert_eq(ret, 3);

	crtc->pipe = kmstest_pipe_to_index(pipe);
	igt_assert(crtc->pipe >= 0);

	ret = sscanf(buf + 6, "%d%*c%d%*c",
		     &crtc->width, &crtc->height);
	igt_assert_eq(ret, 2);
}

static void kmstest_get_crtc(int device, enum pipe pipe, struct kmstest_crtc *crtc)
{
	char tmp[256];
	FILE *file;
	int ncrtc;
	int line;
	long int n;
	int fd;

	fd = igt_debugfs_open(device, "i915_display_info", O_RDONLY);
	file = fdopen(fd, "r");
	igt_skip_on(file == NULL);

	ncrtc = 0;
	line = 0;
	while (fgets(tmp, 256, file) != NULL) {
		if ((strstr(tmp, "CRTC") != NULL) && (line > 0)) {
			if (strstr(tmp, "active=yes") != NULL) {
				crtc->active = true;
				parse_crtc(tmp, crtc);

				n = ftell(file);
				crtc->n_planes = parse_planes(file, NULL);
				igt_assert_lt(0, crtc->n_planes);
				crtc->planes = calloc(crtc->n_planes, sizeof(*crtc->planes));
				igt_assert_f(crtc->planes, "Failed to allocate memory for %d planes\n", crtc->n_planes);

				fseek(file, n, SEEK_SET);
				parse_planes(file, crtc->planes);

				if (crtc->pipe != pipe) {
					free(crtc->planes);
				} else {
					ncrtc++;
					break;
				}
			}
		}

		line++;
	}

	fclose(file);
	close(fd);

	igt_assert(ncrtc == 1);
}

/**
 * igt_assert_plane_visible:
 * @fd: Opened file descriptor
 * @pipe: Display pipe
 * @visibility: Boolean parameter to test against the plane's current visibility state
 *
 * Asserts only if the plane's visibility state matches the status being passed by @visibility
 */
void igt_assert_plane_visible(int fd, enum pipe pipe, int plane_index, bool visibility)
{
	struct kmstest_crtc crtc;
	bool visible = true;

	kmstest_get_crtc(fd, pipe, &crtc);

	igt_assert(plane_index < crtc.n_planes);

	if (crtc.planes[plane_index].pos_x > crtc.width ||
	    crtc.planes[plane_index].pos_y > crtc.height)
		visible = false;

	free(crtc.planes);
	igt_assert_eq(visible, visibility);
}

/**
 * kms_has_vblank:
 * @fd: DRM fd
 *
 * Get the VBlank errno after an attempt to call drmWaitVBlank(). This
 * function is useful for checking if a driver has support or not for VBlank.
 *
 * Returns: true if target driver has VBlank support, otherwise return false.
 */
bool kms_has_vblank(int fd)
{
	drmVBlank dummy_vbl;

	memset(&dummy_vbl, 0, sizeof(drmVBlank));
	dummy_vbl.request.type = DRM_VBLANK_RELATIVE;

	errno = 0;
	drmWaitVBlank(fd, &dummy_vbl);
	return (errno != EOPNOTSUPP);
}

/*
 * A small modeset API
 */

#define LOG_SPACES		"    "
#define LOG_N_SPACES		(sizeof(LOG_SPACES) - 1)

#define LOG_INDENT(d, section)				\
	do {						\
		igt_display_log(d, "%s {\n", section);	\
		igt_display_log_shift(d, 1);		\
	} while (0)
#define LOG_UNINDENT(d)					\
	do {						\
		igt_display_log_shift(d, -1);		\
		igt_display_log(d, "}\n");		\
	} while (0)
#define LOG(d, fmt, ...)	igt_display_log(d, fmt, ## __VA_ARGS__)

static void  __attribute__((format(printf, 2, 3)))
igt_display_log(igt_display_t *display, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	igt_debug("display: ");
	for (i = 0; i < display->log_shift; i++)
		igt_debug("%s", LOG_SPACES);
	igt_vlog(IGT_LOG_DOMAIN, IGT_LOG_DEBUG, fmt, args);
	va_end(args);
}

static void igt_display_log_shift(igt_display_t *display, int shift)
{
	display->log_shift += shift;
	igt_assert(display->log_shift >= 0);
}

static void igt_output_refresh(igt_output_t *output)
{
	igt_display_t *display = output->display;
	unsigned long crtc_idx_mask = 0;

	if (output->pending_pipe != PIPE_NONE)
		crtc_idx_mask = 1 << output->pending_pipe;

	kmstest_free_connector_config(&output->config);

	_kmstest_connector_config(display->drm_fd, output->id, crtc_idx_mask,
				  &output->config, output->force_reprobe);
	output->force_reprobe = false;

	if (!output->name && output->config.connector) {
		drmModeConnector *c = output->config.connector;

		igt_assert_neq(asprintf(&output->name, "%s-%d", kmstest_connector_type_str(c->connector_type), c->connector_type_id),
			       -1);
	}

	if (output->config.connector)
		igt_atomic_fill_connector_props(display, output,
			IGT_NUM_CONNECTOR_PROPS, igt_connector_prop_names);

	LOG(display, "%s: Selecting pipe %s\n", output->name,
	    kmstest_pipe_name(output->pending_pipe));
}

static int
igt_plane_set_property(igt_plane_t *plane, uint32_t prop_id, uint64_t value)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	return drmModeObjectSetProperty(display->drm_fd, plane->drm_plane->plane_id,
				 DRM_MODE_OBJECT_PLANE, prop_id, value);
}

/*
 * Walk a plane's property list to determine its type.  If we don't
 * find a type property, then the kernel doesn't support universal
 * planes and we know the plane is an overlay/sprite.
 */
static int get_drm_plane_type(int drm_fd, uint32_t plane_id)
{
	uint64_t value;
	bool has_prop;

	has_prop = kmstest_get_property(drm_fd, plane_id, DRM_MODE_OBJECT_PLANE,
					"type", NULL, &value, NULL);
	if (has_prop)
		return (int)value;

	return DRM_PLANE_TYPE_OVERLAY;
}

static void igt_plane_reset(igt_plane_t *plane)
{
	/* Reset src coordinates. */
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_X, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_Y, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_W, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_H, 0);

	/* Reset crtc coordinates. */
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_X, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_Y, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_W, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_H, 0);

	/* Reset binding to fb and crtc. */
	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, 0);

	if (igt_plane_has_prop(plane, IGT_PLANE_COLOR_ENCODING))
		igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_ENCODING,
			igt_color_encoding_to_str(IGT_COLOR_YCBCR_BT601));

	if (igt_plane_has_prop(plane, IGT_PLANE_COLOR_RANGE))
		igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_RANGE,
			igt_color_range_to_str(IGT_COLOR_YCBCR_LIMITED_RANGE));

	/* Use default rotation */
	if (igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
		igt_plane_set_prop_value(plane, IGT_PLANE_ROTATION, IGT_ROTATION_0);

	if (igt_plane_has_prop(plane, IGT_PLANE_PIXEL_BLEND_MODE))
		igt_plane_set_prop_enum(plane, IGT_PLANE_PIXEL_BLEND_MODE, "Pre-multiplied");

	if (igt_plane_has_prop(plane, IGT_PLANE_ALPHA))
	{
		uint64_t max_alpha = 0xffff;
		drmModePropertyPtr alpha_prop = drmModeGetProperty(
			plane->pipe->display->drm_fd,
			plane->props[IGT_PLANE_ALPHA]);

		if (alpha_prop)
		{
			if (alpha_prop->flags & DRM_MODE_PROP_RANGE)
			{
				max_alpha = alpha_prop->values[1];
			}

			drmModeFreeProperty(alpha_prop);
		}

		igt_plane_set_prop_value(plane, IGT_PLANE_ALPHA, max_alpha);
	}


	igt_plane_clear_prop_changed(plane, IGT_PLANE_IN_FENCE_FD);
	plane->values[IGT_PLANE_IN_FENCE_FD] = ~0ULL;
	plane->gem_handle = 0;
}

static void igt_pipe_reset(igt_pipe_t *pipe)
{
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_MODE_ID, 0);
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_ACTIVE, 0);
	igt_pipe_obj_clear_prop_changed(pipe, IGT_CRTC_OUT_FENCE_PTR);

	if (igt_pipe_obj_has_prop(pipe, IGT_CRTC_CTM))
		igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_CTM, 0);

	if (igt_pipe_obj_has_prop(pipe, IGT_CRTC_GAMMA_LUT))
		igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_GAMMA_LUT, 0);

	if (igt_pipe_obj_has_prop(pipe, IGT_CRTC_DEGAMMA_LUT))
		igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_DEGAMMA_LUT, 0);

	pipe->out_fence_fd = -1;
}

static void igt_output_reset(igt_output_t *output)
{
	output->pending_pipe = PIPE_NONE;
	output->use_override_mode = false;
	memset(&output->override_mode, 0, sizeof(output->override_mode));

	igt_output_set_prop_value(output, IGT_CONNECTOR_CRTC_ID, 0);

	if (igt_output_has_prop(output, IGT_CONNECTOR_BROADCAST_RGB))
		igt_output_set_prop_value(output, IGT_CONNECTOR_BROADCAST_RGB,
					  BROADCAST_RGB_FULL);

	if (igt_output_has_prop(output, IGT_CONNECTOR_CONTENT_PROTECTION))
		igt_output_set_prop_enum(output, IGT_CONNECTOR_CONTENT_PROTECTION,
					 "Undesired");
}

/**
 * igt_display_reset:
 * @display: a pointer to an #igt_display_t structure
 *
 * Reset basic pipes, connectors and planes on @display back to default values.
 * In particular, the following properties will be reset:
 *
 * For outputs:
 * - %IGT_CONNECTOR_CRTC_ID
 * - %IGT_CONNECTOR_BROADCAST_RGB (if applicable)
 *   %IGT_CONNECTOR_CONTENT_PROTECTION (if applicable)
 * - igt_output_override_mode() to default.
 *
 * For pipes:
 * - %IGT_CRTC_MODE_ID (leaked)
 * - %IGT_CRTC_ACTIVE
 * - %IGT_CRTC_OUT_FENCE_PTR
 *
 * For planes:
 * - %IGT_PLANE_SRC_*
 * - %IGT_PLANE_CRTC_*
 * - %IGT_PLANE_FB_ID
 * - %IGT_PLANE_CRTC_ID
 * - %IGT_PLANE_ROTATION
 * - %IGT_PLANE_IN_FENCE_FD
 */
void igt_display_reset(igt_display_t *display)
{
	enum pipe pipe;
	int i;

	/*
	 * Allow resetting rotation on all planes, which is normally
	 * prohibited on the primary and cursor plane for legacy commits.
	 */
	display->first_commit = true;

	for_each_pipe(display, pipe) {
		igt_pipe_t *pipe_obj = &display->pipes[pipe];
		igt_plane_t *plane;

		for_each_plane_on_pipe(display, pipe, plane)
			igt_plane_reset(plane);

		igt_pipe_reset(pipe_obj);
	}

	for (i = 0; i < display->n_outputs; i++) {
		igt_output_t *output = &display->outputs[i];

		igt_output_reset(output);
	}
}

static void igt_fill_plane_format_mod(igt_display_t *display, igt_plane_t *plane);
static void igt_fill_display_format_mod(igt_display_t *display);

/**
 * igt_display_require:
 * @display: a pointer to an #igt_display_t structure
 * @drm_fd: a drm file descriptor
 *
 * Initialize @display and allocate the various resources required. Use
 * #igt_display_fini to release the resources when they are no longer required.
 *
 * This function automatically skips if the kernel driver doesn't support any
 * CRTC or outputs.
 */
void igt_display_require(igt_display_t *display, int drm_fd)
{
	drmModeRes *resources;
	drmModePlaneRes *plane_resources;
	int i;

	memset(display, 0, sizeof(igt_display_t));

	LOG_INDENT(display, "init");

	display->drm_fd = drm_fd;

	resources = drmModeGetResources(display->drm_fd);
	if (!resources)
		goto out;

	/*
	 * We cache the number of pipes, that number is a physical limit of the
	 * hardware and cannot change of time (for now, at least).
	 */
	display->n_pipes = resources->count_crtcs;
	display->pipes = calloc(sizeof(igt_pipe_t), display->n_pipes);
	igt_assert_f(display->pipes, "Failed to allocate memory for %d pipes\n", display->n_pipes);

	drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	if (drmSetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0)
		display->is_atomic = 1;

	plane_resources = drmModeGetPlaneResources(display->drm_fd);
	igt_assert(plane_resources);

	display->n_planes = plane_resources->count_planes;
	display->planes = calloc(sizeof(igt_plane_t), display->n_planes);
	igt_assert_f(display->planes, "Failed to allocate memory for %d planes\n", display->n_planes);

	for (i = 0; i < plane_resources->count_planes; ++i) {
		igt_plane_t *plane = &display->planes[i];
		uint32_t id = plane_resources->planes[i];

		plane->drm_plane = drmModeGetPlane(display->drm_fd, id);
		igt_assert(plane->drm_plane);

		plane->type = get_drm_plane_type(display->drm_fd, id);

		/*
		 * TODO: Fill in the rest of the plane properties here and
		 * move away from the plane per pipe model to align closer
		 * to the DRM KMS model.
		 */
	}

	for_each_pipe(display, i) {
		igt_pipe_t *pipe = &display->pipes[i];
		igt_plane_t *plane;
		int p = 1;
		int j, type;
		uint8_t last_plane = 0, n_planes = 0;

		pipe->crtc_id = resources->crtcs[i];
		pipe->display = display;
		pipe->pipe = i;
		pipe->plane_cursor = -1;
		pipe->plane_primary = -1;
		pipe->planes = NULL;

		igt_fill_pipe_props(display, pipe, IGT_NUM_CRTC_PROPS, igt_crtc_prop_names);

		/* count number of valid planes */
		for (j = 0; j < display->n_planes; j++) {
			drmModePlane *drm_plane = display->planes[j].drm_plane;
			igt_assert(drm_plane);

			if (drm_plane->possible_crtcs & (1 << i))
				n_planes++;
		}

		igt_assert_lt(0, n_planes);
		pipe->planes = calloc(sizeof(igt_plane_t), n_planes);
		igt_assert_f(pipe->planes, "Failed to allocate memory for %d planes\n", n_planes);
		last_plane = n_planes - 1;

		/* add the planes that can be used with that pipe */
		for (j = 0; j < display->n_planes; j++) {
			igt_plane_t *global_plane = &display->planes[j];
			drmModePlane *drm_plane = global_plane->drm_plane;

			if (!(drm_plane->possible_crtcs & (1 << i)))
				continue;

			type = global_plane->type;

			if (type == DRM_PLANE_TYPE_PRIMARY && pipe->plane_primary == -1) {
				plane = &pipe->planes[0];
				plane->index = 0;
				pipe->plane_primary = 0;
			} else if (type == DRM_PLANE_TYPE_CURSOR && pipe->plane_cursor == -1) {
				plane = &pipe->planes[last_plane];
				plane->index = last_plane;
				pipe->plane_cursor = last_plane;
				display->has_cursor_plane = true;
			} else {
				plane = &pipe->planes[p];
				plane->index = p++;
			}

			igt_assert_f(plane->index < n_planes, "n_planes < plane->index failed\n");
			plane->type = type;
			plane->pipe = pipe;
			plane->drm_plane = drm_plane;
			plane->values[IGT_PLANE_IN_FENCE_FD] = ~0ULL;
			plane->ref = global_plane;

			/*
			 * HACK: point the global plane to the first pipe that
			 * it can go on.
			 */
			if (!global_plane->ref)
				igt_plane_set_pipe(plane, pipe);

			igt_fill_plane_props(display, plane, IGT_NUM_PLANE_PROPS, igt_plane_prop_names);

			igt_fill_plane_format_mod(display, plane);
		}

		/*
		 * At the bare minimum, we should expect to have a primary
		 * plane, and it must be in slot 0.
		 */
		igt_assert_eq(pipe->plane_primary, 0);

		/* Check that we filled every slot exactly once */
		if (display->has_cursor_plane)
			igt_assert_eq(p, last_plane);
		else
			igt_assert_eq(p, n_planes);

		pipe->n_planes = n_planes;
	}

	igt_fill_display_format_mod(display);

	/*
	 * The number of connectors is set, so we just initialize the outputs
	 * array in _init(). This may change when we need dynamic connectors
	 * (say DisplayPort MST).
	 */
	display->n_outputs = resources->count_connectors;
	display->outputs = calloc(display->n_outputs, sizeof(igt_output_t));
	igt_assert_f(display->outputs, "Failed to allocate memory for %d outputs\n", display->n_outputs);

	for (i = 0; i < display->n_outputs; i++) {
		igt_output_t *output = &display->outputs[i];
		drmModeConnector *connector;

		/*
		 * We don't assign each output a pipe unless
		 * a pipe is set with igt_output_set_pipe().
		 */
		output->pending_pipe = PIPE_NONE;
		output->id = resources->connectors[i];
		output->display = display;

		igt_output_refresh(output);

		connector = output->config.connector;
		if (connector && (!connector->count_modes ||
		    connector->connection == DRM_MODE_UNKNOWNCONNECTION)) {
			output->force_reprobe = true;
			igt_output_refresh(output);
		}
	}

	drmModeFreePlaneResources(plane_resources);
	drmModeFreeResources(resources);

	/* Set reasonable default values for every object in the display. */
	igt_display_reset(display);

out:
	LOG_UNINDENT(display);

	if (display->n_pipes && display->n_outputs)
		igt_enable_connectors(drm_fd);
	else
		igt_skip("No KMS driver or no outputs, pipes: %d, outputs: %d\n",
			 display->n_pipes, display->n_outputs);
}

/**
 * igt_display_get_n_pipes:
 * @display: A pointer to an #igt_display_t structure
 *
 * Returns total number of pipes for the given @display
 */
int igt_display_get_n_pipes(igt_display_t *display)
{
	return display->n_pipes;
}

/**
 * igt_display_require_output:
 * @display: A pointer to an #igt_display_t structure
 *
 * Checks whether there's a valid @pipe/@output combination for the given @display
 * Skips test if a valid combination of @pipe and @output is not found
 */
void igt_display_require_output(igt_display_t *display)
{
	enum pipe pipe;
	igt_output_t *output;

	for_each_pipe_with_valid_output(display, pipe, output)
		return;

	igt_skip("No valid crtc/connector combinations found.\n");
}

/**
 * igt_display_require_output_on_pipe:
 * @display: A pointer to an #igt_display_t structure
 * @pipe: Display pipe
 *
 * Checks whether there's a valid @pipe/@output combination for the given @display and @pipe
 * Skips test if a valid @pipe is not found
 */
void igt_display_require_output_on_pipe(igt_display_t *display, enum pipe pipe)
{
	igt_output_t *output;

	igt_skip_on_f(pipe >= igt_display_get_n_pipes(display),
		      "Pipe %s does not exist.\n", kmstest_pipe_name(pipe));

	for_each_valid_output_on_pipe(display, pipe, output)
		return;

	igt_skip("No valid connector found on pipe %s\n", kmstest_pipe_name(pipe));
}

/**
 * igt_output_from_connector:
 * @display: a pointer to an #igt_display_t structure
 * @connector: a pointer to a drmModeConnector
 *
 * Finds the output corresponding to the given connector
 *
 * Returns: A #igt_output_t structure configured to use the connector, or NULL
 * if none was found
 */
igt_output_t *igt_output_from_connector(igt_display_t *display,
					drmModeConnector *connector)
{
	igt_output_t *output, *found = NULL;
	int i;

	for (i = 0; i < display->n_outputs; i++) {
		output = &display->outputs[i];

		if (output->config.connector &&
		    output->config.connector->connector_id ==
		    connector->connector_id) {
			found = output;
			break;
		}
	}

	return found;
}

const drmModeModeInfo *igt_std_1024_mode_get(void)
{
	static const drmModeModeInfo std_1024_mode = {
		.clock = 65000,
		.hdisplay = 1024,
		.hsync_start = 1048,
		.hsync_end = 1184,
		.htotal = 1344,
		.hskew = 0,
		.vdisplay = 768,
		.vsync_start = 771,
		.vsync_end = 777,
		.vtotal = 806,
		.vscan = 0,
		.vrefresh = 60,
		.flags = 0xA,
		.type = 0x40,
		.name = "Custom 1024x768",
	};

	return &std_1024_mode;
}

static void igt_pipe_fini(igt_pipe_t *pipe)
{
	free(pipe->planes);
	pipe->planes = NULL;

	if (pipe->out_fence_fd != -1)
		close(pipe->out_fence_fd);
}

static void igt_output_fini(igt_output_t *output)
{
	kmstest_free_connector_config(&output->config);
	free(output->name);
	output->name = NULL;
}

/**
 * igt_display_fini:
 * @display: a pointer to an #igt_display_t structure
 *
 * Release any resources associated with @display. This does not free @display
 * itself.
 */
void igt_display_fini(igt_display_t *display)
{
	int i;

	for (i = 0; i < display->n_planes; ++i) {
		igt_plane_t *plane = &display->planes[i];

		if (plane->drm_plane) {
			drmModeFreePlane(plane->drm_plane);
			plane->drm_plane = NULL;
		}
	}

	for (i = 0; i < display->n_pipes; i++)
		igt_pipe_fini(&display->pipes[i]);

	for (i = 0; i < display->n_outputs; i++)
		igt_output_fini(&display->outputs[i]);
	free(display->outputs);
	display->outputs = NULL;
	free(display->pipes);
	display->pipes = NULL;
	free(display->planes);
	display->planes = NULL;
}

static void igt_display_refresh(igt_display_t *display)
{
	igt_output_t *output;
	int i;

	unsigned long pipes_in_use = 0;

       /* Check that two outputs aren't trying to use the same pipe */
	for (i = 0; i < display->n_outputs; i++) {
		output = &display->outputs[i];

		if (output->pending_pipe != PIPE_NONE) {
			if (pipes_in_use & (1 << output->pending_pipe))
				goto report_dup;

			pipes_in_use |= 1 << output->pending_pipe;
		}

		if (output->force_reprobe)
			igt_output_refresh(output);
	}

	return;

report_dup:
	for (; i > 0; i--) {
		igt_output_t *b = &display->outputs[i - 1];

		igt_assert_f(output->pending_pipe !=
			     b->pending_pipe,
			     "%s and %s are both trying to use pipe %s\n",
			     igt_output_name(output), igt_output_name(b),
			     kmstest_pipe_name(output->pending_pipe));
	}
}

static igt_pipe_t *igt_output_get_driving_pipe(igt_output_t *output)
{
	igt_display_t *display = output->display;
	enum pipe pipe;

	if (output->pending_pipe == PIPE_NONE) {
		/*
		 * The user hasn't specified a pipe to use, return none.
		 */
		return NULL;
	} else {
		/*
		 * Otherwise, return the pending pipe (ie the pipe that should
		 * drive this output after the commit()
		 */
		pipe = output->pending_pipe;
	}

	igt_assert(pipe >= 0 && pipe < display->n_pipes);

	return &display->pipes[pipe];
}

static igt_plane_t *igt_pipe_get_plane(igt_pipe_t *pipe, int plane_idx)
{
	igt_require_f(plane_idx >= 0 && plane_idx < pipe->n_planes,
		      "Valid pipe->planes plane_idx not found, plane_idx=%d n_planes=%d",
		      plane_idx, pipe->n_planes);

	return &pipe->planes[plane_idx];
}

/**
 * igt_pipe_get_plane_type:
 * @pipe: Target pipe
 * @plane_type: Cursor, primary or an overlay plane
 *
 * Finds a valid plane type for the given @pipe otherwise
 * it skips the test if the right combination of @pipe/@plane_type is not found
 *
 * Returns: A #igt_plane_t structure that matches the requested plane type
 */
igt_plane_t *igt_pipe_get_plane_type(igt_pipe_t *pipe, int plane_type)
{
	int i, plane_idx = -1;

	switch(plane_type) {
	case DRM_PLANE_TYPE_CURSOR:
		plane_idx = pipe->plane_cursor;
		break;
	case DRM_PLANE_TYPE_PRIMARY:
		plane_idx = pipe->plane_primary;
		break;
	case DRM_PLANE_TYPE_OVERLAY:
		for(i = 0; i < pipe->n_planes; i++)
			if (pipe->planes[i].type == DRM_PLANE_TYPE_OVERLAY)
			    plane_idx = i;
		break;
	default:
		break;
	}

	igt_require_f(plane_idx >= 0 && plane_idx < pipe->n_planes,
		      "Valid pipe->planes idx not found. plane_idx=%d plane_type=%d n_planes=%d\n",
		      plane_idx, plane_type, pipe->n_planes);

	return &pipe->planes[plane_idx];
}

/**
 * igt_pipe_count_plane_type:
 * @pipe: Target pipe
 * @plane_type: Cursor, primary or an overlay plane
 *
 * Counts the number of planes of type @plane_type for the provided @pipe.
 *
 * Returns: The number of planes that match the requested plane type
 */
int igt_pipe_count_plane_type(igt_pipe_t *pipe, int plane_type)
{
	int i, count = 0;

	for(i = 0; i < pipe->n_planes; i++)
		if (pipe->planes[i].type == plane_type)
			count++;

	return count;
}

/**
 * igt_pipe_get_plane_type_index:
 * @pipe: Target pipe
 * @plane_type: Cursor, primary or an overlay plane
 * @index: the index of the plane among planes of the same type
 *
 * Get the @index th plane of type @plane_type for the provided @pipe.
 *
 * Returns: The @index th plane that matches the requested plane type
 */
igt_plane_t *igt_pipe_get_plane_type_index(igt_pipe_t *pipe, int plane_type,
					   int index)
{
	int i, type_index = 0;

	for(i = 0; i < pipe->n_planes; i++) {
		if (pipe->planes[i].type != plane_type)
			continue;

		if (type_index == index)
			return &pipe->planes[i];

		type_index++;
	}

	return NULL;
}

static bool output_is_internal_panel(igt_output_t *output)
{
	switch (output->config.connector->connector_type) {
	case DRM_MODE_CONNECTOR_LVDS:
	case DRM_MODE_CONNECTOR_eDP:
	case DRM_MODE_CONNECTOR_DSI:
	case DRM_MODE_CONNECTOR_DPI:
		return true;
	default:
		return false;
	}
}

igt_output_t **__igt_pipe_populate_outputs(igt_display_t *display, igt_output_t **chosen_outputs)
{
	unsigned full_pipe_mask = (1 << (display->n_pipes)) - 1, assigned_pipes = 0;
	igt_output_t *output;
	int i, j;

	memset(chosen_outputs, 0, sizeof(*chosen_outputs) * display->n_pipes);

	/*
	 * Try to assign all outputs to the first available CRTC for
	 * it, start with the outputs restricted to 1 pipe, then increase
	 * number of pipes until we assign connectors to all pipes.
	 */
	for (i = 0; i <= display->n_pipes; i++) {
		for_each_connected_output(display, output) {
			uint32_t pipe_mask = output->config.valid_crtc_idx_mask & full_pipe_mask;
			bool found = false;

			if (output_is_internal_panel(output)) {
				/*
				 * Internal panel should be assigned to pipe A
				 * if possible, so make sure they're enumerated
				 * first.
				 */

				if (i)
					continue;
			} else if (__builtin_popcount(pipe_mask) != i)
				continue;

			for (j = 0; j < display->n_pipes; j++) {
				bool pipe_assigned = assigned_pipes & (1 << j);

				if (pipe_assigned || !(pipe_mask & (1 << j)))
					continue;

				if (!found) {
					/* We found an unassigned pipe, use it! */
					found = true;
					assigned_pipes |= 1 << j;
					chosen_outputs[j] = output;
				} else if (!chosen_outputs[j] ||
					   /*
					    * Overwrite internal panel if not assigned,
					    * external outputs are faster to do modesets
					    */
					   output_is_internal_panel(chosen_outputs[j]))
					chosen_outputs[j] = output;
			}

			if (!found)
				igt_warn("Output %s could not be assigned to a pipe\n",
					 igt_output_name(output));
		}
	}

	return chosen_outputs;
}

/**
 * igt_get_single_output_for_pipe:
 * @display: a pointer to an #igt_display_t structure
 * @pipe: The pipe for which an #igt_output_t must be returned.
 *
 * Get a compatible output for a pipe.
 *
 * Returns: A compatible output for a given pipe, or NULL.
 */
igt_output_t *igt_get_single_output_for_pipe(igt_display_t *display, enum pipe pipe)
{
	igt_output_t *chosen_outputs[display->n_pipes];

	igt_assert(pipe != PIPE_NONE);
	igt_require(pipe < display->n_pipes);

	__igt_pipe_populate_outputs(display, chosen_outputs);

	return chosen_outputs[pipe];
}

static igt_output_t *igt_pipe_get_output(igt_pipe_t *pipe)
{
	igt_display_t *display = pipe->display;
	int i;

	for (i = 0; i < display->n_outputs; i++) {
		igt_output_t *output = &display->outputs[i];

		if (output->pending_pipe == pipe->pipe)
			return output;
	}

	return NULL;
}

static uint32_t igt_plane_get_fb_id(igt_plane_t *plane)
{
	return plane->values[IGT_PLANE_FB_ID];
}

#define CHECK_RETURN(r, fail) {	\
	if (r && !fail)		\
		return r;	\
	igt_assert_eq(r, 0);	\
}

/*
 * Add position and fb changes of a plane to the atomic property set
 */
static void
igt_atomic_prepare_plane_commit(igt_plane_t *plane, igt_pipe_t *pipe,
	drmModeAtomicReq *req)
{
	igt_display_t *display = pipe->display;
	int i;

	igt_assert(plane->drm_plane);

	LOG(display,
	    "populating plane data: %s.%d, fb %u\n",
	    kmstest_pipe_name(pipe->pipe),
	    plane->index,
	    igt_plane_get_fb_id(plane));

	for (i = 0; i < IGT_NUM_PLANE_PROPS; i++) {
		if (!igt_plane_is_prop_changed(plane, i))
			continue;

		/* it's an error to try an unsupported feature */
		igt_assert(plane->props[i]);

		igt_debug("plane %s.%d: Setting property \"%s\" to 0x%"PRIx64"/%"PRIi64"\n",
			kmstest_pipe_name(pipe->pipe), plane->index, igt_plane_prop_names[i],
			plane->values[i], plane->values[i]);

		igt_assert_lt(0, drmModeAtomicAddProperty(req, plane->drm_plane->plane_id,
						  plane->props[i],
						  plane->values[i]));
	}
}

/*
 * Properties that can be changed through legacy SetProperty:
 * - Obviously not the XYWH SRC/CRTC coordinates.
 * - Not CRTC_ID or FENCE_ID, done through SetPlane.
 * - Can't set IN_FENCE_FD, that would be silly.
 *
 * Theoretically the above can all be set through the legacy path
 * with the atomic cap set, but that's not how our legacy plane
 * commit behaves, so blacklist it by default.
 */
#define LEGACY_PLANE_COMMIT_MASK \
	(((1ULL << IGT_NUM_PLANE_PROPS) - 1) & \
	 ~(IGT_PLANE_COORD_CHANGED_MASK | \
	   (1ULL << IGT_PLANE_FB_ID) | \
	   (1ULL << IGT_PLANE_CRTC_ID) | \
	   (1ULL << IGT_PLANE_IN_FENCE_FD)))

/*
 * Commit position and fb changes to a DRM plane via the SetPlane ioctl; if the
 * DRM call to program the plane fails, we'll either fail immediately (for
 * tests that expect the commit to succeed) or return the failure code (for
 * tests that expect a specific error code).
 */
static int igt_drm_plane_commit(igt_plane_t *plane,
				igt_pipe_t *pipe,
				bool fail_on_error)
{
	igt_display_t *display = pipe->display;
	uint32_t fb_id, crtc_id;
	int ret, i;
	uint32_t src_x;
	uint32_t src_y;
	uint32_t src_w;
	uint32_t src_h;
	int32_t crtc_x;
	int32_t crtc_y;
	uint32_t crtc_w;
	uint32_t crtc_h;
	uint64_t changed_mask;
	bool setplane =
		igt_plane_is_prop_changed(plane, IGT_PLANE_FB_ID) ||
		plane->changed & IGT_PLANE_COORD_CHANGED_MASK;

	igt_assert(plane->drm_plane);

	fb_id = igt_plane_get_fb_id(plane);
	crtc_id = pipe->crtc_id;

	if (setplane && fb_id == 0) {
		LOG(display,
		    "SetPlane pipe %s, plane %d, disabling\n",
		    kmstest_pipe_name(pipe->pipe),
		    plane->index);

		ret = drmModeSetPlane(display->drm_fd,
				      plane->drm_plane->plane_id,
				      crtc_id,
				      fb_id,
				      0,    /* flags */
				      0, 0, /* crtc_x, crtc_y */
				      0, 0, /* crtc_w, crtc_h */
				      IGT_FIXED(0,0), /* src_x */
				      IGT_FIXED(0,0), /* src_y */
				      IGT_FIXED(0,0), /* src_w */
				      IGT_FIXED(0,0) /* src_h */);

		CHECK_RETURN(ret, fail_on_error);
	} else if (setplane) {
		src_x = plane->values[IGT_PLANE_SRC_X];
		src_y = plane->values[IGT_PLANE_SRC_Y];
		src_w = plane->values[IGT_PLANE_SRC_W];
		src_h = plane->values[IGT_PLANE_SRC_H];
		crtc_x = plane->values[IGT_PLANE_CRTC_X];
		crtc_y = plane->values[IGT_PLANE_CRTC_Y];
		crtc_w = plane->values[IGT_PLANE_CRTC_W];
		crtc_h = plane->values[IGT_PLANE_CRTC_H];

		LOG(display,
		    "SetPlane %s.%d, fb %u, src = (%d, %d) "
			"%ux%u dst = (%u, %u) %ux%u\n",
		    kmstest_pipe_name(pipe->pipe),
		    plane->index,
		    fb_id,
		    src_x >> 16, src_y >> 16, src_w >> 16, src_h >> 16,
		    crtc_x, crtc_y, crtc_w, crtc_h);

		ret = drmModeSetPlane(display->drm_fd,
				      plane->drm_plane->plane_id,
				      crtc_id,
				      fb_id,
				      0,    /* flags */
				      crtc_x, crtc_y,
				      crtc_w, crtc_h,
				      src_x, src_y,
				      src_w, src_h);

		CHECK_RETURN(ret, fail_on_error);
	}

	changed_mask = plane->changed & LEGACY_PLANE_COMMIT_MASK;

	for (i = 0; i < IGT_NUM_PLANE_PROPS; i++) {
		if (!(changed_mask & (1 << i)))
			continue;

		LOG(display, "SetProp plane %s.%d \"%s\" to 0x%"PRIx64"/%"PRIi64"\n",
			kmstest_pipe_name(pipe->pipe), plane->index, igt_plane_prop_names[i],
			plane->values[i], plane->values[i]);

		igt_assert(plane->props[i]);

		ret = igt_plane_set_property(plane,
					     plane->props[i],
					     plane->values[i]);

		CHECK_RETURN(ret, fail_on_error);
	}

	return 0;
}

/*
 * Commit position and fb changes to a cursor via legacy ioctl's.  If commit
 * fails, we'll either fail immediately (for tests that expect the commit to
 * succeed) or return the failure code (for tests that expect a specific error
 * code).
 */
static int igt_cursor_commit_legacy(igt_plane_t *cursor,
				    igt_pipe_t *pipe,
				    bool fail_on_error)
{
	igt_display_t *display = pipe->display;
	uint32_t crtc_id = pipe->crtc_id;
	int ret;

	if (igt_plane_is_prop_changed(cursor, IGT_PLANE_FB_ID) ||
	    igt_plane_is_prop_changed(cursor, IGT_PLANE_CRTC_W) ||
	    igt_plane_is_prop_changed(cursor, IGT_PLANE_CRTC_H)) {
		if (cursor->gem_handle)
			LOG(display,
			    "SetCursor pipe %s, fb %u %dx%d\n",
			    kmstest_pipe_name(pipe->pipe),
			    cursor->gem_handle,
			    (unsigned)cursor->values[IGT_PLANE_CRTC_W],
			    (unsigned)cursor->values[IGT_PLANE_CRTC_H]);
		else
			LOG(display,
			    "SetCursor pipe %s, disabling\n",
			    kmstest_pipe_name(pipe->pipe));

		ret = drmModeSetCursor(display->drm_fd, crtc_id,
				       cursor->gem_handle,
				       cursor->values[IGT_PLANE_CRTC_W],
				       cursor->values[IGT_PLANE_CRTC_H]);
		CHECK_RETURN(ret, fail_on_error);
	}

	if (igt_plane_is_prop_changed(cursor, IGT_PLANE_CRTC_X) ||
	    igt_plane_is_prop_changed(cursor, IGT_PLANE_CRTC_Y)) {
		int x = cursor->values[IGT_PLANE_CRTC_X];
		int y = cursor->values[IGT_PLANE_CRTC_Y];

		LOG(display,
		    "MoveCursor pipe %s, (%d, %d)\n",
		    kmstest_pipe_name(pipe->pipe),
		    x, y);

		ret = drmModeMoveCursor(display->drm_fd, crtc_id, x, y);
		CHECK_RETURN(ret, fail_on_error);
	}

	return 0;
}

/*
 * Commit position and fb changes to a primary plane via the legacy interface
 * (setmode).
 */
static int igt_primary_plane_commit_legacy(igt_plane_t *primary,
					   igt_pipe_t *pipe,
					   bool fail_on_error)
{
	struct igt_display *display = primary->pipe->display;
	igt_output_t *output = igt_pipe_get_output(pipe);
	drmModeModeInfo *mode;
	uint32_t fb_id, crtc_id;
	int ret;

	/* Primary planes can't be windowed when using a legacy commit */
	igt_assert((primary->values[IGT_PLANE_CRTC_X] == 0 && primary->values[IGT_PLANE_CRTC_Y] == 0));

	/* nor rotated */
	if (!pipe->display->first_commit)
		igt_assert(!igt_plane_is_prop_changed(primary, IGT_PLANE_ROTATION));

	if (!igt_plane_is_prop_changed(primary, IGT_PLANE_FB_ID) &&
	    !(primary->changed & IGT_PLANE_COORD_CHANGED_MASK) &&
	    !igt_pipe_obj_is_prop_changed(primary->pipe, IGT_CRTC_MODE_ID))
		return 0;

	crtc_id = pipe->crtc_id;
	fb_id = output ? igt_plane_get_fb_id(primary) : 0;
	if (fb_id)
		mode = igt_output_get_mode(output);
	else
		mode = NULL;

	if (fb_id) {
		uint32_t src_x = primary->values[IGT_PLANE_SRC_X] >> 16;
		uint32_t src_y = primary->values[IGT_PLANE_SRC_Y] >> 16;

		LOG(display,
		    "%s: SetCrtc pipe %s, fb %u, src (%d, %d), "
		    "mode %dx%d\n",
		    igt_output_name(output),
		    kmstest_pipe_name(pipe->pipe),
		    fb_id,
		    src_x, src_y,
		    mode->hdisplay, mode->vdisplay);

		ret = drmModeSetCrtc(display->drm_fd,
				     crtc_id,
				     fb_id,
				     src_x, src_y,
				     &output->id,
				     1,
				     mode);
	} else {
		LOG(display,
		    "SetCrtc pipe %s, disabling\n",
		    kmstest_pipe_name(pipe->pipe));

		ret = drmModeSetCrtc(display->drm_fd,
				     crtc_id,
				     fb_id,
				     0, 0, /* x, y */
				     NULL, /* connectors */
				     0,    /* n_connectors */
				     NULL  /* mode */);
	}

	CHECK_RETURN(ret, fail_on_error);

	return 0;
}

static int igt_plane_fixup_rotation(igt_plane_t *plane,
				    igt_pipe_t *pipe)
{
	int ret;

	if (!igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
		return 0;

	LOG(pipe->display, "Fixing up initial rotation pipe %s, plane %d\n",
	    kmstest_pipe_name(pipe->pipe), plane->index);

	/* First try the easy case, can we change rotation without problems? */
	ret = igt_plane_set_property(plane, plane->props[IGT_PLANE_ROTATION],
				     plane->values[IGT_PLANE_ROTATION]);
	if (!ret)
		return 0;

	/* Disable the plane, while we tinker with rotation */
	ret = drmModeSetPlane(pipe->display->drm_fd,
			      plane->drm_plane->plane_id,
			      pipe->crtc_id, 0, /* fb_id */
			      0, /* flags */
			      0, 0, 0, 0, /* crtc_x, crtc_y, crtc_w, crtc_h */
			      IGT_FIXED(0,0), IGT_FIXED(0,0), /* src_x, src_y */
			      IGT_FIXED(0,0), IGT_FIXED(0,0)); /* src_w, src_h */

	if (ret && plane->type != DRM_PLANE_TYPE_PRIMARY)
		return ret;

	/* For primary plane, fall back to disabling the crtc. */
	if (ret) {
		ret = drmModeSetCrtc(pipe->display->drm_fd,
				     pipe->crtc_id, 0, 0, 0, NULL, 0, NULL);

		if (ret)
			return ret;
	}

	/* and finally, set rotation property. */
	return igt_plane_set_property(plane, plane->props[IGT_PLANE_ROTATION],
				      plane->values[IGT_PLANE_ROTATION]);
}

/*
 * Commit position and fb changes to a plane.  The value of @s will determine
 * which API is used to do the programming.
 */
static int igt_plane_commit(igt_plane_t *plane,
			    igt_pipe_t *pipe,
			    enum igt_commit_style s,
			    bool fail_on_error)
{
	if (pipe->display->first_commit || (s == COMMIT_UNIVERSAL &&
	     igt_plane_is_prop_changed(plane, IGT_PLANE_ROTATION))) {
		int ret;

		ret = igt_plane_fixup_rotation(plane, pipe);
		CHECK_RETURN(ret, fail_on_error);
	}

	if (plane->type == DRM_PLANE_TYPE_CURSOR && s == COMMIT_LEGACY) {
		return igt_cursor_commit_legacy(plane, pipe, fail_on_error);
	} else if (plane->type == DRM_PLANE_TYPE_PRIMARY && s == COMMIT_LEGACY) {
		return igt_primary_plane_commit_legacy(plane, pipe,
						       fail_on_error);
	} else {
		return igt_drm_plane_commit(plane, pipe, fail_on_error);
	}
}

static bool is_atomic_prop(enum igt_atomic_crtc_properties prop)
{
       if (prop == IGT_CRTC_MODE_ID ||
	   prop == IGT_CRTC_ACTIVE ||
	   prop == IGT_CRTC_OUT_FENCE_PTR)
		return true;

	return false;
}

/*
 * Commit all plane changes to an output.  Note that if @s is COMMIT_LEGACY,
 * enabling/disabling the primary plane will also enable/disable the CRTC.
 *
 * If @fail_on_error is true, any failure to commit plane state will lead
 * to subtest failure in the specific function where the failure occurs.
 * Otherwise, the first error code encountered will be returned and no
 * further programming will take place, which may result in some changes
 * taking effect and others not taking effect.
 */
static int igt_pipe_commit(igt_pipe_t *pipe,
			   enum igt_commit_style s,
			   bool fail_on_error)
{
	int i;
	int ret;

	for (i = 0; i < IGT_NUM_CRTC_PROPS; i++)
		if (igt_pipe_obj_is_prop_changed(pipe, i) &&
		    !is_atomic_prop(i)) {
			igt_assert(pipe->props[i]);

			ret = drmModeObjectSetProperty(pipe->display->drm_fd,
				pipe->crtc_id, DRM_MODE_OBJECT_CRTC,
				pipe->props[i], pipe->values[i]);

			CHECK_RETURN(ret, fail_on_error);
		}

	for (i = 0; i < pipe->n_planes; i++) {
		igt_plane_t *plane = &pipe->planes[i];

		/* skip planes that are handled by another pipe */
		if (plane->ref->pipe != pipe)
			continue;

		ret = igt_plane_commit(plane, pipe, s, fail_on_error);
		CHECK_RETURN(ret, fail_on_error);
	}

	return 0;
}

static int igt_output_commit(igt_output_t *output,
			     enum igt_commit_style s,
			     bool fail_on_error)
{
	int i, ret;

	for (i = 0; i < IGT_NUM_CONNECTOR_PROPS; i++) {
		if (!igt_output_is_prop_changed(output, i))
			continue;

		/* CRTC_ID is set by calling drmModeSetCrtc in the legacy path. */
		if (i == IGT_CONNECTOR_CRTC_ID)
			continue;

		igt_assert(output->props[i]);

		if (s == COMMIT_LEGACY)
			ret = drmModeConnectorSetProperty(output->display->drm_fd, output->id,
							  output->props[i], output->values[i]);
		else
			ret = drmModeObjectSetProperty(output->display->drm_fd, output->id,
						       DRM_MODE_OBJECT_CONNECTOR,
						       output->props[i], output->values[i]);

		CHECK_RETURN(ret, fail_on_error);
	}

	return 0;
}

static uint64_t igt_mode_object_get_prop(igt_display_t *display,
					 uint32_t object_type,
					 uint32_t object_id,
					 uint32_t prop)
{
	drmModeObjectPropertiesPtr proplist;
	bool found = false;
	int i;
	uint64_t ret;

	proplist = drmModeObjectGetProperties(display->drm_fd, object_id, object_type);
	for (i = 0; i < proplist->count_props; i++) {
		if (proplist->props[i] != prop)
			continue;

		found = true;
		break;
	}

	igt_assert(found);

	ret = proplist->prop_values[i];

	drmModeFreeObjectProperties(proplist);
	return ret;
}

/**
 * igt_plane_get_prop:
 * @plane: Target plane.
 * @prop: Property to check.
 *
 * Return current value on a plane for a given property.
 *
 * Returns: The value the property is set to, if this
 * is a blob, the blob id is returned. This can be passed
 * to drmModeGetPropertyBlob() to get the contents of the blob.
 */
uint64_t igt_plane_get_prop(igt_plane_t *plane, enum igt_atomic_plane_properties prop)
{
	igt_assert(igt_plane_has_prop(plane, prop));

	return igt_mode_object_get_prop(plane->pipe->display, DRM_MODE_OBJECT_PLANE,
					plane->drm_plane->plane_id, plane->props[prop]);
}

static bool igt_mode_object_get_prop_enum_value(int drm_fd, uint32_t id, const char *str, uint64_t *val)
{
	drmModePropertyPtr prop = drmModeGetProperty(drm_fd, id);
	int i;

	igt_assert(id);
	igt_assert(prop);

	for (i = 0; i < prop->count_enums; i++)
		if (!strcmp(str, prop->enums[i].name)) {
			*val = prop->enums[i].value;
			drmModeFreeProperty(prop);
			return true;
		}

	return false;
}

bool igt_plane_try_prop_enum(igt_plane_t *plane,
			     enum igt_atomic_plane_properties prop,
			     const char *val)
{
	igt_display_t *display = plane->pipe->display;
	uint64_t uval;

	igt_assert(plane->props[prop]);

	if (!igt_mode_object_get_prop_enum_value(display->drm_fd,
						 plane->props[prop], val, &uval))
		return false;

	igt_plane_set_prop_value(plane, prop, uval);
	return true;
}

void igt_plane_set_prop_enum(igt_plane_t *plane,
			     enum igt_atomic_plane_properties prop,
			     const char *val)
{
	igt_assert(igt_plane_try_prop_enum(plane, prop, val));
}

/**
 * igt_plane_replace_prop_blob:
 * @plane: plane to set property on.
 * @prop: property for which the blob will be replaced.
 * @ptr: Pointer to contents for the property.
 * @length: Length of contents.
 *
 * This function will destroy the old property blob for the given property,
 * and will create a new property blob with the values passed to this function.
 *
 * The new property blob will be committed when you call igt_display_commit(),
 * igt_display_commit2() or igt_display_commit_atomic().
 */
void
igt_plane_replace_prop_blob(igt_plane_t *plane, enum igt_atomic_plane_properties prop, const void *ptr, size_t length)
{
	igt_display_t *display = plane->pipe->display;
	uint64_t *blob = &plane->values[prop];
	uint32_t blob_id = 0;

	if (*blob != 0)
		igt_assert(drmModeDestroyPropertyBlob(display->drm_fd,
						      *blob) == 0);

	if (length > 0)
		igt_assert(drmModeCreatePropertyBlob(display->drm_fd,
						     ptr, length, &blob_id) == 0);

	*blob = blob_id;
	igt_plane_set_prop_changed(plane, prop);
}

/**
 * igt_output_get_prop:
 * @output: Target output.
 * @prop: Property to return.
 *
 * Return current value on an output for a given property.
 *
 * Returns: The value the property is set to, if this
 * is a blob, the blob id is returned. This can be passed
 * to drmModeGetPropertyBlob() to get the contents of the blob.
 */
uint64_t igt_output_get_prop(igt_output_t *output, enum igt_atomic_connector_properties prop)
{
	igt_assert(igt_output_has_prop(output, prop));

	return igt_mode_object_get_prop(output->display, DRM_MODE_OBJECT_CONNECTOR,
					output->id, output->props[prop]);
}

bool igt_output_try_prop_enum(igt_output_t *output,
			      enum igt_atomic_connector_properties prop,
			      const char *val)
{
	igt_display_t *display = output->display;
	uint64_t uval;

	igt_assert(output->props[prop]);

	if (!igt_mode_object_get_prop_enum_value(display->drm_fd,
						 output->props[prop], val, &uval))
		return false;

	igt_output_set_prop_value(output, prop, uval);
	return true;
}

void igt_output_set_prop_enum(igt_output_t *output,
			      enum igt_atomic_connector_properties prop,
			      const char *val)
{
	igt_assert(igt_output_try_prop_enum(output, prop, val));
}

/**
 * igt_output_replace_prop_blob:
 * @output: output to set property on.
 * @prop: property for which the blob will be replaced.
 * @ptr: Pointer to contents for the property.
 * @length: Length of contents.
 *
 * This function will destroy the old property blob for the given property,
 * and will create a new property blob with the values passed to this function.
 *
 * The new property blob will be committed when you call igt_display_commit(),
 * igt_display_commit2() or igt_display_commit_atomic().
 */
void
igt_output_replace_prop_blob(igt_output_t *output, enum igt_atomic_connector_properties prop, const void *ptr, size_t length)
{
	igt_display_t *display = output->display;
	uint64_t *blob = &output->values[prop];
	uint32_t blob_id = 0;

	if (*blob != 0)
		igt_assert(drmModeDestroyPropertyBlob(display->drm_fd,
						      *blob) == 0);

	if (length > 0)
		igt_assert(drmModeCreatePropertyBlob(display->drm_fd,
						     ptr, length, &blob_id) == 0);

	*blob = blob_id;
	igt_output_set_prop_changed(output, prop);
}

/**
 * igt_pipe_obj_get_prop:
 * @pipe: Target pipe.
 * @prop: Property to return.
 *
 * Return current value on a pipe for a given property.
 *
 * Returns: The value the property is set to, if this
 * is a blob, the blob id is returned. This can be passed
 * to drmModeGetPropertyBlob() to get the contents of the blob.
 */
uint64_t igt_pipe_obj_get_prop(igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop)
{
	igt_assert(igt_pipe_obj_has_prop(pipe, prop));

	return igt_mode_object_get_prop(pipe->display, DRM_MODE_OBJECT_CRTC,
					pipe->crtc_id, pipe->props[prop]);
}

bool igt_pipe_obj_try_prop_enum(igt_pipe_t *pipe_obj,
				enum igt_atomic_crtc_properties prop,
				const char *val)
{
	igt_display_t *display = pipe_obj->display;
	uint64_t uval;

	igt_assert(pipe_obj->props[prop]);

	if (!igt_mode_object_get_prop_enum_value(display->drm_fd,
						 pipe_obj->props[prop], val, &uval))
		return false;

	igt_pipe_obj_set_prop_value(pipe_obj, prop, uval);
	return true;
}

void igt_pipe_obj_set_prop_enum(igt_pipe_t *pipe_obj,
				enum igt_atomic_crtc_properties prop,
				const char *val)
{
	igt_assert(igt_pipe_obj_try_prop_enum(pipe_obj, prop, val));
}

/**
 * igt_pipe_obj_replace_prop_blob:
 * @pipe: pipe to set property on.
 * @prop: property for which the blob will be replaced.
 * @ptr: Pointer to contents for the property.
 * @length: Length of contents.
 *
 * This function will destroy the old property blob for the given property,
 * and will create a new property blob with the values passed to this function.
 *
 * The new property blob will be committed when you call igt_display_commit(),
 * igt_display_commit2() or igt_display_commit_atomic().
 *
 * Please use igt_output_override_mode() if you want to set #IGT_CRTC_MODE_ID,
 * it works better with legacy commit.
 */
void
igt_pipe_obj_replace_prop_blob(igt_pipe_t *pipe, enum igt_atomic_crtc_properties prop, const void *ptr, size_t length)
{
	igt_display_t *display = pipe->display;
	uint64_t *blob = &pipe->values[prop];
	uint32_t blob_id = 0;

	if (*blob != 0)
		igt_assert(drmModeDestroyPropertyBlob(display->drm_fd,
						      *blob) == 0);

	if (length > 0)
		igt_assert(drmModeCreatePropertyBlob(display->drm_fd,
						     ptr, length, &blob_id) == 0);

	*blob = blob_id;
	igt_pipe_obj_set_prop_changed(pipe, prop);
}

/*
 * Add crtc property changes to the atomic property set
 */
static void igt_atomic_prepare_crtc_commit(igt_pipe_t *pipe_obj, drmModeAtomicReq *req)
{
	int i;

	for (i = 0; i < IGT_NUM_CRTC_PROPS; i++) {
		if (!igt_pipe_obj_is_prop_changed(pipe_obj, i))
			continue;

		igt_debug("Pipe %s: Setting property \"%s\" to 0x%"PRIx64"/%"PRIi64"\n",
			kmstest_pipe_name(pipe_obj->pipe), igt_crtc_prop_names[i],
			pipe_obj->values[i], pipe_obj->values[i]);

		igt_assert_lt(0, drmModeAtomicAddProperty(req, pipe_obj->crtc_id, pipe_obj->props[i], pipe_obj->values[i]));
	}

	if (pipe_obj->out_fence_fd != -1) {
		close(pipe_obj->out_fence_fd);
		pipe_obj->out_fence_fd = -1;
	}
}

/*
 * Add connector property changes to the atomic property set
 */
static void igt_atomic_prepare_connector_commit(igt_output_t *output, drmModeAtomicReq *req)
{

	int i;

	for (i = 0; i < IGT_NUM_CONNECTOR_PROPS; i++) {
		if (!igt_output_is_prop_changed(output, i))
			continue;

		/* it's an error to try an unsupported feature */
		igt_assert(output->props[i]);

		igt_debug("%s: Setting property \"%s\" to 0x%"PRIx64"/%"PRIi64"\n",
			  igt_output_name(output), igt_connector_prop_names[i],
			  output->values[i], output->values[i]);

		igt_assert_lt(0, drmModeAtomicAddProperty(req,
					  output->config.connector->connector_id,
					  output->props[i],
					  output->values[i]));
	}
}

/*
 * Commit all the changes of all the planes,crtcs, connectors
 * atomically using drmModeAtomicCommit()
 */
static int igt_atomic_commit(igt_display_t *display, uint32_t flags, void *user_data)
{

	int ret = 0, i;
	enum pipe pipe;
	drmModeAtomicReq *req;
	igt_output_t *output;

	if (display->is_atomic != 1)
		return -1;
	req = drmModeAtomicAlloc();

	for_each_pipe(display, pipe) {
		igt_pipe_t *pipe_obj = &display->pipes[pipe];
		igt_plane_t *plane;

		/*
		 * Add CRTC Properties to the property set
		 */
		if (pipe_obj->changed)
			igt_atomic_prepare_crtc_commit(pipe_obj, req);

		for_each_plane_on_pipe(display, pipe, plane) {
			/* skip planes that are handled by another pipe */
			if (plane->ref->pipe != pipe_obj)
				continue;

			if (plane->changed)
				igt_atomic_prepare_plane_commit(plane, pipe_obj, req);
		}

	}

	for (i = 0; i < display->n_outputs; i++) {
		output = &display->outputs[i];

		if (!output->config.connector || !output->changed)
			continue;

		LOG(display, "%s: preparing atomic, pipe: %s\n",
		    igt_output_name(output),
		    kmstest_pipe_name(output->config.pipe));

		igt_atomic_prepare_connector_commit(output, req);
	}

	ret = drmModeAtomicCommit(display->drm_fd, req, flags, user_data);

	drmModeAtomicFree(req);
	return ret;

}

static void
display_commit_changed(igt_display_t *display, enum igt_commit_style s)
{
	int i;
	enum pipe pipe;

	for_each_pipe(display, pipe) {
		igt_pipe_t *pipe_obj = &display->pipes[pipe];
		igt_plane_t *plane;

		if (s == COMMIT_ATOMIC) {
			if (igt_pipe_obj_is_prop_changed(pipe_obj, IGT_CRTC_OUT_FENCE_PTR))
				igt_assert(pipe_obj->out_fence_fd >= 0);

			pipe_obj->values[IGT_CRTC_OUT_FENCE_PTR] = 0;
			pipe_obj->changed = 0;
		} else {
			for (i = 0; i < IGT_NUM_CRTC_PROPS; i++)
				if (!is_atomic_prop(i))
					igt_pipe_obj_clear_prop_changed(pipe_obj, i);

			if (s != COMMIT_UNIVERSAL) {
				igt_pipe_obj_clear_prop_changed(pipe_obj, IGT_CRTC_MODE_ID);
				igt_pipe_obj_clear_prop_changed(pipe_obj, IGT_CRTC_ACTIVE);
			}
		}

		for_each_plane_on_pipe(display, pipe, plane) {
			if (s == COMMIT_ATOMIC) {
				int fd;
				plane->changed = 0;

				fd = plane->values[IGT_PLANE_IN_FENCE_FD];
				if (fd != -1)
					close(fd);

				/* reset fence_fd to prevent it from being set for the next commit */
				plane->values[IGT_PLANE_IN_FENCE_FD] = -1;
			} else {
				plane->changed &= ~IGT_PLANE_COORD_CHANGED_MASK;

				igt_plane_clear_prop_changed(plane, IGT_PLANE_CRTC_ID);
				igt_plane_clear_prop_changed(plane, IGT_PLANE_FB_ID);

				if (s != COMMIT_LEGACY ||
				    !(plane->type == DRM_PLANE_TYPE_PRIMARY ||
				      plane->type == DRM_PLANE_TYPE_CURSOR))
					plane->changed &= ~LEGACY_PLANE_COMMIT_MASK;

				if (display->first_commit)
					igt_plane_clear_prop_changed(plane, IGT_PLANE_ROTATION);
			}
		}
	}

	for (i = 0; i < display->n_outputs; i++) {
		igt_output_t *output = &display->outputs[i];

		if (s != COMMIT_UNIVERSAL)
			output->changed = 0;
		else
			/* no modeset in universal commit, no change to crtc. */
			output->changed &= 1 << IGT_CONNECTOR_CRTC_ID;
	}

	if (display->first_commit) {
		igt_reset_fifo_underrun_reporting(display->drm_fd);

		igt_display_drop_events(display);

		display->first_commit = false;
	}
}

/*
 * Commit all plane changes across all outputs of the display.
 *
 * If @fail_on_error is true, any failure to commit plane state will lead
 * to subtest failure in the specific function where the failure occurs.
 * Otherwise, the first error code encountered will be returned and no
 * further programming will take place, which may result in some changes
 * taking effect and others not taking effect.
 */
static int do_display_commit(igt_display_t *display,
			     enum igt_commit_style s,
			     bool fail_on_error)
{
	int i, ret = 0;
	enum pipe pipe;
	LOG_INDENT(display, "commit");

	/* someone managed to bypass igt_display_require, catch them */
	assert(display->n_pipes && display->n_outputs);

	igt_display_refresh(display);

	if (s == COMMIT_ATOMIC) {
		ret = igt_atomic_commit(display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	} else {
		for_each_pipe(display, pipe) {
			igt_pipe_t *pipe_obj = &display->pipes[pipe];

			ret = igt_pipe_commit(pipe_obj, s, fail_on_error);
			if (ret)
				break;
		}

		for (i = 0; !ret && i < display->n_outputs; i++)
			ret = igt_output_commit(&display->outputs[i], s, fail_on_error);
	}

	LOG_UNINDENT(display);
	CHECK_RETURN(ret, fail_on_error);

	display_commit_changed(display, s);

	igt_debug_wait_for_keypress("modeset");

	return 0;
}

/**
 * igt_display_try_commit_atomic:
 * @display: #igt_display_t to commit.
 * @flags: Flags passed to drmModeAtomicCommit.
 * @user_data: User defined pointer passed to drmModeAtomicCommit.
 *
 * This function is similar to #igt_display_try_commit2, but is
 * used when you want to pass different flags to the actual commit.
 *
 * Useful flags can be DRM_MODE_ATOMIC_ALLOW_MODESET,
 * DRM_MODE_ATOMIC_NONBLOCK, DRM_MODE_PAGE_FLIP_EVENT,
 * or DRM_MODE_ATOMIC_TEST_ONLY.
 *
 * @user_data is returned in the event if you pass
 * DRM_MODE_PAGE_FLIP_EVENT to @flags.
 *
 * This function will return an error if commit fails, instead of
 * aborting the test.
 */
int igt_display_try_commit_atomic(igt_display_t *display, uint32_t flags, void *user_data)
{
	int ret;

	/* someone managed to bypass igt_display_require, catch them */
	assert(display->n_pipes && display->n_outputs);

	LOG_INDENT(display, "commit");

	igt_display_refresh(display);

	ret = igt_atomic_commit(display, flags, user_data);

	LOG_UNINDENT(display);

	if (ret || (flags & DRM_MODE_ATOMIC_TEST_ONLY))
		return ret;

	if (display->first_commit)
		igt_fail_on_f(flags & (DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK),
			      "First commit has to drop all stale events\n");

	display_commit_changed(display, COMMIT_ATOMIC);

	igt_debug_wait_for_keypress("modeset");

	return 0;
}

/**
 * igt_display_commit_atomic:
 * @display: #igt_display_t to commit.
 * @flags: Flags passed to drmModeAtomicCommit.
 * @user_data: User defined pointer passed to drmModeAtomicCommit.
 *
 * This function is similar to #igt_display_commit2, but is
 * used when you want to pass different flags to the actual commit.
 *
 * Useful flags can be DRM_MODE_ATOMIC_ALLOW_MODESET,
 * DRM_MODE_ATOMIC_NONBLOCK, DRM_MODE_PAGE_FLIP_EVENT,
 * or DRM_MODE_ATOMIC_TEST_ONLY.
 *
 * @user_data is returned in the event if you pass
 * DRM_MODE_PAGE_FLIP_EVENT to @flags.
 *
 * This function will abort the test if commit fails.
 */
void igt_display_commit_atomic(igt_display_t *display, uint32_t flags, void *user_data)
{
	int ret = igt_display_try_commit_atomic(display, flags, user_data);

	igt_assert_eq(ret, 0);
}

/**
 * igt_display_commit2:
 * @display: DRM device handle
 * @s: Commit style
 *
 * Commits framebuffer and positioning changes to all planes of each display
 * pipe, using a specific API to perform the programming.  This function should
 * be used to exercise a specific driver programming API; igt_display_commit
 * should be used instead if the API used is unimportant to the test being run.
 *
 * This function should only be used to commit changes that are expected to
 * succeed, since any failure during the commit process will cause the IGT
 * subtest to fail.  To commit changes that are expected to fail, use
 * @igt_try_display_commit2 instead.
 *
 * Returns: 0 upon success.  This function will never return upon failure
 * since igt_fail() at lower levels will longjmp out of it.
 */
int igt_display_commit2(igt_display_t *display,
		       enum igt_commit_style s)
{
	do_display_commit(display, s, true);

	return 0;
}

/**
 * igt_display_try_commit2:
 * @display: DRM device handle
 * @s: Commit style
 *
 * Attempts to commit framebuffer and positioning changes to all planes of each
 * display pipe.  This function should be used to commit changes that are
 * expected to fail, so that the error code can be checked for correctness.
 * For changes that are expected to succeed, use @igt_display_commit instead.
 *
 * Note that in non-atomic commit styles, no display programming will be
 * performed after the first failure is encountered, so only some of the
 * operations requested by a test may have been completed.  Tests that catch
 * errors returned by this function should take care to restore the display to
 * a sane state after a failure is detected.
 *
 * Returns: 0 upon success, otherwise the error code of the first error
 * encountered.
 */
int igt_display_try_commit2(igt_display_t *display, enum igt_commit_style s)
{
	return do_display_commit(display, s, false);
}

/**
 * igt_display_commit:
 * @display: DRM device handle
 *
 * Commits framebuffer and positioning changes to all planes of each display
 * pipe.
 *
 * Returns: 0 upon success.  This function will never return upon failure
 * since igt_fail() at lower levels will longjmp out of it.
 */
int igt_display_commit(igt_display_t *display)
{
	return igt_display_commit2(display, COMMIT_LEGACY);
}

/**
 * igt_display_drop_events:
 * @display: DRM device handle
 *
 * Nonblockingly reads all current events and drops them, for highest
 * reliablility, call igt_display_commit2() first to flush all outstanding
 * events.
 *
 * This will be called on the first commit after igt_display_reset() too,
 * to make sure any stale events are flushed.
 *
 * Returns: Number of dropped events.
 */
int igt_display_drop_events(igt_display_t *display)
{
	int ret = 0;

	/* Clear all events from drm fd. */
	struct pollfd pfd = {
		.fd = display->drm_fd,
		.events = POLLIN
	};

	while (poll(&pfd, 1, 0) > 0) {
		struct drm_event *ev;
		char buf[4096];
		ssize_t retval;

		retval = read(display->drm_fd, &buf, sizeof(buf));
		igt_assert_lt(0, retval);

		for (int i = 0; i < retval; i += ev->length) {
			ev = (struct drm_event *)&buf[i];

			igt_info("Dropping event type %u length %u\n", ev->type, ev->length);
			igt_assert(ev->length + i <= sizeof(buf));
			ret++;
		}
	}

	return ret;
}

/**
 * igt_output_name:
 * @output: Target output
 *
 * Returns: String representing a connector's name, e.g. "DP-1".
 */
const char *igt_output_name(igt_output_t *output)
{
	return output->name;
}

/**
 * igt_output_get_mode:
 * @output: Target output
 *
 * Get the current mode of the given connector
 *
 * Returns: A #drmModeModeInfo struct representing the current mode
 */
drmModeModeInfo *igt_output_get_mode(igt_output_t *output)
{
	if (output->use_override_mode)
		return &output->override_mode;
	else
		return &output->config.default_mode;
}

/**
 * igt_output_override_mode:
 * @output: Output of which the mode will be overridden
 * @mode: New mode, or NULL to disable override.
 *
 * Overrides the output's mode with @mode, so that it is used instead of the
 * mode obtained with get connectors. Note that the mode is used without
 * checking if the output supports it, so this might lead to unexpected results.
 */
void igt_output_override_mode(igt_output_t *output, const drmModeModeInfo *mode)
{
	igt_pipe_t *pipe = igt_output_get_driving_pipe(output);

	if (mode)
		output->override_mode = *mode;

	output->use_override_mode = !!mode;

	if (pipe) {
		if (output->display->is_atomic)
			igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_MODE_ID, igt_output_get_mode(output), sizeof(*mode));
		else
			igt_pipe_obj_set_prop_changed(pipe, IGT_CRTC_MODE_ID);
	}
}

/*
 * igt_output_set_pipe:
 * @output: Target output for which the pipe is being set to
 * @pipe: Display pipe to set to
 *
 * This function sets a @pipe to a specific @output connector by
 * setting the CRTC_ID property of the @pipe. The pipe
 * is only activated for all pipes except PIPE_NONE.
 */
void igt_output_set_pipe(igt_output_t *output, enum pipe pipe)
{
	igt_display_t *display = output->display;
	igt_pipe_t *old_pipe = NULL, *pipe_obj = NULL;;

	igt_assert(output->name);

	if (output->pending_pipe != PIPE_NONE)
		old_pipe = igt_output_get_driving_pipe(output);

	if (pipe != PIPE_NONE)
		pipe_obj = &display->pipes[pipe];

	LOG(display, "%s: set_pipe(%s)\n", igt_output_name(output),
	    kmstest_pipe_name(pipe));
	output->pending_pipe = pipe;

	if (old_pipe) {
		igt_output_t *old_output;

		old_output = igt_pipe_get_output(old_pipe);
		if (!old_output) {
			if (display->is_atomic)
				igt_pipe_obj_replace_prop_blob(old_pipe, IGT_CRTC_MODE_ID, NULL, 0);
			else
				igt_pipe_obj_set_prop_changed(old_pipe, IGT_CRTC_MODE_ID);

			igt_pipe_obj_set_prop_value(old_pipe, IGT_CRTC_ACTIVE, 0);
		}
	}

	igt_output_set_prop_value(output, IGT_CONNECTOR_CRTC_ID, pipe == PIPE_NONE ? 0 : display->pipes[pipe].crtc_id);

	igt_output_refresh(output);

	if (pipe_obj) {
		if (display->is_atomic)
			igt_pipe_obj_replace_prop_blob(pipe_obj, IGT_CRTC_MODE_ID, igt_output_get_mode(output), sizeof(drmModeModeInfo));
		else
			igt_pipe_obj_set_prop_changed(pipe_obj, IGT_CRTC_MODE_ID);

		igt_pipe_obj_set_prop_value(pipe_obj, IGT_CRTC_ACTIVE, 1);
	}
}

/*
 * igt_pipe_refresh:
 * @display: a pointer to an #igt_display_t structure
 * @pipe: Pipe to refresh
 * @force: Should be set to true if mode_blob is no longer considered
 * to be valid, for example after doing an atomic commit during fork or closing display fd.
 *
 * Requests the pipe to be part of the state on next update.
 * This is useful when state may have been out of sync after
 * a fork, or we just want to be sure the pipe is included
 * in the next commit.
 */
void igt_pipe_refresh(igt_display_t *display, enum pipe pipe, bool force)
{
	igt_pipe_t *pipe_obj = &display->pipes[pipe];

	if (force && display->is_atomic) {
		igt_output_t *output = igt_pipe_get_output(pipe_obj);

		pipe_obj->values[IGT_CRTC_MODE_ID] = 0;
		if (output)
			igt_pipe_obj_replace_prop_blob(pipe_obj, IGT_CRTC_MODE_ID, igt_output_get_mode(output), sizeof(drmModeModeInfo));
	} else
		igt_pipe_obj_set_prop_changed(pipe_obj, IGT_CRTC_MODE_ID);
}

igt_plane_t *igt_output_get_plane(igt_output_t *output, int plane_idx)
{
	igt_pipe_t *pipe;

	pipe = igt_output_get_driving_pipe(output);
	igt_assert(pipe);

	return igt_pipe_get_plane(pipe, plane_idx);
}

/**
 * igt_output_get_plane_type:
 * @output: Target output
 * @plane_type: Cursor, primary or an overlay plane
 *
 * Finds a valid plane type for the given @output otherwise
 * the test is skipped if the right combination of @output/@plane_type is not found
 *
 * Returns: A #igt_plane_t structure that matches the requested plane type
 */
igt_plane_t *igt_output_get_plane_type(igt_output_t *output, int plane_type)
{
	igt_pipe_t *pipe;

	pipe = igt_output_get_driving_pipe(output);
	igt_assert(pipe);

	return igt_pipe_get_plane_type(pipe, plane_type);
}

/**
 * igt_output_count_plane_type:
 * @output: Target output
 * @plane_type: Cursor, primary or an overlay plane
 *
 * Counts the number of planes of type @plane_type for the provided @output.
 *
 * Returns: The number of planes that match the requested plane type
 */
int igt_output_count_plane_type(igt_output_t *output, int plane_type)
{
	igt_pipe_t *pipe = igt_output_get_driving_pipe(output);
	igt_assert(pipe);

	return igt_pipe_count_plane_type(pipe, plane_type);
}

/**
 * igt_output_get_plane_type_index:
 * @output: Target output
 * @plane_type: Cursor, primary or an overlay plane
 * @index: the index of the plane among planes of the same type
 *
 * Get the @index th plane of type @plane_type for the provided @output.
 *
 * Returns: The @index th plane that matches the requested plane type
 */
igt_plane_t *igt_output_get_plane_type_index(igt_output_t *output,
					     int plane_type, int index)
{
	igt_pipe_t *pipe = igt_output_get_driving_pipe(output);
	igt_assert(pipe);

	return igt_pipe_get_plane_type_index(pipe, plane_type, index);
}

/**
 * igt_plane_set_fb:
 * @plane: Plane
 * @fb: Framebuffer pointer
 *
 * Pairs a given @framebuffer to a @plane
 *
 * This function also sets a default size and position for the framebuffer
 * to avoid crashes on applications that ignore to set these.
 */
void igt_plane_set_fb(igt_plane_t *plane, struct igt_fb *fb)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: plane_set_fb(%d)\n", kmstest_pipe_name(pipe->pipe),
	    plane->index, fb ? fb->fb_id : 0);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_ID, fb ? pipe->crtc_id : 0);
	igt_plane_set_prop_value(plane, IGT_PLANE_FB_ID, fb ? fb->fb_id : 0);

	if (plane->type == DRM_PLANE_TYPE_CURSOR && fb)
		plane->gem_handle = fb->gem_handle;
	else
		plane->gem_handle = 0;

	/* hack to keep tests working that don't call igt_plane_set_size() */
	if (fb) {
		/* set default plane size as fb size */
		igt_plane_set_size(plane, fb->width, fb->height);

		/* set default src pos/size as fb size */
		igt_fb_set_position(fb, plane, 0, 0);
		igt_fb_set_size(fb, plane, fb->width, fb->height);

		if (igt_plane_has_prop(plane, IGT_PLANE_COLOR_ENCODING))
			igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_ENCODING,
				igt_color_encoding_to_str(fb->color_encoding));
		if (igt_plane_has_prop(plane, IGT_PLANE_COLOR_RANGE))
			igt_plane_set_prop_enum(plane, IGT_PLANE_COLOR_RANGE,
				igt_color_range_to_str(fb->color_range));

		/* Hack to prioritize the plane on the pipe that last set fb */
		igt_plane_set_pipe(plane, pipe);
	} else {
		igt_plane_set_size(plane, 0, 0);

		/* set default src pos/size as fb size */
		igt_fb_set_position(fb, plane, 0, 0);
		igt_fb_set_size(fb, plane, 0, 0);
	}
}

/**
 * igt_plane_set_fence_fd:
 * @plane: plane
 * @fence_fd: fence fd, disable fence_fd by setting it to -1
 *
 * This function sets a fence fd to enable a commit to wait for some event to
 * occur before completing.
 */
void igt_plane_set_fence_fd(igt_plane_t *plane, int fence_fd)
{
	int64_t fd;

	fd = plane->values[IGT_PLANE_IN_FENCE_FD];
	if (fd != -1)
		close(fd);

	if (fence_fd != -1) {
		fd = dup(fence_fd);
		igt_fail_on(fd == -1);
	} else
		fd = -1;

	igt_plane_set_prop_value(plane, IGT_PLANE_IN_FENCE_FD, fd);
}

/**
 * igt_plane_set_pipe:
 * @plane: Target plane pointer
 * @pipe: The pipe to assign the plane to
 *
 */
void igt_plane_set_pipe(igt_plane_t *plane, igt_pipe_t *pipe)
{
	/*
	 * HACK: Point the global plane back to the local plane.
	 * This is used to help apply the correct atomic state while
	 * we're moving away from the single pipe per plane model.
	 */
	plane->ref->ref = plane;
	plane->ref->pipe = pipe;
}

/**
 * igt_plane_set_position:
 * @plane: Plane pointer for which position is to be set
 * @x: X coordinate
 * @y: Y coordinate
 *
 * This function sets a new (x,y) position for the given plane.
 * New position will be committed at plane commit time via drmModeSetPlane().
 */
void igt_plane_set_position(igt_plane_t *plane, int x, int y)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: plane_set_position(%d,%d)\n",
	    kmstest_pipe_name(pipe->pipe), plane->index, x, y);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_X, x);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_Y, y);
}

/**
 * igt_plane_set_size:
 * @plane: plane pointer for which size to be set
 * @w: width
 * @h: height
 *
 * This function sets width and height for requested plane.
 * New size will be committed at plane commit time via
 * drmModeSetPlane().
 */
void igt_plane_set_size(igt_plane_t *plane, int w, int h)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: plane_set_size (%dx%d)\n",
	    kmstest_pipe_name(pipe->pipe), plane->index, w, h);

	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_W, w);
	igt_plane_set_prop_value(plane, IGT_PLANE_CRTC_H, h);
}

/**
 * igt_fb_set_position:
 * @fb: framebuffer pointer
 * @plane: plane
 * @x: X position
 * @y: Y position
 *
 * This function sets position for requested framebuffer as src to plane.
 * New position will be committed at plane commit time via drmModeSetPlane().
 */
void igt_fb_set_position(struct igt_fb *fb, igt_plane_t *plane,
	uint32_t x, uint32_t y)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: fb_set_position(%d,%d)\n",
	    kmstest_pipe_name(pipe->pipe), plane->index, x, y);

	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_X, IGT_FIXED(x, 0));
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_Y, IGT_FIXED(y, 0));
}

/**
 * igt_fb_set_size:
 * @fb: framebuffer pointer
 * @plane: plane
 * @w: width
 * @h: height
 *
 * This function sets fetch rect size from requested framebuffer as src
 * to plane. New size will be committed at plane commit time via
 * drmModeSetPlane().
 */
void igt_fb_set_size(struct igt_fb *fb, igt_plane_t *plane,
	uint32_t w, uint32_t h)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: fb_set_size(%dx%d)\n",
	    kmstest_pipe_name(pipe->pipe), plane->index, w, h);

	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_W, IGT_FIXED(w, 0));
	igt_plane_set_prop_value(plane, IGT_PLANE_SRC_H, IGT_FIXED(h, 0));
}

static const char *rotation_name(igt_rotation_t rotation)
{
	switch (rotation & IGT_ROTATION_MASK) {
	case IGT_ROTATION_0:
		return "0°";
	case IGT_ROTATION_90:
		return "90°";
	case IGT_ROTATION_180:
		return "180°";
	case IGT_ROTATION_270:
		return "270°";
	default:
		igt_assert(0);
	}
}

/**
 * igt_plane_set_rotation:
 * @plane: Plane pointer for which rotation is to be set
 * @rotation: Plane rotation value (0, 90, 180, 270)
 *
 * This function sets a new rotation for the requested @plane.
 * New @rotation will be committed at plane commit time via
 * drmModeSetPlane().
 */
void igt_plane_set_rotation(igt_plane_t *plane, igt_rotation_t rotation)
{
	igt_pipe_t *pipe = plane->pipe;
	igt_display_t *display = pipe->display;

	LOG(display, "%s.%d: plane_set_rotation(%s)\n",
	    kmstest_pipe_name(pipe->pipe),
	    plane->index, rotation_name(rotation));

	igt_plane_set_prop_value(plane, IGT_PLANE_ROTATION, rotation);
}

/**
 * igt_pipe_request_out_fence:
 * @pipe: pipe which out fence will be requested for
 *
 * Marks this pipe for requesting an out fence at the next atomic commit
 * will contain the fd number of the out fence created by KMS.
 */
void igt_pipe_request_out_fence(igt_pipe_t *pipe)
{
	igt_pipe_obj_set_prop_value(pipe, IGT_CRTC_OUT_FENCE_PTR, (ptrdiff_t)&pipe->out_fence_fd);
}

/**
 * igt_wait_for_vblank_count:
 * @drm_fd: A drm file descriptor
 * @pipe: Pipe to wait_for_vblank on
 * @count: Number of vblanks to wait on
 *
 * Waits for a given number of vertical blank intervals
 */
void igt_wait_for_vblank_count(int drm_fd, enum pipe pipe, int count)
{
	drmVBlank wait_vbl;
	uint32_t pipe_id_flag;

	memset(&wait_vbl, 0, sizeof(wait_vbl));
	pipe_id_flag = kmstest_get_vbl_flag(pipe);

	wait_vbl.request.type = DRM_VBLANK_RELATIVE;
	wait_vbl.request.type |= pipe_id_flag;
	wait_vbl.request.sequence = count;

	igt_assert(drmWaitVBlank(drm_fd, &wait_vbl) == 0);
}

/**
 * igt_wait_for_vblank:
 * @drm_fd: A drm file descriptor
 * @pipe: Pipe to wait_for_vblank on
 *
 * Waits for 1 vertical blank intervals
 */
void igt_wait_for_vblank(int drm_fd, enum pipe pipe)
{
	igt_wait_for_vblank_count(drm_fd, pipe, 1);
}

/**
 * igt_enable_connectors:
 * @drm_fd: A drm file descriptor
 *
 * Force connectors to be enabled where this is known to work well. Use
 * #igt_reset_connectors to revert the changes.
 *
 * An exit handler is installed to ensure connectors are reset when the test
 * exits.
 */
void igt_enable_connectors(int drm_fd)
{
	drmModeRes *res;

	res = drmModeGetResources(drm_fd);
	if (!res)
		return;

	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *c;

		/* Do a probe. This may be the first action after booting */
		c = drmModeGetConnector(drm_fd, res->connectors[i]);
		if (!c) {
			igt_warn("Could not read connector %u: %m\n", res->connectors[i]);
			continue;
		}

		/* don't attempt to force connectors that are already connected
		 */
		if (c->connection == DRM_MODE_CONNECTED)
			continue;

		/* just enable VGA for now */
		if (c->connector_type == DRM_MODE_CONNECTOR_VGA) {
			if (!kmstest_force_connector(drm_fd, c, FORCE_CONNECTOR_ON))
				igt_info("Unable to force state on %s-%d\n",
					 kmstest_connector_type_str(c->connector_type),
					 c->connector_type_id);
		}

		drmModeFreeConnector(c);
	}
}

/**
 * igt_reset_connectors:
 *
 * Remove any forced state from the connectors.
 */
void igt_reset_connectors(void)
{
	/* reset the connectors stored in forced_connectors, avoiding any
	 * functions that are not safe to call in signal handlers */
	for (int i = 0; forced_connectors[i]; i++)
		igt_sysfs_set(forced_connectors_device[i],
			      forced_connectors[i],
			      "detect");
}

#if !defined(ANDROID)
/**
 * igt_watch_hotplug:
 *
 * Begin monitoring udev for sysfs hotplug events.
 *
 * Returns: a udev monitor for detecting hotplugs on
 */
struct udev_monitor *igt_watch_hotplug(void)
{
	struct udev *udev;
	struct udev_monitor *mon;
	int ret, flags, fd;

	udev = udev_new();
	igt_assert(udev != NULL);

	mon = udev_monitor_new_from_netlink(udev, "udev");
	igt_assert(mon != NULL);

	ret = udev_monitor_filter_add_match_subsystem_devtype(mon,
							      "drm",
							      "drm_minor");
	igt_assert_eq(ret, 0);
	ret = udev_monitor_filter_update(mon);
	igt_assert_eq(ret, 0);
	ret = udev_monitor_enable_receiving(mon);
	igt_assert_eq(ret, 0);

	/* Set the fd for udev as non blocking */
	fd = udev_monitor_get_fd(mon);
	flags = fcntl(fd, F_GETFL, 0);
	igt_assert(flags);

	flags |= O_NONBLOCK;
	igt_assert_neq(fcntl(fd, F_SETFL, flags), -1);

	return mon;
}

static bool event_detected(struct udev_monitor *mon, int timeout_secs,
			   const char *property)
{
	struct udev_device *dev;
	const char *hotplug_val;
	struct pollfd fd = {
		.fd = udev_monitor_get_fd(mon),
		.events = POLLIN
	};
	bool hotplug_received = false;

	/* Go through all of the events pending on the udev monitor. Once we
	 * receive a hotplug, we continue going through the rest of the events
	 * so that redundant hotplug events don't change the results of future
	 * checks
	 */
	while (!hotplug_received && poll(&fd, 1, timeout_secs * 1000)) {
		dev = udev_monitor_receive_device(mon);

		hotplug_val = udev_device_get_property_value(dev, property);
		if (hotplug_val && atoi(hotplug_val) == 1)
			hotplug_received = true;

		udev_device_unref(dev);
	}

	return hotplug_received;
}

/**
 * igt_hotplug_detected:
 * @mon: A udev monitor initialized with #igt_watch_hotplug
 * @timeout_secs: How long to wait for a hotplug event to occur.
 *
 * Assert that a hotplug event was received since we last checked the monitor.
 *
 * Returns: true if a sysfs hotplug event was received, false if we timed out
 */
bool igt_hotplug_detected(struct udev_monitor *mon, int timeout_secs)
{
	return event_detected(mon, timeout_secs, "HOTPLUG");
}

/**
 * igt_lease_change_detected:
 * @mon: A udev monitor initialized with #igt_watch_hotplug
 * @timeout_secs: How long to wait for a lease change event to occur.
 *
 * Assert that a lease change event was received since we last checked the monitor.
 *
 * Returns: true if a sysfs lease change event was received, false if we timed out
 */
bool igt_lease_change_detected(struct udev_monitor *mon, int timeout_secs)
{
	return event_detected(mon, timeout_secs, "LEASE");
}

/**
 * igt_flush_hotplugs:
 * @mon: A udev monitor initialized with #igt_watch_hotplug
 *
 * Get rid of any pending hotplug events
 */
void igt_flush_hotplugs(struct udev_monitor *mon)
{
	struct udev_device *dev;

	while ((dev = udev_monitor_receive_device(mon)))
		udev_device_unref(dev);
}

/**
 * igt_cleanup_hotplug:
 * @mon: A udev monitor initialized with #igt_watch_hotplug
 *
 * Cleanup the resources allocated by #igt_watch_hotplug
 */
void igt_cleanup_hotplug(struct udev_monitor *mon)
{
	struct udev *udev = udev_monitor_get_udev(mon);

	udev_monitor_unref(mon);
	mon = NULL;
	udev_unref(udev);
}
#endif /*!defined(ANDROID)*/

/**
 * kmstest_get_vbl_flag:
 * @pipe_id: Pipe to convert to flag representation.
 *
 * Convert a pipe id into the flag representation
 * expected in DRM while processing DRM_IOCTL_WAIT_VBLANK.
 */
uint32_t kmstest_get_vbl_flag(uint32_t pipe_id)
{
	if (pipe_id == 0)
		return 0;
	else if (pipe_id == 1)
		return _DRM_VBLANK_SECONDARY;
	else {
		uint32_t pipe_flag = pipe_id << 1;
		igt_assert(!(pipe_flag & ~DRM_VBLANK_HIGH_CRTC_MASK));
		return pipe_flag;
	}
}

static inline const uint32_t *
formats_ptr(const struct drm_format_modifier_blob *blob)
{
	return (const uint32_t *)((const char *)blob + blob->formats_offset);
}

static inline const struct drm_format_modifier *
modifiers_ptr(const struct drm_format_modifier_blob *blob)
{
	return (const struct drm_format_modifier *)((const char *)blob + blob->modifiers_offset);
}

static int igt_count_plane_format_mod(const struct drm_format_modifier_blob *blob_data)
{
	const struct drm_format_modifier *modifiers;
	int count = 0;

	modifiers = modifiers_ptr(blob_data);

	for (int i = 0; i < blob_data->count_modifiers; i++)
		count += igt_hweight(modifiers[i].formats);

	return count;
}

static void igt_fill_plane_format_mod(igt_display_t *display, igt_plane_t *plane)
{
	const struct drm_format_modifier_blob *blob_data;
	drmModePropertyBlobPtr blob;
	uint64_t blob_id;
	int idx = 0;
	int count;

	if (!igt_plane_has_prop(plane, IGT_PLANE_IN_FORMATS)) {
		drmModePlanePtr p = plane->drm_plane;

		count = p->count_formats;

		plane->format_mod_count = count;
		plane->formats = calloc(count, sizeof(plane->formats[0]));
		igt_assert(plane->formats);
		plane->modifiers = calloc(count, sizeof(plane->modifiers[0]));
		igt_assert(plane->modifiers);

		/*
		 * We don't know which modifiers are
		 * supported, so we'll assume linear only.
		 */
		for (int i = 0; i < count; i++) {
			plane->formats[i] = p->formats[i];
			plane->modifiers[i] = DRM_FORMAT_MOD_LINEAR;
		}

		return;
	}

	blob_id = igt_plane_get_prop(plane, IGT_PLANE_IN_FORMATS);

	blob = drmModeGetPropertyBlob(display->drm_fd, blob_id);
	if (!blob)
		return;

	blob_data = (const struct drm_format_modifier_blob *) blob->data;

	count = igt_count_plane_format_mod(blob_data);
	if (!count)
		return;

	plane->format_mod_count = count;
	plane->formats = calloc(count, sizeof(plane->formats[0]));
	igt_assert(plane->formats);
	plane->modifiers = calloc(count, sizeof(plane->modifiers[0]));
	igt_assert(plane->modifiers);

	for (int i = 0; i < blob_data->count_modifiers; i++) {
		for (int j = 0; j < 64; j++) {
			const struct drm_format_modifier *modifiers =
				modifiers_ptr(blob_data);
			const uint32_t *formats = formats_ptr(blob_data);

			if (!(modifiers[i].formats & (1ULL << j)))
				continue;

			plane->formats[idx] = formats[modifiers[i].offset + j];
			plane->modifiers[idx] = modifiers[i].modifier;
			idx++;
			igt_assert_lte(idx, plane->format_mod_count);
		}
	}

	igt_assert_eq(idx, plane->format_mod_count);
}

bool igt_plane_has_format_mod(igt_plane_t *plane, uint32_t format,
			      uint64_t modifier)
{
	int i;

	for (i = 0; i < plane->format_mod_count; i++) {
		if (plane->formats[i] == format &&
		    plane->modifiers[i] == modifier)
			return true;

	}

	return false;
}

static int igt_count_display_format_mod(igt_display_t *display)
{
	enum pipe pipe;
	int count = 0;

	for_each_pipe(display, pipe) {
		igt_plane_t *plane;

		for_each_plane_on_pipe(display, pipe, plane) {
			count += plane->format_mod_count;
		}
	}

	return count;
}

static void
igt_add_display_format_mod(igt_display_t *display, uint32_t format,
			   uint64_t modifier)
{
	int i;

	for (i = 0; i < display->format_mod_count; i++) {
		if (display->formats[i] == format &&
		    display->modifiers[i] == modifier)
			return;

	}

	display->formats[i] = format;
	display->modifiers[i] = modifier;
	display->format_mod_count++;
}

static void igt_fill_display_format_mod(igt_display_t *display)
{
	int count = igt_count_display_format_mod(display);
	enum pipe pipe;

	if (!count)
		return;

	display->formats = calloc(count, sizeof(display->formats[0]));
	igt_assert(display->formats);
	display->modifiers = calloc(count, sizeof(display->modifiers[0]));
	igt_assert(display->modifiers);

	for_each_pipe(display, pipe) {
		igt_plane_t *plane;

		for_each_plane_on_pipe(display, pipe, plane) {
			for (int i = 0; i < plane->format_mod_count; i++) {
				igt_add_display_format_mod(display,
							   plane->formats[i],
							   plane->modifiers[i]);
				igt_assert_lte(display->format_mod_count, count);
			}
		}
	}
}

bool igt_display_has_format_mod(igt_display_t *display, uint32_t format,
				uint64_t modifier)
{
	int i;

	for (i = 0; i < display->format_mod_count; i++) {
		if (display->formats[i] == format &&
		    display->modifiers[i] == modifier)
			return true;

	}

	return false;
}
