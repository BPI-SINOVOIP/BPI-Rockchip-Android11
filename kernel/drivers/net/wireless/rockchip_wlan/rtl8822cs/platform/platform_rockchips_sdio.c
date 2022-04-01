/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifdef CONFIG_PLATFORM_OPS
#include <linux/delay.h>		/* msleep() */
#include <linux/rfkill-wlan.h>		/* rockchip_wifi_xxx() */
#include <drv_types.h>

#ifdef CONFIG_GPIO_WAKEUP
extern unsigned int oob_irq;
#endif

/*
 * Return:
 *	0:	power on successfully
 *	others: power on failed
 */
int platform_wifi_power_on(void)
{
	int ret = 0;


	RTW_PRINT("\n");
	RTW_PRINT("=======================================================\n");
	RTW_PRINT("==== Launching Wi-Fi driver! (Powered by Rockchip) ====\n");
	RTW_PRINT("=======================================================\n");
	RTW_PRINT("Realtek SDIO WiFi driver (Powered by Rockchip) init.\n");
	rockchip_wifi_power(1);
	msleep(100);
	rockchip_wifi_set_carddetect(1);

#ifdef CONFIG_GPIO_WAKEUP
	oob_irq = rockchip_wifi_get_oob_irq();
	RTW_PRINT("get oob_irq=%d\n", oob_irq);
#endif

	return ret;
}

void platform_wifi_power_off(void)
{
	RTW_PRINT("\n");
	RTW_PRINT("=======================================================\n");
	RTW_PRINT("=== Dislaunching Wi-Fi driver! (Powered by Rockchip) ==\n");
	RTW_PRINT("=======================================================\n");
	RTW_PRINT("Realtek SDIO WiFi driver (Powered by Rockchip) init.\n");
	rockchip_wifi_set_carddetect(0);
	msleep(100);
	rockchip_wifi_power(0);
}
#endif /* !CONFIG_PLATFORM_OPS */
