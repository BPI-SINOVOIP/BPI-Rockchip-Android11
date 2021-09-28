/*
 * Copyright © 2006 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "igt_aux.h"
#include "intel_io.h"
#include "intel_chipset.h"
#include "drmtest.h"

/* kernel types for intel_vbt_defs.h */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define __packed __attribute__ ((packed))

#define _INTEL_BIOS_PRIVATE
#include "intel_vbt_defs.h"

/* no bother to include "edid.h" */
#define _H_ACTIVE(x) (x[2] + ((x[4] & 0xF0) << 4))
#define _H_SYNC_OFF(x) (x[8] + ((x[11] & 0xC0) << 2))
#define _H_SYNC_WIDTH(x) (x[9] + ((x[11] & 0x30) << 4))
#define _H_BLANK(x) (x[3] + ((x[4] & 0x0F) << 8))
#define _V_ACTIVE(x) (x[5] + ((x[7] & 0xF0) << 4))
#define _V_SYNC_OFF(x) ((x[10] >> 4) + ((x[11] & 0x0C) << 2))
#define _V_SYNC_WIDTH(x) ((x[10] & 0x0F) + ((x[11] & 0x03) << 4))
#define _V_BLANK(x) (x[6] + ((x[7] & 0x0F) << 8))
#define _PIXEL_CLOCK(x) (x[0] + (x[1] << 8)) * 10000

#define YESNO(val) ((val) ? "yes" : "no")

/* This is not for mapping to memory layout. */
struct bdb_block {
	uint8_t id;
	uint32_t size;
	const void *data;
};

struct context {
	const struct vbt_header *vbt;
	const struct bdb_header *bdb;
	int size;

	uint32_t devid;
	int panel_type;
	bool dump_all_panel_types;
	bool hexdump;
};

/* Get BDB block size given a pointer to Block ID. */
static uint32_t _get_blocksize(const uint8_t *block_base)
{
	/* The MIPI Sequence Block v3+ has a separate size field. */
	if (*block_base == BDB_MIPI_SEQUENCE && *(block_base + 3) >= 3)
		return *((const uint32_t *)(block_base + 4));
	else
		return *((const uint16_t *)(block_base + 1));
}

static struct bdb_block *find_section(struct context *context, int section_id)
{
	const struct bdb_header *bdb = context->bdb;
	int length = context->size;
	struct bdb_block *block;
	const uint8_t *base = (const uint8_t *)bdb;
	int index = 0;
	uint32_t total, current_size;
	unsigned char current_id;

	/* skip to first section */
	index += bdb->header_size;
	total = bdb->bdb_size;
	if (total > length)
		total = length;

	block = malloc(sizeof(*block));
	if (!block) {
		fprintf(stderr, "out of memory\n");
		exit(EXIT_FAILURE);
	}

	/* walk the sections looking for section_id */
	while (index + 3 < total) {
		current_id = *(base + index);
		current_size = _get_blocksize(base + index);
		index += 3;

		if (index + current_size > total)
			return NULL;

		if (current_id == section_id) {
			block->id = current_id;
			block->size = current_size;
			block->data = base + index;
			return block;
		}

		index += current_size;
	}

	free(block);
	return NULL;
}

static void dump_general_features(struct context *context,
				  const struct bdb_block *block)
{
	const struct bdb_general_features *features = block->data;

	printf("\tPanel fitting: ");
	switch (features->panel_fitting) {
	case 0:
		printf("disabled\n");
		break;
	case 1:
		printf("text only\n");
		break;
	case 2:
		printf("graphics only\n");
		break;
	case 3:
		printf("text & graphics\n");
		break;
	}
	printf("\tFlexaim: %s\n", YESNO(features->flexaim));
	printf("\tMessage: %s\n", YESNO(features->msg_enable));
	printf("\tClear screen: %d\n", features->clear_screen);
	printf("\tDVO color flip required: %s\n", YESNO(features->color_flip));

	printf("\tExternal VBT: %s\n", YESNO(features->download_ext_vbt));
	printf("\tEnable SSC: %s\n", YESNO(features->enable_ssc));
	if (features->enable_ssc) {
		if (!context->devid)
			printf("\tSSC frequency: <unknown platform>\n");
		else if (IS_VALLEYVIEW(context->devid) ||
			 IS_CHERRYVIEW(context->devid) ||
			 IS_BROXTON(context->devid))
			printf("\tSSC frequency: 100 MHz\n");
		else if (HAS_PCH_SPLIT(context->devid))
			printf("\tSSC frequency: %s\n", features->ssc_freq ?
			       "100 MHz" : "120 MHz");
		else
			printf("\tSSC frequency: %s\n", features->ssc_freq ?
			       "100 MHz (66 MHz on 855)" : "96 MHz (48 MHz on 855)");
	}
	printf("\tLFP on override: %s\n",
	       YESNO(features->enable_lfp_on_override));
	printf("\tDisable SSC on clone: %s\n",
	       YESNO(features->disable_ssc_ddt));
	printf("\tUnderscan support for VGA timings: %s\n",
	       YESNO(features->underscan_vga_timings));
	if (context->bdb->version >= 183)
		printf("\tDynamic CD clock: %s\n", YESNO(features->display_clock_mode));
	printf("\tHotplug support in VBIOS: %s\n",
	       YESNO(features->vbios_hotplug_support));

	printf("\tDisable smooth vision: %s\n",
	       YESNO(features->disable_smooth_vision));
	printf("\tSingle DVI for CRT/DVI: %s\n", YESNO(features->single_dvi));
	if (context->bdb->version >= 181)
		printf("\tEnable 180 degree rotation: %s\n", YESNO(features->rotate_180));
	printf("\tInverted FDI Rx polarity: %s\n", YESNO(features->fdi_rx_polarity_inverted));
	if (context->bdb->version >= 160) {
		printf("\tExtended VBIOS mode: %s\n", YESNO(features->vbios_extended_mode));
		printf("\tCopy iLFP DTD to SDVO LVDS DTD: %s\n", YESNO(features->copy_ilfp_dtd_to_sdvo_lvds_dtd));
		printf("\tBest fit panel timing algorithm: %s\n", YESNO(features->panel_best_fit_timing));
		printf("\tIgnore strap state: %s\n", YESNO(features->ignore_strap_state));
	}

	printf("\tLegacy monitor detect: %s\n",
	       YESNO(features->legacy_monitor_detect));

	printf("\tIntegrated CRT: %s\n", YESNO(features->int_crt_support));
	printf("\tIntegrated TV: %s\n", YESNO(features->int_tv_support));
	printf("\tIntegrated EFP: %s\n", YESNO(features->int_efp_support));
	printf("\tDP SSC enable: %s\n", YESNO(features->dp_ssc_enable));
	if (features->dp_ssc_enable) {
		if (IS_VALLEYVIEW(context->devid) || IS_CHERRYVIEW(context->devid) ||
		    IS_BROXTON(context->devid))
			printf("\tSSC frequency: 100 MHz\n");
		else if (HAS_PCH_SPLIT(context->devid))
			printf("\tSSC frequency: %s\n", features->dp_ssc_freq ?
			       "100 MHz" : "120 MHz");
		else
			printf("\tSSC frequency: %s\n", features->dp_ssc_freq ?
			       "100 MHz" : "96 MHz");
	}
	printf("\tDP SSC dongle supported: %s\n", YESNO(features->dp_ssc_dongle_supported));
}

static void dump_backlight_info(struct context *context,
				const struct bdb_block *block)
{
	const struct bdb_lfp_backlight_data *backlight = block->data;
	const struct lfp_backlight_data_entry *blc;

	if (sizeof(*blc) != backlight->entry_size) {
		printf("\tBacklight struct sizes don't match (expected %zu, got %u), skipping\n",
		     sizeof(*blc), backlight->entry_size);
		return;
	}

	blc = &backlight->data[context->panel_type];

	printf("\tInverter type: %d\n", blc->type);
	printf("\t     polarity: %d\n", blc->active_low_pwm);
	printf("\t     PWM freq: %d\n", blc->pwm_freq_hz);
	printf("\tMinimum brightness: %d\n", blc->min_brightness);
}

