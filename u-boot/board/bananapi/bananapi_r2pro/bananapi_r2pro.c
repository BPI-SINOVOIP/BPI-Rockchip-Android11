/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * (C) Copyright 2020 Rockchip Electronics Co., Ltd
 */

#include <stdlib.h>
#include <dt_table.h>
#include <common.h>
#include <dwc3-uboot.h>
#include <usb.h>
#include <mapmem.h>

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

int board_boot_env(void)
{
	char cmd[64];
	char *addr_r;

	addr_r = env_get("scriptaddr");

	sprintf(cmd, "fatload mmc 1 %s rk_env.ini", addr_r);
	run_command(cmd, 0);

	sprintf(cmd, "source %s", addr_r);
	run_command(cmd, 0);

	return 0;
}

int board_select_fdt_index(ulong dt_table_hdr)
{
	const struct dt_table_header *hdr;
	u32 entry_count;
	char *idx = NULL;
	int index;

	hdr = map_sysmem(dt_table_hdr, sizeof(*hdr));
	entry_count = fdt32_to_cpu(hdr->dt_entry_count);

	printf("find %d dtbos\n", entry_count);

	board_boot_env();

	idx = env_get("androidboot.dtbo_idx");
	if (!idx) {
		printf("No androidboot.dtbo_idx configured\n");
		printf("And no dtbos will be applied\n");
		return 0;
	}

	index = atoi(idx);
	printf("dtbos to be applied: %d\n", index);

	return index;
}

char *get_board_variant(void)
{
	int board_rev = get_display_id();

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
