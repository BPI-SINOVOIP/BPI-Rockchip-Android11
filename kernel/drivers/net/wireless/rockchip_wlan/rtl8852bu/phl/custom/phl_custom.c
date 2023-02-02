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
#define _PHL_CUSTOM_C_
#include "../phl_headers.h"
#include "phl_custom_fb.h"

#ifdef CONFIG_PHL_CUSTOM_FEATURE

#define MAX_DATA_SIZE	(512)
enum custom_status {
	CUS_STAT_CMD_USED = BIT0,
	CUS_STAT_RPT_USED = BIT1,
	CUS_STAT_RPT_SENDING = BIT2,
};

struct phl_custom_ctx {
	struct phl_info_t *phl_info;
	u8 status; /* refer to enum custom_status*/
	u8 cmd_buf[MAX_DATA_SIZE];
	u8 rpt_buf[MAX_DATA_SIZE];
#ifdef CONFIG_PHL_CUSTOM_FEATURE_FB
	struct _custom_facebook_ctx fb_ctx;
#endif
};

bool
_custom_rpt_notify_check(void* priv, struct phl_msg* msg)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;
	bool ret = true;
	if (!TEST_STATUS_FLAG(ctx->status, CUS_STAT_RPT_USED) ||
	    TEST_STATUS_FLAG(ctx->status, CUS_STAT_RPT_SENDING)) {
		ret = false;
	}
	return ret;
}

void
_custom_rpt_notify_complete(void* priv, struct phl_msg* msg)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;

	CLEAR_STATUS_FLAG(ctx->status, CUS_STAT_RPT_USED);
	CLEAR_STATUS_FLAG(ctx->status, CUS_STAT_RPT_SENDING);
}

void
_custom_rpt_notify_start(void* priv,
                         struct phl_msg* msg,
                         struct phl_msg_attribute* attr)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;
	struct rtw_custom_decrpt *evt_rpt = (struct rtw_custom_decrpt *)ctx->rpt_buf;

	SET_STATUS_FLAG(ctx->status, CUS_STAT_RPT_SENDING);

	SET_MSG_MDL_ID_FIELD(msg->msg_id, PHL_MDL_CUSTOM);
	SET_MSG_EVT_ID_FIELD(msg->msg_id, MSG_EVT_CUSTOM_CMD_DONE);

	msg->inbuf = (u8*) evt_rpt;
	msg->inlen = evt_rpt->len + sizeof(struct rtw_custom_decrpt);

	attr->completion.completion = _custom_rpt_notify_complete;
	attr->completion.priv = ctx;
}

static void
_indicate_custome_evt_rpt(struct phl_custom_ctx *ctx)
{
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};

	if (!_custom_rpt_notify_check((void*)ctx, &msg))
		return;

	_custom_rpt_notify_start((void*)ctx, &msg, &attr);

	if (phl_msg_hub_send(ctx->phl_info,
	                     &attr, &msg) != RTW_PHL_STATUS_SUCCESS) {
		PHL_ERR("%s send msg fail\n", __func__);
		_custom_rpt_notify_complete((void*)ctx, &msg);
	}
}
enum rtw_phl_status
_phl_custom_prepare_default_fail_rpt(struct phl_custom_ctx *ctx,
                                     struct phl_msg* msg)
{
	struct rtw_custom_decrpt *cmd = (struct rtw_custom_decrpt *)(msg->inbuf);
	u8 ret = MDL_RET_FAIL;

	if (TEST_STATUS_FLAG(ctx->status, CUS_STAT_RPT_USED))
		return RTW_PHL_STATUS_RESOURCE;

