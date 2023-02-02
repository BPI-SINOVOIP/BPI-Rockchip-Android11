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
#define _PHL_PHY_MGNT_C_
#include "phl_headers.h"
#ifdef CONFIG_CMD_DISP

enum rtw_phl_status phl_disp_eng_bk_module_deinit(struct phl_info_t *phl);
enum rtw_phl_status _disp_eng_get_dispr_by_idx(struct phl_info_t *phl,
                                               u8 band_idx,
                                               void **dispr);

int share_thread_hdl(void *param)
{
	struct phl_info_t *phl_info = (struct phl_info_t *)param;
	void *d = phl_to_drvpriv(phl_info);
	struct phl_cmd_dispatch_engine *disp_eng = &(phl_info->disp_eng);
	u8 i = 0;

	PHL_INFO("%s enter\n", __FUNCTION__);
	while (!_os_thread_check_stop(d, &(disp_eng->share_thread))) {

		_os_sema_down(d, &disp_eng->msg_q_sema);

		/* A simple for-loop would guarantee
		 * all dispatcher split entire bandwidth of shared thread evenly,
		 * if adopting FIFO rule here,
		 * would lead to disproportionate distribution of thread bandwidth.
		*/
		for (i = 0 ; i < disp_eng->phy_num; i++) {
			if(_os_thread_check_stop(d, &(disp_eng->share_thread)))
				break;
			dispr_share_thread_loop_hdl(disp_eng->dispatcher[i]);
		}
	}
	for (i = 0 ; i < disp_eng->phy_num; i++)
		dispr_share_thread_leave_hdl(disp_eng->dispatcher[i]);
	_os_thread_wait_stop(d, &(disp_eng->share_thread));
	PHL_INFO("%s down\n", __FUNCTION__);
	return 0;
}

