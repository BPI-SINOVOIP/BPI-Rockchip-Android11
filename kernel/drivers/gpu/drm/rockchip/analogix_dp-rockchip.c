/*
 * Rockchip SoC DP (Display Port) interface driver.
 *
 * Copyright (C) Fuzhou Rockchip Electronics Co., Ltd.
 * Author: Andy Yan <andy.yan@rock-chips.com>
 *         Yakir Yang <ykk@rock-chips.com>
 *         Jeff Chen <jeff.chen@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/component.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/clk.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>

#include <uapi/linux/videodev2.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/bridge/analogix_dp.h>

#include "../bridge/analogix/analogix_dp_core.h"

#include "rockchip_drm_drv.h"
#include "rockchip_drm_psr.h"
#include "rockchip_drm_vop.h"

#define RK3288_GRF_SOC_CON6		0x25c
#define RK3288_EDP_LCDC_SEL		BIT(5)
#define RK3399_GRF_SOC_CON20		0x6250
#define RK3399_EDP_LCDC_SEL		BIT(5)

#define HIWORD_UPDATE(val, mask)	(val | (mask) << 16)

#define PSR_WAIT_LINE_FLAG_TIMEOUT_MS	100

#define to_dp(nm)	container_of(nm, struct rockchip_dp_device, nm)

/**
 * struct rockchip_dp_chip_data - splite the grf setting of kind of chips
 * @lcdsel_grf_reg: grf register offset of lcdc select
 * @lcdsel_big: reg value of selecting vop big for eDP
 * @lcdsel_lit: reg value of selecting vop little for eDP
 * @chip_type: specific chip type
 * @ssc: check if SSC is supported by source
 * @audio: check if audio is supported by source
 */
struct rockchip_dp_chip_data {
	u32	lcdsel_grf_reg;
	u32	lcdsel_big;
	u32	lcdsel_lit;
	u32	chip_type;
	bool	ssc;
	bool	audio;
};

struct rockchip_dp_device {
	struct drm_device        *drm_dev;
	struct device            *dev;
	struct drm_encoder       encoder;
	struct drm_bridge	 *bridge;
	struct drm_display_mode  mode;

	int			 num_clks;
	u8 id;
	struct clk_bulk_data	 *clks;
	struct regmap            *grf;
	struct reset_control     *rst;
	struct reset_control     *apb_reset;
	struct regulator         *vcc_supply;
	struct regulator         *vccio_supply;

	struct platform_device *audio_pdev;
	const struct rockchip_dp_chip_data *data;

	struct analogix_dp_device *adp;
	struct analogix_dp_plat_data plat_data;
	struct rockchip_drm_sub_dev sub_dev;
};

static int rockchip_dp_audio_hw_params(struct device *dev, void *data,
				       struct hdmi_codec_daifmt *daifmt,
				       struct hdmi_codec_params *params)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return analogix_dp_audio_hw_params(dp->adp, daifmt, params);
}

static void rockchip_dp_audio_shutdown(struct device *dev, void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	analogix_dp_audio_shutdown(dp->adp);
}

static int rockchip_dp_audio_startup(struct device *dev, void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return analogix_dp_audio_startup(dp->adp);
}

static int rockchip_dp_audio_get_eld(struct device *dev, void *data,
				     u8 *buf, size_t len)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return analogix_dp_audio_get_eld(dp->adp, buf, len);
}

static const struct hdmi_codec_ops rockchip_dp_audio_codec_ops = {
	.hw_params = rockchip_dp_audio_hw_params,
	.audio_startup = rockchip_dp_audio_startup,
	.audio_shutdown = rockchip_dp_audio_shutdown,
	.get_eld = rockchip_dp_audio_get_eld,
};

static int analogix_dp_psr_set(struct drm_encoder *encoder, bool enabled)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;

	if (!analogix_dp_psr_enabled(dp->adp))
		return 0;

	DRM_DEV_DEBUG(dp->dev, "%s PSR...\n", enabled ? "Entry" : "Exit");

	ret = rockchip_drm_wait_vact_end(dp->encoder.crtc,
					 PSR_WAIT_LINE_FLAG_TIMEOUT_MS);
	if (ret) {
		DRM_DEV_ERROR(dp->dev, "line flag interrupt did not arrive\n");
		return -ETIMEDOUT;
	}

	if (enabled)
		return analogix_dp_enable_psr(dp->adp);
	else
		return analogix_dp_disable_psr(dp->adp);
}

