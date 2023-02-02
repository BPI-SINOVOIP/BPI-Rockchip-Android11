/******************************************************************************
 *
 * Copyright(c) 2007 - 2020  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
#include "halbb_precomp.h"

void halbb_cfg_bb_rpl_ofst(struct bb_info *bb, enum bb_band_t band, u8 path, u32 addr, u32 data)
{
	struct bb_gain_info *gain = &bb->bb_gain_i;
	u8 i = 0;
	u8 bw = (u8)(addr & 0xf0) >> 4;
	u8 rxsc_start = (u8)(addr & 0xf);
	u8 rxsc = 0;
	s8 ofst = 0;

	if (bw == (u8)CHANNEL_WIDTH_20) {
		gain->rpl_ofst_20[band][path] = (s8)data;
		BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
		       band, path, (20 << bw), rxsc, gain->rpl_ofst_20[band][path]);
	} else if (bw == (u8)CHANNEL_WIDTH_40){
		if (rxsc_start == BB_RXSC_START_IDX_FULL) {
			gain->rpl_ofst_40[band][path][0] = (s8)data;
			BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
			       band, path, (20 << bw), rxsc,
			       gain->rpl_ofst_40[band][path][0]);
		} else if (rxsc_start == BB_RXSC_START_IDX_20) {
			for (i = 0; i < 2; i++) {
				rxsc = BB_RXSC_START_IDX_20 + i;
				ofst = (s8)((data >> (8 * i)) & 0xff);
				gain->rpl_ofst_40[band][path][rxsc] = ofst;
				BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
				       band, path, (20 << bw), rxsc,
				       gain->rpl_ofst_40[band][path][rxsc]);
			}
		}

	} else if (bw == (u8)CHANNEL_WIDTH_80){
		if (rxsc_start == BB_RXSC_START_IDX_FULL) {
			gain->rpl_ofst_80[band][path][0] = (s8)data;
			BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
			       band, path, (20 << bw), rxsc,
			       gain->rpl_ofst_80[band][path][0]);
		} else if (rxsc_start == BB_RXSC_START_IDX_20) {
			for (i = 0; i < 4; i++) {
				rxsc = BB_RXSC_START_IDX_20 + i;
				ofst = (s8)((data >> (8 * i)) & 0xff);
				gain->rpl_ofst_80[band][path][rxsc] = ofst;
				BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
				       band, path, (20 << bw), rxsc,
				       gain->rpl_ofst_80[band][path][rxsc]);
			}
		} else if (rxsc_start == BB_RXSC_START_IDX_40) {
			for (i = 0; i < 2; i++) {
				rxsc = BB_RXSC_START_IDX_40 + i;
				ofst = (s8)((data >> (8 * i)) & 0xff);
				gain->rpl_ofst_80[band][path][rxsc] = ofst;
				BB_DBG(bb, DBG_INIT, "RPL[Band:%d][path=%d][%dM][rxsc=%d]=%d\n",
				       band, path, (20 << bw), rxsc,
				       gain->rpl_ofst_80[band][path][rxsc]);
			}
		}
	}
}

bool halbb_init_cr_default(struct bb_info *bb, bool is_form_folder, u32 folder_len,
		    u32 *folder_array, enum phl_phy_idx phy_idx)
{
	bool result = true;

	if (!bb->bb_cmn_info_init_ready) {
		BB_WARNING("bb_cmn_info_init_ready = false");
		return false;
	}

	if (is_form_folder) {
		if (!folder_array) {
			BB_WARNING("[%s] folder_array=NULL\n", __func__);
			return false;
		}

		if (folder_len == 0) {
			BB_WARNING("[%s] folder_len=0\n", __func__);
			return false;
		}
	}

#ifdef HALBB_DBCC_SUPPORT
	if (phy_idx == HW_PHY_1 && !bb->hal_com->dbcc_en) {
		BB_WARNING("[%s]\n",__func__);
		if (!bb->bb_dbg_i.cr_dbg_mode_en)
			return false;
	}
#endif

	BB_DBG(bb, DBG_INIT, "[%s] ic=%d\n", __func__, bb->hal_com->chip_id);

	switch (bb->ic_type) {

	case BB_RTL8852AA:
	#ifdef BB_8852A_CAV_SUPPORT
		result = halbb_cfg_bbcr_ax_8852a(bb, is_form_folder, folder_len,
						 folder_array, phy_idx);
	#else
		BB_WARNING("[%s] NOT Support 8852AA\n", __func__);
		result = false;
	#endif
		break;

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		result = halbb_cfg_bbcr_ax_8852a_2(bb, is_form_folder, folder_len,
						   folder_array, phy_idx);
		halbb_tpu_mac_cr_init(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		result = halbb_cfg_bbcr_ax_8852b(bb, is_form_folder, folder_len,
						   folder_array, phy_idx);
		halbb_tpu_mac_cr_init(bb, phy_idx);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		result = halbb_cfg_bbcr_ax_8852c(bb, is_form_folder, folder_len,
						   folder_array, phy_idx);
		halbb_tpu_mac_cr_init(bb, phy_idx);
		break;
	#endif

	default:
		BB_WARNING("[%s] ic=%d\n", __func__, bb->hal_com->chip_id);
		break;
	}

	BB_DBG(bb, DBG_INIT, "BB_CR_init_success = %d\n", result);
	return result;
}

bool halbb_init_gain_table(struct bb_info *bb, bool is_form_folder, u32 folder_len,
			   u32 *folder_array, enum phl_phy_idx phy_idx)
{
	bool result = true;

	if (!bb->bb_cmn_info_init_ready) {
		BB_WARNING("bb_cmn_info_init_ready = false");
		return false;
	}

	if (is_form_folder) {
		if (!folder_array) {
			BB_WARNING("[%s] folder_array=NULL\n", __func__);
			return false;
		}

		if (folder_len == 0) {
			BB_WARNING("[%s] folder_len=0\n", __func__);
			return false;
		}
	}

#ifdef HALBB_DBCC_SUPPORT
	if (phy_idx == HW_PHY_1 && !bb->hal_com->dbcc_en) {
		BB_WARNING("[%s]\n",__func__);
		if (!bb->bb_dbg_i.cr_dbg_mode_en)
			return false;
	}
#endif

	BB_DBG(bb, DBG_INIT, "[%s] ic=%d\n", __func__, bb->hal_com->chip_id);

	switch (bb->ic_type) {

	#ifdef BB_8852A_2_SUPPORT
	case BB_RTL8852A:
		result &= halbb_cfg_bb_gain_ax_8852a_2(bb, is_form_folder,
						       folder_len, folder_array);
		break;
	#endif

	#ifdef BB_8852B_SUPPORT
	case BB_RTL8852B:
		result &= halbb_cfg_bb_gain_ax_8852b(bb, is_form_folder,
						     folder_len, folder_array);
		break;
	#endif

	#ifdef BB_8852C_SUPPORT
	case BB_RTL8852C:
		result &= halbb_cfg_bb_gain_ax_8852c(bb, is_form_folder,
						       folder_len, folder_array);
		break;
	#endif

	default:
		BB_WARNING("[%s] ic=%d\n", __func__, bb->hal_com->chip_id);
		break;
	}

	BB_DBG(bb, DBG_INIT, "BB_Gain_table_init_success = %d\n", result);
	return result;
}

bool halbb_init_reg(struct bb_info *bb)
{
	struct rtw_para_info_t *reg = NULL;
	bool rpt_0 = true, rpt_1 = true, rpt_gain = true;

	reg = &bb->phl_com->phy_sw_cap[HW_PHY_0].bb_phy_reg_info;
	rpt_0 = halbb_init_cr_default(bb, reg->para_src, reg->para_data_len, reg->para_data, HW_PHY_0);

	if (bb->hal_com->dbcc_en) {
		reg = &bb->phl_com->phy_sw_cap[HW_PHY_1].bb_phy_reg_info;
		rpt_1 = halbb_init_cr_default(bb, reg->para_src, reg->para_data_len, reg->para_data, HW_PHY_1);	
	}

	reg = &bb->phl_com->phy_sw_cap[HW_PHY_0].bb_phy_reg_gain_info;
	rpt_gain = halbb_init_gain_table(bb, reg->para_src, reg->para_data_len, reg->para_data, HW_PHY_0);

	if (rpt_0 && rpt_1 && rpt_gain)
		return true;
	else
		return false;
}

void halbb_rx_gain_table_dbg(struct bb_info *bb, char input[][16], 
			     u32 *_used, char *output, u32 *_out_len)
{
	struct bb_gain_info *gain = &bb->bb_gain_i;
	u32 val[10] = {0};
	u8 i = 0, j = 0;

	if (_os_strcmp(input[1], "-h") == 0) {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "{show}\n");
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 "set {lna, tia} band path idx val\n");
		return;
	}

	if (_os_strcmp(input[1], "show") == 0) {
		for (i = 0; i < BB_GAIN_BAND_NUM; i++) {
			if (i == 0) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 		"===[2G]===\n");
			} else {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 		"===[5G-%s]===\n", (i == 1) ? ("Low") : ((i == 2) ? "Mid" : "High"));
			}
			for (j = 0; j < HALBB_MAX_PATH; j++) {
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					"LNA_gain[Path=%d] = {%d, %d, %d, %d, %d, %d, %d}\n",
					j,
					gain->lna_gain[i][j][0],
					gain->lna_gain[i][j][1],
					gain->lna_gain[i][j][2],
					gain->lna_gain[i][j][3],
					gain->lna_gain[i][j][4],
					gain->lna_gain[i][j][5],
					gain->lna_gain[i][j][6]);
				BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
					"TIA_gain[Path=%d] = {%d, %d}\n",
					j,
					gain->tia_gain[i][j][0],
					gain->tia_gain[i][j][1]);
			}
		} 
	} else if (_os_strcmp(input[1], "set") == 0) {
		HALBB_SCAN(input[3], DCMD_DECIMAL, &val[0]);
		HALBB_SCAN(input[4], DCMD_DECIMAL, &val[1]);
		HALBB_SCAN(input[5], DCMD_DECIMAL, &val[2]);
		HALBB_SCAN(input[6], DCMD_DECIMAL, &val[3]);
		
		if (_os_strcmp(input[2], "lna") == 0) {
			if (val[0] >= BB_GAIN_BAND_NUM ||
			    val[1] >= HALBB_MAX_PATH ||
			    val[2] >= IC_LNA_NUM) {
			    BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 	"Set Err\n");
			    return;
			}
			gain->lna_gain[val[0]][val[1]][val[2]] = (s8)val[3];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
		 		"Set lna_gain[%d][%d][%d] = %d\n",
		 		val[0], val[1], val[2], val[3]);
			
		} else if (_os_strcmp(input[2], "tia") == 0) {
			if (val[0] >= BB_GAIN_BAND_NUM ||
			    val[1] >= HALBB_MAX_PATH ||
			    val[2] >= IC_TIA_NUM) {
			    BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
			 	"Set Err\n");
			    return;
			}
			gain->tia_gain[val[0]][val[1]][val[2]] = (s8)val[3];
			BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
		 		"Set tia_gain[%d][%d][%d] = %d\n",
		 		val[0], val[1], val[2], val[3]);
		}
		halbb_set_gain_error(bb, bb->hal_com->band[bb->bb_phy_idx].cur_chandef.center_ch);
	} else {
		BB_DBG_CNSL(*_out_len, *_used, output + *_used, *_out_len - *_used,
		 	"Set Err\n");
	}
}