enum rtw_phl_status
phl_disp_eng_init(struct phl_info_t *phl, u8 phy_num)
{
	u8 i = 0;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	void *d = phl_to_drvpriv(phl);
	enum rtw_phl_status status = RTW_PHL_STATUS_SUCCESS;

	if (disp_eng->dispatcher != NULL) {
		PHL_ERR("[PHY]: %s, not empty\n",__FUNCTION__);
		return RTW_PHL_STATUS_FAILURE;
	}

	disp_eng->phl_info = phl;
	disp_eng->phy_num = phy_num;
#ifdef CONFIG_CMD_DISP_SOLO_MODE
	disp_eng->thread_mode = SOLO_THREAD_MODE;
#else
	disp_eng->thread_mode = SHARE_THREAD_MODE;
#endif
	disp_eng->dispatcher = _os_mem_alloc(d, sizeof(void*) * phy_num);
#ifdef CONFIG_CMD_DISP_SOLO_MODE
	_os_sema_init(d, &(disp_eng->dispr_ctrl_sema), 1);
#endif
	if (disp_eng->dispatcher == NULL) {
		disp_eng->phy_num = 0;
		PHL_ERR("[PHY]: %s, alloc fail\n",__FUNCTION__);
		return RTW_PHL_STATUS_RESOURCE;
	}

	for (i = 0 ; i < phy_num; i++) {
		status = dispr_init(phl, &(disp_eng->dispatcher[i]), i);
		if(status != RTW_PHL_STATUS_SUCCESS)
			break;
	}

	if (status != RTW_PHL_STATUS_SUCCESS)
		phl_disp_eng_deinit(phl);

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_disp_eng_deinit(struct phl_info_t *phl)
{
	u8 i = 0;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	void *d = phl_to_drvpriv(phl);

	if (disp_eng->dispatcher == NULL)
		return RTW_PHL_STATUS_FAILURE;

	phl_disp_eng_bk_module_deinit(phl);

	for (i = 0 ; i < disp_eng->phy_num; i++) {
		if(disp_eng->dispatcher[i] == NULL)
			continue;
		dispr_deinit(phl, disp_eng->dispatcher[i]);
		disp_eng->dispatcher[i] = NULL;
	}
#ifdef CONFIG_CMD_DISP_SOLO_MODE
	_os_sema_free(d, &(disp_eng->dispr_ctrl_sema));
#endif
	if (disp_eng->phy_num) {
		_os_mem_free(d, disp_eng->dispatcher,
				sizeof(void *) * (disp_eng->phy_num));
		disp_eng->dispatcher = NULL;
		disp_eng->phy_num = 0;
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_disp_eng_bk_module_deinit(struct phl_info_t *phl)
{
	u8 i = 0;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);

	for (i = 0 ; i < disp_eng->phy_num; i++) {
		if(disp_eng->dispatcher[i] == NULL)
			continue;
		dispr_module_deinit(disp_eng->dispatcher[i]);
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_disp_eng_start(struct phl_info_t *phl)
{
	u8 i = 0;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	void *d = phl_to_drvpriv(phl);

	_os_sema_init(d, &(disp_eng->msg_q_sema), 0);
	if (!disp_eng_is_solo_thread_mode(phl)) {
		_os_thread_init(d, &(disp_eng->share_thread), share_thread_hdl, phl,
				"disp_eng_share_thread");
		_os_thread_schedule(d, &(disp_eng->share_thread));
	}
	for (i = 0 ; i < disp_eng->phy_num; i++){
		if(disp_eng->dispatcher[i] == NULL)
			continue;
		dispr_start(disp_eng->dispatcher[i]);
		dispr_module_start(disp_eng->dispatcher[i]);
	}

	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_disp_eng_stop(struct phl_info_t *phl)
{
	u8 i = 0;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	void *d = phl_to_drvpriv(phl);
	u8 solo_mode = (disp_eng_is_solo_thread_mode(phl)) ? (true) : (false);

	if (disp_eng->dispatcher == NULL) {
		PHL_ERR("[PHY]: %s, abnomarl state\n",__FUNCTION__);
		return RTW_PHL_STATUS_SUCCESS;
	}

	for (i = 0 ; i < disp_eng->phy_num; i++) {
		if(disp_eng->dispatcher[i] == NULL)
			continue;
		dispr_module_stop(disp_eng->dispatcher[i]);
		if (solo_mode == true)
			dispr_stop(disp_eng->dispatcher[i]);
		else
			dispr_share_thread_stop_prior_hdl(disp_eng->dispatcher[i]);
	}

	if (solo_mode == false) {
		_os_thread_stop(d, &(disp_eng->share_thread));
		_os_sema_up(d, &(disp_eng->msg_q_sema));
		_os_thread_deinit(d, &(disp_eng->share_thread));

		for (i = 0 ; i < disp_eng->phy_num; i++)
			dispr_share_thread_stop_post_hdl(disp_eng->dispatcher[i]);
	}
	_os_sema_free(d, &(disp_eng->msg_q_sema));
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
rtw_phl_register_module(void *phl,
                        u8 band_idx,
                        enum phl_module_id id,
                        struct phl_bk_module_ops *ops)
{
	return phl_disp_eng_register_module((struct phl_info_t *)phl,
	                                    band_idx, id, ops);
}

enum rtw_phl_status
rtw_phl_deregister_module(void *phl,u8 band_idx, enum phl_module_id id)
{
	return phl_disp_eng_deregister_module((struct phl_info_t *)phl,
	                                      band_idx,
	                                      id);
}

u8 rtw_phl_is_fg_empty(void *phl,u8 band_idx)
{
	return phl_disp_eng_is_fg_empty((struct phl_info_t *)phl, band_idx);
}

enum rtw_phl_status
rtw_phl_send_msg_to_dispr(void *phl,
                          struct phl_msg *msg,
                          struct phl_msg_attribute *attr,
                          u32 *msg_hdl)
{
	return phl_disp_eng_send_msg(phl, msg, attr, msg_hdl);
}

enum rtw_phl_status
rtw_phl_cancel_dispr_msg(void *phl, u8 band_idx, u32 *msg_hdl)
{
	return phl_disp_eng_cancel_msg(phl, band_idx, msg_hdl);
}

enum rtw_phl_status
rtw_phl_add_cmd_token_req(void *phl,
                          u8 band_idx,
                          struct phl_cmd_token_req *req,
                          u32 *req_hdl)
{
	return phl_disp_eng_add_token_req(phl, band_idx, req, req_hdl);
}

enum rtw_phl_status
rtw_phl_cancel_cmd_token(void *phl, u8 band_idx, u32 *req_hdl)
{
	return phl_disp_eng_cancel_token_req(phl, band_idx, req_hdl);
}

enum rtw_phl_status
rtw_phl_free_cmd_token(void *phl, u8 band_idx, u32 *req_hdl)
{
	return phl_disp_eng_free_token(phl, band_idx, req_hdl);
}

enum rtw_phl_status
rtw_phl_set_cur_cmd_info(void *phl,
                         u8 band_idx,
                         struct phl_module_op_info* op_info)
{
	return phl_disp_eng_set_cur_cmd_info(phl, band_idx, op_info);
}

enum rtw_phl_status
rtw_phl_query_cur_cmd_info(void *phl,
                           u8 band_idx,
                           struct phl_module_op_info* op_info)
{
	return phl_disp_eng_query_cur_cmd_info(phl, band_idx, op_info);
}

enum rtw_phl_status
rtw_phl_set_bk_module_info(void *phl,
                           u8 band_idx,
                           enum phl_module_id id,
                           struct phl_module_op_info *op_info)
{
	return phl_disp_eng_set_bk_module_info(phl, band_idx, id, op_info);
}

enum rtw_phl_status
rtw_phl_query_bk_module_info(void *phl,
                             u8 band_idx,
                             enum phl_module_id id,
                             struct phl_module_op_info *op_info)
{
	return phl_disp_eng_query_bk_module_info(phl, band_idx, id, op_info);
}

enum rtw_phl_status
rtw_phl_set_msg_disp_seq(void *phl,
                         struct phl_msg_attribute *attr,
                         struct msg_self_def_seq* seq)
{
	return phl_disp_eng_set_msg_disp_seq(phl, attr, seq);
}

enum rtw_phl_status
_disp_eng_get_dispr_by_idx(struct phl_info_t *phl, u8 band_idx, void **dispr)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);

	if (band_idx > (disp_eng->phy_num - 1) || (dispr == NULL)) {
		PHL_WARN("%s invalid input :%d\n", __func__, band_idx);
		return RTW_PHL_STATUS_INVALID_PARAM;
	}
	(*dispr) = disp_eng->dispatcher[band_idx];
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status
phl_disp_eng_register_module(struct phl_info_t *phl,
                             u8 band_idx,
                             enum phl_module_id id,
                             struct phl_bk_module_ops *ops)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	u8 idx = band_idx;

	if ((band_idx + 1) > disp_eng->phy_num) {
		PHL_WARN("%s invalid input :%d\n", __func__, band_idx);
		return RTW_PHL_STATUS_INVALID_PARAM;
	}

	return dispr_register_module(disp_eng->dispatcher[idx], id, ops);
}

enum rtw_phl_status
phl_disp_eng_deregister_module(struct phl_info_t *phl,
                               u8 band_idx,
                               enum phl_module_id id)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	u8 idx = band_idx;

	if ((band_idx + 1) > disp_eng->phy_num) {
		PHL_WARN("%s invalid input :%d\n", __func__, band_idx);
		return RTW_PHL_STATUS_INVALID_PARAM;
	}

	return dispr_deregister_module(disp_eng->dispatcher[idx], id);
}

void disp_eng_notify_share_thread(struct phl_info_t *phl, void *dispr)
{
	void *d = phl_to_drvpriv(phl);
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);

	_os_sema_up(d, &(disp_eng->msg_q_sema));
}

u8 phl_disp_eng_is_dispr_busy(struct phl_info_t *phl, u8 band_idx)
{
	void* dispr = NULL;
	void *handle = NULL;

	if (_disp_eng_get_dispr_by_idx(phl, band_idx, &dispr) != RTW_PHL_STATUS_SUCCESS)
		return false;
	if (dispr_get_cur_cmd_req(dispr, &handle) == RTW_PHL_STATUS_SUCCESS)
		return true;
	else
		return false;
}

enum rtw_phl_status
phl_disp_eng_set_cur_cmd_info(struct phl_info_t *phl,
                              u8 band_idx,
                              struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_set_cur_cmd_info(dispr, op_info);
}

enum rtw_phl_status
phl_disp_eng_query_cur_cmd_info(struct phl_info_t *phl,
                                u8 band_idx,
                                struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_query_cur_cmd_info(dispr, op_info);
}

enum rtw_phl_status
phl_disp_eng_set_bk_module_info(struct phl_info_t *phl,
                                u8 band_idx,
                                enum phl_module_id id,
                                struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;
	void* handle = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	status = dispr_get_bk_module_handle(dispr, id, &handle);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_set_bk_module_info(dispr, handle, op_info);
}

enum rtw_phl_status
phl_disp_eng_query_bk_module_info(struct phl_info_t *phl,
                                  u8 band_idx,
                                  enum phl_module_id id,
                                  struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;
	void* handle = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	status = dispr_get_bk_module_handle(dispr, id, &handle);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_query_bk_module_info(dispr, handle, op_info);
}

enum rtw_phl_status
phl_disp_eng_set_src_info(struct phl_info_t *phl,
                          struct phl_msg *msg,
                          struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, msg->band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_set_src_info(dispr, msg, op_info);
}

enum rtw_phl_status
phl_disp_eng_query_src_info(struct phl_info_t *phl,
                            struct phl_msg *msg,
                            struct phl_module_op_info *op_info)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, msg->band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_query_src_info(dispr, msg, op_info);
}

enum rtw_phl_status
phl_disp_eng_send_msg(struct phl_info_t *phl,
                      struct phl_msg *msg,
                      struct phl_msg_attribute *attr,
                      u32 *msg_hdl)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, msg->band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_send_msg(dispr, msg, attr, msg_hdl);
}

