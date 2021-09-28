/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_LAYOUT_H
#define __CROS_EC_FLASH_LAYOUT_H

/*
 * The flash memory is implemented in two halves. The SoC bootrom will look for
 * a first-stage bootloader (aka "RO firmware") at the beginning of each of the
 * two halves and prefer the newer one if both are valid. The chosen bootloader
 * also looks in each half of the flash for a valid application image (("RW
 * firmware"), so we have two possible RW images as well. The RO and RW images
 * are not tightly coupled, so either RO image can choose to boot either RW
 * image. RO images are provided by the SoC team, and can be updated separately
 * from the RW images.
 */

#define CITADEL_FLASH_BASE     0x40000
#define CITADEL_FLASH_SIZE     (512 * 1024)
#define CITADEL_FLASH_HALF     (CITADEL_FLASH_SIZE >> 1)
#define CITADEL_RO_SIZE        0x4000
#define CITADEL_RO_A_MEM_OFF   0
#define CITADEL_RO_B_MEM_OFF   CITADEL_FLASH_HALF
#define CITADEL_RW_A_MEM_OFF   CITADEL_RO_SIZE
#define CITADEL_RW_B_MEM_OFF   (CITADEL_FLASH_HALF + CITADEL_RW_A_MEM_OFF)

#define DAUNTLESS_FLASH_BASE   0x80000
#define DAUNTLESS_FLASH_SIZE   (1024 * 1024)
#define DAUNTLESS_FLASH_HALF   (DAUNTLESS_FLASH_SIZE >> 1)
#define DAUNTLESS_RO_SIZE      0x4000
#define DAUNTLESS_RO_A_MEM_OFF 0
#define DAUNTLESS_RO_B_MEM_OFF DAUNTLESS_FLASH_HALF
#define DAUNTLESS_RW_A_MEM_OFF DAUNTLESS_RO_SIZE
#define DAUNTLESS_RW_B_MEM_OFF (DAUNTLESS_FLASH_HALF + DAUNTLESS_RW_A_MEM_OFF)

#endif	/* __CROS_EC_FLASH_LAYOUT_H */
