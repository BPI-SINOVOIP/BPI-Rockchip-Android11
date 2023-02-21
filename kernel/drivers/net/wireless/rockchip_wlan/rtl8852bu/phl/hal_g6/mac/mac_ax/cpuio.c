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
#include "cpuio.h"

#define MAX_MACID_NUM		256

static u32 band_pkt_drop(struct mac_ax_adapter *adapter,
			 struct mac_ax_pkt_drop_info *info, u8 once);
static u32 hw_link_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info);
static u32 macid_pkt_drop_all(struct mac_ax_adapter *adapter, u8 macid);
static u32 ac_pkt_drop(struct mac_ax_adapter *adapter, u8 macid,
		       enum pkt_drop_ac ac);
static u32 deq_enq_to_tail(struct mac_ax_adapter *adapter,
			   struct deq_enq_info *info);
static u32 get_1st_pktid(struct mac_ax_adapter *adapter,
			 struct first_pid_info *info);
static u32 get_next_pktid(struct mac_ax_adapter *adapter,
			  struct next_pid_info *info);
static void set_dmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid);
static void set_cmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid);
static void rel_dmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid);
static void rel_cmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid);
static u32 set_hiq_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info);
static u32 rel_hiq_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info);
static void ss_hw_len_udn_clr(struct mac_ax_adapter *adapter);
static u32 hiq_link_drop(struct mac_ax_adapter *adapter, u8 band);
static u32 mg0_link_drop(struct mac_ax_adapter *adapter, u8 band);

u32 mac_dle_buf_req_wd(struct mac_ax_adapter *adapter,
		       struct cpuio_buf_req_t *buf_req_p)
{
	u32 val32, timeout;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, buf_req_p->len, B_AX_WD_BUF_REQ_LEN);
	val32 |= B_AX_WD_BUF_REQ_EXEC;
	MAC_REG_W32(R_AX_WD_BUF_REQ, val32);

	timeout = DLE_BUF_REQ_DLY_CNT;
	while (timeout--) {
		val32 = MAC_REG_R32(R_AX_WD_BUF_STATUS);
		if (val32 & B_AX_WD_BUF_STAT_DONE)
			break;
		PLTFM_DELAY_US(DLE_BUF_REQ_DLY_US);
	}

	if (!++timeout) {
		PLTFM_MSG_ERR("[ERR]dle buf req wd %d timeout\n",
			      buf_req_p->len);
		return MACPOLLTO;
	}

	buf_req_p->pktid = GET_FIELD(val32, B_AX_WD_BUF_STAT_PKTID);

	if (buf_req_p->pktid == 0xfff) {
		PLTFM_MSG_ERR("[ERR]dle buf req wd %d no pktid\n",
			      buf_req_p->len);
		return MACNOBUF;
	}

	return MACSUCCESS;
}

u32 mac_dle_buf_req_pl(struct mac_ax_adapter *adapter,
		       struct cpuio_buf_req_t *buf_req_p)
{
	u32 val32, timeout;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = 0;
	val32 = SET_CLR_WORD(val32, buf_req_p->len, B_AX_PL_BUF_REQ_LEN);
	val32 |= B_AX_PL_BUF_REQ_EXEC;
	MAC_REG_W32(R_AX_PL_BUF_REQ, val32);

	timeout = DLE_BUF_REQ_DLY_CNT;
	while (timeout--) {
		val32 = MAC_REG_R32(R_AX_PL_BUF_STATUS);
		if (val32 & B_AX_PL_BUF_STAT_DONE)
			break;
		PLTFM_DELAY_US(DLE_BUF_REQ_DLY_US);
	}

	if (!++timeout) {
		PLTFM_MSG_ERR("[ERR]dle buf req pl %d timeout\n",
			      buf_req_p->len);
		return MACPOLLTO;
	}

	buf_req_p->pktid = GET_FIELD(val32, B_AX_PL_BUF_STAT_PKTID);

	if (buf_req_p->pktid == 0xfff) {
		PLTFM_MSG_ERR("[ERR]dle buf req pl %d no pktid\n",
			      buf_req_p->len);
		return MACNOBUF;
	}

	return MACSUCCESS;
}

u32 mac_set_cpuio_wd(struct mac_ax_adapter *adapter,
		     struct cpuio_ctrl_t *ctrl_para_p)
{
	u32 val_op0, val_op1, val_op2;
	u32 val32, cmd_type, timeout;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	cmd_type = ctrl_para_p->cmd_type;

	val_op2 = 0;
	val_op2 = SET_CLR_WORD(val_op2, ctrl_para_p->start_pktid,
			       B_AX_WD_CPUQ_OP_STRT_PKTID);
	val_op2 = SET_CLR_WORD(val_op2, ctrl_para_p->end_pktid,
			       B_AX_WD_CPUQ_OP_END_PKTID);
	MAC_REG_W32(R_AX_WD_CPUQ_OP_2, val_op2);