enum rtw_phl_status
phl_disp_eng_cancel_msg(struct phl_info_t *phl, u8 band_idx, u32 *msg_hdl)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_cancel_msg(dispr, msg_hdl);
}

enum rtw_phl_status
phl_disp_eng_clr_pending_msg(struct phl_info_t *phl, u8 band_idx)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_clr_pending_msg(dispr);
}

enum rtw_phl_status
phl_disp_eng_add_token_req(struct phl_info_t *phl,
                           u8 band_idx,
                           struct phl_cmd_token_req *req,
                           u32 *req_hdl)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_add_token_req(dispr, req, req_hdl);
}

enum rtw_phl_status
phl_disp_eng_cancel_token_req(struct phl_info_t *phl, u8 band_idx, u32 *req_hdl)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_cancel_token_req(dispr, req_hdl);
}

enum rtw_phl_status
phl_disp_eng_free_token(struct phl_info_t *phl, u8 band_idx, u32 *req_hdl)
{
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, band_idx, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_free_token(dispr, req_hdl);
}

enum rtw_phl_status
phl_disp_eng_notify_dev_io_status(struct phl_info_t *phl,
                                  u8 band_idx,
                                  enum phl_module_id mdl_id,
                                  bool allow_io)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	u8 i = 0;

	band_idx = HW_BAND_MAX; /* force all band stop IO */
	if (band_idx != HW_BAND_MAX)
		return dispr_notify_dev_io_status(disp_eng->dispatcher[band_idx], mdl_id, allow_io);

	for (i = 0; i < disp_eng->phy_num; i++)
		dispr_notify_dev_io_status(disp_eng->dispatcher[i], mdl_id, allow_io);

	return RTW_PHL_STATUS_SUCCESS;
}