static const struct {
	unsigned short type;
	const char *name;
} child_device_types[] = {
	{ DEVICE_TYPE_NONE, "none" },
	{ DEVICE_TYPE_CRT, "CRT" },
	{ DEVICE_TYPE_TV, "TV" },
	{ DEVICE_TYPE_EFP, "EFP" },
	{ DEVICE_TYPE_LFP, "LFP" },
	{ DEVICE_TYPE_CRT_DPMS, "CRT" },
	{ DEVICE_TYPE_CRT_DPMS_HOTPLUG, "CRT" },
	{ DEVICE_TYPE_TV_COMPOSITE, "TV composite" },
	{ DEVICE_TYPE_TV_MACROVISION, "TV" },
	{ DEVICE_TYPE_TV_RF_COMPOSITE, "TV" },
	{ DEVICE_TYPE_TV_SVIDEO_COMPOSITE, "TV S-Video" },
	{ DEVICE_TYPE_TV_SCART, "TV SCART" },
	{ DEVICE_TYPE_TV_CODEC_HOTPLUG_PWR, "TV" },
	{ DEVICE_TYPE_EFP_HOTPLUG_PWR, "EFP" },
	{ DEVICE_TYPE_EFP_DVI_HOTPLUG_PWR, "DVI" },
	{ DEVICE_TYPE_EFP_DVI_I, "DVI-I" },
	{ DEVICE_TYPE_EFP_DVI_D_DUAL, "DL-DVI-D" },
	{ DEVICE_TYPE_EFP_DVI_D_HDCP, "DVI-D" },
	{ DEVICE_TYPE_OPENLDI_HOTPLUG_PWR, "OpenLDI" },
	{ DEVICE_TYPE_OPENLDI_DUALPIX, "OpenLDI" },
	{ DEVICE_TYPE_LFP_PANELLINK, "PanelLink" },
	{ DEVICE_TYPE_LFP_CMOS_PWR, "CMOS LFP" },
	{ DEVICE_TYPE_LFP_LVDS_PWR, "LVDS" },
	{ DEVICE_TYPE_LFP_LVDS_DUAL, "LVDS" },
	{ DEVICE_TYPE_LFP_LVDS_DUAL_HDCP, "LVDS" },
	{ DEVICE_TYPE_INT_LFP, "LFP" },
	{ DEVICE_TYPE_INT_TV, "TV" },
	{ DEVICE_TYPE_DP, "DisplayPort" },
	{ DEVICE_TYPE_DP_DUAL_MODE, "DisplayPort/HDMI/DVI" },
	{ DEVICE_TYPE_DP_DVI, "DisplayPort/DVI" },
	{ DEVICE_TYPE_HDMI, "HDMI/DVI" },
	{ DEVICE_TYPE_DVI, "DVI" },
	{ DEVICE_TYPE_eDP, "eDP" },
	{ DEVICE_TYPE_MIPI, "MIPI" },
};
static const int num_child_device_types =
	sizeof(child_device_types) / sizeof(child_device_types[0]);

static const char *child_device_type(unsigned short type)
{
	int i;

	for (i = 0; i < num_child_device_types; i++)
		if (child_device_types[i].type == type)
			return child_device_types[i].name;

	return "unknown";
}

static const struct {
	unsigned short mask;
	const char *name;
} child_device_type_bits[] = {
	{ DEVICE_TYPE_CLASS_EXTENSION, "Class extension" },
	{ DEVICE_TYPE_POWER_MANAGEMENT, "Power management" },
	{ DEVICE_TYPE_HOTPLUG_SIGNALING, "Hotplug signaling" },
	{ DEVICE_TYPE_INTERNAL_CONNECTOR, "Internal connector" },
	{ DEVICE_TYPE_NOT_HDMI_OUTPUT, "HDMI output" }, /* decoded as inverse */
	{ DEVICE_TYPE_MIPI_OUTPUT, "MIPI output" },
	{ DEVICE_TYPE_COMPOSITE_OUTPUT, "Composite output" },
	{ DEVICE_TYPE_DUAL_CHANNEL, "Dual channel" },
	{ 1 << 7, "Content protection" },
	{ DEVICE_TYPE_HIGH_SPEED_LINK, "High speed link" },
	{ DEVICE_TYPE_LVDS_SIGNALING, "LVDS signaling" },
	{ DEVICE_TYPE_TMDS_DVI_SIGNALING, "TMDS/DVI signaling" },
	{ DEVICE_TYPE_VIDEO_SIGNALING, "Video signaling" },
	{ DEVICE_TYPE_DISPLAYPORT_OUTPUT, "DisplayPort output" },
	{ DEVICE_TYPE_DIGITAL_OUTPUT, "Digital output" },
	{ DEVICE_TYPE_ANALOG_OUTPUT, "Analog output" },
};

static void dump_child_device_type_bits(uint16_t type)
{
	int i;

	type ^= DEVICE_TYPE_NOT_HDMI_OUTPUT;

	for (i = 0; i < ARRAY_SIZE(child_device_type_bits); i++) {
		if (child_device_type_bits[i].mask & type)
			printf("\t\t\t%s\n", child_device_type_bits[i].name);
	}
}

static const struct {
	unsigned char handle;
	const char *name;
} child_device_handles[] = {
	{ DEVICE_HANDLE_CRT, "CRT" },
	{ DEVICE_HANDLE_EFP1, "EFP 1 (HDMI/DVI/DP)" },
	{ DEVICE_HANDLE_EFP2, "EFP 2 (HDMI/DVI/DP)" },
	{ DEVICE_HANDLE_EFP3, "EFP 3 (HDMI/DVI/DP)" },
	{ DEVICE_HANDLE_EFP4, "EFP 4 (HDMI/DVI/DP)" },
	{ DEVICE_HANDLE_LPF1, "LFP 1 (eDP)" },
	{ DEVICE_HANDLE_LFP2, "LFP 2 (eDP)" },
};
static const int num_child_device_handles =
	sizeof(child_device_handles) / sizeof(child_device_handles[0]);

static const char *child_device_handle(unsigned char handle)
{
	int i;

	for (i = 0; i < num_child_device_handles; i++)
		if (child_device_handles[i].handle == handle)
			return child_device_handles[i].name;

	return "unknown";
}

static const char *dvo_port_names[] = {
	[DVO_PORT_HDMIA] = "HDMI-A",
	[DVO_PORT_HDMIB] = "HDMI-B",
	[DVO_PORT_HDMIC] = "HDMI-C",
	[DVO_PORT_HDMID] = "HDMI-D",
	[DVO_PORT_LVDS] = "LVDS",
	[DVO_PORT_TV] = "TV",
	[DVO_PORT_CRT] = "CRT",
	[DVO_PORT_DPB] = "DP-B",
	[DVO_PORT_DPC] = "DP-C",
	[DVO_PORT_DPD] = "DP-D",
	[DVO_PORT_DPA] = "DP-A",
	[DVO_PORT_DPE] = "DP-E",
	[DVO_PORT_HDMIE] = "HDMI-E",
	[DVO_PORT_MIPIA] = "MIPI-A",
	[DVO_PORT_MIPIB] = "MIPI-B",
	[DVO_PORT_MIPIC] = "MIPI-C",
	[DVO_PORT_MIPID] = "MIPI-D",
};

static const char *dvo_port(uint8_t type)
{
	if (type < ARRAY_SIZE(dvo_port_names) && dvo_port_names[type])
		return dvo_port_names[type];
	else
		return "unknown";
}

static const char *mipi_bridge_type(uint8_t type)
{
	switch (type) {
	case 1:
		return "ASUS";
	case 2:
		return "Toshiba";
	case 3:
		return "Renesas";
	default:
		return "unknown";
	}
}

static void dump_hmdi_max_data_rate(uint8_t hdmi_max_data_rate)
{
	static const uint16_t max_data_rate[] = {
		[HDMI_MAX_DATA_RATE_PLATFORM] = 0,
		[HDMI_MAX_DATA_RATE_297] = 297,
		[HDMI_MAX_DATA_RATE_165] = 165,
	};

	if (hdmi_max_data_rate >= ARRAY_SIZE(max_data_rate))
		printf("\t\tHDMI max data rate: <unknown> (0x%02x)\n",
		       hdmi_max_data_rate);
	else if (hdmi_max_data_rate == HDMI_MAX_DATA_RATE_PLATFORM)
		printf("\t\tHDMI max data rate: <platform max> (0x%02x)\n",
		       hdmi_max_data_rate);
	else
		printf("\t\tHDMI max data rate: %d MHz (0x%02x)\n",
		       max_data_rate[hdmi_max_data_rate],
		       hdmi_max_data_rate);
}

