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
#define _HAL_TEST_MP_CONFIG_C_
#include "../../hal_headers.h"
#include "../../../test/mp/phl_test_mp_def.h"

#ifdef CONFIG_HAL_TEST_MP
enum rtw_hal_status rtw_hal_mp_config_start_dut(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: NOT READY!!!\n", __FUNCTION__);
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_get_dev_info(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	arg->bustype = 0;
	// arg->bustype = mp->phl_com->hci_type;

	switch(hal_info->hal_com->chip_id) {
	case CHIP_WIFI6_8852A:
		arg->chipid = 0x8852a;
		break;
	case CHIP_WIFI6_8834A:
		arg->chipid = 0x8834a;
		break;
	case CHIP_WIFI6_8852B:
		arg->chipid = 0x8852b;
		break;
	case CHIP_WIFI6_MAX:
	default:
		PHL_INFO("Unknown chip: %d\n", hal_info->hal_com->chip_id);
		return RTW_HAL_STATUS_FAILURE;
		break;
	}

	PHL_INFO("%s: bustype = 0x%x, chipid = 0x%x\n",
		 __FUNCTION__, arg->bustype, arg->chipid);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mp_config_set_phy_idx(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	PHL_INFO("%s: phy index = 0x%x\n", __FUNCTION__, arg->cur_phy);

	mp->cur_phy = arg->cur_phy;
	hal_status = RTW_HAL_STATUS_SUCCESS;

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_get_bw_mode(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	/* call hal api*/
	hal_status = RTW_HAL_STATUS_SUCCESS;

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_set_trx_path(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;
	struct rtw_phl_com_t *phl_com = mp->phl_com;

	u8 bb_rx_path, bb_tx_path = 0;

	PHL_INFO("%s: ant_tx=0x%x, ant_rx=0x%x\n",
		 __FUNCTION__, arg->ant_tx, arg->ant_rx);

	if((hal_info->hal_com->chip_id == CHIP_WIFI6_8852A) &&
	   (phl_com->phy_cap[mp->cur_phy].txss == 1)) {
		if(arg->ant_tx != RF_PATH_B){
			hal_status = RTW_HAL_STATUS_FAILURE;
			PHL_INFO("%s: Invaled TX path (%d)\n",
		 		 __FUNCTION__, arg->ant_tx);
			goto exit;
		}
	}

	bb_tx_path = arg->ant_tx; /* warning !!! it should be convert to enum bb_path, to be discussed */
	bb_rx_path = arg->ant_rx; /* warning !!! it should be convert to enum bb_path, to be discussed */

	hal_status = rtw_hal_bb_cfg_tx_path(hal_info->hal_com, bb_tx_path);
	if(hal_status != RTW_HAL_STATUS_SUCCESS){
		PHL_INFO("%s: rtw_hal_bb_cfg_tx_path fail (%x)\n",
			 __FUNCTION__, hal_status);
		goto exit;
	}

	hal_status = rtw_hal_bb_cfg_rx_path(hal_info->hal_com, bb_rx_path);
	if(hal_status != RTW_HAL_STATUS_SUCCESS){
		PHL_INFO("%s: rtw_hal_bb_cfg_rx_path fail (%x)\n",
			 __FUNCTION__, hal_status);
		goto exit;
	}

exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_reset_phy_cnt(
	struct mp_context *mp)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s !\n", __FUNCTION__);

	hal_status = rtw_hal_bb_set_reset_cnt(mp->hal);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_reset_mac_cnt(
	struct mp_context *mp)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s !\n", __FUNCTION__);

	hal_status = rtw_hal_mac_set_reset_rx_cnt(mp->hal, mp->cur_phy);
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_reset_drv_cnt(
	struct mp_context *mp)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	struct rtw_trx_stat *trx_stat = &hal_com->trx_stat;

	PHL_INFO("%s !\n", __FUNCTION__);

	trx_stat->rx_ok_cnt = 0;
	trx_stat->rx_err_cnt = 0;
	hal_status = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_set_modulation(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("set modulation = 0x%x\n", arg->modulation);
	/* need assign arg->modulation to driver current wirelessmode */

	PHL_INFO("%s: NOT READY!!!\n", __FUNCTION__);
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_get_modulation(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: NOT READY!!!\n", __FUNCTION__);
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_set_rate(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: NOT READY!!!\n", __FUNCTION__);
	hal_status = RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("%s: status = %d\n", __FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_get_mac_addr(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 mac_addr[6] = {0x00, 0xE0, 0x4C, 0xbb, 0xbb, 0xbb};

	PHL_INFO("%s: NOT READY!!! Assign fixed mac address.\n", __FUNCTION__);
	_os_mem_cpy(mp->phl_com->drv_priv, arg->mac_addr, mac_addr, 6);
	hal_status = RTW_HAL_STATUS_SUCCESS;

	/* [TEMP] need assign driver mac addr to arg->mac_addr */
	PHL_INFO("%s: get mac addr = %x:%x:%x:%x:%x:%x\n", __FUNCTION__,
		 arg->mac_addr[0],arg->mac_addr[1],arg->mac_addr[2],
		 arg->mac_addr[3],arg->mac_addr[4],arg->mac_addr[5]);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_set_mac_addr(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 mac_addr[6] = {0};

	PHL_INFO("%s: NOT READY!!! ignore it.\n", __FUNCTION__);
	_os_mem_cpy(mp->phl_com->drv_priv, mac_addr, arg->mac_addr, 6);
	hal_status = RTW_HAL_STATUS_SUCCESS;

	/* [TEMP] need assign arg->mac_addr to driver mac addr */
	PHL_INFO("%s: set mac addr = %x:%x:%x:%x:%x:%x\n", __FUNCTION__,
		mac_addr[0],mac_addr[1],mac_addr[2],
		mac_addr[3],mac_addr[4],mac_addr[5]);

	return hal_status;
}

u8 hal_mp_primary_channel_decision(u8 cent_ch, enum channel_width bw, u8 tx_sc)
{
	u8 pri_ch = 0;

	if(bw == CHANNEL_WIDTH_20){
		pri_ch = cent_ch;
	}
	else if(bw == CHANNEL_WIDTH_40){
		if(tx_sc == 1)
			pri_ch = cent_ch + 2;
		else if(tx_sc == 2)
			pri_ch = cent_ch - 2;
		else
			pri_ch = cent_ch - 2; /* tx_sc == 0 */
	}
	else if(bw == CHANNEL_WIDTH_80){
		if((tx_sc == 10) || (tx_sc == 4))
			pri_ch = cent_ch - 6;
		else if((tx_sc == 9) || (tx_sc == 1))
			pri_ch = cent_ch + 2;
		else if(tx_sc == 2)
			pri_ch = cent_ch - 2;
		else if(tx_sc == 3)
			pri_ch = cent_ch + 6;
		else
			pri_ch = cent_ch - 6; /* tx_sc == 0 */
	}
	else{
		PHL_WARN("%s: 80+80 and 160 not consider yet!\n", __FUNCTION__);
	}

	return pri_ch;
}

enum chan_offset
hal_mp_chan_offset_decision(u8 pri_ch, u8 cent_ch, enum channel_width bw)
{
	enum chan_offset offset = CHAN_OFFSET_NO_EXT;

	if(bw == CHANNEL_WIDTH_20){
		offset = CHAN_OFFSET_NO_EXT;
	}
	else if(bw == CHANNEL_WIDTH_40){
		if(cent_ch > pri_ch)
			offset = CHAN_OFFSET_UPPER;
		else
			offset = CHAN_OFFSET_LOWER;
	}
	else if(bw == CHANNEL_WIDTH_80){
		if(cent_ch > pri_ch){
			if((cent_ch - pri_ch) == 6)
				offset = CHAN_OFFSET_UPPER;
			else
				offset = CHAN_OFFSET_LOWER;
		}
		else{
			if((pri_ch - cent_ch) == 6)
				offset = CHAN_OFFSET_LOWER;
			else
				offset = CHAN_OFFSET_UPPER;
		}
	}
	else{
		PHL_WARN("%s: 80+80 and 160 not consider yet!\n", __FUNCTION__);
	}
	return offset;
}

enum rtw_hal_status rtw_hal_mp_config_set_ch_bw(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	u8 pri_chan = 0;
	u8 cent_ch = arg->channel;
	u8 tx_sc = arg->sc_idx;
	struct rtw_chan_def chdef = {0};
	enum channel_width bw = arg->bandwidth;

	PHL_INFO("%s: phy index = %d.\n", __FUNCTION__, mp->cur_phy);
	PHL_INFO("%s: central channel = %d.\n", __FUNCTION__, arg->channel);
	PHL_INFO("%s: bandwith = %d.\n", __FUNCTION__, arg->bandwidth);
	PHL_INFO("%s: sc_idx = %d.\n", __FUNCTION__, arg->sc_idx);

	pri_chan = hal_mp_primary_channel_decision(cent_ch, bw, tx_sc);

	if(pri_chan == 0){
		PHL_ERR("%s Invalid primary channel!\n", __FUNCTION__);
		hal_status = RTW_HAL_STATUS_FAILURE;
		goto exit;
	}

	chdef.chan = pri_chan;
	chdef.band = rtw_phl_get_band_type(pri_chan);
	chdef.bw = bw;
	chdef.offset = hal_mp_chan_offset_decision(pri_chan, cent_ch, bw);

	PHL_INFO("%s: pri_ch(%d) bw(%d) offset(%d)\n",
			__FUNCTION__, chdef.chan, chdef.bw, chdef.offset);

	hal_status = rtw_hal_set_ch_bw(mp->hal,mp->cur_phy,
			&chdef, false);
	if(hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s Set ch bw fail!\n", __FUNCTION__);
		goto exit;
	}
	
	hal_status = rtw_hal_bb_set_txsc(mp->hal, arg->sc_idx, mp->cur_phy);
	if(hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s Set txsc fail!\n", __FUNCTION__);
		goto exit;
	}

	hal_status = rtw_hal_rf_chl_rfk_trigger(mp->hal, mp->cur_phy, false);
	if(hal_status != RTW_HAL_STATUS_SUCCESS) {
		PHL_ERR("%s chl rfk fail!\n", __FUNCTION__);
		goto exit;
	}
	
exit:
	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_swith_btc_path(
	struct mp_context *mp, struct mp_config_arg *arg)
{
	mp->phl_com->dev_cap.btc_mode = arg->btc_mode;
	mp->phl_com->dev_sw_cap.btc_mode = arg->btc_mode;

	PHL_INFO("%s: btc_mode = %d.\n", __FUNCTION__, arg->btc_mode);
	PHL_INFO("%s: phl_com->dev_sw_cap.btc_mode = %d.\n", __FUNCTION__, mp->phl_com->dev_sw_cap.btc_mode);

#ifdef CONFIG_BTCOEX
	rtw_hal_btc_init_coex_cfg_ntfy(mp->hal);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_mp_trigger_fw_conflict(struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	PHL_INFO("%s: offset = %d.\n", __FUNCTION__, arg->offset);

	hal_status = rtw_hal_mac_trigger_fw_conflict(mp->hal, arg->offset, arg->voltag);

	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_config_set_gpio(struct mp_context *mp, struct mp_config_arg *arg)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;

	hal_status = rtw_hal_mac_set_sw_gpio_mode(hal_info, RTW_AX_SW_IO_MODE_OUTPUT_PP, arg->gpio_id);
	if(hal_status != RTW_HAL_STATUS_SUCCESS){
		PHL_INFO("%s: hal_status = %d\n", __FUNCTION__, hal_status);
		return hal_status;
	}

	hal_status = rtw_hal_mac_sw_gpio_ctrl(hal_info, arg->gpio_enable, arg->gpio_id);
	PHL_INFO("%s: hal_status = %d\n", __FUNCTION__, hal_status);
	return hal_status;
}

enum rtw_hal_status rtw_hal_mp_ic_hw_setting_init(struct mp_context *mp)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	hal_status = rtw_hal_bb_ic_hw_setting_init(mp->hal);

	return hal_status;
}


u32 rtw_hal_get_uuid(struct mp_context *mp)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)mp->hal;
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	return hal_com->uuid;
}

#endif /* CONFIG_HAL_TEST_MP */
