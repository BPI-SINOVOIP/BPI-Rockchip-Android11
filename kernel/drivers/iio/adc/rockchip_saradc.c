/*
 * Rockchip Successive Approximation Register (SAR) A/D Converter
 * Copyright (C) 2014 ROCKCHIP, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>

#define SARADC_DATA			0x00

#define SARADC_STAS			0x04
#define SARADC_STAS_BUSY		BIT(0)

#define SARADC_CTRL			0x08
#define SARADC_CTRL_IRQ_STATUS		BIT(6)
#define SARADC_CTRL_IRQ_ENABLE		BIT(5)
#define SARADC_CTRL_POWER_CTRL		BIT(3)
#define SARADC_CTRL_CHN_MASK		0x7

#define SARADC_DLY_PU_SOC		0x0c
#define SARADC_DLY_PU_SOC_MASK		0x3f

#define SARADC_TIMEOUT			msecs_to_jiffies(100)

struct rockchip_saradc_data {
	int				num_bits;
	const struct iio_chan_spec	*channels;
	int				num_channels;
	unsigned long			clk_rate;
};

struct rockchip_saradc {
	void __iomem		*regs;
	struct clk		*pclk;
	struct clk		*clk;
	struct completion	completion;
	struct regulator	*vref;
	int			uv_vref;
	struct reset_control	*reset;
	const struct rockchip_saradc_data *data;
	u16			last_val;
	bool			suspended;
#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
	struct timer_list	timer;
	bool			test;
	u32			chn;
	spinlock_t		lock;
#endif
};

static int rockchip_saradc_read_raw(struct iio_dev *indio_dev,
				    struct iio_chan_spec const *chan,
				    int *val, int *val2, long mask)
{
	struct rockchip_saradc *info = iio_priv(indio_dev);

#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
	if (info->test)
		return 0;
#endif
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&indio_dev->mlock);

		if (info->suspended) {
			mutex_unlock(&indio_dev->mlock);
			return -EBUSY;
		}

		reinit_completion(&info->completion);

		/* 8 clock periods as delay between power up and start cmd */
		writel_relaxed(8, info->regs + SARADC_DLY_PU_SOC);

		/* Select the channel to be used and trigger conversion */
		writel(SARADC_CTRL_POWER_CTRL
				| (chan->channel & SARADC_CTRL_CHN_MASK)
				| SARADC_CTRL_IRQ_ENABLE,
		       info->regs + SARADC_CTRL);

		if (!wait_for_completion_timeout(&info->completion,
						 SARADC_TIMEOUT)) {
			writel_relaxed(0, info->regs + SARADC_CTRL);
			mutex_unlock(&indio_dev->mlock);
			return -ETIMEDOUT;
		}

		*val = info->last_val;
		mutex_unlock(&indio_dev->mlock);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		/* It is a dummy regulator */
		if (info->uv_vref < 0)
			return info->uv_vref;

		*val = info->uv_vref / 1000;
		*val2 = info->data->num_bits;
		return IIO_VAL_FRACTIONAL_LOG2;
	default:
		return -EINVAL;
	}
}

static irqreturn_t rockchip_saradc_isr(int irq, void *dev_id)
{
	struct rockchip_saradc *info = dev_id;
#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
	unsigned long flags;
#endif

	/* Read value */
	info->last_val = readl_relaxed(info->regs + SARADC_DATA);
	info->last_val &= GENMASK(info->data->num_bits - 1, 0);

	/* Clear irq & power down adc */
	writel_relaxed(0, info->regs + SARADC_CTRL);

	complete(&info->completion);
#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
	spin_lock_irqsave(&info->lock, flags);
	if (info->test) {
		pr_info("chn[%d] val = %d\n", info->chn, info->last_val);
		mod_timer(&info->timer, jiffies + HZ/1000);
	}
	spin_unlock_irqrestore(&info->lock, flags);
#endif
	return IRQ_HANDLED;
}

static const struct iio_info rockchip_saradc_iio_info = {
	.read_raw = rockchip_saradc_read_raw,
};

#define ADC_CHANNEL(_index, _id) {				\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = _index,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),	\
	.datasheet_name = _id,					\
}

static const struct iio_chan_spec rockchip_saradc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
};

static const struct rockchip_saradc_data saradc_data = {
	.num_bits = 10,
	.channels = rockchip_saradc_iio_channels,
	.num_channels = ARRAY_SIZE(rockchip_saradc_iio_channels),
	.clk_rate = 1000000,
};

static const struct iio_chan_spec rockchip_rk3066_tsadc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
};

static const struct rockchip_saradc_data rk3066_tsadc_data = {
	.num_bits = 12,
	.channels = rockchip_rk3066_tsadc_iio_channels,
	.num_channels = ARRAY_SIZE(rockchip_rk3066_tsadc_iio_channels),
	.clk_rate = 50000,
};