static void dump_child_device(struct context *context,
			      const struct child_device_config *child)
{
	if (!child->device_type)
		return;

	printf("\tChild device info:\n");
	printf("\t\tDevice handle: 0x%04x (%s)\n", child->handle,
	       child_device_handle(child->handle));
	printf("\t\tDevice type: 0x%04x (%s)\n", child->device_type,
	       child_device_type(child->device_type));
	dump_child_device_type_bits(child->device_type);

	if (context->bdb->version < 152) {
		printf("\t\tSignature: %.*s\n", (int)sizeof(child->device_id), child->device_id);
	} else {
		printf("\t\tI2C speed: 0x%02x\n", child->i2c_speed);
		printf("\t\tDP onboard redriver: 0x%02x\n", child->dp_onboard_redriver);
		printf("\t\tDP ondock redriver: 0x%02x\n", child->dp_ondock_redriver);
		printf("\t\tHDMI level shifter value: 0x%02x\n", child->hdmi_level_shifter_value);
		dump_hmdi_max_data_rate(child->hdmi_max_data_rate);
		printf("\t\tOffset to DTD buffer for edidless CHILD: 0x%02x\n", child->dtd_buf_ptr);
		printf("\t\tEdidless EFP: %s\n", YESNO(child->edidless_efp));
		printf("\t\tCompression enable: %s\n", YESNO(child->compression_enable));
		printf("\t\tCompression method CPS: %s\n", YESNO(child->compression_method));
		printf("\t\tDual pipe ganged eDP: %s\n", YESNO(child->ganged_edp));
		printf("\t\tCompression structure index: 0x%02x)\n", child->compression_structure_index);
		printf("\t\tSlave DDI port: 0x%02x (%s)\n", child->slave_port, dvo_port(child->slave_port));
	}

	printf("\t\tAIM offset: %d\n", child->addin_offset);
	printf("\t\tDVO Port: 0x%02x (%s)\n", child->dvo_port, dvo_port(child->dvo_port));

	printf("\t\tAIM I2C pin: 0x%02x\n", child->i2c_pin);
	printf("\t\tAIM Slave address: 0x%02x\n", child->slave_addr);
	printf("\t\tDDC pin: 0x%02x\n", child->ddc_pin);
	printf("\t\tEDID buffer ptr: 0x%02x\n", child->edid_ptr);
	printf("\t\tDVO config: 0x%02x\n", child->dvo_cfg);

	if (context->bdb->version < 155) {
		printf("\t\tDVO2 Port: 0x%02x (%s)\n", child->dvo2_port, dvo_port(child->dvo2_port));
		printf("\t\tI2C2 pin: 0x%02x\n", child->i2c2_pin);
		printf("\t\tSlave2 address: 0x%02x\n", child->slave2_addr);
		printf("\t\tDDC2 pin: 0x%02x\n", child->ddc2_pin);
	} else {
		printf("\t\tEFP routed through dock: %s\n", YESNO(child->efp_routed));
		printf("\t\tLane reversal: %s\n", YESNO(child->lane_reversal));
		printf("\t\tOnboard LSPCON: %s\n", YESNO(child->lspcon));
		printf("\t\tIboost enable: %s\n", YESNO(child->iboost));
		printf("\t\tHPD sense invert: %s\n", YESNO(child->hpd_invert));
		printf("\t\tHDMI compatible? %s\n", YESNO(child->hdmi_support));
		printf("\t\tDP compatible? %s\n", YESNO(child->dp_support));
		printf("\t\tTMDS compatible? %s\n", YESNO(child->tmds_support));
		printf("\t\tAux channel: 0x%02x\n", child->aux_channel);
		printf("\t\tDongle detect: 0x%02x\n", child->dongle_detect);
	}

	printf("\t\tPipe capabilities: 0x%02x\n", child->pipe_cap);
	printf("\t\tSDVO stall signal available: %s\n", YESNO(child->sdvo_stall));
	printf("\t\tHotplug connect status: 0x%02x\n", child->hpd_status);
	printf("\t\tIntegrated encoder instead of SDVO: %s\n", YESNO(child->integrated_encoder));
	printf("\t\tDVO wiring: 0x%02x\n", child->dvo_wiring);

	if (context->bdb->version < 171) {
		printf("\t\tDVO2 wiring: 0x%02x\n", child->dvo2_wiring);
	} else {
		printf("\t\tMIPI bridge type: %02x (%s)\n", child->mipi_bridge_type,
		       mipi_bridge_type(child->mipi_bridge_type));
	}

	printf("\t\tDevice class extension: 0x%02x\n", child->extended_type);
	printf("\t\tDVO function: 0x%02x\n", child->dvo_function);

	if (context->bdb->version >= 195) {
		printf("\t\tDP USB type C support: %s\n", YESNO(child->dp_usb_type_c));
		printf("\t\t2X DP GPIO index: 0x%02x\n", child->dp_gpio_index);
		printf("\t\t2X DP GPIO pin number: 0x%02x\n", child->dp_gpio_pin_num);
	}

	if (context->bdb->version >= 196) {
		printf("\t\tIBoost level for HDMI: 0x%02x\n", child->hdmi_iboost_level);
		printf("\t\tIBoost level for DP/eDP: 0x%02x\n", child->dp_iboost_level);
	}
}


static void dump_child_devices(struct context *context, const uint8_t *devices,
			       uint8_t child_dev_num, uint8_t child_dev_size)
{
	struct child_device_config *child;
	int i;

	/*
	 * Use a temp buffer so dump_child_device() doesn't have to worry about
	 * accessing the struct beyond child_dev_size. The tail, if any, remains
	 * initialized to zero.
	 */
	child = calloc(1, sizeof(*child));

	for (i = 0; i < child_dev_num; i++) {
		memcpy(child, devices + i * child_dev_size,
		       min(sizeof(*child), child_dev_size));

		dump_child_device(context, child);
	}

	free(child);
}

static void dump_general_definitions(struct context *context,
				     const struct bdb_block *block)
{
	const struct bdb_general_definitions *defs = block->data;
	int child_dev_num;

	child_dev_num = (block->size - sizeof(*defs)) / defs->child_dev_size;

	printf("\tCRT DDC GMBUS addr: 0x%02x\n", defs->crt_ddc_gmbus_pin);
	printf("\tUse ACPI DPMS CRT power states: %s\n",
	       YESNO(defs->dpms_acpi));
	printf("\tSkip CRT detect at boot: %s\n",
	       YESNO(defs->skip_boot_crt_detect));
	printf("\tUse DPMS on AIM devices: %s\n", YESNO(defs->dpms_aim));
	printf("\tBoot display type: 0x%02x%02x\n", defs->boot_display[1],
	       defs->boot_display[0]);
	printf("\tChild device size: %d\n", defs->child_dev_size);
	printf("\tChild device count: %d\n", child_dev_num);

	dump_child_devices(context, defs->devices,
			   child_dev_num, defs->child_dev_size);
}

static void dump_legacy_child_devices(struct context *context,
				      const struct bdb_block *block)
{
	const struct bdb_legacy_child_devices *defs = block->data;
	int child_dev_num;

	child_dev_num = (block->size - sizeof(*defs)) / defs->child_dev_size;

	printf("\tChild device size: %d\n", defs->child_dev_size);
	printf("\tChild device count: %d\n", child_dev_num);

	dump_child_devices(context, defs->devices,
			   child_dev_num, defs->child_dev_size);
}

static void dump_lvds_options(struct context *context,
			      const struct bdb_block *block)
{
	const struct bdb_lvds_options *options = block->data;

	if (context->panel_type == options->panel_type)
		printf("\tPanel type: %d\n", options->panel_type);
	else
		printf("\tPanel type: %d (override %d)\n",
		       options->panel_type, context->panel_type);
	printf("\tLVDS EDID available: %s\n", YESNO(options->lvds_edid));
	printf("\tPixel dither: %s\n", YESNO(options->pixel_dither));
	printf("\tPFIT auto ratio: %s\n", YESNO(options->pfit_ratio_auto));
	printf("\tPFIT enhanced graphics mode: %s\n",
	       YESNO(options->pfit_gfx_mode_enhanced));
	printf("\tPFIT enhanced text mode: %s\n",
	       YESNO(options->pfit_text_mode_enhanced));
	printf("\tPFIT mode: %d\n", options->pfit_mode);
}

static void dump_lvds_ptr_data(struct context *context,
			       const struct bdb_block *block)
{
	const struct bdb_lvds_lfp_data_ptrs *ptrs = block->data;

	printf("\tNumber of entries: %d\n", ptrs->lvds_entries);
}

static void dump_lvds_data(struct context *context,
			   const struct bdb_block *block)
{
	const struct bdb_lvds_lfp_data *lvds_data = block->data;
	struct bdb_block *ptrs_block;
	const struct bdb_lvds_lfp_data_ptrs *ptrs;
	int num_entries;
	int i;
	int hdisplay, hsyncstart, hsyncend, htotal;
	int vdisplay, vsyncstart, vsyncend, vtotal;
	float clock;
	int lfp_data_size, dvo_offset;

	ptrs_block = find_section(context, BDB_LVDS_LFP_DATA_PTRS);
	if (!ptrs_block) {
		printf("No LVDS ptr block\n");
		return;
	}

	ptrs = ptrs_block->data;

	lfp_data_size =
	    ptrs->ptr[1].fp_timing_offset - ptrs->ptr[0].fp_timing_offset;
	dvo_offset =
	    ptrs->ptr[0].dvo_timing_offset - ptrs->ptr[0].fp_timing_offset;

	num_entries = block->size / lfp_data_size;

	printf("  Number of entries: %d (preferred block marked with '*')\n",
	       num_entries);

	for (i = 0; i < num_entries; i++) {
		const uint8_t *lfp_data_ptr =
		    (const uint8_t *) lvds_data->data + lfp_data_size * i;
		const uint8_t *timing_data = lfp_data_ptr + dvo_offset;
		const struct lvds_lfp_data_entry *lfp_data =
		    (const struct lvds_lfp_data_entry *)lfp_data_ptr;
		char marker;

		if (i != context->panel_type && !context->dump_all_panel_types)
			continue;

		if (i == context->panel_type)
			marker = '*';
		else
			marker = ' ';

		hdisplay = _H_ACTIVE(timing_data);
		hsyncstart = hdisplay + _H_SYNC_OFF(timing_data);
		hsyncend = hsyncstart + _H_SYNC_WIDTH(timing_data);
		htotal = hdisplay + _H_BLANK(timing_data);

		vdisplay = _V_ACTIVE(timing_data);
		vsyncstart = vdisplay + _V_SYNC_OFF(timing_data);
		vsyncend = vsyncstart + _V_SYNC_WIDTH(timing_data);
		vtotal = vdisplay + _V_BLANK(timing_data);
		clock = _PIXEL_CLOCK(timing_data) / 1000;

		printf("%c\tpanel type %02i: %dx%d clock %d\n", marker,
		       i, lfp_data->fp_timing.x_res, lfp_data->fp_timing.y_res,
		       _PIXEL_CLOCK(timing_data));
		printf("\t\tinfo:\n");
		printf("\t\t  LVDS: 0x%08lx\n",
		       (unsigned long)lfp_data->fp_timing.lvds_reg_val);
		printf("\t\t  PP_ON_DELAYS: 0x%08lx\n",
		       (unsigned long)lfp_data->fp_timing.pp_on_reg_val);
		printf("\t\t  PP_OFF_DELAYS: 0x%08lx\n",
		       (unsigned long)lfp_data->fp_timing.pp_off_reg_val);
		printf("\t\t  PP_DIVISOR: 0x%08lx\n",
		       (unsigned long)lfp_data->fp_timing.pp_cycle_reg_val);
		printf("\t\t  PFIT: 0x%08lx\n",
		       (unsigned long)lfp_data->fp_timing.pfit_reg_val);
		printf("\t\ttimings: %d %d %d %d %d %d %d %d %.2f (%s)\n",
		       hdisplay, hsyncstart, hsyncend, htotal,
		       vdisplay, vsyncstart, vsyncend, vtotal, clock,
		       (hsyncend > htotal || vsyncend > vtotal) ?
		       "BAD!" : "good");
	}

	free(ptrs_block);
}

