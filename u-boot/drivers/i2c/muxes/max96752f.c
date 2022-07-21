// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Rockchip Electronics Co., Ltd
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <max96752f.h>

static int max96752f_select(struct udevice *mux, struct udevice *bus,
			    uint channel)
{
	return 0;
}

static int max96752f_deselect(struct udevice *mux, struct udevice *bus,
			      uint channel)
{
	return 0;
}

static const struct i2c_mux_ops max96752f_ops = {
	.select = max96752f_select,
	.deselect = max96752f_deselect,
};

static uint addr_list[] = {
	0x48, 0x4a, 0x4c, 0x68, 0x6a, 0x6c, 0x28, 0x2a
};

static void max96752f_check_addr(struct udevice *dev)
{
	struct dm_i2c_chip *chip = dev_get_parent_platdata(dev);
	uint addr = chip->chip_addr;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(addr_list); i++) {
		chip->chip_addr = addr_list[i];

		ret = dm_i2c_reg_read(dev, 0x000d);
		if (ret < 0)
			continue;

		if (ret == 0x82) {
			dm_i2c_reg_write(dev, 0x0000, addr << 1);
			break;
		}
	}

	chip->chip_addr = addr;
}

static int max96752f_probe(struct udevice *dev)
{
	u32 stream_id = dev_read_u32_default(dev->parent, "reg", 0);
	int ret;

	ret = i2c_set_chip_offset_len(dev, 2);
	if (ret)
		return ret;

	max96752f_check_addr(dev);

	ret = dm_i2c_reg_read(dev, 0x000d);
	if (ret < 0) {
		dev_err(dev, "%s: failed to get dev id: %d\n", __func__, ret);
		return ret;
	}

	dm_i2c_reg_clrset(dev, 0x0050, STR_SEL, FIELD_PREP(STR_SEL, stream_id));
	dm_i2c_reg_clrset(dev, 0x0073, TX_SRC_ID, FIELD_PREP(TX_SRC_ID, stream_id));

	return 0;
}

static const struct udevice_id max96752f_of_match[] = {
	{ .compatible = "maxim,max96752f" },
	{}
};

U_BOOT_DRIVER(max96752f) = {
	.name = "max96752f",
	.id = UCLASS_I2C_MUX,
	.of_match = max96752f_of_match,
	.bind = dm_scan_fdt_dev,
	.probe = max96752f_probe,
	.ops = &max96752f_ops,
};