static const struct iio_chan_spec rockchip_rk3399_saradc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
	ADC_CHANNEL(3, "adc3"),
	ADC_CHANNEL(4, "adc4"),
	ADC_CHANNEL(5, "adc5"),
};

static const struct rockchip_saradc_data rk3399_saradc_data = {
	.num_bits = 10,
	.channels = rockchip_rk3399_saradc_iio_channels,
	.num_channels = ARRAY_SIZE(rockchip_rk3399_saradc_iio_channels),
	.clk_rate = 1000000,
};

static const struct iio_chan_spec rockchip_rk3568_saradc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
	ADC_CHANNEL(2, "adc2"),
	ADC_CHANNEL(3, "adc3"),
	ADC_CHANNEL(4, "adc4"),
	ADC_CHANNEL(5, "adc5"),
	ADC_CHANNEL(6, "adc6"),
	ADC_CHANNEL(7, "adc7"),
};

static const struct rockchip_saradc_data rk3568_saradc_data = {
	.num_bits = 10,
	.channels = rockchip_rk3568_saradc_iio_channels,
	.num_channels = ARRAY_SIZE(rockchip_rk3568_saradc_iio_channels),
	.clk_rate = 1000000,
};

static const struct of_device_id rockchip_saradc_match[] = {
	{
		.compatible = "rockchip,saradc",
		.data = &saradc_data,
	}, {
		.compatible = "rockchip,rk3066-tsadc",
		.data = &rk3066_tsadc_data,
	}, {
		.compatible = "rockchip,rk3399-saradc",
		.data = &rk3399_saradc_data,
	}, {
		.compatible = "rockchip,rk3568-saradc",
		.data = &rk3568_saradc_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_saradc_match);

/**
 * Reset SARADC Controller.
 */
static void rockchip_saradc_reset_controller(struct reset_control *reset)
{
	reset_control_assert(reset);
	usleep_range(10, 20);
	reset_control_deassert(reset);
}

static void rockchip_saradc_clk_disable(void *data)
{
	struct rockchip_saradc *info = data;

	clk_disable_unprepare(info->clk);
}

static void rockchip_saradc_pclk_disable(void *data)
{
	struct rockchip_saradc *info = data;

	clk_disable_unprepare(info->pclk);
}

static void rockchip_saradc_regulator_disable(void *data)
{
	struct rockchip_saradc *info = data;

	regulator_disable(info->vref);
}

#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
static void rockchip_saradc_timer(struct timer_list *t)
{
	struct rockchip_saradc *info = from_timer(info, t, timer);

	/* 8 clock periods as delay between power up and start cmd */
	writel_relaxed(8, info->regs + SARADC_DLY_PU_SOC);

	/* Select the channel to be used and trigger conversion */
	writel(SARADC_CTRL_POWER_CTRL | (info->chn & SARADC_CTRL_CHN_MASK) |
	       SARADC_CTRL_IRQ_ENABLE, info->regs + SARADC_CTRL);
}

static ssize_t saradc_test_chn_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	u32 val = 0;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rockchip_saradc *info = iio_priv(indio_dev);
	unsigned long flags;

	err = kstrtou32(buf, 10, &val);
	if (err)
		return err;

	spin_lock_irqsave(&info->lock, flags);

	if (val > SARADC_CTRL_CHN_MASK && info->test) {
		info->test = false;
		del_timer_sync(&info->timer);
		spin_unlock_irqrestore(&info->lock, flags);
		return size;
	}

	if (!info->test && val < SARADC_CTRL_CHN_MASK) {
		info->test = true;
		info->chn = val;
		mod_timer(&info->timer, jiffies + HZ/1000);
	}

	spin_unlock_irqrestore(&info->lock, flags);

	return size;
}

static DEVICE_ATTR_WO(saradc_test_chn);

static struct attribute *saradc_attrs[] = {
	&dev_attr_saradc_test_chn.attr,
	NULL
};

static const struct attribute_group rockchip_saradc_attr_group = {
	.attrs = saradc_attrs,
};

static void rockchip_saradc_remove_sysgroup(void *data)
{
	struct platform_device *pdev = data;

	sysfs_remove_group(&pdev->dev.kobj, &rockchip_saradc_attr_group);
}
#endif

