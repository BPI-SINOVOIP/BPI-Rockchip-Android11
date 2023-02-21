/** @file */
/******************************************************************************
 *
 * Copyright(c) 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#ifndef _MAC_AX_POWER_SAVING_H_
#define _MAC_AX_POWER_SAVING_H_

#include "../type.h"
#include "fwcmd.h"
#include "role.h"

#define	MACID_GRP_SH	5
#define	MACID_GRP_MASK	0x1F
#define PORT_SH		4

/**
 * @enum last_rpwm_mode
 *
 * @brief last_rpwm_mode
 *
 * @var last_rpwm_mode::LAST_RPWM_PS
 * Please Place Description here.
 * @var last_rpwm_mode::LAST_RPWM_ACTIVE
 * Please Place Description here.
 */
enum last_rpwm_mode {
	LAST_RPWM_PS        = 0x0,
	LAST_RPWM_ACTIVE    = 0x6,
};

/**
 * @struct lps_parm
 * @brief lps_parm
 *
 * @var lps_parm::macid
 * Please Place Description here.
 * @var lps_parm::psmode
 * Please Place Description here.
 * @var lps_parm::rlbm
 * Please Place Description here.
 * @var lps_parm::smartps
 * Please Place Description here.
 * @var lps_parm::awakeinterval
 * Please Place Description here.
 * @var lps_parm::vouapsd
 * Please Place Description here.
 * @var lps_parm::viuapsd
 * Please Place Description here.
 * @var lps_parm::beuapsd
 * Please Place Description here.
 * @var lps_parm::bkuapsd
 * Please Place Description here.
 * @var lps_parm::rsvd
 * Please Place Description here.
 * @var lps_parm::lastrpwm
 * Please Place Description here.
 * @var lps_parm::rsvd1
 * Please Place Description here.
 */
struct lps_parm {
	u32 macid:8;
	u32 psmode:8;
	u32 rlbm:4;
	u32 smartps:4;
	u32 awakeinterval:8;
	u32 vouapsd:1;
	u32 viuapsd:1;
	u32 beuapsd:1;
	u32 bkuapsd:1;
	u32 rsvd:4;
	u32 lastrpwm:8;
	u32 rsvd1:16;
};

/**
 * @struct ps_rpwm_parm
 * @brief ps_rpwm_parm
 *
 * @var ps_rpwm_parm::req_pwr_state
 * Please Place Description here.
 * @var ps_rpwm_parm::notify_wake
 * Please Place Description here.
 * @var ps_rpwm_parm::rsvd0
 */
struct ps_rpwm_parm {
	enum mac_ax_rpwm_req_pwr_state req_pwr_state;
	u32 notify_wake:1;
	u32 rsvd0:31;
};

/**
 * @macid_grp_list
 *
 * @brief macid_grp_list
 *
 * @var macid_grp_list::MACID_GRP_0
 * Please Place Description here.
 * @var macid_grp_list::MACID_GRP_1
 * Please Place Description here.
 * @var macid_grp_list::MACID_GRP_2
 * Please Place Description here.
 * @var macid_grp_list::MACID_GRP_3
 * Please Place Description here.
 */
enum macid_grp_list {
	MACID_GRP_0	= 0,
	MACID_GRP_1	= 1,
	MACID_GRP_2	= 2,
	MACID_GRP_3	= 3,
};

/**
 * @struct ips_cfg
 * @brief ips_cfg
 *
 * @var ips_cfg::macid
 * Please Place Description here.
 * @var ips_cfg::enable
 * Please Place Description here.
 * @var ips_cfg::rsvd0
 * Please Place Description here.
 */
struct ips_cfg {
	u32 macid:8;
	u32 enable:1;
	u32 rsvd0:23;
};

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief mac_cfg_lps
 *
 * @param *adapter
 * @param macid
 * @param ps_mode
 * @param *lps_info
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_cfg_lps(struct mac_ax_adapter *adapter, u8 macid,
		enum mac_ax_ps_mode ps_mode, struct mac_ax_lps_info *lps_info);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief mac_ps_pwr_state
 *
 * @param *adapter
 * @param action
 * @param req_pwr_state
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_ps_pwr_state(struct mac_ax_adapter *adapter,
		     enum mac_ax_pwr_state_action action,
		     enum mac_ax_rpwm_req_pwr_state req_pwr_state);

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief mac_chk_leave_lps
 *
 * @param *adapter
 * @param macid
 * @return Please Place Description here.
 * @retval u32
 */

u32 mac_chk_leave_lps(struct mac_ax_adapter *adapter, u8 macid);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief _is_in_lps
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u8
 */

u8 _is_in_lps(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief reset_lps_seq_num
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval void
 */
void reset_lps_seq_num(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup IPS
 * @{
 */

/**
 * @brief mac_cfg_ips
 *
 * @param *adapter
 * @param macid
 * @param enable
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_cfg_ips(struct mac_ax_adapter *adapter, u8 macid, u8 enable);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup IPS
 * @{
 */

/**
 * @brief mac_chk_leave_ips
 *
 * @param *adapter
 * @param macid
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_chk_leave_ips(struct mac_ax_adapter *adapter, u8 macid);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup IPS
 * @{
 */

/**
 * @brief _is_in_ips
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u8
 */
u8 _is_in_ips(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief mac_ps_notify_wake
 *
 * @param *adapter
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_ps_notify_wake(struct mac_ax_adapter *adapter);
/**
 * @}
 * @}
 */

/**
 * @addtogroup PowerSaving
 * @{
 * @addtogroup LPS
 * @{
 */

/**
 * @brief mac_cfg_ps_advance_parm
 *
 * @param *adapter
 * @param parm
 * @return Please Place Description here.
 * @retval u32
 */
u32 mac_cfg_ps_advance_parm(struct mac_ax_adapter *adapter,
			    struct mac_ax_ps_adv_parm *parm);
/**
 * @}
 * @}
 */

#endif // #define _MAC_AX_POWER_SAVING_H_