	val_op1 = 0;
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->src_pid,
			       B_AX_WD_CPUQ_OP_SRC_PID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->src_qid,
			       B_AX_WD_CPUQ_OP_SRC_QID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->dst_pid,
			       B_AX_WD_CPUQ_OP_DST_PID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->dst_qid,
			       B_AX_WD_CPUQ_OP_DST_QID);
	MAC_REG_W32(R_AX_WD_CPUQ_OP_1, val_op1);

	val_op0 = 0;
	val_op0 = SET_CLR_WORD(val_op0, cmd_type,
			       B_AX_WD_CPUQ_OP_CMD_TYPE);
	val_op0 = SET_CLR_WORD(val_op0, ctrl_para_p->macid,
			       B_AX_WD_CPUQ_OP_MACID);
	val_op0 = SET_CLR_WORD(val_op0, ctrl_para_p->pkt_num,
			       B_AX_WD_CPUQ_OP_PKTNUM);
	val_op0 |= B_AX_WD_CPUQ_OP_EXEC;
	MAC_REG_W32(R_AX_WD_CPUQ_OP_0, val_op0);

	timeout = SET_CPUIO_DLY_CNT;
	do {
		val32 = MAC_REG_R32(R_AX_WD_CPUQ_OP_STATUS);
		if (val32 & B_AX_WD_CPUQ_OP_STAT_DONE)
			break;
		PLTFM_DELAY_US(SET_CPUIO_DLY_US);
		timeout--;
	} while (timeout);

	if (!timeout) {
		PLTFM_MSG_ERR("[ERR]set cpuio wd timeout\n");
		PLTFM_MSG_ERR("[ERR]op_0=0x%X, op_1=0x%X, op_2=0x%X\n",
			      val_op0, val_op1, val_op2);
		return MACPOLLTO;
	}

	if (cmd_type == CPUIO_OP_CMD_GET_1ST_PID ||
	    cmd_type == CPUIO_OP_CMD_GET_NEXT_PID)
		ctrl_para_p->pktid = GET_FIELD(val32, B_AX_WD_CPUQ_OP_PKTID);

	return MACSUCCESS;
}

u32 mac_set_cpuio_pl(struct mac_ax_adapter *adapter,
		     struct cpuio_ctrl_t *ctrl_para_p)
{
	u32 val_op0, val_op1, val_op2;
	u32 val32, cmd_type, timeout;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	cmd_type = ctrl_para_p->cmd_type;

	val_op2 = 0;
	val_op2 = SET_CLR_WORD(val_op2, ctrl_para_p->start_pktid,
			       B_AX_PL_CPUQ_OP_STRT_PKTID);
	val_op2 = SET_CLR_WORD(val_op2, ctrl_para_p->end_pktid,
			       B_AX_PL_CPUQ_OP_END_PKTID);
	MAC_REG_W32(R_AX_PL_CPUQ_OP_2, val_op2);

	val_op1 = 0;
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->src_pid,
			       B_AX_PL_CPUQ_OP_SRC_PID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->src_qid,
			       B_AX_PL_CPUQ_OP_SRC_QID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->dst_pid,
			       B_AX_PL_CPUQ_OP_DST_PID);
	val_op1 = SET_CLR_WORD(val_op1, ctrl_para_p->dst_qid,
			       B_AX_PL_CPUQ_OP_DST_QID);
	MAC_REG_W32(R_AX_PL_CPUQ_OP_1, val_op1);

	val_op0 = 0;
	val_op0 = SET_CLR_WORD(val_op0, cmd_type,
			       B_AX_PL_CPUQ_OP_CMD_TYPE);
	val_op0 = SET_CLR_WORD(val_op0, ctrl_para_p->macid,
			       B_AX_PL_CPUQ_OP_MACID);
	val_op0 = SET_CLR_WORD(val_op0, ctrl_para_p->pkt_num,
			       B_AX_PL_CPUQ_OP_PKTNUM);
	val_op0 |= B_AX_PL_CPUQ_OP_EXEC;
	MAC_REG_W32(R_AX_PL_CPUQ_OP_0, val_op0);

	timeout = SET_CPUIO_DLY_CNT;
	do {
		val32 = MAC_REG_R32(R_AX_PL_CPUQ_OP_STATUS);
		if (val32 & B_AX_PL_CPUQ_OP_STAT_DONE)
			break;
		PLTFM_DELAY_US(SET_CPUIO_DLY_US);
		timeout--;
	} while (timeout);

	if (!timeout) {
		PLTFM_MSG_ERR("[ERR]set cpuio pl timeout\n");
		PLTFM_MSG_ERR("[ERR]op_0=0x%X, op_1=0x%X, op_2=0x%X\n",
			      val_op0, val_op1, val_op2);
		return MACPOLLTO;
	}

	if (cmd_type == CPUIO_OP_CMD_GET_1ST_PID ||
	    cmd_type == CPUIO_OP_CMD_GET_NEXT_PID)
		ctrl_para_p->pktid = GET_FIELD(val32, B_AX_PL_CPUQ_OP_PKTID);

	return MACSUCCESS;
}

