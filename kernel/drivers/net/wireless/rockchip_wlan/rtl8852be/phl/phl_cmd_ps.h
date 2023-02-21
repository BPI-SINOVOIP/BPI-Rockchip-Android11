/******************************************************************************
 *
 * Copyright(c) 2021 Realtek Corporation.
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
#ifndef _PHL_CMD_PS_H_
#define _PHL_CMD_PS_H_
#ifdef CONFIG_POWER_SAVE
enum ps_mdl_opcode {
	PS_MDL_OP_NONE = 0,
	PS_MDL_OP_CANCEL_PWR_REQ,
	PS_MDL_OP_CUR_PWR_LVL,
	PS_MDL_OP_BASIC_INFO,
	PS_MDL_OP_BTC_PWR_REQ
};

#define MAX_CMD_PS_RSON_LENGTH 30
struct phl_cmd_ps_basic_info {
	u8 ps_mode;
	u8 cur_pwr_lvl;
	bool rej_pwr_req;
	bool btc_req_pwr;
	enum phl_ps_rt_rson rt_stop_rson;
	bool ap_active;
	bool gc_active;
	struct rtw_phl_stainfo_t *sta;
	char enter_rson[MAX_CMD_PS_RSON_LENGTH];
	char leave_rson[MAX_CMD_PS_RSON_LENGTH];
};

enum rtw_phl_status phl_register_ps_module(struct phl_info_t *phl_info);
u8 phl_ps_get_cur_pwr_lvl(struct phl_info_t *phl_info);
bool phl_ps_is_datapath_allowed(struct phl_info_t *phl_info);
void phl_ps_tx_pkt_ntfy(struct phl_info_t *phl_info);
void rtw_phl_ps_set_rt_cap(void *phl, u8 band_idx, bool ps_allow, enum phl_ps_rt_rson rt_rson);
void phl_ps_dbg_set_ps(struct phl_info_t *phl_info, u8 ps_mode, bool enter);

#endif
#endif /* _PHL_CMD_PS_H_ */
