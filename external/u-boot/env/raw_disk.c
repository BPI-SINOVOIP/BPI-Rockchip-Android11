// SPDX-License-Identifier: GPL-2.0+
/*
 * (c) Copyright 2019 by Google, Inc
 *
 * Author:
 *   David Anderson <dvander@google.com>
 */

#include <common.h>

#include <command.h>
#include <dm/device.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <search.h>
#include <errno.h>
#include <mmc.h>
#include <virtio_types.h>
#include <virtio.h>

#ifdef CONFIG_CMD_SAVEENV
static int env_raw_disk_save(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);
	struct blk_desc *dev_desc = NULL;
	disk_partition_t info;
	uint blk_cnt, n;
	int part, err;

	err = env_export(env_new);
	if (err)
		return err;

	part = blk_get_device_part_str(CONFIG_ENV_RAW_DISK_INTERFACE,
					CONFIG_ENV_RAW_DISK_DEVICE_AND_PART,
					&dev_desc, &info, 1);
	if (part < 0)
		return 1;

	printf("Writing to disk...");

	blk_cnt = ALIGN(CONFIG_ENV_SIZE, info.blksz) / info.blksz;
	n = blk_dwrite(dev_desc, info.start, blk_cnt, (u_char *)env_new);
	if (n != blk_cnt) {
		puts("failed\n");
		return 1;
	}

	puts("done\n");
	return 0;
}
#endif /* CONFIG_CMD_SAVEENV */

static int env_raw_disk_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);
	struct blk_desc *dev_desc = NULL;
	disk_partition_t info;
	uint blk_cnt, n;
	int part;

#ifdef CONFIG_MMC
	if (!strcmp(CONFIG_ENV_RAW_DISK_INTERFACE, "mmc"))
		mmc_initialize(NULL);
#endif
#ifdef CONFIG_VIRTIO
	if (!strcmp(CONFIG_ENV_RAW_DISK_INTERFACE, "virtio"))
		virtio_init();
#endif

	part = blk_get_device_part_str(CONFIG_ENV_RAW_DISK_INTERFACE,
					CONFIG_ENV_RAW_DISK_DEVICE_AND_PART,
					&dev_desc, &info, 1);
	if (part < 0) {
		set_default_env(NULL, 0);
		return -EINVAL;
	}

	blk_cnt = ALIGN(CONFIG_ENV_SIZE, info.blksz) / info.blksz;
	n = blk_dread(dev_desc, info.start, blk_cnt, buf);
	if (n != blk_cnt) {
		set_default_env(NULL, 0);
		return -EIO;
	}

	return env_import(buf, 1);
}

U_BOOT_ENV_LOCATION(raw_disk) = {
	.location	= ENVL_RAW_DISK,
	ENV_NAME("Raw Disk")
	.load		= env_raw_disk_load,
#ifdef CONFIG_CMD_SAVEENV
	.save		= env_save_ptr(env_raw_disk_save),
#endif
};