static void dump_driver_feature(struct context *context,
				const struct bdb_block *block)
{
	const struct bdb_driver_features *feature = block->data;

	printf("\tBoot Device Algorithm: %s\n", feature->boot_dev_algorithm ?
	       "driver default" : "os default");
	printf("\tBlock display switching when DVD active: %s\n",
	       YESNO(feature->block_display_switch));
	printf("\tAllow display switching when in Full Screen DOS: %s\n",
	       YESNO(feature->allow_display_switch));
	printf("\tHot Plug DVO: %s\n", YESNO(feature->hotplug_dvo));
	printf("\tDual View Zoom: %s\n", YESNO(feature->dual_view_zoom));
	printf("\tDriver INT 15h hook: %s\n", YESNO(feature->int15h_hook));
	printf("\tEnable Sprite in Clone Mode: %s\n",
	       YESNO(feature->sprite_in_clone));
	printf("\tUse 00000110h ID for Primary LFP: %s\n",
	       YESNO(feature->primary_lfp_id));
	printf("\tBoot Mode X: %u\n", feature->boot_mode_x);
	printf("\tBoot Mode Y: %u\n", feature->boot_mode_y);
	printf("\tBoot Mode Bpp: %u\n", feature->boot_mode_bpp);
	printf("\tBoot Mode Refresh: %u\n", feature->boot_mode_refresh);
	printf("\tEnable LFP as primary: %s\n",
	       YESNO(feature->enable_lfp_primary));
	printf("\tSelective Mode Pruning: %s\n",
	       YESNO(feature->selective_mode_pruning));
	printf("\tDual-Frequency Graphics Technology: %s\n",
	       YESNO(feature->dual_frequency));
	printf("\tDefault Render Clock Frequency: %s\n",
	       feature->render_clock_freq ? "low" : "high");
	printf("\tNT 4.0 Dual Display Clone Support: %s\n",
	       YESNO(feature->nt_clone_support));
	printf("\tDefault Power Scheme user interface: %s\n",
	       feature->power_scheme_ui ? "3rd party" : "CUI");
	printf
	    ("\tSprite Display Assignment when Overlay is Active in Clone Mode: %s\n",
	     feature->sprite_display_assign ? "primary" : "secondary");
	printf("\tDisplay Maintain Aspect Scaling via CUI: %s\n",
	       YESNO(feature->cui_aspect_scaling));
	printf("\tPreserve Aspect Ratio: %s\n",
	       YESNO(feature->preserve_aspect_ratio));
	printf("\tEnable SDVO device power down: %s\n",
	       YESNO(feature->sdvo_device_power_down));
	printf("\tCRT hotplug: %s\n", YESNO(feature->crt_hotplug));
	printf("\tLVDS config: ");
	switch (feature->lvds_config) {
	case BDB_DRIVER_NO_LVDS:
		printf("No LVDS\n");
		break;
	case BDB_DRIVER_INT_LVDS:
		printf("Integrated LVDS\n");
		break;
	case BDB_DRIVER_SDVO_LVDS:
		printf("SDVO LVDS\n");
		break;
	case BDB_DRIVER_EDP:
		printf("Embedded DisplayPort\n");
		break;
	}
	printf("\tDefine Display statically: %s\n",
	       YESNO(feature->static_display));
	printf("\tLegacy CRT max X: %d\n", feature->legacy_crt_max_x);
	printf("\tLegacy CRT max Y: %d\n", feature->legacy_crt_max_y);
	printf("\tLegacy CRT max refresh: %d\n",
	       feature->legacy_crt_max_refresh);
	printf("\tEnable DRRS: %s\n", YESNO(feature->drrs_enabled));
	printf("\tEnable PSR: %s\n", YESNO(feature->psr_enabled));
}

static void dump_edp(struct context *context,
		     const struct bdb_block *block)
{
	const struct bdb_edp *edp = block->data;
	int bpp, msa;
	int i;

	for (i = 0; i < 16; i++) {
		if (i != context->panel_type && !context->dump_all_panel_types)
			continue;

		printf("\tPanel %d%s\n", i, context->panel_type == i ? " *" : "");

		printf("\t\tPower Sequence: T3 %d T7 %d T9 %d T10 %d T12 %d\n",
		       edp->power_seqs[i].t3,
		       edp->power_seqs[i].t7,
		       edp->power_seqs[i].t9,
		       edp->power_seqs[i].t10,
		       edp->power_seqs[i].t12);

		bpp = (edp->color_depth >> (i * 2)) & 3;

		printf("\t\tPanel color depth: ");
		switch (bpp) {
		case EDP_18BPP:
			printf("18 bpp\n");
			break;
		case EDP_24BPP:
			printf("24 bpp\n");
			break;
		case EDP_30BPP:
			printf("30 bpp\n");
			break;
		default:
			printf("(unknown value %d)\n", bpp);
			break;
		}

		msa = (edp->sdrrs_msa_timing_delay >> (i * 2)) & 3;
		printf("\t\teDP sDRRS MSA Delay: Lane %d\n", msa + 1);

		printf("\t\tFast link params:\n");
		printf("\t\t\trate: ");
		if (edp->fast_link_params[i].rate == EDP_RATE_1_62)
			printf("1.62G\n");
		else if (edp->fast_link_params[i].rate == EDP_RATE_2_7)
			printf("2.7G\n");
		printf("\t\t\tlanes: ");
		switch (edp->fast_link_params[i].lanes) {
		case EDP_LANE_1:
			printf("x1 mode\n");
			break;
		case EDP_LANE_2:
			printf("x2 mode\n");
			break;
		case EDP_LANE_4:
			printf("x4 mode\n");
			break;
		default:
			printf("(unknown value %d)\n",
			       edp->fast_link_params[i].lanes);
			break;
		}
		printf("\t\t\tpre-emphasis: ");
		switch (edp->fast_link_params[i].preemphasis) {
		case EDP_PREEMPHASIS_NONE:
			printf("none\n");
			break;
		case EDP_PREEMPHASIS_3_5dB:
			printf("3.5dB\n");
			break;
		case EDP_PREEMPHASIS_6dB:
			printf("6dB\n");
			break;
		case EDP_PREEMPHASIS_9_5dB:
			printf("9.5dB\n");
			break;
		default:
			printf("(unknown value %d)\n",
			       edp->fast_link_params[i].preemphasis);
			break;
		}
		printf("\t\t\tvswing: ");
		switch (edp->fast_link_params[i].vswing) {
		case EDP_VSWING_0_4V:
			printf("0.4V\n");
			break;
		case EDP_VSWING_0_6V:
			printf("0.6V\n");
			break;
		case EDP_VSWING_0_8V:
			printf("0.8V\n");
			break;
		case EDP_VSWING_1_2V:
			printf("1.2V\n");
			break;
		default:
			printf("(unknown value %d)\n",
			       edp->fast_link_params[i].vswing);
			break;
		}

		if (context->bdb->version >= 162) {
			bool val = (edp->edp_s3d_feature >> i) & 1;
			printf("\t\tStereo 3D feature: %s\n", YESNO(val));
		}

		if (context->bdb->version >= 165) {
			bool val = (edp->edp_t3_optimization >> i) & 1;
			printf("\t\tT3 optimization: %s\n", YESNO(val));
		}

		if (context->bdb->version >= 173) {
			int val = (edp->edp_vswing_preemph >> (i * 4)) & 0xf;

			printf("\t\tVswing/preemphasis table selection: ");
			switch (val) {
			case 0:
				printf("Low power (200 mV)\n");
				break;
			case 1:
				printf("Default (400 mV)\n");
				break;
			default:
				printf("(unknown value %d)\n", val);
				break;
			}
		}

		if (context->bdb->version >= 182) {
			bool val = (edp->fast_link_training >> i) & 1;
			printf("\t\tFast link training: %s\n", YESNO(val));
		}

		if (context->bdb->version >= 185) {
			bool val = (edp->dpcd_600h_write_required >> i) & 1;
			printf("\t\tDPCD 600h write required: %s\n", YESNO(val));
		}

		if (context->bdb->version >= 186) {
			printf("\t\tPWM delays:\n"
			       "\t\t\tPWM on to backlight enable: %d\n"
			       "\t\t\tBacklight disable to PWM off: %d\n",
			       edp->pwm_delays[i].pwm_on_to_backlight_enable,
			       edp->pwm_delays[i].backlight_disable_to_pwm_off);
		}

		if (context->bdb->version >= 199) {
			bool val = (edp->full_link_params_provided >> i) & 1;

			printf("\t\tFull link params provided: %s\n", YESNO(val));
			printf("\t\tFull link params:\n");
			printf("\t\t\tpre-emphasis: ");
			switch (edp->full_link_params[i].preemphasis) {
			case EDP_PREEMPHASIS_NONE:
				printf("none\n");
				break;
			case EDP_PREEMPHASIS_3_5dB:
				printf("3.5dB\n");
				break;
			case EDP_PREEMPHASIS_6dB:
				printf("6dB\n");
				break;
			case EDP_PREEMPHASIS_9_5dB:
				printf("9.5dB\n");
				break;
			default:
				printf("(unknown value %d)\n",
				       edp->full_link_params[i].preemphasis);
				break;
			}
			printf("\t\t\tvswing: ");
			switch (edp->full_link_params[i].vswing) {
			case EDP_VSWING_0_4V:
				printf("0.4V\n");
				break;
			case EDP_VSWING_0_6V:
				printf("0.6V\n");
				break;
			case EDP_VSWING_0_8V:
				printf("0.8V\n");
				break;
			case EDP_VSWING_1_2V:
				printf("1.2V\n");
				break;
			default:
				printf("(unknown value %d)\n",
				       edp->full_link_params[i].vswing);
				break;
			}
		}
	}
}

