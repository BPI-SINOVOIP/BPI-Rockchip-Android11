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
#ifndef _PHL_STA_H_
#define _PHL_STA_H_

/*********** macid ctrl section ***********/
enum rtw_phl_status
phl_macid_ctrl_init(struct phl_info_t *phl);

enum rtw_phl_status
phl_macid_ctrl_deinit(struct phl_info_t *phl);

u16 rtw_phl_get_macid_max_num(void *phl);

u16 rtw_phl_wrole_bcmc_id_get(void *phl, struct rtw_wifi_role_t *wrole);

u8 rtw_phl_macid_is_bmc(void *phl, u16 macid);

u8 rtw_phl_macid_is_used(void *phl, u16 macid);

static inline bool
phl_macid_is_valid(struct phl_info_t *phl_info, u16 macid)
{
	return (macid < phl_info->macid_ctrl.max_num) ? true : false;
}
u16 rtw_phl_get_macid_by_addr(void *phl, u8 *addr);

/*********** stainfo_ctrl section ***********/
enum rtw_phl_status
phl_stainfo_ctrl_init(struct phl_info_t *phl_info);

enum rtw_phl_status
phl_stainfo_ctrl_deinie(struct phl_info_t *phl_info);

void phl_clean_sta_bcn_info(struct phl_info_t *phl, struct rtw_phl_stainfo_t *sta);

struct rtw_bcn_offset * phl_get_sta_bcn_offset_info(struct phl_info_t *phl,
					struct rtw_wifi_role_t *wrole);

void phl_bcn_watchdog(struct phl_info_t *phl);

bool phl_calc_offset_from_tbtt(struct phl_info_t *phl,
			struct rtw_wifi_role_t *wrole, u64 cur_t, u16 *ofst);

enum rtw_phl_status rtw_phl_tbtt_sync(struct phl_info_t *phl,
		struct rtw_wifi_role_t *src_role,
		struct rtw_wifi_role_t *tgt_role,
		u16 sync_ofst, bool sync_now_once, u16 *diff_t);

#ifdef DBG_PHL_STAINFO
void phl_dump_stactrl(const char *caller,
                      const int line,
                      bool show_caller,
                      struct phl_info_t *phl_info);
#define PHL_DUMP_STACTRL(_phl_info) phl_dump_stactrl(__FUNCTION__, __LINE__, false, _phl_info);
#define PHL_DUMP_STACTRL_EX(_phl_info) phl_dump_stactrl(__FUNCTION__, __LINE__, true, _phl_info);

void phl_dump_stainfo_all(const char *caller,
                          const int line, bool show_caller,
                          struct phl_info_t *phl_info);
#define PHL_DUMP_STAINFO(_phl_info) phl_dump_stainfo_all(__FUNCTION__, __LINE__, false, _phl_info);
#define PHL_DUMP_STAINFO_EX(_phl_info) phl_dump_stainfo_all(__FUNCTION__, __LINE__, true, _phl_info);

void phl_dump_stainfo_per_role(const char *caller,
                               const int line,
                               bool show_caller,
                               struct phl_info_t *phl_info,
                               struct rtw_wifi_role_t *wrole);
#define PHL_DUMP_ROLE_STAINFO(_phl_info, wrole) phl_dump_stainfo_per_role(__FUNCTION__, __LINE__, false, _phl_info, wrole);
#define PHL_DUMP_ROLE_STAINFO_EX(_phl_info, wrole) phl_dump_stainfo_per_role(__FUNCTION__, __LINE__, true, _phl_info, wrole);
#else
#define PHL_DUMP_STACTRL(_phl_info)
#define PHL_DUMP_STACTRL_EX(_phl_info)

#define PHL_DUMP_STAINFO(_phl_info)
#define PHL_DUMP_STAINFO_EX(_phl_info)

#define PHL_DUMP_ROLE_STAINFO(_phl_info, wrole)
#define PHL_DUMP_ROLE_STAINFO_EX(_phl_info, wrole)
#endif
/*********** phl stainfo section ***********/
/*WIFI sta_info management section*/
struct rtw_phl_stainfo_t *
phl_alloc_stainfo_sw(struct phl_info_t *phl_info,
                     u8 *sta_addr,
                     struct rtw_wifi_role_t *wrole);

enum rtw_phl_status
phl_alloc_stainfo_hw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta);

enum rtw_phl_status
phl_free_stainfo_sw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta);

enum rtw_phl_status
phl_free_stainfo_hw(struct phl_info_t *phl_info, struct rtw_phl_stainfo_t *sta);

enum rtw_phl_status
rtw_phl_cmd_alloc_stainfo(void *phl,
						struct rtw_phl_stainfo_t **sta,
						u8 *sta_addr,
						struct rtw_wifi_role_t *wrole,
						bool alloc, bool only_hw,
						enum phl_cmd_type cmd_type,
						u32 cmd_timeout);

enum rtw_phl_status
phl_wifi_role_free_stainfo_hw(struct phl_info_t *phl_info,
						struct rtw_wifi_role_t *wrole);
enum rtw_phl_status
phl_wifi_role_free_stainfo_sw(struct phl_info_t *phl_info,
				struct rtw_wifi_role_t *role);
enum rtw_phl_status
phl_wifi_role_free_stainfo(struct phl_info_t *phl_info,
                           struct rtw_wifi_role_t *role);

#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
phl_update_media_status_hdl(struct phl_info_t *phl_info, u8 *param);
#endif

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_macid(void *phl, u16 macid);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_addr(void *phl, struct rtw_wifi_role_t *wrole, u8 *addr);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_by_addr_ex(void *phl, u8 *addr);

struct rtw_phl_stainfo_t *
rtw_phl_get_stainfo_self(void *phl, struct rtw_wifi_role_t *wrole);

u8 phl_get_min_rssi_bcn(struct phl_info_t *phl_info);

#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
phl_cmd_change_stainfo_hdl(struct phl_info_t *phl_info, u8 *param);
#endif

enum rtw_phl_status
phl_change_stainfo(struct phl_info_t *phl_info,
                   struct rtw_phl_stainfo_t *sta,
                   enum phl_upd_mode mode);
void
phl_sta_trx_tfc_upd(struct phl_info_t *phl_info);

#ifdef CONFIG_CMD_DISP
enum rtw_phl_status
phl_cmd_set_key_hdl(struct phl_info_t *phl_info, u8 *param);
enum rtw_phl_status
phl_cmd_alloc_stainfo_hdl(struct phl_info_t *phl_info, u8 *param);
#endif /* CONFIG_CMD_DISP */

bool phl_self_stainfo_chk(struct phl_info_t *phl_info,
                          struct rtw_wifi_role_t *wrole,
                          struct rtw_phl_stainfo_t *sta);

#endif  /*_PHL_STA_H_*/
