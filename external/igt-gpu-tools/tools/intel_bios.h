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

#ifndef _INTEL_BIOS_H_
#define _INTEL_BIOS_H_

#include <stdint.h>

#define DEVICE_HANDLE_CRT	0x01
#define DEVICE_HANDLE_EFP1	0x04
#define DEVICE_HANDLE_EFP2	0x40
#define DEVICE_HANDLE_EFP3	0x20
#define DEVICE_HANDLE_EFP4	0x10
#define DEVICE_HANDLE_LPF1	0x08
#define DEVICE_HANDLE_LFP2	0x80

#define DEVICE_TYPE_DP_DVI		0x68d6
#define DEVICE_TYPE_DVI			0x68d2
#define DEVICE_TYPE_MIPI		0x7cc2

struct bdb_legacy_child_devices {
	uint8_t child_dev_size;
	uint8_t devices[0]; /* presumably 7 * 33 */
} __attribute__ ((packed));

#define BDB_DRIVER_NO_LVDS	0
#define BDB_DRIVER_INT_LVDS	1
#define BDB_DRIVER_SDVO_LVDS	2
#define BDB_DRIVER_EDP		3

struct edp_power_seq {
	uint16_t t3;
	uint16_t t7;
	uint16_t t9;
	uint16_t t10;
	uint16_t t12;
} __attribute__ ((packed));

/* Block 52 contains MiPi Panel info
 * 6 such enteries will there. Index into correct
 * entery is based on the panel_index in #40 LFP
 */
#define MAX_MIPI_CONFIGURATIONS        6
struct mipi_config {
	uint16_t panel_id;

	/* General Params */
	uint32_t dithering:1;
	uint32_t rsvd1:1;
	uint32_t panel_type:1;
	uint32_t panel_arch_type:2;
	uint32_t cmd_mode:1;
	uint32_t vtm:2;
	uint32_t cabc:1;
	uint32_t pwm_blc:1;

	/* Bit 13:10
	 * 000 - Reserved, 001 - RGB565, 002 - RGB666,
	 * 011 - RGB666Loosely packed, 100 - RGB888,
	 * others - rsvd
	 */
	uint32_t videomode_color_format:4;

	/* Bit 15:14
	 * 0 - No rotation, 1 - 90 degree
	 * 2 - 180 degree, 3 - 270 degree
	 */
	uint32_t rotation:2;
	uint32_t bta:1;
	uint32_t rsvd2:15;

	/* 2 byte Port Description */
	uint16_t dual_link:2;
	uint16_t lane_cnt:2;
	uint16_t pixel_overlap:3;
	uint16_t rsvd3:9;

	/* 2 byte DSI COntroller params */
	/* 0 - Using DSI PHY, 1 - TE usage */
	uint16_t dsi_usage:1;
	uint16_t rsvd4:15;

	uint8_t rsvd5[5];
	uint32_t dsi_ddr_clk;
	uint32_t bridge_ref_clk;

	uint8_t byte_clk_sel:2;
	uint8_t rsvd6:6;

	/* DPHY Flags */
	uint16_t dphy_param_valid:1;
	uint16_t eot_disabled:1;
	uint16_t clk_stop:1;
	uint16_t rsvd7:13;

	uint32_t hs_tx_timeout;
	uint32_t lp_rx_timeout;
	uint32_t turn_around_timeout;
	uint32_t device_reset_timer;
	uint32_t master_init_timer;
	uint32_t dbi_bw_timer;
	uint32_t lp_byte_clk_val;

	/*  4 byte Dphy Params */
	uint32_t prepare_cnt:6;
	uint32_t rsvd8:2;
	uint32_t clk_zero_cnt:8;
	uint32_t trail_cnt:5;
	uint32_t rsvd9:3;
	uint32_t exit_zero_cnt:6;
	uint32_t rsvd10:2;

	uint32_t clk_lane_switch_cnt;
	uint32_t hl_switch_cnt;

	uint32_t rsvd11[6];

	/* timings based on dphy spec */
	uint8_t tclk_miss;
	uint8_t tclk_post;
	uint8_t rsvd12;
	uint8_t tclk_pre;
	uint8_t tclk_prepare;
	uint8_t tclk_settle;
	uint8_t tclk_term_enable;
	uint8_t tclk_trail;
	uint16_t tclk_prepare_clkzero;
	uint8_t rsvd13;
	uint8_t td_term_enable;
	uint8_t teot;
	uint8_t ths_exit;
	uint8_t ths_prepare;
	uint16_t ths_prepare_hszero;
	uint8_t rsvd14;
	uint8_t ths_settle;
	uint8_t ths_skip;
	uint8_t ths_trail;
	uint8_t tinit;
	uint8_t tlpx;
	uint8_t rsvd15[3];

	/* GPIOs */
	uint8_t panel_enable;
	uint8_t bl_enable;
	uint8_t pwm_enable;
	uint8_t reset_r_n;
	uint8_t pwr_down_r;
	uint8_t stdby_r_n;

} __attribute__ ((packed));

/* Block 52 contains MiPi configuration block
 * 6 * bdb_mipi_config, followed by 6 pps data
 * block below
 */
struct mipi_pps_data {
	uint16_t panel_on_delay;
	uint16_t bl_enable_delay;
	uint16_t bl_disable_delay;
	uint16_t panel_off_delay;
	uint16_t panel_power_cycle_delay;
} __attribute__ ((packed));

/* MIPI Sequence Block definitions */
enum mipi_seq {
	MIPI_SEQ_END = 0,
	MIPI_SEQ_ASSERT_RESET,
	MIPI_SEQ_INIT_OTP,
	MIPI_SEQ_DISPLAY_ON,
	MIPI_SEQ_DISPLAY_OFF,
	MIPI_SEQ_DEASSERT_RESET,
	MIPI_SEQ_BACKLIGHT_ON,		/* sequence block v2+ */
	MIPI_SEQ_BACKLIGHT_OFF,		/* sequence block v2+ */
	MIPI_SEQ_TEAR_ON,		/* sequence block v2+ */
	MIPI_SEQ_TEAR_OFF,		/* sequence block v3+ */
	MIPI_SEQ_POWER_ON,		/* sequence block v3+ */
	MIPI_SEQ_POWER_OFF,		/* sequence block v3+ */
	MIPI_SEQ_MAX
};

enum mipi_seq_element {
	MIPI_SEQ_ELEM_END = 0,
	MIPI_SEQ_ELEM_SEND_PKT,
	MIPI_SEQ_ELEM_DELAY,
	MIPI_SEQ_ELEM_GPIO,
	MIPI_SEQ_ELEM_I2C,		/* sequence block v2+ */
	MIPI_SEQ_ELEM_SPI,		/* sequence block v3+ */
	MIPI_SEQ_ELEM_PMIC,		/* sequence block v3+ */
	MIPI_SEQ_ELEM_MAX
};

#endif /* _INTEL_BIOS_H_ */