u32 mac_wde_pkt_drop(struct mac_ax_adapter *adapter,
		     struct mac_ax_pkt_drop_info *info)
{
	u32 ret;

	switch (info->sel) {
	case MAC_AX_PKT_DROP_SEL_MACID_BE_ONCE:
		ret = ac_pkt_drop(adapter, info->macid, PKT_DROP_BE);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_MACID_BK_ONCE:
		ret = ac_pkt_drop(adapter, info->macid, PKT_DROP_BK);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_MACID_VI_ONCE:
		ret = ac_pkt_drop(adapter, info->macid, PKT_DROP_VI);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_MACID_VO_ONCE:
		ret = ac_pkt_drop(adapter, info->macid, PKT_DROP_VO);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_MACID_ALL:
		ret = macid_pkt_drop_all(adapter, info->macid);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_BAND_ONCE:
		ret = band_pkt_drop(adapter, info, 1);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_BAND:
		ret = band_pkt_drop(adapter, info, 0);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_REL_MACID:
		rel_dmac_macid_drop(adapter, info->macid);
		rel_cmac_macid_drop(adapter, info->macid);
		break;
	case MAC_AX_PKT_DROP_SEL_REL_HIQ_PORT:
	case MAC_AX_PKT_DROP_SEL_REL_HIQ_MBSSID:
		ret = rel_hiq_drop(adapter, info);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_HIQ_ONCE:
	case MAC_AX_PKT_DROP_SEL_MG0_ONCE:
		ret = hw_link_drop(adapter, info);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_HIQ_PORT:
	case MAC_AX_PKT_DROP_SEL_HIQ_MBSSID:
		ret = set_hiq_drop(adapter, info);
		if (ret != MACSUCCESS)
			return ret;
		break;
	default:
		return MACNOITEM;
	}

	return MACSUCCESS;
}

