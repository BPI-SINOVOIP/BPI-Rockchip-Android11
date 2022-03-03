#ifndef __BANANAPI_COMMON_H
#define __BANANAPI_COMMON_H

/*
 * Board revision in the form of YYYYMMDD as hexadecimal
 * ex) BOARD_REVISION(2018, 07, 16)  -> 0x20180716
 */

#define BOARD_REVISION(y,m,d)   (((0x##y & 0xffff) << 16) \
                | ((0x##m & 0xff) << 8) | ((0x##d & 0xff) << 0))

#define IS_RANGE(x, min, max)   ((x) > (min) && (x) < (max))

#define HWID_ADC_CHANNEL        1
#define DSI0_ADC_CHANNEL		5
#define DSI1_ADC_CHANNEL		2
#define EDP_ADC_CHANNEL			4
#define LVDS_ADC_CHANNEL		5

#define BANANAPI_R2PRO_HDMI	0	/*hdmi only*/
#define BANANAPI_R2PRO_LCD0	1	/*800x1280 dsi*/
#define BANANAPI_R2PRO_LCD1	2	/*1200x1920 dsi*/

#if defined(CONFIG_TARGET_BANANAPI_R2PRO)
int board_is_bananapi_r2pro(void);
int get_display_id(void);
#endif

#endif
