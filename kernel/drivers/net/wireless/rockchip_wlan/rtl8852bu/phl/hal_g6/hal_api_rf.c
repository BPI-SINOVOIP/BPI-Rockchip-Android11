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
#define _HAL_API_RF_C_
#include "hal_headers.h"
#include "phy/rf/halrf_api.h"
#include "phy/rf/halrf_export_fun.h"

#ifdef USE_TRUE_PHY
enum rtw_hal_status
rtw_hal_rf_init(struct rtw_phl_com_t *phl_com,
			struct hal_info_t *hal_info)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	hal_status = halrf_init(phl_com, hal_com, &(hal_info->rf));

	if ((hal_status != RTW_HAL_STATUS_SUCCESS) ||
		(hal_info->rf == NULL)) {
		PHL_ERR("[PHL] rtw_halrf_init failed status(%d), hal_info->rf(%p)\n",
			hal_status, hal_info->rf);
	}

	return hal_status;
}

void rtw_hal_rf_deinit(struct rtw_phl_com_t *phl_com,
			struct hal_info_t *hal_info)
{
	struct rtw_hal_com_t *hal_com = hal_info->hal_com;

	halrf_deinit(phl_com, hal_com, hal_info->rf);
}

void rtw_hal_init_rf_reg(struct rtw_phl_com_t *phl_com, void *hal)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	struct rtw_para_info_t *phy_reg_info = NULL;
	struct rtw_para_pwrlmt_info_t *pwrlmt_info = NULL;
	u8 phy_idx = 0;

	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
		phy_reg_info = &phl_com->phy_sw_cap[phy_idx].rf_radio_a_info;
		halrf_config_radio_a_reg(hal_info->rf, phy_reg_info->para_src,
				phy_reg_info->para_data_len, phy_reg_info->para_data);
	}

	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
		phy_reg_info = &phl_com->phy_sw_cap[phy_idx].rf_radio_b_info;
		halrf_config_radio_b_reg(hal_info->rf , phy_reg_info->para_src,
				phy_reg_info->para_data_len, phy_reg_info->para_data);
	}

	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
		phy_reg_info = &phl_com->phy_sw_cap[phy_idx].rf_txpwr_byrate_info;
		halrf_config_store_power_by_rate(hal_info->rf , phy_reg_info->para_src,
				phy_reg_info->para_data_len, phy_reg_info->para_data);
	}
	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
			phy_reg_info = &phl_com->phy_sw_cap[phy_idx].rf_txpwrtrack_info;
			halrf_config_store_power_track(hal_info->rf , phy_reg_info->para_src,
					phy_reg_info->para_data_len, phy_reg_info->para_data);
	}

	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
		pwrlmt_info = &phl_com->phy_sw_cap[phy_idx].rf_txpwrlmt_info;
		halrf_config_store_power_limit(hal_info->rf , pwrlmt_info->para_src,
				pwrlmt_info->para_data_len, pwrlmt_info->para_data);
	}

	for (phy_idx = 0; phy_idx < 1 ; phy_idx++) {
		pwrlmt_info = &phl_com->phy_sw_cap[phy_idx].rf_txpwrlmt_ru_info;
		halrf_config_store_power_limit_ru(hal_info->rf , pwrlmt_info->para_src,
				pwrlmt_info->para_data_len, pwrlmt_info->para_data);
	}
}

void rtw_hal_rf_dm_init(struct hal_info_t *hal_info)
{
	halrf_dm_init(hal_info->rf);
}

enum rtw_hal_status
rtw_hal_rf_get_efuse_info(struct rtw_hal_com_t *hal_com,
	u8 *efuse_map, enum rtw_efuse_info info_type, void *value,
	u8 size, u8 map_valid)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	PHL_INFO("%s\n", __FUNCTION__);

	if(halrf_get_efuse_info(hal_info->rf, efuse_map, info_type, value, size,
							map_valid))
		return RTW_HAL_STATUS_SUCCESS;
	else
		return RTW_HAL_STATUS_FAILURE;
}

void
rtw_hal_rf_set_power_table_switch(struct rtw_hal_com_t *hal_com,
				enum phl_phy_idx phy_idx,
				u8 pwrbyrate_type, u8 pwrlmt_type)
{
/*	struct hal_info_t *hal_info = hal_com->hal_priv;

	halrf_set_power_table_switch(hal_info->rf, phy_idx,
				pwrbyrate_type, pwrlmt_type);*/
}