static u32 band_pkt_drop(struct mac_ax_adapter *adapter,
			 struct mac_ax_pkt_drop_info *info, u8 once)
{
	u32 ret;
	u32 qid;
	u8 band = info->band;
	u8 macid_grp_num = MAX_MACID_NUM >> 5;
	u32 macid_band_sel[MAX_MACID_NUM >> 5] = {0};
	u32 macid_ctrl_sel[MAX_MACID_NUM >> 5] = {0};
	u32 macid_pause_bak[MAX_MACID_NUM >> 5] = {0};
	u32 macid_sleep_bak[MAX_MACID_NUM >> 5] = {0};
	u8 i_u8;
	u16 i_u16;
	u8 macid_grp;
	u8 macid_sh;
	u16 macid_num = adapter->hw_info->macid_num;
	u8 dmac_macid_drop = 0;
	struct mac_role_tbl *role;
	struct deq_enq_info q_info;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);
	struct mac_ax_sch_tx_en_cfg sch_backup;
	struct mac_ax_tb_ppdu_ctrl ac_dis_bak;

	ret = check_mac_en(adapter, band, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	PLTFM_MEMSET(&q_info, 0, sizeof(struct deq_enq_info));

	for (i_u16 = 0; i_u16 < macid_num; i_u16++) {
		role = mac_role_srch(adapter, (u8)i_u16);
		if (role) {
			macid_sh = i_u16 & (32 - 1);
			macid_grp = i_u16 >> 5;
			if (role->info.band)
				macid_band_sel[macid_grp] |= BIT(macid_sh);
			else
				macid_band_sel[macid_grp] &= ~(BIT(macid_sh));
		}
	}

	if (band == MAC_AX_BAND_1) {
		for (i_u8 = 0; i_u8 < macid_grp_num; i_u8++)
			macid_ctrl_sel[i_u8] = macid_band_sel[i_u8];
	} else {
		for (i_u8 = 0; i_u8 < macid_grp_num; i_u8++)
			macid_ctrl_sel[i_u8] = ~macid_band_sel[i_u8];
	}

	/* TBD, pause H2C in val, mask format, Soar, 20200109 */

	macid_sleep_bak[0] = MAC_REG_R32(R_AX_MACID_SLEEP_0);
	MAC_REG_W32(R_AX_MACID_SLEEP_0, macid_sleep_bak[0] | macid_ctrl_sel[0]);
	macid_pause_bak[0] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_0);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_0, macid_pause_bak[0] |
		    macid_ctrl_sel[0]);
	macid_sleep_bak[1] = MAC_REG_R32(R_AX_MACID_SLEEP_1);
	MAC_REG_W32(R_AX_MACID_SLEEP_1, macid_sleep_bak[1] | macid_ctrl_sel[1]);
	macid_pause_bak[1] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_1);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_1, macid_pause_bak[1] |
		    macid_ctrl_sel[1]);
	macid_sleep_bak[2] = MAC_REG_R32(R_AX_MACID_SLEEP_2);
	MAC_REG_W32(R_AX_MACID_SLEEP_2, macid_sleep_bak[2] | macid_ctrl_sel[2]);
	macid_pause_bak[2] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_2);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_2, macid_pause_bak[2] |
		    macid_ctrl_sel[2]);
	macid_sleep_bak[3] = MAC_REG_R32(R_AX_MACID_SLEEP_3);
	MAC_REG_W32(R_AX_MACID_SLEEP_3, macid_sleep_bak[3] | macid_ctrl_sel[3]);
	macid_pause_bak[3] = MAC_REG_R32(R_AX_SS_MACID_PAUSE_3);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_3, macid_pause_bak[3] |
		    macid_ctrl_sel[3]);

	sch_backup.band = band;
	ret = stop_sch_tx(adapter, SCH_TX_SEL_ALL, &sch_backup);
	if (ret != MACSUCCESS)
		return ret;

	ac_dis_bak.band = band;
	ret = stop_ac_tb_tx(adapter, TB_STOP_SEL_ALL, &ac_dis_bak);
	if (ret != MACSUCCESS)
		return ret;

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV)) {
			PLTFM_MSG_TRACE("[TRACE]:do not support DMAC drop\n");
			dmac_macid_drop = 0;
		} else {
			dmac_macid_drop = 1;
		}
	} else {
		dmac_macid_drop = 1;
	}

	if (once == 0 && dmac_macid_drop == 1) {
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_0,
			    MAC_REG_R32(R_AX_DMAC_MACID_DROP_0) |
			    macid_ctrl_sel[0]);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_1,
			    MAC_REG_R32(R_AX_DMAC_MACID_DROP_1) |
			    macid_ctrl_sel[1]);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_2,
			    MAC_REG_R32(R_AX_DMAC_MACID_DROP_2) |
			    macid_ctrl_sel[2]);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_3,
			    MAC_REG_R32(R_AX_DMAC_MACID_DROP_3) |
			    macid_ctrl_sel[3]);
	}

	ret = tx_idle_poll_band(adapter, band, 0);
	if (ret != MACSUCCESS)
		return ret;

	q_info.dst_pid = WDE_DLE_PID_WDRLS;
	q_info.dst_qid = WDE_DLE_QID_WDRLS_DROP;
	q_info.src_pid =
		(band == MAC_AX_BAND_1) ? WDE_DLE_PID_C1 : WDE_DLE_PID_C0;

	/* need to modify for 8852C, soar */
	if (band == MAC_AX_BAND_1) {
		for (qid = WDE_DLE_QID_BCN_C1; qid <= WDE_DLE_QID_MG2_C1;
		     qid++) {
			if (qid == WDE_DLE_QID_HI_C1) {
				ret = hiq_link_drop(adapter, band);
				if (ret != MACSUCCESS)
					return ret;
			} else {
				q_info.src_qid = (u8)qid;
				ret = deq_enq_all(adapter, &q_info);
				if (ret != MACSUCCESS)
					return ret;
			}
		}
	} else {
		for (qid = WDE_DLE_QID_BCN_C0; qid <= WDE_DLE_QID_MG2_C0;
		     qid++) {
			if (qid == WDE_DLE_QID_HI_C0) {
				ret = hiq_link_drop(adapter, band);
				if (ret != MACSUCCESS)
					return ret;
			} else {
				q_info.src_qid = (u8)qid;
				ret = deq_enq_all(adapter, &q_info);
				if (ret != MACSUCCESS)
					return ret;
			}
		}
	}

	for (i_u16 = 0; i_u16 < macid_num; i_u16++) {
		macid_grp = i_u16 >> 5;
		if (macid_ctrl_sel[macid_grp] & BIT(i_u16 & 31)) {
			q_info.macid = (u8)i_u16;
			q_info.src_pid = WDE_DLE_PID_C0;
			q_info.dst_pid = WDE_DLE_PID_WDRLS;
			q_info.dst_qid = WDE_DLE_QID_WDRLS_DROP;

			for (qid = WDE_DLE_QID_BE;
			     qid <= WDE_DLE_QID_VO; qid++) {
				q_info.src_qid = (u8)qid;
				ret = deq_enq_all(adapter, &q_info);
				if (ret != MACSUCCESS)
					return ret;
			}
		}
	}

	PLTFM_DELAY_MS(1);
	ss_hw_len_udn_clr(adapter);

	/* TBD, pause H2C in val, mask format, Soar, 20200109 */

	MAC_REG_W32(R_AX_MACID_SLEEP_0, macid_sleep_bak[0]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_0, macid_pause_bak[0]);
	MAC_REG_W32(R_AX_MACID_SLEEP_1, macid_sleep_bak[1]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_1, macid_pause_bak[1]);
	MAC_REG_W32(R_AX_MACID_SLEEP_2, macid_sleep_bak[2]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_2, macid_pause_bak[2]);
	MAC_REG_W32(R_AX_MACID_SLEEP_3, macid_sleep_bak[3]);
	MAC_REG_W32(R_AX_SS_MACID_PAUSE_3, macid_pause_bak[3]);

	ret = resume_sch_tx(adapter, &sch_backup);
	if (ret != MACSUCCESS)
		return ret;

	ret = set_hw_tb_ppdu_ctrl(adapter, &ac_dis_bak);
	if (ret != MACSUCCESS)
		return ret;

	return ret;
}