static int rockchip_dp_pre_init(struct rockchip_dp_device *dp)
{
	reset_control_assert(dp->rst);
	usleep_range(10, 20);
	reset_control_deassert(dp->rst);

	reset_control_assert(dp->apb_reset);
	usleep_range(10, 20);
	reset_control_deassert(dp->apb_reset);

	return 0;
}

static int rockchip_dp_poweron_start(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);
	int ret;

	if (dp->vcc_supply) {
		ret = regulator_enable(dp->vcc_supply);
		if (ret)
			dev_warn(dp->dev, "failed to enable vcc: %d\n", ret);
	}

	if (dp->vccio_supply) {
		ret = regulator_enable(dp->vccio_supply);
		if (ret)
			dev_warn(dp->dev, "failed to enable vccio: %d\n", ret);
	}

	ret = rockchip_dp_pre_init(dp);
	if (ret < 0) {
		DRM_DEV_ERROR(dp->dev, "failed to dp pre init %d\n", ret);
		return ret;
	}

	return ret;
}

static int rockchip_dp_poweron_end(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);

	return rockchip_drm_psr_inhibit_put(&dp->encoder);
}

static int rockchip_dp_powerdown(struct analogix_dp_plat_data *plat_data)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);
	int ret;

	ret = rockchip_drm_psr_inhibit_get(&dp->encoder);
	if (ret != 0)
		return ret;

	if (dp->vccio_supply)
		regulator_disable(dp->vccio_supply);

	if (dp->vcc_supply)
		regulator_disable(dp->vcc_supply);

	return 0;
}

static int rockchip_dp_get_modes(struct analogix_dp_plat_data *plat_data,
				 struct drm_connector *connector)
{
	struct drm_display_info *di = &connector->display_info;
	/* VOP couldn't output YUV video format for eDP rightly */
	u32 mask = DRM_COLOR_FORMAT_YCRCB444 | DRM_COLOR_FORMAT_YCRCB422;
	int ret = 0;

	if ((di->color_formats & mask)) {
		DRM_DEBUG_KMS("Swapping display color format from YUV to RGB\n");
		di->color_formats &= ~mask;
		di->color_formats |= DRM_COLOR_FORMAT_RGB444;
		di->bpc = 8;
	}

	if (list_empty(&connector->probed_modes) && !plat_data->panel) {
		ret = rockchip_drm_add_modes_noedid(connector);
		DRM_ERROR("analogix dp get edid mode failed, use default mode\n");
	}

	return ret;
}

static int rockchip_dp_bridge_attach(struct analogix_dp_plat_data *plat_data,
				     struct drm_bridge *bridge,
				     struct drm_connector *connector)
{
	struct rockchip_dp_device *dp = to_dp(plat_data);
	int ret;

	if (dp->bridge) {
		ret = drm_bridge_attach(&dp->encoder, dp->bridge, bridge);
		if (ret) {
			DRM_ERROR("Failed to attach bridge to drm: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static bool
rockchip_dp_drm_encoder_mode_fixup(struct drm_encoder *encoder,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	/* do nothing */
	return true;
}

static void rockchip_dp_drm_encoder_mode_set(struct drm_encoder *encoder,
					     struct drm_display_mode *mode,
					     struct drm_display_mode *adjusted)
{
	/* do nothing */
}

static void rockchip_dp_drm_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;
	u32 val;

	if (!dp->data->lcdsel_grf_reg)
		return;

	ret = drm_of_encoder_active_endpoint_id(dp->dev->of_node, encoder);
	if (ret < 0)
		return;

	if (ret)
		val = dp->data->lcdsel_lit;
	else
		val = dp->data->lcdsel_big;

	DRM_DEV_DEBUG(dp->dev, "vop %s output to dp\n", (ret) ? "LIT" : "BIG");

	ret = regmap_write(dp->grf, dp->data->lcdsel_grf_reg, val);
	if (ret != 0)
		DRM_DEV_ERROR(dp->dev, "Could not write to GRF: %d\n", ret);
}

static void rockchip_dp_drm_encoder_disable(struct drm_encoder *encoder)
{
	struct drm_crtc *crtc = encoder->crtc;
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc->state);

	s->output_if &= ~VOP_OUTPUT_IF_eDP0;
}

static int
rockchip_dp_drm_encoder_atomic_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
				      struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);
	struct drm_display_info *di = &conn_state->connector->display_info;