int rtw_hal_rf_get_predefined_pw_lmt_regu_type_from_str(const char *str)
{
	return halrf_get_predefined_pw_lmt_regu_type_from_str(str);
}

const char * const *rtw_hal_rf_get_predefined_pw_lmt_regu_type_str_array(u8 *num)
{
	return halrf_get_predefined_pw_lmt_regu_type_str_array(num);
}

const char *rtw_hal_rf_get_pw_lmt_regu_type_str(struct hal_info_t *hal_info, enum band_type band)
{
	return halrf_get_pw_lmt_regu_type_str(hal_info->rf, band);
}

enum rtw_hal_status rtw_hal_rf_read_pwr_table(
	struct rtw_hal_com_t *hal_com, u8 rf_path, u16 rate,
	u8 bandwidth, u8 channel, u8 offset, u8 dcm,
	u8 beamforming, s16 *get_item)
{
	int ret = RTW_HAL_STATUS_SUCCESS;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	*get_item = halrf_get_power(hal_info->rf, rf_path, rate, dcm,offset, bandwidth, beamforming, channel);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_wl_tx_power_control(struct rtw_hal_com_t *hal_com,
	u32 tx_power_val)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	PHL_INFO("%s\n", __FUNCTION__);

	halrf_wl_tx_power_control(hal_info->rf, tx_power_val);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_pwrtrack(struct hal_info_t *hal_info, u8 *txpwr_track_status)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	*txpwr_track_status = halrf_get_power_track(hal_info->rf);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_set_pwrtrack(struct hal_info_t *hal_info, u8 phy_idx, u8 txpwr_track_status)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	halrf_set_power_track(hal_info->rf, phy_idx, txpwr_track_status);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_get_thermal(struct hal_info_t *hal_info, u8 rf_path, u8 *thermal)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	*thermal = halrf_get_thermal(hal_info->rf, rf_path);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_set_tssi(struct hal_info_t *hal_info, u8 phy_idx, u8 rf_path, u32 tssi_de)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	halrf_set_tssi_de_for_tx_verify(hal_info->rf, phy_idx, tssi_de, rf_path);

	return ret;

}

enum rtw_hal_status rtw_hal_rf_set_tssi_offset(struct hal_info_t *hal_info, u8 phy_idx, u32 tssi_de_offset, u8 rf_path)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	halrf_set_tssi_de_offset(hal_info->rf, phy_idx, tssi_de_offset, rf_path);

	return ret;

}

enum rtw_hal_status rtw_hal_rf_get_tssi(struct hal_info_t *hal_info, u8 phy_idx, u8 rf_path, u32 *tssi_de)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	*tssi_de = halrf_get_tssi_de(hal_info->rf, phy_idx, rf_path);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_get_online_tssi_de(struct hal_info_t *hal_info, u8 phy_idx, u8 rf_path,s32 dbm, s32 pout, s32 *tssi_de)
{
	int ret = RTW_HAL_STATUS_SUCCESS;

	PHL_INFO("[MP HAL API] %s \n", __FUNCTION__);

	*tssi_de = halrf_get_online_tssi_de(hal_info->rf, phy_idx, rf_path, dbm, pout);

	return ret;
}