static void dump_psr(struct context *context,
		     const struct bdb_block *block)
{
	const struct bdb_psr *psr_block = block->data;
	int i;
	uint32_t psr2_tp_time;

	/* The same block ID was used for something else before? */
	if (context->bdb->version < 165)
		return;

	psr2_tp_time = psr_block->psr2_tp2_tp3_wakeup_time;
	for (i = 0; i < 16; i++) {
		const struct psr_table *psr = &psr_block->psr_table[i];

		if (i != context->panel_type && !context->dump_all_panel_types)
			continue;

		printf("\tPanel %d%s\n", i, context->panel_type == i ? " *" : "");

		printf("\t\tFull link: %s\n", YESNO(psr->full_link));
		printf("\t\tRequire AUX to wakeup: %s\n", YESNO(psr->require_aux_to_wakeup));

		switch (psr->lines_to_wait) {
		case 0:
		case 1:
			printf("\t\tLines to wait before link standby: %d\n",
			       psr->lines_to_wait);
			break;
		case 2:
		case 3:
			printf("\t\tLines to wait before link standby: %d\n",
			       1 << psr->lines_to_wait);
			break;
		default:
			printf("\t\tLines to wait before link standby: (unknown) (0x%x)\n",
			       psr->lines_to_wait);
			break;
		}

		printf("\t\tIdle frames to for PSR enable: %d\n",
		       psr->idle_frames);

		printf("\t\tTP1 wakeup time: %d usec (0x%x)\n",
		       psr->tp1_wakeup_time * 100,
		       psr->tp1_wakeup_time);

		printf("\t\tTP2/TP3 wakeup time: %d usec (0x%x)\n",
		       psr->tp2_tp3_wakeup_time * 100,
		       psr->tp2_tp3_wakeup_time);

		if (context->bdb->version >= 226) {
			int index;
			static const uint16_t psr2_tp_times[] = {500, 100, 2500, 5};

			index = (psr2_tp_time >> (i * 2)) & 0x3;
			printf("\t\tPSR2 TP2/TP3 wakeup time: %d usec (0x%x)\n",
			       psr2_tp_times[index], index);
		}
	}
}

static void
print_detail_timing_data(const struct lvds_dvo_timing *dvo_timing)
{
	int display, sync_start, sync_end, total;

	display = (dvo_timing->hactive_hi << 8) | dvo_timing->hactive_lo;
	sync_start = display +
		((dvo_timing->hsync_off_hi << 8) | dvo_timing->hsync_off_lo);
	sync_end = sync_start + ((dvo_timing->hsync_pulse_width_hi << 8) |
				 dvo_timing->hsync_pulse_width_lo);
	total = display +
		((dvo_timing->hblank_hi << 8) | dvo_timing->hblank_lo);
	printf("\thdisplay: %d\n", display);
	printf("\thsync [%d, %d] %s\n", sync_start, sync_end,
	       dvo_timing->hsync_positive ? "+sync" : "-sync");
	printf("\thtotal: %d\n", total);

	display = (dvo_timing->vactive_hi << 8) | dvo_timing->vactive_lo;
	sync_start = display + ((dvo_timing->vsync_off_hi << 8) |
				dvo_timing->vsync_off_lo);
	sync_end = sync_start + ((dvo_timing->vsync_pulse_width_hi << 8) |
				 dvo_timing->vsync_pulse_width_lo);
	total = display +
		((dvo_timing->vblank_hi << 8) | dvo_timing->vblank_lo);
	printf("\tvdisplay: %d\n", display);
	printf("\tvsync [%d, %d] %s\n", sync_start, sync_end,
	       dvo_timing->vsync_positive ? "+sync" : "-sync");
	printf("\tvtotal: %d\n", total);

	printf("\tclock: %d\n", dvo_timing->clock * 10);
}

static void dump_sdvo_panel_dtds(struct context *context,
				 const struct bdb_block *block)
{
	const struct lvds_dvo_timing *dvo_timing = block->data;
	int n, count;

	count = block->size / sizeof(struct lvds_dvo_timing);
	for (n = 0; n < count; n++) {
		printf("%d:\n", n);
		print_detail_timing_data(dvo_timing++);
	}
}

static void dump_sdvo_lvds_options(struct context *context,
				   const struct bdb_block *block)
{
	const struct bdb_sdvo_lvds_options *options = block->data;

	printf("\tbacklight: %d\n", options->panel_backlight);
	printf("\th40 type: %d\n", options->h40_set_panel_type);
	printf("\ttype: %d\n", options->panel_type);
	printf("\tssc_clk_freq: %d\n", options->ssc_clk_freq);
	printf("\tals_low_trip: %d\n", options->als_low_trip);
	printf("\tals_high_trip: %d\n", options->als_high_trip);
	/*
	u8 sclalarcoeff_tab_row_num;
	u8 sclalarcoeff_tab_row_size;
	u8 coefficient[8];
	*/
	printf("\tmisc[0]: %x\n", options->panel_misc_bits_1);
	printf("\tmisc[1]: %x\n", options->panel_misc_bits_2);
	printf("\tmisc[2]: %x\n", options->panel_misc_bits_3);
	printf("\tmisc[3]: %x\n", options->panel_misc_bits_4);
}