	/*
	 * The hardware IC designed that VOP must output the RGB10 video
	 * format to eDP controller, and if eDP panel only support RGB8,
	 * then eDP controller should cut down the video data, not via VOP
	 * controller, that's why we need to hardcode the VOP output mode
	 * to RGA10 here.
	 */

	s->output_mode = ROCKCHIP_OUT_MODE_AAAA;
	s->output_type = DRM_MODE_CONNECTOR_eDP;
	s->output_if |= VOP_OUTPUT_IF_eDP0;
	s->output_bpc = di->bpc;
	if (di->num_bus_formats)
		s->bus_format = di->bus_formats[0];
	else
		s->bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	s->bus_flags = di->bus_flags;
	s->tv_state = &conn_state->tv;
	s->eotf = TRADITIONAL_GAMMA_SDR;
	s->color_space = V4L2_COLORSPACE_DEFAULT;

	return 0;
}

static int rockchip_dp_drm_encoder_loader_protect(struct drm_encoder *encoder,
						  bool on)
{
	struct rockchip_dp_device *dp = to_dp(encoder);
	int ret;

	if (on) {
		if (dp->vcc_supply) {
			ret = regulator_enable(dp->vcc_supply);
			if (ret)
				dev_warn(dp->dev,
					 "failed to enable vcc: %d\n", ret);
		}

		if (dp->vccio_supply) {
			ret = regulator_enable(dp->vccio_supply);
			if (ret)
				dev_warn(dp->dev,
					 "failed to enable vccio: %d\n", ret);
		}

		rockchip_drm_psr_inhibit_put(&dp->encoder);
	}

	return 0;
}

static int rockchip_dp_get_property(struct drm_connector *connector,
				    const struct drm_connector_state *state,
				    struct drm_property *property,
				    u64 *val,
				    struct analogix_dp_plat_data *data)
{
	struct drm_encoder *encoder = data->encoder;
	struct rockchip_dp_device *dp = to_dp(encoder);
	struct rockchip_drm_private *private = connector->dev->dev_private;

	if (property == private->connector_id_prop) {
		*val = dp->id;
		return 0;
	}

	DRM_ERROR("failed to get rockchip analogic dp property\n");
	return -EINVAL;
}

static int rockchip_dp_attach_properties(struct drm_connector *connector)
{
	struct rockchip_drm_private *private = connector->dev->dev_private;

	drm_object_attach_property(&connector->base, private->connector_id_prop, 0);

	return 0;
}

static const struct analogix_dp_property_ops rockchip_dp_encoder_property_ops = {
	.get_property = rockchip_dp_get_property,
	.attach_properties = rockchip_dp_attach_properties,
};

static struct drm_encoder_helper_funcs rockchip_dp_encoder_helper_funcs = {
	.mode_fixup = rockchip_dp_drm_encoder_mode_fixup,
	.mode_set = rockchip_dp_drm_encoder_mode_set,
	.enable = rockchip_dp_drm_encoder_enable,
	.disable = rockchip_dp_drm_encoder_disable,
	.atomic_check = rockchip_dp_drm_encoder_atomic_check,
	.loader_protect = rockchip_dp_drm_encoder_loader_protect,
};

static struct drm_encoder_funcs rockchip_dp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int rockchip_dp_of_probe(struct rockchip_dp_device *dp)
{
	struct device *dev = dp->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (of_property_read_bool(np, "rockchip,grf")) {
		dp->grf = syscon_regmap_lookup_by_phandle(np, "rockchip,grf");
		if (IS_ERR(dp->grf)) {
			DRM_DEV_ERROR(dev, "failed to get rockchip,grf\n");
			return PTR_ERR(dp->grf);
		}
	}