void phl_disp_eng_notify_shall_stop(struct phl_info_t *phl)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	u8 i = 0;

	for (i = 0; i < disp_eng->phy_num; i++) {
		if (is_dispr_started(disp_eng->dispatcher[i]))
			dispr_notify_shall_stop(disp_eng->dispatcher[i]);
	}
}

u8 phl_disp_eng_is_fg_empty(struct phl_info_t *phl, u8 band_idx)
{
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	u8 i = 0;

	if (band_idx != HW_BAND_MAX)
		return dispr_is_fg_empty(disp_eng->dispatcher[band_idx]);

	for (i = 0; i < disp_eng->phy_num; i++)
		if (false == dispr_is_fg_empty(disp_eng->dispatcher[i]))
			return false;

	return true;
}

#ifdef CONFIG_CMD_DISP_SOLO_MODE
void dispr_ctrl_sema_down(struct phl_info_t *phl_info)
{
	_os_sema_down(phl_to_drvpriv(phl_info),
			&(phl_info->disp_eng.dispr_ctrl_sema));
}

void dispr_ctrl_sema_up(struct phl_info_t *phl_info)
{
	_os_sema_up(phl_to_drvpriv(phl_info),
			&(phl_info->disp_eng.dispr_ctrl_sema));
}
#endif

enum rtw_phl_status
phl_disp_eng_set_msg_disp_seq(struct phl_info_t *phl,
                              struct phl_msg_attribute *attr,
                              struct msg_self_def_seq *seq)
{
#ifdef CONFIG_CMD_DISP_SUPPORT_CUSTOM_SEQ
	enum rtw_phl_status status = RTW_PHL_STATUS_FAILURE;
	struct phl_cmd_dispatch_engine *disp_eng = &(phl->disp_eng);
	void* dispr = NULL;