	phl_custom_prepare_evt_rpt(ctx,
                                   cmd->evt_id,
                                   cmd->customer_id,
                                   &ret,
                                   1);
	return RTW_PHL_STATUS_SUCCESS;
}
static enum phl_mdl_ret_code
_phl_custom_hdl_fail_evt(void* dispr,
                         struct phl_custom_ctx* ctx,
                         struct phl_msg* msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;
	struct rtw_custom_decrpt *cmd = NULL;

	cmd = (struct rtw_custom_decrpt *)(msg->inbuf);
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
		case MSG_EVT_EDCA_ADJUST:
#ifdef CONFIG_PHL_CUSTOM_FEATURE_FB
			if(cmd->customer_id == CUS_ID_FB) {
				ret = phl_custom_hdl_fb_fail_evt(dispr,
				                                 ctx,
				                                 &(ctx->fb_ctx),
				                                 msg);
			}
#else
			ret = MDL_RET_IGNORE;
#endif
			break;
		default:
			ret = MDL_RET_IGNORE;
			break;
	}
	return ret;
}
static enum phl_mdl_ret_code
_phl_custom_hdl_internal_evt(void* dispr,
			     struct phl_custom_ctx* ctx,
			     struct phl_msg* msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;
	struct rtw_custom_decrpt *cmd = NULL;

	cmd = (struct rtw_custom_decrpt *)(msg->inbuf);
	switch (MSG_EVT_ID_FIELD(msg->msg_id)) {
		case MSG_EVT_CUSTOME_FEATURE_ENABLE:
		case MSG_EVT_CUSTOME_FEATURE_QUERY:
		case MSG_EVT_CUSTOME_TESTMODE_PARAM:
		case MSG_EVT_CUSTOME_SET_WIFI_ROLE:
		case MSG_EVT_AMPDU_CFG:
		case MSG_EVT_AMPDU_QUERY:
		case MSG_EVT_PDTHR_CFG:
		case MSG_EVT_PDTHR_QUERY:
		case MSG_EVT_POP_CFG:
		case MSG_EVT_POP_QUERY:
#ifdef CONFIG_PHL_CUSTOM_FEATURE_FB
			if(cmd->customer_id == CUS_ID_FB) {
				ret = phl_custom_hdl_fb_evt(dispr,
				                            ctx,
				                            &(ctx->fb_ctx),
				                            msg);
			}
#else
			ret = MDL_RET_IGNORE;
#endif
			break;
		default:
			ret = MDL_RET_IGNORE;
			break;
	}
	return ret;
}
static enum phl_mdl_ret_code
_phl_custom_hdl_external_evt(void* dispr,
                             struct phl_custom_ctx* ctx,
                             struct phl_msg* msg)
{
	return MDL_RET_IGNORE;
}

void
_phl_custom_cmd_completion(void* priv, struct phl_msg* msg)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;
	CLEAR_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED);
}