	ret = devm_clk_bulk_get_all(dev, &dp->clks);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "failed to get clocks %d\n", ret);
		return ret;
	}

	dp->num_clks = ret;

	dp->rst = devm_reset_control_get(dev, "dp");
	if (IS_ERR(dp->rst)) {
		DRM_DEV_ERROR(dev, "failed to get dp reset control\n");
		return PTR_ERR(dp->rst);
	}

	dp->apb_reset = devm_reset_control_get_optional(dev, "apb");
	if (IS_ERR(dp->apb_reset)) {
		DRM_DEV_ERROR(dev, "failed to get apb reset control\n");
		return PTR_ERR(dp->apb_reset);
	}

	dp->vcc_supply = devm_regulator_get_optional(dev, "vcc");
	if (IS_ERR(dp->vcc_supply)) {
		if (PTR_ERR(dp->vcc_supply) != -ENODEV) {
			ret = PTR_ERR(dp->vcc_supply);
			dev_err(dev, "failed to get vcc regulator: %d\n", ret);
			return ret;
		}

		dp->vcc_supply = NULL;
	}

	dp->vccio_supply = devm_regulator_get_optional(dev, "vccio");
	if (IS_ERR(dp->vccio_supply)) {
		if (PTR_ERR(dp->vccio_supply) != -ENODEV) {
			ret = PTR_ERR(dp->vccio_supply);
			dev_err(dev, "failed to get vccio regulator: %d\n",
				ret);
			return ret;
		}

		dp->vccio_supply = NULL;
	}

	return 0;
}

static int rockchip_dp_drm_create_encoder(struct rockchip_dp_device *dp)
{
	struct drm_encoder *encoder = &dp->encoder;
	struct drm_device *drm_dev = dp->drm_dev;
	struct device *dev = dp->dev;
	int ret;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev,
							     dev->of_node);
	DRM_DEBUG_KMS("possible_crtcs = 0x%x\n", encoder->possible_crtcs);

	ret = drm_encoder_init(drm_dev, encoder, &rockchip_dp_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);
	if (ret) {
		DRM_ERROR("failed to initialize encoder with drm\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &rockchip_dp_encoder_helper_funcs);

	return 0;
}

static int rockchip_dp_bind(struct device *dev, struct device *master,
			    void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);
	const struct rockchip_dp_chip_data *dp_data;
	struct drm_device *drm_dev = data;
	int ret;

	dp_data = of_device_get_match_data(dev);
	if (!dp_data)
		return -ENODEV;

	dp->data = dp_data;
	dp->drm_dev = drm_dev;

	ret = rockchip_dp_drm_create_encoder(dp);
	if (ret) {
		DRM_ERROR("failed to create drm encoder\n");
		return ret;
	}

	dp->plat_data.encoder = &dp->encoder;
	dp->plat_data.ssc = dp->data->ssc;
	dp->plat_data.dev_type = dp->data->chip_type;
	dp->plat_data.power_on_start = rockchip_dp_poweron_start;
	dp->plat_data.power_on_end = rockchip_dp_poweron_end;
	dp->plat_data.power_off = rockchip_dp_powerdown;
	dp->plat_data.get_modes = rockchip_dp_get_modes;
	dp->plat_data.attach = rockchip_dp_bridge_attach;
	dp->plat_data.property_ops = &rockchip_dp_encoder_property_ops;

	ret = rockchip_drm_psr_register(&dp->encoder, analogix_dp_psr_set);
	if (ret < 0)
		goto err_cleanup_encoder;

	if (dp->data->audio) {
		struct hdmi_codec_pdata codec_data = {
			.ops = &rockchip_dp_audio_codec_ops,
			.spdif = 1,
			.i2s = 1,
			.max_i2s_channels = 2,
		};

		dp->audio_pdev =
			platform_device_register_data(dev, HDMI_CODEC_DRV_NAME,
						      PLATFORM_DEVID_AUTO,
						      &codec_data,
						      sizeof(codec_data));
		if (IS_ERR(dp->audio_pdev)) {
			ret = PTR_ERR(dp->audio_pdev);
			goto err_unreg_psr;
		}
	}

	dp->adp = analogix_dp_bind(dev, dp->drm_dev, &dp->plat_data);
	if (IS_ERR(dp->adp)) {
		ret = PTR_ERR(dp->adp);
		goto err_unreg_audio;
	}

	dp->sub_dev.connector = &dp->adp->connector;
	dp->sub_dev.of_node = dev->of_node;
	rockchip_drm_register_sub_dev(&dp->sub_dev);

	return 0;
err_unreg_audio:
	if (dp->audio_pdev)
		platform_device_unregister(dp->audio_pdev);
err_unreg_psr:
	rockchip_drm_psr_unregister(&dp->encoder);
err_cleanup_encoder:
	dp->encoder.funcs->destroy(&dp->encoder);
	return ret;
}