static void dump_mipi_config(struct context *context,
			     const struct bdb_block *block)
{
	const struct bdb_mipi_config *start = block->data;
	const struct mipi_config *config;
	const struct mipi_pps_data *pps;

	config = &start->config[context->panel_type];
	pps = &start->pps[context->panel_type];

	printf("\tGeneral Param\n");
	printf("\t\t BTA disable: %s\n", config->bta ? "Disabled" : "Enabled");
	printf("\t\t Panel Rotation: %d degrees\n", config->rotation * 90);

	printf("\t\t Video Mode Color Format: ");
	if (config->videomode_color_format == 0)
		printf("Not supported\n");
	else if (config->videomode_color_format == 1)
		printf("RGB565\n");
	else if (config->videomode_color_format == 2)
		printf("RGB666\n");
	else if (config->videomode_color_format == 3)
		printf("RGB666 Loosely Packed\n");
	else if (config->videomode_color_format == 4)
		printf("RGB888\n");
	printf("\t\t PPS GPIO Pins: %s \n", config->pwm_blc ? "Using SOC" : "Using PMIC");
	printf("\t\t CABC Support: %s\n", config->cabc ? "supported" : "not supported");
	printf("\t\t Mode: %s\n", config->cmd_mode ? "COMMAND" : "VIDEO");
	printf("\t\t Video transfer mode: %s (0x%x)\n",
	       config->vtm == 1 ? "non-burst with sync pulse" :
	       config->vtm == 2 ? "non-burst with sync events" :
	       config->vtm == 3 ? "burst" : "<unknown>",
	       config->vtm);
	printf("\t\t Dithering: %s\n", config->dithering ? "done in Display Controller" : "done in Panel Controller");

	printf("\tPort Desc\n");
	printf("\t\t Pixel overlap: %d\n", config->pixel_overlap);
	printf("\t\t Lane Count: %d\n", config->lane_cnt + 1);
	printf("\t\t Dual Link Support: ");
	if (config->dual_link == 0)
		printf("not supported\n");
	else if (config->dual_link == 1)
		printf("Front Back mode\n");
	else
		printf("Pixel Alternative Mode\n");

	printf("\tDphy Flags\n");
	printf("\t\t Clock Stop: %s\n", config->clk_stop ? "ENABLED" : "DISABLED");
	printf("\t\t EOT disabled: %s\n\n", config->eot_disabled ? "EOT not to be sent" : "EOT to be sent");

	printf("\tHSTxTimeOut: 0x%x\n", config->hs_tx_timeout);
	printf("\tLPRXTimeOut: 0x%x\n", config->lp_rx_timeout);
	printf("\tTurnAroundTimeOut: 0x%x\n", config->turn_around_timeout);
	printf("\tDeviceResetTimer: 0x%x\n", config->device_reset_timer);
	printf("\tMasterinitTimer: 0x%x\n", config->master_init_timer);
	printf("\tDBIBandwidthTimer: 0x%x\n", config->dbi_bw_timer);
	printf("\tLpByteClkValue: 0x%x\n\n", config->lp_byte_clk_val);

	printf("\tDphy Params\n");
	printf("\t\tExit to zero Count: 0x%x\n", config->exit_zero_cnt);
	printf("\t\tTrail Count: 0x%X\n", config->trail_cnt);
	printf("\t\tClk zero count: 0x%x\n", config->clk_zero_cnt);
	printf("\t\tPrepare count:0x%x\n\n", config->prepare_cnt);

	printf("\tClockLaneSwitchingCount: 0x%x\n", config->clk_lane_switch_cnt);
	printf("\tHighToLowSwitchingCount: 0x%x\n\n", config->hl_switch_cnt);

	printf("\tTimings based on Dphy spec\n");
	printf("\t\tTClkMiss: 0x%x\n", config->tclk_miss);
	printf("\t\tTClkPost: 0x%x\n", config->tclk_post);
	printf("\t\tTClkPre: 0x%x\n", config->tclk_pre);
	printf("\t\tTClkPrepare: 0x%x\n", config->tclk_prepare);
	printf("\t\tTClkSettle: 0x%x\n", config->tclk_settle);
	printf("\t\tTClkTermEnable: 0x%x\n\n", config->tclk_term_enable);

	printf("\tTClkTrail: 0x%x\n", config->tclk_trail);
	printf("\tTClkPrepareTClkZero: 0x%x\n", config->tclk_prepare_clkzero);
	printf("\tTHSExit: 0x%x\n", config->ths_exit);
	printf("\tTHsPrepare: 0x%x\n", config->ths_prepare);
	printf("\tTHsPrepareTHsZero: 0x%x\n", config->ths_prepare_hszero);
	printf("\tTHSSettle: 0x%x\n", config->ths_settle);
	printf("\tTHSSkip: 0x%x\n", config->ths_skip);
	printf("\tTHsTrail: 0x%x\n", config->ths_trail);
	printf("\tTInit: 0x%x\n", config->tinit);
	printf("\tTLPX: 0x%x\n", config->tlpx);

	printf("\tMIPI PPS\n");
	printf("\t\tPanel power ON delay: %d\n", pps->panel_on_delay);
	printf("\t\tPanel power on to Backlight enable delay: %d\n", pps->bl_enable_delay);
	printf("\t\tBacklight disable to Panel power OFF delay: %d\n", pps->bl_disable_delay);
	printf("\t\tPanel power OFF delay: %d\n", pps->panel_off_delay);
	printf("\t\tPanel power cycle delay: %d\n", pps->panel_power_cycle_delay);
}

static const uint8_t *mipi_dump_send_packet(const uint8_t *data, uint8_t seq_version)
{
	uint8_t flags, type;
	uint16_t len, i;

	flags = *data++;
	type = *data++;
	len = *((const uint16_t *) data);
	data += 2;

	printf("\t\tSend DCS: Port %s, VC %d, %s, Type %02x, Length %u, Data",
	       (flags >> 3) & 1 ? "C" : "A",
	       (flags >> 1) & 3,
	       flags & 1 ? "HS" : "LP",
	       type,
	       len);
	for (i = 0; i < len; i++)
		printf(" %02x", *data++);
	printf("\n");

	return data;
}

static const uint8_t *mipi_dump_delay(const uint8_t *data, uint8_t seq_version)
{
	printf("\t\tDelay: %u us\n", *((const uint32_t *)data));

	return data + 4;
}

static const uint8_t *mipi_dump_gpio(const uint8_t *data, uint8_t seq_version)
{
	uint8_t index, number, flags;

	if (seq_version >= 3) {
		index = *data++;
		number = *data++;
		flags = *data++;

		printf("\t\tGPIO index %u, number %u, set %d (0x%02x)\n",
		       index, number, flags & 1, flags);
	} else {
		index = *data++;
		flags = *data++;

		printf("\t\tGPIO index %u, source %d, set %d (0x%02x)\n",
		       index, (flags >> 1) & 3, flags & 1, flags);
	}

	return data;
}

static const uint8_t *mipi_dump_i2c(const uint8_t *data, uint8_t seq_version)
{
	uint8_t flags, index, bus, offset, len, i;
	uint16_t address;

	flags = *data++;
	index = *data++;
	bus = *data++;
	address = *((const uint16_t *) data);
	data += 2;
	offset = *data++;
	len = *data++;

	printf("\t\tSend I2C: Flags %02x, Index %02x, Bus %02x, Address %04x, Offset %02x, Length %u, Data",
	       flags, index, bus, address, offset, len);
	for (i = 0; i < len; i++)
		printf(" %02x", *data++);
	printf("\n");

	return data;
}

typedef const uint8_t * (*fn_mipi_elem_dump)(const uint8_t *data, uint8_t seq_version);

static const fn_mipi_elem_dump dump_elem[] = {
	[MIPI_SEQ_ELEM_SEND_PKT] = mipi_dump_send_packet,
	[MIPI_SEQ_ELEM_DELAY] = mipi_dump_delay,
	[MIPI_SEQ_ELEM_GPIO] = mipi_dump_gpio,
	[MIPI_SEQ_ELEM_I2C] = mipi_dump_i2c,
};

static const char * const seq_name[] = {
	[MIPI_SEQ_ASSERT_RESET] = "MIPI_SEQ_ASSERT_RESET",
	[MIPI_SEQ_INIT_OTP] = "MIPI_SEQ_INIT_OTP",
	[MIPI_SEQ_DISPLAY_ON] = "MIPI_SEQ_DISPLAY_ON",
	[MIPI_SEQ_DISPLAY_OFF]  = "MIPI_SEQ_DISPLAY_OFF",
	[MIPI_SEQ_DEASSERT_RESET] = "MIPI_SEQ_DEASSERT_RESET",
	[MIPI_SEQ_BACKLIGHT_ON] = "MIPI_SEQ_BACKLIGHT_ON",
	[MIPI_SEQ_BACKLIGHT_OFF] = "MIPI_SEQ_BACKLIGHT_OFF",
	[MIPI_SEQ_TEAR_ON] = "MIPI_SEQ_TEAR_ON",
	[MIPI_SEQ_TEAR_OFF] = "MIPI_SEQ_TEAR_OFF",
	[MIPI_SEQ_POWER_ON] = "MIPI_SEQ_POWER_ON",
	[MIPI_SEQ_POWER_OFF] = "MIPI_SEQ_POWER_OFF",
};

static const char *sequence_name(enum mipi_seq seq_id)
{
	if (seq_id < ARRAY_SIZE(seq_name) && seq_name[seq_id])
		return seq_name[seq_id];
	else
		return "(unknown)";
}

static const uint8_t *dump_sequence(const uint8_t *data, uint8_t seq_version)
{
	fn_mipi_elem_dump mipi_elem_dump;

	printf("\tSequence %u - %s\n", *data, sequence_name(*data));

	/* Skip Sequence Byte. */
	data++;

	/* Skip Size of Sequence. */
	if (seq_version >= 3)
		data += 4;

	while (1) {
		uint8_t operation_byte = *data++;
		uint8_t operation_size = 0;

		if (operation_byte == MIPI_SEQ_ELEM_END)
			break;

		if (operation_byte < ARRAY_SIZE(dump_elem))
			mipi_elem_dump = dump_elem[operation_byte];
		else
			mipi_elem_dump = NULL;

		/* Size of Operation. */
		if (seq_version >= 3)
			operation_size = *data++;

		if (mipi_elem_dump) {
			const uint8_t *next = data + operation_size;

			data = mipi_elem_dump(data, seq_version);

			if (operation_size && next != data)
				printf("Error: Inconsistent operation size: %d\n",
					operation_size);
		} else if (operation_size) {
			/* We have size, skip. */
			data += operation_size;
		} else {
			/* No size, can't skip without parsing. */
			printf("Error: Unsupported MIPI element %u\n",
			       operation_byte);
			return NULL;
		}
	}

	return data;
}