enum phl_mdl_ret_code
_phl_custom_mdl_init(void* phl_info,
                     void* dispr,
                     void** priv)
{
	struct phl_info_t *phl = (struct phl_info_t *)phl_info;
	struct phl_custom_ctx* ctx = NULL;
	void *d = phl_to_drvpriv(phl);

	FUNCIN();
	if (priv == NULL)
		return MDL_RET_FAIL;

	(*priv) = NULL;
	ctx = (struct phl_custom_ctx *)_os_mem_alloc(d, sizeof(struct phl_custom_ctx));
	if (ctx == NULL) {
		PHL_ERR(" %s, alloc fail\n",__FUNCTION__);
		return MDL_RET_FAIL;
	}
	ctx->phl_info = phl_info;
	(*priv) = (void*)ctx;
	PHL_INFO(" %s, size phl_custom_ctx(%d)\n",
			__FUNCTION__, (int)sizeof(struct phl_custom_ctx));
	return MDL_RET_SUCCESS;
}
void
_phl_custom_mdl_deinit(void* dispr, void* priv)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx*)priv;
	void *d = phl_to_drvpriv(ctx->phl_info);

	_os_mem_free(d, ctx, sizeof(struct phl_custom_ctx));
	PHL_INFO(" %s\n", __FUNCTION__);
}
enum phl_mdl_ret_code
_phl_custom_mdl_start(void* dispr, void* priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}
enum phl_mdl_ret_code
_phl_custom_mdl_stop(void* dispr, void* priv)
{
	enum phl_mdl_ret_code ret = MDL_RET_FAIL;

	FUNCIN();
	FUNCOUT();

	ret = MDL_RET_SUCCESS;
	return ret;
}
enum phl_mdl_ret_code
_phl_custom_mdl_msg_hdlr(void* dispr,
                         void* priv,
                         struct phl_msg* msg)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx*)priv;

	FUNCIN();
	if (IS_MSG_FAIL(msg->msg_id)) {
		ret = _phl_custom_hdl_fail_evt(dispr, priv, msg);
		_indicate_custome_evt_rpt(ctx);
		return MDL_RET_IGNORE;
	}

	switch (MSG_MDL_ID_FIELD(msg->msg_id)) {
		case PHL_MDL_CUSTOM:
			ret = _phl_custom_hdl_internal_evt(dispr, priv, msg);
			break;
		default:
			ret = _phl_custom_hdl_external_evt(dispr, priv, msg);
			break;
	}
	_indicate_custome_evt_rpt(ctx);
	FUNCOUT();
	return ret;
}
enum phl_mdl_ret_code
_phl_custom_mdl_set_info(void* dispr,
                         void* priv,
                         struct phl_module_op_info* info)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;
	void *d = phl_to_drvpriv(ctx->phl_info);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	struct rtw_custom_decrpt *cmd = NULL;
	u8 idx = 0xff;

	phl_dispr_get_idx(dispr, &idx);
	FUNCIN();
	switch (info->op_code) {
		case BK_MODL_OP_INPUT_CMD:
			if (TEST_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED) ||
				info->inlen > MAX_DATA_SIZE) {
				PHL_ERR("%s buf len err or used\n", __func__);
				break;
			}
			SET_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED);
			_os_mem_cpy(d, ctx->cmd_buf, info->inbuf, info->inlen);
			cmd = (struct rtw_custom_decrpt *)ctx->cmd_buf;
			SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_CUSTOM);
			SET_MSG_EVT_ID_FIELD(msg.msg_id, (u16)cmd->evt_id);
			msg.inbuf = ctx->cmd_buf;
			msg.inlen = info->inlen;
			attr.completion.priv = priv;
			attr.completion.completion = _phl_custom_cmd_completion;
			msg.band_idx = idx;
			if (phl_disp_eng_send_msg(ctx->phl_info,
			                          &msg,
			                          &attr,
			                          NULL) == RTW_PHL_STATUS_SUCCESS) {
				ret = MDL_RET_SUCCESS;
			} else {
				CLEAR_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED);
				PHL_ERR("%s send msg fail\n", __func__);
				ret = MDL_RET_FAIL;
			}
			break;
		case BK_MODL_OP_CUS_SET_ROLE_CAP:
			ret = phl_custom_fb_set_role_cap(dispr,
			                             (void *)ctx,
			                             &ctx->fb_ctx,
			                             info);
			break;
		default:
			break;
	}
	FUNCOUT();
	return ret;
}
enum phl_mdl_ret_code
_phl_custom_mdl_query_info(void* dispr,
                           void* priv,
                           struct phl_module_op_info* info)
{
	enum phl_mdl_ret_code ret = MDL_RET_IGNORE;
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)priv;
	void *d = phl_to_drvpriv(ctx->phl_info);
	struct phl_msg msg = {0};
	struct phl_msg_attribute attr = {0};
	struct rtw_custom_decrpt *cmd = NULL;
	u8 idx = 0xff;

	FUNCIN();
	switch(info->op_code) {
		case BK_MODL_OP_INPUT_CMD:
			if (TEST_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED) ||
				info->inlen > MAX_DATA_SIZE) {
				PHL_ERR("%s buf len err or used\n", __func__);
				break;
			}
			SET_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED);
			_os_mem_cpy(d, ctx->cmd_buf, info->inbuf, info->inlen);
			_os_mem_cpy(d, ctx->rpt_buf, info->outbuf, info->outlen);
			cmd = (struct rtw_custom_decrpt *)ctx->cmd_buf;
			SET_MSG_MDL_ID_FIELD(msg.msg_id, PHL_MDL_CUSTOM);
			SET_MSG_EVT_ID_FIELD(msg.msg_id, (u16)cmd->evt_id);
			msg.inbuf = ctx->cmd_buf;
			msg.inlen = info->inlen;
			msg.outbuf = ctx->rpt_buf;
			msg.outlen = info->outlen;
			attr.completion.priv = priv;
			attr.completion.completion = _phl_custom_cmd_completion;
			msg.band_idx = idx;
			if (phl_disp_eng_send_msg(ctx->phl_info,
			                          &msg,
			                          &attr,
			                          NULL) == RTW_PHL_STATUS_SUCCESS) {
				ret = MDL_RET_SUCCESS;
			} else {
				CLEAR_STATUS_FLAG(ctx->status, CUS_STAT_CMD_USED);
				PHL_ERR("%s send msg fail\n", __func__);
				ret = MDL_RET_FAIL;
			}
			break;
		case BK_MODL_OP_CUS_UPDATE_ROLE_CAP:
	   		ret = phl_custom_fb_update_opt_ie(dispr, (void*)ctx, &ctx->fb_ctx, info);
		break;
	default:
		break;
	}

	FUNCOUT();
	return ret;
}