static void rockchip_dp_unbind(struct device *dev, struct device *master,
			       void *data)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	rockchip_drm_unregister_sub_dev(&dp->sub_dev);
	if (dp->audio_pdev)
		platform_device_unregister(dp->audio_pdev);
	analogix_dp_unbind(dp->adp);
	rockchip_drm_psr_unregister(&dp->encoder);
	dp->encoder.funcs->destroy(&dp->encoder);

	dp->adp = ERR_PTR(-ENODEV);
}

static const struct component_ops rockchip_dp_component_ops = {
	.bind = rockchip_dp_bind,
	.unbind = rockchip_dp_unbind,
};

static int rockchip_dp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drm_panel *panel = NULL;
	struct drm_bridge *bridge = NULL;
	struct rockchip_dp_device *dp;
	int ret, id;

	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, 0, &panel, &bridge);
	if (ret < 0 && ret != -ENODEV)
		return ret;

	dp = devm_kzalloc(dev, sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	id = of_alias_get_id(dev->of_node, "edp");
	if (id < 0)
		id = 0;
	dp->id = id;
	dp->dev = dev;
	dp->adp = ERR_PTR(-ENODEV);
	dp->plat_data.panel = panel;
	dp->plat_data.skip_connector = !!bridge;
	dp->bridge = bridge;

	ret = rockchip_dp_of_probe(dp);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, dp);

	return component_add(dev, &rockchip_dp_component_ops);
}

static int rockchip_dp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rockchip_dp_component_ops);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rockchip_dp_suspend(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	if (IS_ERR(dp->adp))
		return 0;

	return analogix_dp_suspend(dp->adp);
}

static int rockchip_dp_resume(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	if (IS_ERR(dp->adp))
		return 0;

	return analogix_dp_resume(dp->adp);
}

static int rockchip_dp_runtime_suspend(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	clk_bulk_disable_unprepare(dp->num_clks, dp->clks);

	return 0;
}

static int rockchip_dp_runtime_resume(struct device *dev)
{
	struct rockchip_dp_device *dp = dev_get_drvdata(dev);

	return clk_bulk_prepare_enable(dp->num_clks, dp->clks);
}
#endif

static const struct dev_pm_ops rockchip_dp_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend_late = rockchip_dp_suspend,
	.resume_early = rockchip_dp_resume,
	.runtime_suspend = rockchip_dp_runtime_suspend,
	.runtime_resume = rockchip_dp_runtime_resume,
#endif
};

static const struct rockchip_dp_chip_data rk3399_edp = {
	.lcdsel_grf_reg = RK3399_GRF_SOC_CON20,
	.lcdsel_big = HIWORD_UPDATE(0, RK3399_EDP_LCDC_SEL),
	.lcdsel_lit = HIWORD_UPDATE(RK3399_EDP_LCDC_SEL, RK3399_EDP_LCDC_SEL),
	.chip_type = RK3399_EDP,
	.ssc = true,
};

static const struct rockchip_dp_chip_data rk3368_edp = {
	.chip_type = RK3368_EDP,
	.ssc = true,
};

static const struct rockchip_dp_chip_data rk3288_dp = {
	.lcdsel_grf_reg = RK3288_GRF_SOC_CON6,
	.lcdsel_big = HIWORD_UPDATE(0, RK3288_EDP_LCDC_SEL),
	.lcdsel_lit = HIWORD_UPDATE(RK3288_EDP_LCDC_SEL, RK3288_EDP_LCDC_SEL),
	.chip_type = RK3288_DP,
	.ssc = true,
};

static const struct rockchip_dp_chip_data rk3568_edp = {
	.chip_type = RK3568_EDP,
	.ssc = true,
	.audio = true,
};

static const struct of_device_id rockchip_dp_dt_ids[] = {
	{.compatible = "rockchip,rk3288-dp", .data = &rk3288_dp },
	{.compatible = "rockchip,rk3368-edp", .data = &rk3368_edp },
	{.compatible = "rockchip,rk3399-edp", .data = &rk3399_edp },
	{.compatible = "rockchip,rk3568-edp", .data = &rk3568_edp },
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_dp_dt_ids);

struct platform_driver rockchip_dp_driver = {
	.probe = rockchip_dp_probe,
	.remove = rockchip_dp_remove,
	.driver = {
		   .name = "rockchip-dp",
		   .pm = &rockchip_dp_pm_ops,
		   .of_match_table = of_match_ptr(rockchip_dp_dt_ids),
	},
};
