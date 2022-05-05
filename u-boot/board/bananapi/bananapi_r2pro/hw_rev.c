/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * (C) Copyright 2020 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <adc.h>
#include <bananapi-common.h>

uint32_t get_adc_value(uint8_t channel)
{
	int ret;
	uint32_t value;

	ret = adc_channel_single_shot("saradc", channel, &value);
	if (ret) {
		printf("failed to read adc channel %d, ret=%d\n", channel, ret);
		return ret;
	}

	return value;
}

int get_display_id(void)
{
	uint32_t dsi0_value;
	uint32_t dsi1_value;
	uint32_t edp_value;
	uint32_t lvds_value;

	dsi0_value = get_adc_value(DSI0_ADC_CHANNEL);
	dsi1_value = get_adc_value(DSI1_ADC_CHANNEL);
	edp_value = get_adc_value(EDP_ADC_CHANNEL);
	lvds_value = get_adc_value(LVDS_ADC_CHANNEL);

	printf("dsi0_adc=%d\n", dsi0_value);
	printf("dsi1_adc=%d\n", dsi1_value);
	printf("edp_adc=%d\n", edp_value);
	printf("lvds_adc=%d\n", lvds_value);

    /* support dual dsi connect same panel at the same time*/
	if (IS_RANGE(dsi0_value, 0, 50) || IS_RANGE(dsi1_value, 0, 50)) {
		printf("800x1280 dsi connected\n");
		return BANANAPI_R2PRO_LCD0;
	}

	if (IS_RANGE(dsi0_value, 150, 200) || IS_RANGE(dsi1_value, 150, 200)) {
		printf("1200x1920 dsi connected\n");
		return BANANAPI_R2PRO_LCD1;
	}

	if (IS_RANGE(edp_value, 0, 50)) {
		printf("edp connected\n");
		return BANANAPI_R2PRO_LCD2;
	}

	if (IS_RANGE(lvds_value, 330, 380)) {
		printf("lvds connected\n");
		return BANANAPI_R2PRO_LCD3;
	}

	return BANANAPI_R2PRO_HDMI;
}

static unsigned int get_hw_revision(void)
{
	int hw_rev;
	uint32_t hw_value;

	hw_value = get_adc_value(HWID_ADC_CHANNEL);
	if (IS_RANGE(hw_value, 990, 1030))
		hw_rev = BOARD_REVISION(2021, 07, 26);

	printf("hwid_adc=%d, hw_rev=0x%x\n", hw_value, hw_rev);

	return hw_rev;
}

int board_revision(void)
{
	return get_hw_revision();
}

int board_is_bananapi_r2pro(void)
{
        return (board_revision() == 0x20210726);
}