enum rtw_phl_status
phl_register_custom_module(struct phl_info_t *phl_info, u8 band_idx)
{
	enum rtw_phl_status phl_status = RTW_PHL_STATUS_FAILURE;
	struct phl_bk_module_ops bk_ops = {0};

	bk_ops.init = _phl_custom_mdl_init;
	bk_ops.deinit = _phl_custom_mdl_deinit;
	bk_ops.start = _phl_custom_mdl_start;
	bk_ops.stop = _phl_custom_mdl_stop;
	bk_ops.msg_hdlr = _phl_custom_mdl_msg_hdlr;
	bk_ops.query_info = _phl_custom_mdl_query_info;
	bk_ops.set_info = _phl_custom_mdl_set_info;

	phl_status = phl_disp_eng_register_module(phl_info,
	                                          band_idx,
	                                          PHL_MDL_CUSTOM,
	                                          &bk_ops);
	if (RTW_PHL_STATUS_SUCCESS != phl_status)
		PHL_ERR("%s register Custom module in cmd disp failed\n", __func__);

	return phl_status;

}

enum rtw_phl_status
phl_custom_prepare_evt_rpt(void *custom_ctx,
                           u32 evt_id,
                           u32 customer_id,
                           u8 *rpt,
                           u32 rpt_len)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)custom_ctx;
	struct rtw_custom_decrpt *evt_rpt = (struct rtw_custom_decrpt *)ctx->rpt_buf;
	void *d = phl_to_drvpriv(ctx->phl_info);
	u8* val = (u8*)(evt_rpt +1);

	if ((rpt_len > (MAX_DATA_SIZE - sizeof(struct rtw_custom_decrpt))) ||
	   TEST_STATUS_FLAG(ctx->status, CUS_STAT_RPT_USED))
		return RTW_PHL_STATUS_RESOURCE;

	SET_STATUS_FLAG(ctx->status, CUS_STAT_RPT_USED);
	evt_rpt->evt_id = evt_id;
	evt_rpt->customer_id = customer_id;
	evt_rpt->len = rpt_len;
	_os_mem_cpy(d, val, rpt, rpt_len);
	return RTW_PHL_STATUS_SUCCESS;
}
struct phl_info_t*
phl_custom_get_phl_info(void *custom_ctx)
{
	struct phl_custom_ctx *ctx = (struct phl_custom_ctx *)custom_ctx;

	return ctx->phl_info;
}

/**
 * phl_custom_init_role_cap
 * 1. role cap customization
 * input:
 * @phl_info: (struct phl_info_t *)
 * @hw_band:hw_band
 * @role_cap: (struct role_cap_t)
 * return: rtw_phl_status
 */
enum rtw_phl_status
phl_custom_init_role_cap(struct phl_info_t *phl_info,
                         u8 hw_band,
                         struct role_cap_t *role_cap)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_SUCCESS;

	status = phl_custom_fb_init_role_cap(phl_info, hw_band, role_cap);

	return status;
}
#endif