static u32 hw_link_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info)
{
	u32 ret;
	u8 band = info->band;
	struct mac_ax_sch_tx_en_cfg sch_backup;
	enum ptcl_tx_sel ptcl_sel;
	enum sch_tx_sel sch_sel;

	ret = check_mac_en(adapter, band, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	switch (info->sel) {
	case MAC_AX_PKT_DROP_SEL_MG0_ONCE:
		ptcl_sel = PTCL_TX_SEL_MG0;
		sch_sel = SCH_TX_SEL_MG0;
		break;
	case MAC_AX_PKT_DROP_SEL_HIQ_ONCE:
		ptcl_sel = PTCL_TX_SEL_HIQ;
		sch_sel = SCH_TX_SEL_HIQ;
		break;
	default:
		return MACNOITEM;
	}

	sch_backup.band = band;
	ret = stop_sch_tx(adapter, sch_sel, &sch_backup);
	if (ret != MACSUCCESS)
		return ret;

	ret = tx_idle_poll_sel(adapter, ptcl_sel, band);
	if (ret != MACSUCCESS)
		return ret;

	switch (info->sel) {
	case MAC_AX_PKT_DROP_SEL_MG0_ONCE:
		ret = mg0_link_drop(adapter, band);
		if (ret != MACSUCCESS)
			return ret;
		break;
	case MAC_AX_PKT_DROP_SEL_HIQ_ONCE:
		ret = hiq_link_drop(adapter, band);
		if (ret != MACSUCCESS)
			return ret;
		break;
	default:
		return MACNOITEM;
	}

	ret = resume_sch_tx(adapter, &sch_backup);
	if (ret != MACSUCCESS)
		return ret;

	return ret;
}

static u32 macid_pkt_drop_all(struct mac_ax_adapter *adapter, u8 macid)
{
	u32 ret;
	u32 qid;
	struct deq_enq_info info;
	struct macid_tx_bak bak;
	struct mac_role_tbl *role;

	PLTFM_MEMSET(&info, 0, sizeof(struct deq_enq_info));

	role = mac_role_srch(adapter, macid);
	if (!role) {
		PLTFM_MSG_ERR("[ERR]:role info is null\n");
		return MACNOITEM;
	}

	ret = stop_macid_tx(adapter, role, TB_STOP_SEL_ALL, &bak);
	if (ret != MACSUCCESS)
		return ret;

	set_dmac_macid_drop(adapter, macid);
	set_cmac_macid_drop(adapter, macid);

	ret = tx_idle_poll_macid(adapter, role);
	if (ret != MACSUCCESS)
		return ret;

	info.macid = macid;
	info.src_pid = WDE_DLE_PID_C0;
	info.dst_pid = WDE_DLE_PID_WDRLS;
	info.dst_qid = WDE_DLE_QID_WDRLS_DROP;

	for (qid = WDE_DLE_QID_BE; qid <= WDE_DLE_QID_VO; qid++) {
		info.src_qid = (u8)qid;
		ret = deq_enq_all(adapter, &info);
		if (ret != MACSUCCESS)
			return ret;
	}

	ret = resume_macid_tx(adapter, role, &bak);
	if (ret != MACSUCCESS)
		return ret;

	return ret;
}

static u32 ac_pkt_drop(struct mac_ax_adapter *adapter, u8 macid,
		       enum pkt_drop_ac ac)
{
	u32 ret;
	struct deq_enq_info info;
	struct mac_role_tbl *role;
	enum tb_stop_sel stop_sel;
	struct macid_tx_bak bak;

	PLTFM_MEMSET(&info, 0, sizeof(struct deq_enq_info));

	role = mac_role_srch(adapter, macid);
	if (!role) {
		PLTFM_MSG_ERR("[ERR]:role info is null\n");
		return MACNOITEM;
	}

	switch (ac) {
	case PKT_DROP_BE:
		stop_sel = TB_STOP_SEL_BE;
		break;
	case PKT_DROP_BK:
		stop_sel = TB_STOP_SEL_BK;
		break;
	case PKT_DROP_VI:
		stop_sel = TB_STOP_SEL_VI;
		break;
	case PKT_DROP_VO:
		stop_sel = TB_STOP_SEL_VO;
		break;
	default:
		return MACNOITEM;
	}

	ret = stop_macid_tx(adapter, role, stop_sel, &bak);
	if (ret != MACSUCCESS)
		return ret;

	ret = tx_idle_poll_macid(adapter, role);
	if (ret != MACSUCCESS)
		return ret;

	info.macid = macid;
	info.src_pid = WDE_DLE_PID_C0;
	info.src_qid = ac;
	info.dst_pid = WDE_DLE_PID_WDRLS;
	info.dst_qid = WDE_DLE_QID_WDRLS_DROP;

	ret = deq_enq_all(adapter, &info);
	if (ret != MACSUCCESS)
		return ret;

	ret = resume_macid_tx(adapter, role, &bak);
	if (ret != MACSUCCESS)
		return ret;

	return ret;
}

static u32 deq_enq_to_tail(struct mac_ax_adapter *adapter,
			   struct deq_enq_info *info)
{
	struct cpuio_ctrl_t cpuio;
	u32 ret;

	PLTFM_MEMSET(&cpuio, 0, sizeof(struct cpuio_ctrl_t));

	cpuio.cmd_type = CPUIO_OP_CMD_DEQ_ENQ_TO_TAIL;
	cpuio.macid = info->macid;
	cpuio.pkt_num = 0;
	cpuio.src_pid = info->src_pid;
	cpuio.src_qid = info->src_qid;
	cpuio.dst_pid = info->dst_pid;
	cpuio.dst_qid = info->dst_qid;
	cpuio.start_pktid = info->pktid;
	cpuio.end_pktid = info->pktid;

	ret = mac_set_cpuio_wd(adapter, &cpuio);
	if (ret != MACSUCCESS)
		return ret;

	return MACSUCCESS;
}

u32 deq_enq_all(struct mac_ax_adapter *adapter, struct deq_enq_info *info)
{
	struct cpuio_ctrl_t cpuio;
	struct first_pid_info pktid_info;
	u32 ret;

	PLTFM_MEMSET(&cpuio, 0, sizeof(struct cpuio_ctrl_t));

	pktid_info.macid = info->macid;
	pktid_info.src_pid = info->src_pid;
	pktid_info.src_qid = info->src_qid;

	ret = get_1st_pktid(adapter, &pktid_info);
	if (ret != MACSUCCESS)
		return ret;

	if (pktid_info.pktid != 0xFFF) {
		cpuio.cmd_type = CPUIO_OP_CMD_DEQ_ENQ_ALL;
		cpuio.macid = info->macid;
		cpuio.src_pid = info->src_pid;
		cpuio.src_qid = info->src_qid;
		cpuio.dst_pid = info->dst_pid;
		cpuio.dst_qid = info->dst_qid;

		ret = mac_set_cpuio_wd(adapter, &cpuio);
		if (ret != MACSUCCESS)
			return ret;
	}

	return MACSUCCESS;
}

static u32 get_1st_pktid(struct mac_ax_adapter *adapter,
			 struct first_pid_info *info)
{
	u32 ret = MACSUCCESS;
	struct cpuio_ctrl_t cpuio;

	PLTFM_MEMSET(&cpuio, 0, sizeof(struct cpuio_ctrl_t));

	cpuio.cmd_type = CPUIO_OP_CMD_GET_1ST_PID;
	cpuio.macid = info->macid;
	cpuio.src_pid = info->src_pid;
	cpuio.src_qid = info->src_qid;
	ret = mac_set_cpuio_wd(adapter, &cpuio);

	info->pktid = cpuio.pktid;

	return ret;
}

static u32 get_next_pktid(struct mac_ax_adapter *adapter,
			  struct next_pid_info *info)
{
	u32 ret = MACSUCCESS;
	struct cpuio_ctrl_t cpuio;

	PLTFM_MEMSET(&cpuio, 0, sizeof(struct cpuio_ctrl_t));

	cpuio.cmd_type = CPUIO_OP_CMD_GET_NEXT_PID;
	cpuio.macid = info->macid;
	cpuio.src_pid = info->src_pid;
	cpuio.src_qid = info->src_qid;
	cpuio.start_pktid = info->start_pktid;
	ret = mac_set_cpuio_wd(adapter, &cpuio);

	info->pktid = cpuio.pktid;

	return ret;
}

static void set_dmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid)
{
	u32 val32;
	u8 macid_sh = macid & (32 - 1);
	u8 macid_grp = macid >> 5;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		if (is_cv(adapter, CBV)) {
			PLTFM_MSG_TRACE("hw do not support dmac drop!\n");
			return;
		} else {
			if (MAC_REG_R32(R_AX_SS_DBG_3) &
			    B_AX_SS_HW_DECR_LEN_UDN)
				PLTFM_MSG_WARN("STA len underflow bef drop\n");
		}
	} else {
		if (MAC_REG_R32(R_AX_SS_DBG_3) &
		    B_AX_SS_HW_DECR_LEN_UDN)
			PLTFM_MSG_WARN("STA len underflow bef drop\n");
	}

	switch (macid_grp) {
	case 0:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_0);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_0, val32 | BIT(macid_sh));
		break;
	case 1:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_1);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_1, val32 | BIT(macid_sh));
		break;
	case 2:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_2);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_2, val32 | BIT(macid_sh));
		break;
	case 3:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_3);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_3, val32 | BIT(macid_sh));
		break;
	default:
		break;
	}
}