static int rockchip_saradc_probe(struct platform_device *pdev)
{
	struct rockchip_saradc *info = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct iio_dev *indio_dev = NULL;
	struct resource	*mem;
	const struct of_device_id *match;
	int ret;
	int irq;

	if (!np)
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}
	info = iio_priv(indio_dev);

	match = of_match_device(rockchip_saradc_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "failed to match device\n");
		return -ENODEV;
	}

	info->data = match->data;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	info->regs = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(info->regs))
		return PTR_ERR(info->regs);

	/*
	 * The reset should be an optional property, as it should work
	 * with old devicetrees as well
	 */
	info->reset = devm_reset_control_get_exclusive(&pdev->dev,
						       "saradc-apb");
	if (IS_ERR(info->reset)) {
		ret = PTR_ERR(info->reset);
		if (ret != -ENOENT)
			return ret;

		dev_dbg(&pdev->dev, "no reset control found\n");
		info->reset = NULL;
	}

	init_completion(&info->completion);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, rockchip_saradc_isr,
			       0, dev_name(&pdev->dev), info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq %d\n", irq);
		return ret;
	}

	info->pclk = devm_clk_get(&pdev->dev, "apb_pclk");
	if (IS_ERR(info->pclk)) {
		dev_err(&pdev->dev, "failed to get pclk\n");
		return PTR_ERR(info->pclk);
	}

	info->clk = devm_clk_get(&pdev->dev, "saradc");
	if (IS_ERR(info->clk)) {
		dev_err(&pdev->dev, "failed to get adc clock\n");
		return PTR_ERR(info->clk);
	}

	info->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(info->vref)) {
		dev_err(&pdev->dev, "failed to get regulator, %ld\n",
			PTR_ERR(info->vref));
		return PTR_ERR(info->vref);
	}

	if (info->reset)
		rockchip_saradc_reset_controller(info->reset);

	/*
	 * Use a default value for the converter clock.
	 * This may become user-configurable in the future.
	 */
	ret = clk_set_rate(info->clk, info->data->clk_rate);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set adc clk rate, %d\n", ret);
		return ret;
	}

	ret = regulator_enable(info->vref);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable vref regulator\n");
		return ret;
	}
	ret = devm_add_action_or_reset(&pdev->dev,
				       rockchip_saradc_regulator_disable, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n",
			ret);
		return ret;
	}

	info->uv_vref = regulator_get_voltage(info->vref);

	ret = clk_prepare_enable(info->pclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable pclk\n");
		return ret;
	}
	ret = devm_add_action_or_reset(&pdev->dev,
				       rockchip_saradc_pclk_disable, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n",
			ret);
		return ret;
	}

	ret = clk_prepare_enable(info->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable converter clock\n");
		return ret;
	}
	ret = devm_add_action_or_reset(&pdev->dev,
				       rockchip_saradc_clk_disable, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n",
			ret);
		return ret;
	}

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->info = &rockchip_saradc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	indio_dev->channels = info->data->channels;
	indio_dev->num_channels = info->data->num_channels;

#ifdef CONFIG_ROCKCHIP_SARADC_TEST_CHN
	spin_lock_init(&info->lock);
	timer_setup(&info->timer, rockchip_saradc_timer, 0);
	ret = sysfs_create_group(&pdev->dev.kobj, &rockchip_saradc_attr_group);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(&pdev->dev,
				       rockchip_saradc_remove_sysgroup, pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n",
			ret);
		return ret;
	}
#endif
	return devm_iio_device_register(&pdev->dev, indio_dev);
}

#ifdef CONFIG_PM_SLEEP
static int rockchip_saradc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rockchip_saradc *info = iio_priv(indio_dev);

	/* Avoid reading saradc when suspending */
	mutex_lock(&indio_dev->mlock);

	clk_disable_unprepare(info->clk);
	clk_disable_unprepare(info->pclk);
	regulator_disable(info->vref);

	info->suspended = true;

	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int rockchip_saradc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct rockchip_saradc *info = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(info->vref);
	if (ret)
		return ret;

	ret = clk_prepare_enable(info->pclk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(info->clk);
	if (ret)
		clk_disable_unprepare(info->pclk);

	info->suspended = false;

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(rockchip_saradc_pm_ops,
			 rockchip_saradc_suspend, rockchip_saradc_resume);

static struct platform_driver rockchip_saradc_driver = {
	.probe		= rockchip_saradc_probe,
	.driver		= {
		.name	= "rockchip-saradc",
		.of_match_table = rockchip_saradc_match,
		.pm	= &rockchip_saradc_pm_ops,
	},
};

#ifdef CONFIG_ROCKCHIP_THUNDER_BOOT
static int __init rockchip_saradc_driver_init(void)
{
	return platform_driver_register(&rockchip_saradc_driver);
}
fs_initcall(rockchip_saradc_driver_init);

static void __exit rockchip_saradc_driver_exit(void)
{
	platform_driver_unregister(&rockchip_saradc_driver);
}
module_exit(rockchip_saradc_driver_exit);
#else
module_platform_driver(rockchip_saradc_driver);
#endif

MODULE_AUTHOR("Heiko Stuebner <heiko@sntech.de>");
MODULE_DESCRIPTION("Rockchip SARADC driver");
MODULE_LICENSE("GPL v2");
