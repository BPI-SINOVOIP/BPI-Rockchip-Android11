/*
 * Copyright 2019 Rockchip Electronics Co., Ltd
 * Dayao Ji <jdy@rock-chips.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _UPGRADEAPI_H_
#define _UPGRADEAPI_H_

bool do_rk_partition_upgrade(char *szFw,void *pCallback=NULL,void *pProgressCallback=NULL,char nBoot=0,char *szBootDev=NULL);
bool do_rk_firmware_upgrade(char *szFw,void *pCallback=NULL,void *pProgressCallback=NULL,char *szBootDev=NULL);
bool do_rk_backup_recovery(void *pCallback=NULL,void *pProgressCallback=NULL);
bool do_rk_sparse_update(const char *partitionName, const char *src_path);
bool do_rk_gpt_update(char *szFw,void *pCallback,void *pProgressCallback,char *szBootDev);
#endif