static void set_cmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid)
{
	u32 val32;
	u8 macid_sh = macid & (32 - 1);
	u8 macid_grp = macid >> 5;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	switch (macid_grp) {
	case 0:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_0);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_0, val32 | BIT(macid_sh));
		break;
	case 1:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_1);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_1, val32 | BIT(macid_sh));
		break;
	case 2:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_2);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_2, val32 | BIT(macid_sh));
		break;
	case 3:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_3);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_3, val32 | BIT(macid_sh));
		break;
	default:
		break;
	}
}

static void rel_dmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid)
{
	u32 val32;
	u8 macid_sh = macid & (32 - 1);
	u8 macid_grp = macid >> 5;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	switch (macid_grp) {
	case 0:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_0);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_0, val32 & ~(BIT(macid_sh)));
		break;
	case 1:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_1);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_1, val32 & ~(BIT(macid_sh)));
		break;
	case 2:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_2);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_2, val32 & ~(BIT(macid_sh)));
		break;
	case 3:
		val32 = MAC_REG_R32(R_AX_DMAC_MACID_DROP_3);
		MAC_REG_W32(R_AX_DMAC_MACID_DROP_3, val32 & ~(BIT(macid_sh)));
		break;
	default:
		break;
	}

	ss_hw_len_udn_clr(adapter);
}