/* Find the sequence block and size for the given panel. */
static const uint8_t *
find_panel_sequence_block(const struct bdb_mipi_sequence *sequence,
			  uint16_t panel_id, uint32_t total, uint32_t *seq_size)
{
	const uint8_t *data = &sequence->data[0];
	uint8_t current_id;
	uint32_t current_size;
	int header_size = sequence->version >= 3 ? 5 : 3;
	int index = 0;
	int i;

	/* skip new block size */
	if (sequence->version >= 3)
		data += 4;

	for (i = 0; i < MAX_MIPI_CONFIGURATIONS && index < total; i++) {
		if (index + header_size > total) {
			fprintf(stderr, "Invalid sequence block (header)\n");
			return NULL;
		}

		current_id = *(data + index);
		if (sequence->version >= 3)
			current_size = *((const uint32_t *)(data + index + 1));
		else
			current_size = *((const uint16_t *)(data + index + 1));

		index += header_size;

		if (index + current_size > total) {
			fprintf(stderr, "Invalid sequence block\n");
			return NULL;
		}

		if (current_id == panel_id) {
			*seq_size = current_size;
			return data + index;
		}

		index += current_size;
	}

	fprintf(stderr, "Sequence block detected but no valid configuration\n");

	return NULL;
}

static int goto_next_sequence(const uint8_t *data, int index, int total)
{
	uint16_t len;

	/* Skip Sequence Byte. */
	for (index = index + 1; index < total; index += len) {
		uint8_t operation_byte = *(data + index);
		index++;

		switch (operation_byte) {
		case MIPI_SEQ_ELEM_END:
			return index;
		case MIPI_SEQ_ELEM_SEND_PKT:
			if (index + 4 > total)
				return 0;

			len = *((const uint16_t *)(data + index + 2)) + 4;
			break;
		case MIPI_SEQ_ELEM_DELAY:
			len = 4;
			break;
		case MIPI_SEQ_ELEM_GPIO:
			len = 2;
			break;
		case MIPI_SEQ_ELEM_I2C:
			if (index + 7 > total)
				return 0;
			len = *(data + index + 6) + 7;
			break;
		default:
			fprintf(stderr, "Unknown operation byte\n");
			return 0;
		}
	}

	return 0;
}

static int goto_next_sequence_v3(const uint8_t *data, int index, int total)
{
	int seq_end;
	uint16_t len;
	uint32_t size_of_sequence;

	/*
	 * Could skip sequence based on Size of Sequence alone, but also do some
	 * checking on the structure.
	 */
	if (total < 5) {
		fprintf(stderr, "Too small sequence size\n");
		return 0;
	}

	/* Skip Sequence Byte. */
	index++;

	/*
	 * Size of Sequence. Excludes the Sequence Byte and the size itself,
	 * includes MIPI_SEQ_ELEM_END byte, excludes the final MIPI_SEQ_END
	 * byte.
	 */
	size_of_sequence = *((const uint32_t *)(data + index));
	index += 4;

	seq_end = index + size_of_sequence;
	if (seq_end > total) {
		fprintf(stderr, "Invalid sequence size\n");
		return 0;
	}

	for (; index < total; index += len) {
		uint8_t operation_byte = *(data + index);
		index++;

		if (operation_byte == MIPI_SEQ_ELEM_END) {
			if (index != seq_end) {
				fprintf(stderr, "Invalid element structure\n");
				return 0;
			}
			return index;
		}

		len = *(data + index);
		index++;

		/*
		 * FIXME: Would be nice to check elements like for v1/v2 in
		 * goto_next_sequence() above.
		 */
		switch (operation_byte) {
		case MIPI_SEQ_ELEM_SEND_PKT:
		case MIPI_SEQ_ELEM_DELAY:
		case MIPI_SEQ_ELEM_GPIO:
		case MIPI_SEQ_ELEM_I2C:
		case MIPI_SEQ_ELEM_SPI:
		case MIPI_SEQ_ELEM_PMIC:
			break;
		default:
			fprintf(stderr, "Unknown operation byte %u\n",
				operation_byte);
			break;
		}
	}

	return 0;
}

static void dump_mipi_sequence(struct context *context,
			       const struct bdb_block *block)
{
	const struct bdb_mipi_sequence *sequence = block->data;
	const uint8_t *data;
	uint32_t seq_size;
	int index = 0, i;
	const uint8_t *sequence_ptrs[MIPI_SEQ_MAX] = {};

	/* Check if we have sequence block as well */
	if (!sequence) {
		printf("No MIPI Sequence found\n");
		return;
	}

	printf("\tSequence block version v%u\n", sequence->version);

	/* Fail gracefully for forward incompatible sequence block. */
	if (sequence->version >= 4) {
		fprintf(stderr, "Unable to parse MIPI Sequence Block v%u\n",
			sequence->version);
		return;
	}

	data = find_panel_sequence_block(sequence, context->panel_type,
					 block->size, &seq_size);
	if (!data)
		return;

	/* Parse the sequences. Corresponds to VBT parsing in the kernel. */
	for (;;) {
		uint8_t seq_id = *(data + index);
		if (seq_id == MIPI_SEQ_END)
			break;

		if (seq_id >= MIPI_SEQ_MAX) {
			fprintf(stderr, "Unknown sequence %u\n", seq_id);
			return;
		}

		sequence_ptrs[seq_id] = data + index;

		if (sequence->version >= 3)
			index = goto_next_sequence_v3(data, index, seq_size);
		else
			index = goto_next_sequence(data, index, seq_size);
		if (!index) {
			fprintf(stderr, "Invalid sequence %u\n", seq_id);
			return;
		}
	}

	/* Dump the sequences. Corresponds to sequence execution in kernel. */
	for (i = 0; i < ARRAY_SIZE(sequence_ptrs); i++)
		if (sequence_ptrs[i])
			dump_sequence(sequence_ptrs[i], sequence->version);
}

/* get panel type from lvds options block, or -1 if block not found */
static int get_panel_type(struct context *context)
{
	struct bdb_block *block;
	const struct bdb_lvds_options *options;
	int panel_type;

	block = find_section(context, BDB_LVDS_OPTIONS);
	if (!block)
		return -1;

	options = block->data;
	panel_type = options->panel_type;

	free(block);

	return panel_type;
}

static int
get_device_id(unsigned char *bios, int size)
{
    int device;
    int offset = (bios[0x19] << 8) + bios[0x18];

    if (offset + 7 >= size)
	return -1;

    if (bios[offset] != 'P' ||
	bios[offset+1] != 'C' ||
	bios[offset+2] != 'I' ||
	bios[offset+3] != 'R')
	return -1;

    device = (bios[offset+7] << 8) + bios[offset+6];

    return device;
}

struct dumper {
	uint8_t id;
	const char *name;
	void (*dump)(struct context *context,
		     const struct bdb_block *block);
};

struct dumper dumpers[] = {
	{
		.id = BDB_GENERAL_FEATURES,
		.name = "General features block",
		.dump = dump_general_features,
	},
	{
		.id = BDB_GENERAL_DEFINITIONS,
		.name = "General definitions block",
		.dump = dump_general_definitions,
	},
	{
		.id = BDB_CHILD_DEVICE_TABLE,
		.name = "Legacy child devices block",
		.dump = dump_legacy_child_devices,
	},
	{
		.id = BDB_LVDS_OPTIONS,
		.name = "LVDS options block",
		.dump = dump_lvds_options,
	},
	{
		.id = BDB_LVDS_LFP_DATA_PTRS,
		.name = "LVDS timing pointer data",
		.dump = dump_lvds_ptr_data,
	},
	{
		.id = BDB_LVDS_LFP_DATA,
		.name = "LVDS panel data block",
		.dump = dump_lvds_data,
	},
	{
		.id = BDB_LVDS_BACKLIGHT,
		.name = "Backlight info block",
		.dump = dump_backlight_info,
	},
	{
		.id = BDB_SDVO_LVDS_OPTIONS,
		.name = "SDVO LVDS options block",
		.dump = dump_sdvo_lvds_options,
	},
	{
		.id = BDB_SDVO_PANEL_DTDS,
		.name = "SDVO panel dtds",
		.dump = dump_sdvo_panel_dtds,
	},
	{
		.id = BDB_DRIVER_FEATURES,
		.name = "Driver feature data block",
		.dump = dump_driver_feature,
	},
	{
		.id = BDB_EDP,
		.name = "eDP block",
		.dump = dump_edp,
	},
	{
		.id = BDB_PSR,
		.name = "PSR block",
		.dump = dump_psr,
	},
	{
		.id = BDB_MIPI_CONFIG,
		.name = "MIPI configuration block",
		.dump = dump_mipi_config,
	},
	{
		.id = BDB_MIPI_SEQUENCE,
		.name = "MIPI sequence block",
		.dump = dump_mipi_sequence,
	},
};

static void hex_dump(const void *data, uint32_t size)
{
	int i;
	const uint8_t *p = data;

	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			printf("\t%04x: ", i);
		printf("%02x", p[i]);
		if (i % 16 == 15) {
			if (i + 1 < size)
				printf("\n");
		} else if (i % 8 == 7) {
			printf("  ");
		} else {
			printf(" ");
		}
	}
	printf("\n\n");
}

static void hex_dump_block(const struct bdb_block *block)
{
	hex_dump(block->data, block->size);
}

static bool dump_section(struct context *context, int section_id)
{
	struct dumper *dumper = NULL;
	struct bdb_block *block;
	int i;

	block = find_section(context, section_id);
	if (!block)
		return false;

	for (i = 0; i < ARRAY_SIZE(dumpers); i++) {
		if (block->id == dumpers[i].id) {
			dumper = &dumpers[i];
			break;
		}
	}

	if (dumper && dumper->name)
		printf("BDB block %d - %s:\n", block->id, dumper->name);
	else
		printf("BDB block %d - Unknown, no decoding available:\n",
		       block->id);

	if (context->hexdump)
		hex_dump_block(block);
	if (dumper && dumper->dump)
		dumper->dump(context, block);
	printf("\n");

	free(block);

	return true;
}

