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
#ifndef __PHL_DBG_CMD_H__
#define __PHL_DBG_CMD_H__

#include "../phl_headers.h"
#include "../phl_types.h"
#include "../phl_struct.h"

#define	PHL_DBG_MON_INFO(max_buff_len, used_len, buff_addr, remain_len, fmt, ...)\
	do {									\
		u32 *used_len_tmp = &(used_len);				\
		if (*used_len_tmp < max_buff_len)				\
			*used_len_tmp += _os_snprintf(buff_addr, remain_len, fmt, ##__VA_ARGS__);\
	} while (0)

struct phl_dbg_cmd_info {
	char name[16];
	u8 id;
};

enum rtw_phl_status
rtw_phl_dbg_core_cmd(struct phl_info_t *phl_info, struct rtw_proc_cmd *incmd, char *output, u32 out_len);

#ifdef CONFIG_PHL_TEST_SUITE

enum PHL_DBG_CMD_ID {
	PHL_DBG_MON_HELP,
	PHL_DBG_MON_TEST,
	PHL_DBG_COMP,
	PHL_DBG_DUMP_WROLE,
	PHL_DBG_SET_CH_BW,
	PHL_DBG_SHOW_RX_RATE,
	PHL_DBG_ASOC_STA,
	PHL_DBG_SOUND,
	#ifdef CONFIG_FSM
	PHL_DBG_FSM,
	#endif
	PHL_DBG_TRX_STATS,
	PHL_SHOW_RSSI_STAT,
	PHL_DBG_SER,
	PHL_DBG_WOW,
#ifdef CONFIG_POWER_SAVE
	PHL_DBG_PS,
#endif
	PHL_DBG_ECSA,
	PHL_DBG_MCC,
	PHL_DBG_LTR,
	PHL_DBG_CFG_TX_DUTY
};

static const struct phl_dbg_cmd_info phl_dbg_cmd_i[] = {
	{"-h", PHL_DBG_MON_HELP}, /*@do not move this element to other position*/
	{"test", PHL_DBG_MON_TEST},
	{"dbgcomp", PHL_DBG_COMP},
	{"role", PHL_DBG_DUMP_WROLE},
	{"set_ch", PHL_DBG_SET_CH_BW},
	{"rxrate", PHL_DBG_SHOW_RX_RATE},
	{"asoc_sta", PHL_DBG_ASOC_STA},
	{"sound", PHL_DBG_SOUND},
	#ifdef CONFIG_FSM
	{"fsm",PHL_DBG_FSM},
	#endif
	{"trx_stats", PHL_DBG_TRX_STATS},
	{"show_rssi", PHL_SHOW_RSSI_STAT},
	{"ser", PHL_DBG_SER},
	{"wow", PHL_DBG_WOW},
#ifdef CONFIG_POWER_SAVE
	{"ps", PHL_DBG_PS},
#endif
	{"ecsa", PHL_DBG_ECSA},
	{"mcc", PHL_DBG_MCC},
	{"ltr", PHL_DBG_LTR},
	{"tx_duty", PHL_DBG_CFG_TX_DUTY}
};

enum rtw_hal_status
rtw_phl_dbg_proc_cmd(struct phl_info_t *phl_info,
		     struct rtw_proc_cmd *incmd,
		     char *output,
		     u32 out_len);
bool
_get_hex_from_string(char *szstr, u32 *val);
#else

#define rtw_phl_dbg_proc_cmd(_phl_info, _incmd, _output, _out_len) RTW_HAL_STATUS_SUCCESS

#endif
#endif