static void rel_cmac_macid_drop(struct mac_ax_adapter *adapter, u8 macid)
{
	u32 val32;
	u8 macid_sh = macid & (32 - 1);
	u8 macid_grp = macid >> 5;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	switch (macid_grp) {
	case 0:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_0);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_0, val32 & ~(BIT(macid_sh)));
		break;
	case 1:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_1);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_1, val32 & ~(BIT(macid_sh)));
		break;
	case 2:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_2);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_2, val32 & ~(BIT(macid_sh)));
		break;
	case 3:
		val32 = MAC_REG_R32(R_AX_CMAC_MACID_DROP_3);
		MAC_REG_W32(R_AX_CMAC_MACID_DROP_3, val32 & ~(BIT(macid_sh)));
		break;
	default:
		break;
	}
}

static u32 set_hiq_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info)
{
	u32 val32;
	u32 addr;
	u16 mbssid_sh;
	u8 port_sh;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	addr = info->band ? R_AX_MBSSID_DROP_0_C1 : R_AX_MBSSID_DROP_0;
	mbssid_sh = BIT(0) << info->mbssid;
	port_sh = BIT(0) << info->port;

	val32 = MAC_REG_R32(addr);
	switch (info->sel) {
	case MAC_AX_PKT_DROP_SEL_HIQ_PORT:
		val32 |= port_sh << B_AX_PORT_DROP_4_0_SH;
		if (info->port == 0)
			val32 |= BIT(0);
		break;
	case MAC_AX_PKT_DROP_SEL_HIQ_MBSSID:
		val32 |= mbssid_sh;
		break;
	default:
		return MACNOITEM;
	}
	MAC_REG_W32(addr, val32);

	return MACSUCCESS;
}