/* print a description of the VBT of the form <bdb-version>-<vbt-signature> */
static void print_description(struct context *context)
{
	const struct vbt_header *vbt = context->vbt;
	const struct bdb_header *bdb = context->bdb;
	char *desc = strndup((char *)vbt->signature, sizeof(vbt->signature));
	char *p;

	for (p = desc + strlen(desc) - 1; p >= desc && isspace(*p); p--)
		*p = '\0';

	for (p = desc; *p; p++) {
		if (!isalnum(*p))
			*p = '-';
		else
			*p = tolower(*p);
	}

	p = desc;
	if (strncmp(p, "-vbt-", 5) == 0)
		p += 5;

	printf("%d-%s\n", bdb->version, p);

	free (desc);
}

static void dump_headers(struct context *context)
{
	const struct vbt_header *vbt = context->vbt;
	const struct bdb_header *bdb = context->bdb;
	int i, j = 0;

	printf("VBT header:\n");
	if (context->hexdump)
		hex_dump(vbt, vbt->header_size);

	printf("\tVBT signature:\t\t\"%.*s\"\n",
	       (int)sizeof(vbt->signature), vbt->signature);
	printf("\tVBT version:\t\t0x%04x (%d.%d)\n", vbt->version,
	       vbt->version / 100, vbt->version % 100);
	printf("\tVBT header size:\t0x%04x (%u)\n",
	       vbt->header_size, vbt->header_size);
	printf("\tVBT size:\t\t0x%04x (%u)\n", vbt->vbt_size, vbt->vbt_size);
	printf("\tVBT checksum:\t\t0x%02x\n", vbt->vbt_checksum);
	printf("\tBDB offset:\t\t0x%08x (%u)\n", vbt->bdb_offset, vbt->bdb_offset);

	printf("\n");

	printf("BDB header:\n");
	if (context->hexdump)
		hex_dump(bdb, bdb->header_size);

	printf("\tBDB signature:\t\t\"%.*s\"\n",
	       (int)sizeof(bdb->signature), bdb->signature);
	printf("\tBDB version:\t\t%d\n", bdb->version);
	printf("\tBDB header size:\t0x%04x (%u)\n",
	       bdb->header_size, bdb->header_size);
	printf("\tBDB size:\t\t0x%04x (%u)\n", bdb->bdb_size, bdb->bdb_size);
	printf("\n");

	printf("BDB blocks present:");
	for (i = 0; i < 256; i++) {
		struct bdb_block *block;

		block = find_section(context, i);
		if (!block)
			continue;

		if (j++ % 16)
			printf(" %3d", i);
		else
			printf("\n\t%3d", i);

		free(block);
	}
	printf("\n\n");
}

enum opt {
	OPT_UNKNOWN = '?',
	OPT_END = -1,
	OPT_FILE,
	OPT_DEVID,
	OPT_PANEL_TYPE,
	OPT_ALL_PANELS,
	OPT_HEXDUMP,
	OPT_BLOCK,
	OPT_USAGE,
	OPT_HEADER,
	OPT_DESCRIBE,
};

static void usage(const char *toolname)
{
	fprintf(stderr, "usage: %s", toolname);
	fprintf(stderr, " --file=<rom_file>"
			" [--devid=<device_id>]"
			" [--panel-type=<panel_type>]"
			" [--all-panels]"
			" [--hexdump]"
			" [--block=<block_no>]"
			" [--header]"
			" [--describe]"
			" [--help]\n");
}

int main(int argc, char **argv)
{
	uint8_t *VBIOS;
	int index;
	enum opt opt;
	int fd;
	struct vbt_header *vbt = NULL;
	int vbt_off, bdb_off, i;
	const char *filename = NULL;
	const char *toolname = argv[0];
	struct stat finfo;
	int size;
	struct context context = {
		.panel_type = -1,
	};
	char *endp;
	int block_number = -1;
	bool header_only = false, describe = false;

	static struct option options[] = {
		{ "file",	required_argument,	NULL,	OPT_FILE },
		{ "devid",	required_argument,	NULL,	OPT_DEVID },
		{ "panel-type",	required_argument,	NULL,	OPT_PANEL_TYPE },
		{ "all-panels",	no_argument,		NULL,	OPT_ALL_PANELS },
		{ "hexdump",	no_argument,		NULL,	OPT_HEXDUMP },
		{ "block",	required_argument,	NULL,	OPT_BLOCK },
		{ "header",	no_argument,		NULL,	OPT_HEADER },
		{ "describe",	no_argument,		NULL,	OPT_DESCRIBE },
		{ "help",	no_argument,		NULL,	OPT_USAGE },
		{ 0 }
	};

	for (opt = 0; opt != OPT_END; ) {
		opt = getopt_long(argc, argv, "", options, &index);

		switch (opt) {
		case OPT_FILE:
			filename = optarg;
			break;
		case OPT_DEVID:
			context.devid = strtoul(optarg, &endp, 16);
			if (!context.devid || *endp) {
				fprintf(stderr, "invalid devid '%s'\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case OPT_PANEL_TYPE:
			context.panel_type = strtoul(optarg, &endp, 0);
			if (*endp || context.panel_type > 15) {
				fprintf(stderr, "invalid panel type '%s'\n",
					optarg);
				return EXIT_FAILURE;
			}
			break;
		case OPT_ALL_PANELS:
			context.dump_all_panel_types = true;
			break;
		case OPT_HEXDUMP:
			context.hexdump = true;
			break;
		case OPT_BLOCK:
			block_number = strtoul(optarg, &endp, 0);
			if (*endp) {
				fprintf(stderr, "invalid block number '%s'\n",
					optarg);
				return EXIT_FAILURE;
			}
			break;
		case OPT_HEADER:
			header_only = true;
			break;
		case OPT_DESCRIBE:
			describe = true;
			break;
		case OPT_END:
			break;
		case OPT_USAGE: /* fall-through */
		case OPT_UNKNOWN:
			usage(toolname);
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (!filename) {
		if (argc == 1) {
			/* for backwards compatibility */
			filename = argv[0];
		} else {
			usage(toolname);
			return EXIT_FAILURE;
		}
	}

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Couldn't open \"%s\": %s\n",
			filename, strerror(errno));
		return EXIT_FAILURE;
	}

	if (stat(filename, &finfo)) {
		fprintf(stderr, "Failed to stat \"%s\": %s\n",
			filename, strerror(errno));
		return EXIT_FAILURE;
	}
	size = finfo.st_size;

	if (size == 0) {
		int len = 0, ret;
		size = 8192;
		VBIOS = malloc (size);
		while ((ret = read(fd, VBIOS + len, size - len))) {
			if (ret < 0) {
				fprintf(stderr, "Failed to read \"%s\": %s\n",
					filename, strerror(errno));
				return EXIT_FAILURE;
			}

			len += ret;
			if (len == size) {
				size *= 2;
				VBIOS = realloc(VBIOS, size);
			}
		}
	} else {
		VBIOS = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		if (VBIOS == MAP_FAILED) {
			fprintf(stderr, "Failed to map \"%s\": %s\n",
				filename, strerror(errno));
			return EXIT_FAILURE;
		}
	}

	/* Scour memory looking for the VBT signature */
	for (i = 0; i + 4 < size; i++) {
		if (!memcmp(VBIOS + i, "$VBT", 4)) {
			vbt_off = i;
			vbt = (struct vbt_header *)(VBIOS + i);
			break;
		}
	}

	if (!vbt) {
		fprintf(stderr, "VBT signature missing\n");
		return EXIT_FAILURE;
	}

	bdb_off = vbt_off + vbt->bdb_offset;
	if (bdb_off >= size - sizeof(struct bdb_header)) {
		fprintf(stderr, "Invalid VBT found, BDB points beyond end of data block\n");
		return EXIT_FAILURE;
	}

	context.vbt = vbt;
	context.bdb = (const struct bdb_header *)(VBIOS + bdb_off);
	context.size = size;

	if (!context.devid) {
		const char *devid_string = getenv("DEVICE");
		if (devid_string)
			context.devid = strtoul(devid_string, NULL, 16);
	}
	if (!context.devid)
		context.devid = get_device_id(VBIOS, size);
	if (!context.devid)
		fprintf(stderr, "Warning: could not find PCI device ID!\n");

	if (context.panel_type == -1)
		context.panel_type = get_panel_type(&context);
	if (context.panel_type == -1) {
		fprintf(stderr, "Warning: panel type not set, using 0\n");
		context.panel_type = 0;
	}

	if (describe) {
		print_description(&context);
	} else if (header_only) {
		dump_headers(&context);
	} else if (block_number != -1) {
		/* dump specific section only */
		if (!dump_section(&context, block_number)) {
			fprintf(stderr, "Block %d not found\n", block_number);
			return EXIT_FAILURE;
		}
	} else {
		dump_headers(&context);

		/* dump all sections  */
		for (i = 0; i < 256; i++)
			dump_section(&context, i);
	}

	return 0;
}
