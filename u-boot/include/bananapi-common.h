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

#if defined(CONFIG_TARGET_BANANAPI_R2PRO)
int board_is_bananapi_r2pro(void);
#endif

#endif