static u32 rel_hiq_drop(struct mac_ax_adapter *adapter,
			struct mac_ax_pkt_drop_info *info)
{
	u32 val32;
	u32 addr;
	u16 mbssid_sh;
	u8 port_sh;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	addr = info->band ? R_AX_MBSSID_DROP_0_C1 : R_AX_MBSSID_DROP_0;
	mbssid_sh = BIT(0) << info->mbssid;
	port_sh = BIT(0) << info->port;

	val32 = MAC_REG_R32(addr);
	switch (info->sel) {
	case MAC_AX_PKT_DROP_SEL_REL_HIQ_PORT:
		val32 &= ~(port_sh << B_AX_PORT_DROP_4_0_SH);
		if (info->port == 0)
			val32 &= ~(BIT(0));
		break;
	case MAC_AX_PKT_DROP_SEL_REL_HIQ_MBSSID:
		val32 &= ~mbssid_sh;
		break;
	default:
		return MACNOITEM;
	}
	MAC_REG_W32(addr, val32);

	return MACSUCCESS;
}

static void ss_hw_len_udn_clr(struct mac_ax_adapter *adapter)
{
	u32 val32;
	struct mac_ax_intf_ops *ops = adapter_to_intf_ops(adapter);

	val32 = MAC_REG_R32(R_AX_SS_DBG_3);
	if (val32 & B_AX_SS_HW_DECR_LEN_UDN) {
		val32 &= ~(B_AX_SS_HW_ADD_LEN_OVF | B_AX_SS_SW_DECR_LEN_UDN |
			 B_AX_SS_HW_DECR_LEN_UDN | B_AX_SS_ATM_ERR |
			 B_AX_SS_DEL_STA_ERR | B_AX_SS_ADD_STA_ERR);
		MAC_REG_W32(R_AX_SS_DBG_3, val32 | B_AX_SS_HW_DECR_LEN_UDN);
	}
}

static u32 hiq_link_drop(struct mac_ax_adapter *adapter, u8 band)
{
	u32 ret;
	u8 mbid, port;
	struct deq_enq_info q_info;

	ret = check_mac_en(adapter, band, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	PLTFM_MEMSET(&q_info, 0, sizeof(struct deq_enq_info));

	q_info.dst_pid = WDE_DLE_PID_WDRLS;
	q_info.dst_qid = WDE_DLE_QID_WDRLS_DROP;
	if (band == MAC_AX_BAND_1) {
		q_info.src_pid = WDE_DLE_PID_C1;
		q_info.src_qid = WDE_DLE_QID_HI_C1;
	} else {
		q_info.src_pid = WDE_DLE_PID_C0;
		q_info.src_qid = WDE_DLE_QID_HI_C0;
	}

	if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852A)) {
		ret = deq_enq_all(adapter, &q_info);
		if (ret != MACSUCCESS)
			return ret;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852B)) {
		ret = deq_enq_all(adapter, &q_info);
		if (ret != MACSUCCESS)
			return ret;
	} else if (is_chip_id(adapter, MAC_AX_CHIP_ID_8852C) ||
		   is_chip_id(adapter, MAC_AX_CHIP_ID_8192XB)) {
		for (mbid = 0; mbid < adapter->hw_info->mbssid_num; mbid++) {
			q_info.macid = mbid;
			ret = deq_enq_all(adapter, &q_info);
			if (ret != MACSUCCESS)
				return ret;
		}
		for (port = 1; port < adapter->hw_info->port_num; port++) {
			q_info.macid = port << WDE_DLE_SUBQID_PORT_SH;
			ret = deq_enq_all(adapter, &q_info);
			if (ret != MACSUCCESS)
				return ret;
		}
	} else {
		return MACNOITEM;
	}

	return ret;
}

static u32 mg0_link_drop(struct mac_ax_adapter *adapter, u8 band)
{
	u32 ret;
	struct deq_enq_info q_info;

	ret = check_mac_en(adapter, band, MAC_AX_CMAC_SEL);
	if (ret != MACSUCCESS)
		return ret;

	PLTFM_MEMSET(&q_info, 0, sizeof(struct deq_enq_info));

	q_info.dst_pid = WDE_DLE_PID_WDRLS;
	q_info.dst_qid = WDE_DLE_QID_WDRLS_DROP;
	if (band == MAC_AX_BAND_1) {
		q_info.src_pid = WDE_DLE_PID_C1;
		q_info.src_qid = WDE_DLE_QID_MG0_C1;
	} else {
		q_info.src_pid = WDE_DLE_PID_C0;
		q_info.src_qid = WDE_DLE_QID_MG0_C0;
	}

	ret = deq_enq_all(adapter, &q_info);
	if (ret != MACSUCCESS)
		return ret;

	return ret;
}