enum rtw_hal_status
rtw_hal_rf_set_continuous_tx(struct hal_info_t *hal_info)
{
	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_singletone_tx(struct hal_info_t *hal_info,
						u8 is_on,enum rf_path path)
{
	PHL_INFO("%s: enable = %d path = %x\n", __FUNCTION__, is_on, path);
	halrf_lo_test(hal_info->rf, is_on, path);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_suppression_tx(struct hal_info_t *hal_info)
{
	PHL_INFO("%s\n", __FUNCTION__);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_ctrl_dbcc(struct rtw_hal_com_t *hal_com,
	bool dbcc_en)
{
	/*need replace with rf api*/
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal_com->hal_priv;

	if (halrf_set_dbcc(hal_info->rf, dbcc_en))
		hal_status = RTW_HAL_STATUS_SUCCESS;

	return hal_status;
}

#define DBG_RFK_TIME
enum rtw_hal_status rtw_hal_rf_chl_rfk_trigger(void *hal,
						u8 phy_idx, u8 force)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	#ifdef DBG_RFK_TIME
	u32 iqk_start = _os_get_cur_time_ms();
	#endif

	PHL_INFO("[DBG_RFK]%s: phy_idx(%d), force(%d)\n", __func__,
		phy_idx, force);
	rtw_hal_mac_ser_ctrl(hal_info, false);

	hal_status = halrf_chl_rfk_trigger(hal_info->rf, phy_idx, force);

	#ifdef DBG_RFK_TIME
	PHL_INFO("[DBG_RFK]%s: RFK take %d (ms)\n", __func__,
		phl_get_passing_time_ms(iqk_start));
	#endif
	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	rtw_hal_mac_ser_ctrl(hal_info, true);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_dack_trigger(struct hal_info_t *hal_info,
						u8 force)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_dack_trigger(hal_info->rf, force);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_iqk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	hal_status = halrf_iqk_trigger(hal_info->rf, phy_idx, force);

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_lck_trigger(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_lck_trigger(hal_info->rf);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_dpk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	hal_status = halrf_dpk_trigger(hal_info->rf, phy_idx, force);

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_tssi_trigger(struct hal_info_t *hal_info,
				u8 phy_idx)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_tssi_trigger(hal_info->rf, phy_idx);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_gapk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_gapk_trigger(hal_info->rf, phy_idx, force);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}


enum rtw_hal_status rtw_hal_rf_set_capability_dack(struct hal_info_t *hal_info,
				u8 enable)
{
#if 0
	halrf_dack_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_iqk(struct hal_info_t *hal_info,
				u8 enable)
{
#if 0
	halrf_iqk_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_dpk(struct hal_info_t *hal_info,
						u8 enable)
{
#if 0
	halrf_dpk_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_dpk_track(struct hal_info_t *hal_info,
						u8 enable)
{
#if 0
	halrf_dpk_track_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_set_capability_tssi(struct hal_info_t *hal_info,
				u8 enable)
{
#if 0
	halrf_tssi_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_gapk(struct hal_info_t *hal_info,
						u8 enable)
{
#if 0
	halrf_gapk_onoff(hal_info->rf, enable);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_get_capability_dack(struct hal_info_t *hal_info,
						u8 *enable)
{
#if 0
	*enable = halrf_get_dack_onoff(hal_info->rf);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_iqk(struct hal_info_t *hal_info,
				u8 *enable)
{
#if 0
	*enable = halrf_get_iqk_onoff(hal_info->rf);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_dpk(struct hal_info_t *hal_info,
					u8 *enable)
{
#if 0
	*enable = halrf_get_dpk_onoff(hal_info->rf);
#endif
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_dpk_track(struct hal_info_t *hal_info,
				u8 *enable)
{
#if 0
	*enable = halrf_get_dpk_track_onoff(hal_info->rf);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_tssi(struct hal_info_t *hal_info,
				u8 *enable)
{
#if 0
	*enable = halrf_get_tssi_onoff(hal_info->rf);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_gapk(struct hal_info_t *hal_info,
					u8 *enable)
{
#if 0
	*enable = halrf_get_gapk_onoff(hal_info->rf);
#endif

	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_get_tssi_de_value(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_get_tssi_de_value(hal_info->rf);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_set_tssi_de_tx_verify(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_set_tssi_de_tx_verify(hal_info->rf);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_get_txpwr_final_abs(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

#if 0
	hal_status = halrf_get_txpwr_final_abs(hal_info->rf);
#else
	hal_status = RTW_HAL_STATUS_SUCCESS;
#endif

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

bool
rtw_hal_rf_proc_cmd(struct hal_info_t *hal_info,
					struct rtw_proc_cmd *incmd,
					char *output, u32 out_len)
{

	if(incmd->in_type == RTW_ARG_TYPE_BUF)
		halrf_cmd(hal_info->rf, incmd->in.buf, output, out_len);

	else if(incmd->in_type == RTW_ARG_TYPE_ARRAY){

		halrf_cmd_parser(hal_info->rf, incmd->in.vector,
					incmd->in_cnt_len, output, out_len);
	}

	return true;
}

enum rtw_hal_status rtw_hal_rf_watchdog(struct hal_info_t *hal_info)
{

	halrf_watchdog(hal_info->rf);

	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_power(struct hal_info_t *hal_info, enum phl_phy_idx phy,
					enum phl_pwr_table pwr_table)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	if(halrf_set_power(hal_info->rf, phy, pwr_table))
	{
		hal_status = RTW_HAL_STATUS_SUCCESS;
	}
	else
	{
		hal_status = RTW_HAL_STATUS_FAILURE;
	}

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_set_gain_offset(struct hal_info_t *hal_info, u8 cur_phy_idx,
						s8 offset, u8 rf_path)
{

	enum rtw_hal_status ret=RTW_HAL_STATUS_SUCCESS;
	PHL_INFO("[MP HAL API]%s\n", __FUNCTION__);

	halrf_set_rx_gain_offset_for_rx_verify(hal_info->rf, cur_phy_idx, offset, rf_path);

	return ret;
}

enum rtw_hal_status rtw_hal_rf_trigger_dpk_tracking(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_FAILURE;

	hal_status = halrf_dpk_tracking(hal_info->rf);

	if (hal_status != RTW_HAL_STATUS_SUCCESS)
		PHL_ERR("[MP HAL API] %s failed status(%d)\n",__FUNCTION__, hal_status);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_rf_get_default_rfe_type(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	hal_com->dev_hw_cap.rfe_type = halrf_get_default_rfe_type(hal_info->rf);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_get_default_xtal(struct rtw_hal_com_t *hal_com)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	hal_com->dev_hw_cap.xcap = halrf_get_default_xtal(hal_info->rf);
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_tssi_avg(struct hal_info_t *hal_info, u8 cur_phy_idx,
						s32 xdbm)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_set_tssi_avg_mp(hal_info->rf, cur_phy_idx, xdbm);

	return hal_status;
}

void rtw_hal_rf_do_tssi_scan(struct hal_info_t *hal_info, u8 cur_phy_idx)
{
	halrf_do_tssi_scan(hal_info->rf, cur_phy_idx);
}

enum rtw_hal_status
rtw_hal_rf_update_ext_pwr_lmt_table(struct hal_info_t *hal_info)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_power_limit_set_ext_pwr_limit_table(hal_info->rf, HW_PHY_0);
	halrf_power_limit_set_ext_pwr_limit_ru_table(hal_info->rf, HW_PHY_0);

	return hal_status;
}

enum rtw_hal_status
rtw_hal_rf_config_radio_to_fw(struct hal_info_t *hal_info)
{
	halrf_config_radio_to_fw(hal_info->rf);

	return RTW_HAL_STATUS_SUCCESS;
}

bool
rtw_hal_rf_check_efuse_data(struct rtw_hal_com_t *hal_com, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	return halrf_tssi_check_efuse_data(hal_info->rf, phy_idx);
}

void
rtw_hal_rf_disconnect_notify(void *hal, struct rtw_chan_def *chandef)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	halrf_disconnect_notify(hal_info->rf, chandef);
}

bool
rtw_hal_rf_check_mcc_ch(void *hal, struct rtw_chan_def *chandef)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;
	return halrf_check_mcc_ch(hal_info->rf, chandef);
}

void
rtw_hal_rf_dpk_switch(void *hal, bool enable)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_rf_dpk_switch(): enable(%d)\n",
		enable);
	halrf_dpk_switch(hal_info->rf, enable);
}

void
rtw_hal_rf_tssi_config(void *hal, enum phl_phy_idx phy_idx, bool enable)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	PHL_TRACE(COMP_PHL_MCC, _PHL_INFO_, "rtw_hal_rf_tssi_config(): enable(%d), phy_idx(%d)\n",
		enable, phy_idx);
	if (enable)
		halrf_tssi_enable(hal_info->rf, phy_idx);
	else
		halrf_tssi_disable(hal_info->rf, phy_idx);
}

enum rtw_hal_status
rtw_hal_rf_set_ch_bw(struct hal_info_t *hal_info, enum phl_phy_idx phy, u8 center_ch,
						enum band_type band, enum channel_width bw)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_ctrl_bw_ch(hal_info->rf, phy, center_ch, band, bw);

	return hal_status;
}

void
rtw_hal_rf_get_efuse_ex(struct rtw_hal_com_t *hal_com, enum phl_phy_idx phy_idx)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	halrf_tssi_get_efuse_ex(hal_info->rf, phy_idx);

}


/* PSD */
enum rtw_hal_status rtw_hal_rf_psd_init(struct hal_info_t *hal_info, u8 cur_phy_idx,
					u8 path, u8 iq_path, u32 avg, u32 fft)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_psd_init(hal_info->rf, cur_phy_idx, path, iq_path, avg, fft);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_psd_restore(struct hal_info_t *hal_info, u8 cur_phy_idx)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_psd_restore(hal_info->rf, cur_phy_idx);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_psd_get_point_data(struct hal_info_t *hal_info, u8 cur_phy_idx,
					s32 point, u32 *value)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	*value = halrf_psd_get_point_data(hal_info->rf, cur_phy_idx, point);

	return hal_status;
}

enum rtw_hal_status rtw_hal_rf_psd_query(struct hal_info_t *hal_info, u8 cur_phy_idx,
					u32 point, u32 start_point, u32 stop_point, u32 *outbuf)
{
	enum rtw_hal_status hal_status = RTW_HAL_STATUS_SUCCESS;

	halrf_psd_query(hal_info->rf, cur_phy_idx, point, start_point, stop_point, outbuf);

	return hal_status;
}

void rtw_hal_rf_rx_ant(struct hal_info_t *hal_info, /*enum halrf_ant*/u8 ant)
{
	halrf_2g_rxant(hal_info->rf, ant);
}

enum halrf_thermal_status
rtw_hal_rf_get_ther_protected_threshold(
	struct hal_info_t *hal_info
)
{
	s8 val = 0;
	enum halrf_thermal_status status = HALRF_THERMAL_STATUS_UNKNOWN;

	val = halrf_get_ther_protected_threshold(hal_info->rf);

	if(val == -1){
		status = HALRF_THERMAL_STATUS_ABOVE_THRESHOLD;
		PHL_TRACE(COMP_PHL_RF, _PHL_INFO_, "[HALRF] Thermal above threshold!\n");
	}
	else if(val == 0){
		status = HALRF_THERMAL_STATUS_STAY_THRESHOLD;
		PHL_TRACE(COMP_PHL_RF, _PHL_INFO_, "[HALRF] Thermal stay at threshold!\n");
	}
	else if(val == 1){
		status = HALRF_THERMAL_STATUS_BELOW_THRESHOLD;
		PHL_TRACE(COMP_PHL_RF, _PHL_INFO_, "[HALRF] Thermal below threshold!\n");
	}
	else{
		status = HALRF_THERMAL_STATUS_UNKNOWN;
		PHL_TRACE(COMP_PHL_RF, _PHL_WARNING_, "[HALRF] Thermal unknown status!\n");
	}
	return status;
}

void rtw_hal_rf_notification(struct hal_info_t *hal_info,
			     enum phl_msg_evt_id event,
			     enum phl_phy_idx phy_idx)
{
	halrf_wifi_event_notify(hal_info->rf, event, phy_idx);
}

void
rtw_hal_rf_tssi_scan_ch(struct rtw_hal_com_t *hal_com,
		enum phl_phy_idx phy_idx, enum rf_path path)
{
	struct hal_info_t *hal_info = hal_com->hal_priv;

	halrf_tssi_scan_ch(hal_info->rf, path);

	halrf_tssi_set_efuse_to_de(hal_info->rf, phy_idx);
}

s8 rtw_hal_rf_get_power_limit(void *hal, enum phl_phy_idx phy,
	u16 rate, u8 bandwidth, u8 beamforming, u8 tx_num, u8 channel)
{
	struct hal_info_t *hal_info = (struct hal_info_t *)hal;

	return halrf_get_power_limit(hal_info->rf, phy, RF_PATH_A, rate,
		bandwidth, beamforming, tx_num, channel);
}

void rtw_hal_rf_cmd_notification(struct hal_info_t *hal_info,
                             void *hal_cmd,
                             enum phl_phy_idx phy_idx)
{
	/*to do*/
}

#else /*ifdef USE_TRUE_PHY*/
enum rtw_hal_status
rtw_hal_rf_init(struct rtw_phl_com_t *phl_com, struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}
void rtw_hal_rf_deinit(struct rtw_phl_com_t *phl_com,
				struct hal_info_t *hal_info)
{
}

void rtw_hal_init_rf_reg(struct rtw_phl_com_t *phl_com, void *hal)
{
}

void rtw_hal_rf_dm_init(struct hal_info_t *hal_info)
{
}

enum rtw_hal_status
rtw_hal_rf_get_efuse_info(struct rtw_hal_com_t *hal_com, u8 *efuse_map,
				enum rtw_efuse_info info_type, void *value,
				u8 size, u8 map_valid)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_read_pwr_table(struct rtw_hal_com_t *hal_com, u8 rf_path, u16 rate,
				u8 bandwidth, u8 channel, u8 offset, u8 dcm,
				u8 beamforming, s16 *get_item)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_wl_tx_power_control(struct rtw_hal_com_t *hal_com,
				u32 tx_power_val)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_pwrtrack(struct hal_info_t *hal_info,
						u8 *txpwr_track_status)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_pwrtrack(struct hal_info_t *hal_info,
				u8 phy_idx, u8 txpwr_track_status)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_thermal(struct hal_info_t *hal_info,
						u8 rf_path, u8 *thermal)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_get_default_xtal(struct rtw_hal_com_t *hal_com)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_get_default_rfe_type(struct rtw_hal_com_t *hal_com)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_tssi(struct hal_info_t *hal_info,
				u8 phy_idx, u8 rf_path, u32 tssi_de)
{
	return RTW_HAL_STATUS_SUCCESS;
}
enum rtw_hal_status rtw_hal_rf_set_tssi_offset(struct hal_info_t *hal_info, u8 phy_idx, u32 tssi_de_offset, u8 rf_path)
{
	return RTW_HAL_STATUS_SUCCESS;
}
enum rtw_hal_status rtw_hal_rf_get_tssi(struct hal_info_t *hal_info,
					u8 phy_idx, u8 rf_path, u32 *tssi_de)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_online_tssi_de(
	struct hal_info_t *hal_info, u8 phy_idx, u8 rf_path,
	s32 dbm, s32 pout, s32 *tssi_de)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_continuous_tx(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_singletone_tx(struct hal_info_t *hal_info,
						u8 is_on,enum rf_path path)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_suppression_tx(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_ctrl_dbcc(struct rtw_hal_com_t *hal_com,
	bool dbcc_en)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_chl_rfk_trigger(void *hal, u8 phy_idx, u8 force)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_dack_trigger(struct hal_info_t *hal_info,
						u8 force)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_iqk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_lck_trigger(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_dpk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_tssi_trigger(struct hal_info_t *hal_info,
						u8 phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_gapk_trigger(struct hal_info_t *hal_info,
						u8 phy_idx, u8 force)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_set_capability_dack(struct hal_info_t *hal_info,
						u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_iqk(struct hal_info_t *hal_info,
						u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_dpk(struct hal_info_t *hal_info,
						u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_set_capability_dpk_track(struct hal_info_t *hal_info, u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status
rtw_hal_rf_set_capability_tssi(struct hal_info_t *hal_info, u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_capability_gapk(struct hal_info_t *hal_info,
						u8 enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_get_capability_dack(struct hal_info_t *hal_info,
						u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_iqk(struct hal_info_t *hal_info,
						u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_dpk(struct hal_info_t *hal_info,
						u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status
rtw_hal_rf_get_capability_dpk_track(struct hal_info_t *hal_info, u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_tssi(struct hal_info_t *hal_info,
						u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_capability_gapk(struct hal_info_t *hal_info,
						u8 *enable)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_get_tssi_de_value(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_tssi_de_tx_verify(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_get_txpwr_final_abs(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool rtw_hal_rf_proc_cmd(struct hal_info_t *hal_info,
				struct rtw_proc_cmd *incmd,
				char *output, u32 out_len)
{
	return true;
}

enum rtw_hal_status rtw_hal_rf_watchdog(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}
enum rtw_hal_status
rtw_hal_rf_set_power(struct hal_info_t *hal_info, enum phl_phy_idx phy,
					enum phl_pwr_table pwr_table)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_set_gain_offset(struct hal_info_t *hal_info, u8 cur_phy_idx,
						s8 offset, u8 rf_path)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_trigger_dpk_tracking(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void rtw_hal_rf_do_tssi_scan(struct hal_info_t *hal_info, u8 cur_phy_idx)
{

}

enum rtw_hal_status
rtw_hal_rf_config_radio_to_fw(struct hal_info_t *hal_info)
{
	return RTW_HAL_STATUS_SUCCESS;
}


enum rtw_hal_status rtw_hal_rf_set_tssi_avg(struct hal_info_t *hal_info, u8 cur_phy_idx,
						s32 xdbm)
{
	return RTW_HAL_STATUS_SUCCESS;
}

bool
rtw_hal_rf_check_efuse_data(struct rtw_hal_com_t *hal_com, enum phl_phy_idx phy_idx)
{
	return true;
}

enum rtw_hal_status
rtw_hal_rf_set_ch_bw(struct hal_info_t *hal_info, enum phl_phy_idx phy, u8 center_ch,
					enum band_type band, enum channel_width bw)

{
	return RTW_HAL_STATUS_SUCCESS;
}

/* PSD */
enum rtw_hal_status rtw_hal_rf_psd_init(struct hal_info_t *hal_info, u8 cur_phy_idx,
					u8 path, u8 iq_path, u32 avg, u32 fft)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_psd_restore(struct hal_info_t *hal_info, u8 cur_phy_idx)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_psd_get_point_data(struct hal_info_t *hal_info, u8 cur_phy_idx,
					s32 point, u32 *value)
{
	return RTW_HAL_STATUS_SUCCESS;
}

enum rtw_hal_status rtw_hal_rf_psd_query(struct hal_info_t *hal_info, u8 cur_phy_idx,
					u32 point, u32 start_point, u32 stop_point, u32 *outbuf)
{
	return RTW_HAL_STATUS_SUCCESS;
}

void
rtw_hal_rf_disconnect_notify(void *hal, struct rtw_chan_def *chandef)
{
}

bool
rtw_hal_rf_check_mcc_ch(void *hal, struct rtw_chan_def *chandef)
{
	return false;
}

void
rtw_hal_rf_dpk_switch(void *hal, bool enable)
{

}

void
rtw_hal_rf_tssi_config(void *hal, enum phl_phy_idx phy_idx, bool enable)
{

}

void
rtw_hal_rf_get_efuse_ex(struct rtw_hal_com_t *hal_com, enum phl_phy_idx phy_idx)
{
}

void
rtw_hal_rf_set_power_table_switch(struct rtw_hal_com_t *hal_com,
				enum phl_phy_idx phy_idx,
				u8 pwrbyrate_type, u8 pwrlmt_type)
{
	return;
}

int rtw_hal_rf_get_predefined_pw_lmt_regu_type_from_str(const char *str)
{
	return -1;
}

const char * const *rtw_hal_rf_get_predefined_pw_lmt_regu_type_str_array(u8 *num)
{
	return NULL;
}

const char *rtw_hal_rf_get_pw_lmt_regu_type_str(struct hal_info_t *hal_info, enum band_type band)
{
	return NULL;
}

void rtw_hal_rf_rx_ant(struct hal_info_t *hal_info, /*enum halrf_ant*/u8 ant)
{

}

enum halrf_thermal_status
rtw_hal_rf_get_ther_protected_threshold(
	struct hal_info_t *hal_info
)
{
	return HALRF_THERMAL_STATUS_BELOW_THRESHOLD;
}

void rtw_hal_rf_notification(struct hal_info_t *hal_info,
                             enum phl_msg_evt_id event,
                             enum phl_phy_idx phy_idx)
{
	return;
}

void
rtw_hal_rf_tssi_scan_ch(struct rtw_hal_com_t *hal_com,
		enum phl_phy_idx phy_idx, enum rf_path path)

{
	return;
}

s8 rtw_hal_rf_get_power_limit(void *hal, enum phl_phy_idx phy,
	u16 rate, u8 bandwidth, u8 beamforming, u8 tx_num, u8 channel)
{
	return 0;
}

void rtw_hal_rf_cmd_notification(struct hal_info_t *hal_info,
                             void *hal_cmd,
                             enum phl_phy_idx phy_idx)
{

}
#endif /*ifdef USE_TRUE_PHY*/
