/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * (C) Copyright 2020 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <dwc3-uboot.h>
#include <usb.h>
#include <bananapi-common.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_USB_DWC3
static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_HIGH,
	.base = 0xfcc00000,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 0,
	.dis_u2_susphy_quirk = 1,
	.usb2_phyif_utmi_width = 16,
};

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(0);
	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	return dwc3_uboot_init(&dwc3_device_data);
}
#endif

#define BANANAPI_R2PRO_HDMI	0
#define BANANAPI_R2PRO_LCD0	1
#define BANANAPI_R2PRO_LCD1	2
char *get_board_variant(void)
{
	int board_rev = 1;

	switch(board_rev) {
		case BANANAPI_R2PRO_HDMI:
			return "r2pro-hdmi";
		case BANANAPI_R2PRO_LCD0:
			return "r2pro-lcd0";
		case BANANAPI_R2PRO_LCD1:
			return "r2pro-lcd1";
		default:
			return "r2pro";
	}
}

void set_dtb_name(void)
{
	char dtb_name[64] = {0, };

	snprintf(dtb_name, ARRAY_SIZE(dtb_name),
			"rk3568-bananapi-%s.dtb", get_board_variant());

	printf("dtb variant: %s\n", dtb_name);

	env_set("dtb_name", dtb_name);
}

int rk_board_late_init(void)
{
	if (board_is_bananapi_r2pro()) {
		printf("board: Bananapi R2Pro\n");
		env_set("board", "bananapi_r2pro");
	}

	return 0;
}