	status = _disp_eng_get_dispr_by_idx(phl, HW_BAND_0, &dispr);
	if (RTW_PHL_STATUS_SUCCESS != status)
		return status;

	return dispr_set_dispatch_seq(dispr, attr, seq);
#else
	return RTW_PHL_STATUS_FAILURE;
#endif
}

u8 phl_disp_query_mdl_id(struct phl_info_t *phl, void *bk_mdl)
{
	return disp_query_mdl_id(phl, bk_mdl);
}

#else
enum rtw_phl_status rtw_phl_set_bk_module_info(void *phl, u8 band_idx,
		enum phl_module_id id, struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status rtw_phl_query_bk_module_info(void *phl, u8 band_idx,
		enum phl_module_id id, struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status phl_disp_eng_init(struct phl_info_t *phl, u8 phy_num)
{
	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status phl_disp_eng_deinit(struct phl_info_t *phl)
{
	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status phl_disp_eng_start(struct phl_info_t *phl)
{
	return RTW_PHL_STATUS_SUCCESS;
}
enum rtw_phl_status phl_disp_eng_stop(struct phl_info_t *phl)
{
	return RTW_PHL_STATUS_SUCCESS;
}

enum rtw_phl_status phl_disp_eng_register_module(struct phl_info_t *phl,
						 u8 band_idx,
						 enum phl_module_id id,
						 struct phl_bk_module_ops *ops)
{
	return RTW_PHL_STATUS_FAILURE;
}

enum rtw_phl_status phl_disp_eng_deregister_module(struct phl_info_t *phl,
						   u8 band_idx,
						   enum phl_module_id id)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_dispr_get_idx(void *dispr, u8 *idx)
{
	return RTW_PHL_STATUS_FAILURE;
}

u8 phl_disp_eng_is_dispr_busy(struct phl_info_t *phl, u8 band_idx)
{
	return true;
}
enum rtw_phl_status phl_disp_eng_set_cur_cmd_info(struct phl_info_t *phl, u8 band_idx,
					       struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_query_cur_cmd_info(struct phl_info_t *phl, u8 band_idx,
						 struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_set_bk_module_info(struct phl_info_t *phl, u8 band_idx,
						enum phl_module_id id, struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_query_bk_module_info(struct phl_info_t *phl, u8 band_idx,
							enum phl_module_id id, struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_set_src_info(struct phl_info_t *phl, struct phl_msg *msg,
						struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_query_src_info(struct phl_info_t *phl, struct phl_msg *msg,
						struct phl_module_op_info *op_info)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_send_msg(struct phl_info_t *phl, struct phl_msg *msg,
						struct phl_msg_attribute *attr, u32 *msg_hdl)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_cancel_msg(struct phl_info_t *phl, u8 band_idx, u32 *msg_hdl)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_clr_pending_msg(struct phl_info_t *phl, u8 band_idx)
{
	return RTW_PHL_STATUS_FAILURE;
}

enum rtw_phl_status phl_disp_eng_add_token_req(struct phl_info_t *phl, u8 band_idx,
					    struct phl_cmd_token_req *req, u32 *req_hdl)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_cancel_token_req(struct phl_info_t *phl, u8 band_idx, u32 *req_hdl)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_free_token(struct phl_info_t *phl, u8 band_idx, u32 *req_hdl)
{
	return RTW_PHL_STATUS_FAILURE;
}
enum rtw_phl_status phl_disp_eng_notify_dev_io_status(struct phl_info_t *phl, u8 band_idx,
							enum phl_module_id mdl_id, bool allow_io)
{
	return RTW_PHL_STATUS_FAILURE;
}
}

void
phl_disp_eng_notify_shall_stop(struct phl_info_t *phl)
{
}

enum rtw_phl_status rtw_phl_set_msg_disp_seq(void *phl,
						struct phl_msg_attribute *attr,
						struct msg_self_def_seq* seq)
{
	return RTW_PHL_STATUS_FAILURE;
}

u8 phl_disp_query_mdl_id(struct phl_info_t *phl, void *bk_mdl)
{
	return PHL_MDL_ID_MAX;
}

#endif
