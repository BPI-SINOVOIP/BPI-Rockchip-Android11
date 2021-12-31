/*
 * SPDX-License-Identifier:     GPL-2.0+
 *
 * (C) Copyright 2020 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <adc.h>
#include <bananapi-common.h>

static unsigned int get_hw_revision(void)
{
	int ret, hw_rev;
	uint32_t value;

        ret = adc_channel_single_shot("saradc", HWID_ADC_CHANNEL, &value);
	if (ret) {
		printf("failed to read hwid adc, ret=%d\n", ret);
		return ret;
	}

        if (IS_RANGE(value, 990, 1030))
		hw_rev = BOARD_REVISION(2021, 07, 26);

        printf("ADC=%d, hw_rev=0x%x\n", value, hw_rev);

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
