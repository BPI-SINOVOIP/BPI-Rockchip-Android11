/******************************************************************************
 *
 * Copyright(c) 2019 - 2020 Realtek Corporation.
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
#ifndef _HAL_STRUCT_H_
#define _HAL_STRUCT_H_


struct hal_info_t;

#define hal_get_trx_ops(_halinfo)	(_halinfo->trx_ops)
/**
 * struct hal_trx_ops - hw ic specific operations
 *
 * @init: the function for initializing IC specific data and hw configuration
 * @deinit: the function for deinitializing IC specific data and hw configuration
 * @query_tx_res: the function for querying hw tx resource
 * @query_rx_res: the function for querying hw rx resource
 * @map_hw_tx_chnl: the function for getting mapping hw tx channel
 * @qsel_to_tid: the function for converting hw qsel to tid value
 * @query_txch_num: the function for querying total hw tx dma channels number
 * @query_rxch_num: the function for querying total hw rx dma channels number
 * @update_wd: the function for updating wd page for xmit packet
 * @update_txbd: the function for updating tx bd for xmit packet
 * @tx_start: the function to trigger hw to start tx
 * @get_fwcmd_queue_idx: the function to get fwcmd queue idx
 * @check_rxrdy: the function check if hw rx buffer is ready to access
 * @handle_rxbd_info: the function handling hw rxbd information
 * @handle_rx_buffer: the function handling hw rx buffer
 * @update_rxbd: the function for updating rx bd for recv packet
 * @notify_rxdone: the function to notify hw rx done
 * @handle_wp_rpt: the function parsing wp report content
 */
struct hal_trx_ops {
	u8 (*map_hw_tx_chnl)(u16 macid, enum rtw_phl_ring_cat cat, u8 band);
	u8 (*query_txch_num)(void);
	u8 (*query_rxch_num)(void);

#ifdef CONFIG_PCI_HCI
	enum rtw_hal_status (*init)(struct hal_info_t *hal, u8 *txbd_buf, u8 *rxbd_buf);
	void (*deinit)(struct hal_info_t *hal);

	u16 (*query_tx_res)(struct rtw_hal_com_t *hal_com, u8 dma_ch,
			    u16 *host_idx, u16 *hw_idx);
	u16 (*query_rx_res)(struct rtw_hal_com_t *hal_com, u8 dma_ch,
			    u16 *host_idx, u16 *hw_idx);
	void (*cfg_dma_io)(struct hal_info_t *hal, u8 en);
	void (*cfg_txdma)(struct hal_info_t *hal, u8 en, u8 dma_ch);
	void (*cfg_wow_txdma)(struct hal_info_t *hal, u8 en);
	void (*cfg_txhci)(struct hal_info_t *hal, u8 en);
	void (*cfg_rxhci)(struct hal_info_t *hal, u8 en);
	void (*clr_rwptr)(struct hal_info_t *hal);
	void (*rst_bdram)(struct hal_info_t *hal);
	u8 (*poll_txdma_idle)(struct hal_info_t *hal);
	void (*cfg_rsvd_ctrl)(struct hal_info_t *hal);
	u8 (*qsel_to_tid)(struct hal_info_t *hal, u8 qsel_id, u8 tid_indic);

	enum rtw_hal_status
		(*update_wd)(struct hal_info_t *hal, struct rtw_phl_pkt_req *req);
	enum rtw_hal_status
		(*update_txbd)(struct hal_info_t *hal,
				struct tx_base_desc *txbd_ring,
				struct rtw_wd_page *wd_page,
				u8 ch_idx, u16 wd_num);
	enum rtw_hal_status
		(*tx_start)(struct hal_info_t *hal,
				struct tx_base_desc *txbd, u8 dma_ch);

	u8 (*get_fwcmd_queue_idx)(void);

	u8 (*check_rxrdy)(struct rtw_phl_com_t *phl_com, u8 *rxbuf, u8 dma_ch);
	enum rtw_hal_status
		(*handle_rx_buffer)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal,
					u8 *buf, u32 buf_size,
					struct rtw_phl_rx_pkt *rxpkt);
	u8 (*handle_rxbd_info)(struct hal_info_t *hal, u8 *rxbuf, u16 *buf_size);

	enum rtw_hal_status
		(*update_rxbd)(struct hal_info_t *hal,
				struct rx_base_desc *rxbd,
				struct rtw_rx_buf *rx_buf);

	enum rtw_hal_status
		(*notify_rxdone)(struct hal_info_t *hal,
				struct rx_base_desc *rxbd, u8 ch, u16 rxcnt);

	u16 (*handle_wp_rpt)(struct hal_info_t *hal, u8 *rp, u16 len,
			     u8 *sw_retry, u8 *dma_ch, u16 *wp_seq, u8 *mac_id,
			     u8 *ac_queue, u8 *txsts);
#endif /*CONFIG_PCI_HCI*/

#ifdef CONFIG_USB_HCI
	enum rtw_hal_status (*init)(struct hal_info_t *hal);
	void (*deinit)(struct hal_info_t *hal);

	enum rtw_hal_status
	(*hal_fill_wd)(struct hal_info_t *hal, struct rtw_xmit_req *tx_req,
				u8 *wd_buf, u32 *wd_len);
	u8 (*get_bulkout_id)(struct hal_info_t *hal, u8 ch_dma, u8 mode);
	enum rtw_hal_status
		(*handle_rx_buffer)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal,
					u8 *buf, u32 buf_size,
					struct rtw_phl_rx_pkt *rxpkt);
 	enum rtw_hal_status
		(*query_hal_info)(struct hal_info_t *hal, u8 info_id, void *value);
	enum rtw_hal_status
		(*usb_tx_agg_cfg)(struct hal_info_t *hal, u8* wd_buf, u8 agg_num);
	enum rtw_hal_status
		(*usb_rx_agg_cfg)(struct hal_info_t *hal, u8 mode, u8 agg_mode,
			u8 drv_define, u8 timeout, u8 size, u8 pkt_num);
	u8 (*get_fwcmd_queue_idx)(void);
	u8 (*get_max_bulkout_wd_num)(struct hal_info_t *hal);
	void (*cfg_dma_io)(struct hal_info_t *hal, u8 en);
	void (*cfg_txdma)(struct hal_info_t *hal, u8 en, u8 dma_ch);
	void (*cfg_txhci)(struct hal_info_t *hal, u8 en);
	void (*cfg_rxhci)(struct hal_info_t *hal, u8 en);
	void (*clr_rwptr)(struct hal_info_t *hal);
	void (*rst_bdram)(struct hal_info_t *hal);
	void (*cfg_rsvd_ctrl)(struct hal_info_t *hal);
	u16 (*handle_wp_rpt)(struct hal_info_t *hal, u8 *rp, u16 len,
			u8 *mac_id, u8 *ac_queue, u8 *txsts);
#endif /*CONFIG_USB_HCI*/

#ifdef CONFIG_SDIO_HCI
	enum rtw_hal_status (*init)(struct hal_info_t *hal);
	void (*deinit)(struct hal_info_t *hal);
	u16 (*query_tx_res)(struct rtw_hal_com_t *hal_com, u8 dma_ch,
			    u16 *host_idx, u16 *hw_idx);
	u16 (*query_rx_res)(struct rtw_hal_com_t *hal_com, u8 dma_ch,
			    u16 *host_idx, u16 *hw_idx);

	enum rtw_hal_status
	(*hal_fill_wd)(struct hal_info_t *hal, struct rtw_xmit_req *tx_req,
				u8 *wd_buf, u32 *wd_len);
	u8 (*get_fwcmd_queue_idx)(void);
	void (*cfg_dma_io)(struct hal_info_t *hal, u8 en);
	void (*cfg_txdma)(struct hal_info_t *hal, u8 en, u8 dma_ch);
	void (*cfg_txhci)(struct hal_info_t *hal, u8 en);
	void (*cfg_rxhci)(struct hal_info_t *hal, u8 en);
	void (*clr_rwptr)(struct hal_info_t *hal);
	void (*rst_bdram)(struct hal_info_t *hal);
	void (*cfg_rsvd_ctrl)(struct hal_info_t *hal);

	enum rtw_hal_status(*handle_rx_buffer)(struct rtw_phl_com_t *phl_com,
					       struct hal_info_t *hal,
					       u8 *buf, u32 buf_size,
					       struct rtw_phl_rx_pkt *rxpkt);
#endif

};

#define hal_get_ops(_halinfo)	(&_halinfo->hal_ops)

struct hal_ops_t {
	/*** initialize section ***/
	void (*read_chip_version)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	void (*init_hal_spec)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	void (*init_default_value)(struct hal_info_t *hal, struct hal_intr_mask_cfg *cfg);
	u32 (*hal_hci_configure)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal,
					struct rtw_ic_info *ic_info);

	enum rtw_hal_status (*hal_get_efuse)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	enum rtw_hal_status (*hal_init)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	void (*hal_deinit)(struct rtw_phl_com_t *phl_com,
			   struct hal_info_t *hal);
	enum rtw_hal_status (*hal_start)(struct rtw_phl_com_t *phl_com,
					 struct hal_info_t *hal);
	enum rtw_hal_status (*hal_stop)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	enum rtw_hal_status (*hal_cfg_fw)(struct rtw_phl_com_t *phl_com,
					  struct hal_info_t *hal,
					  char *ic_name,
					  enum rtw_fw_type fw_type);
#ifdef CONFIG_WOWLAN
	enum rtw_hal_status (*hal_wow_init)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal, struct rtw_phl_stainfo_t *sta);
	enum rtw_hal_status (*hal_wow_deinit)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal, struct rtw_phl_stainfo_t *sta);
#endif /* CONFIG_WOWLAN */

	/* MP */
	enum rtw_hal_status (*hal_mp_init)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	enum rtw_hal_status (*hal_mp_deinit)(struct rtw_phl_com_t *phl_com,
					struct hal_info_t *hal);
	/*IO ops*/
	u32 (*read_macreg)(struct hal_info_t *hal,
			u32 offset, u32 bit_mask);
	void (*write_macreg)(struct hal_info_t *hal,
			u32 offset, u32 bit_mask, u32 data);
	u32 (*read_bbreg)(struct hal_info_t *hal,
			u32 offset, u32 bit_mask);
	void (*write_bbreg)(struct hal_info_t *hal,
			u32 offset, u32 bit_mask, u32 data);
	u32 (*read_rfreg)(struct hal_info_t *hal,
			enum rf_path path, u32 offset, u32 bit_mask);
	void (*write_rfreg)(struct hal_info_t *hal,
			enum rf_path path, u32 offset, u32 bit_mask, u32 data);
#ifdef RTW_WKARD_BUS_WRITE
	enum rtw_hal_status (*write_reg_post_cfg)(struct hal_info_t *hal_info,
						  u32 offset, u32 value);
#endif

	/*** interrupt hdl section ***/
	void (*enable_interrupt)(struct hal_info_t *hal);
	void (*disable_interrupt)(struct hal_info_t *hal);
	void (*config_interrupt)(struct hal_info_t *hal, enum rtw_phl_config_int int_mode);
	bool (*recognize_interrupt)(struct hal_info_t *hal);
	bool (*recognize_halt_c2h_interrupt)(struct hal_info_t *hal);
	void (*clear_interrupt)(struct hal_info_t *hal);
	u32 (*interrupt_handler)(struct hal_info_t *hal);
	void (*restore_interrupt)(struct hal_info_t *hal);
	void (*restore_rx_interrupt)(struct hal_info_t *hal);

#ifdef RTW_PHL_BCN //hal_ops_t
	enum rtw_hal_status (*cfg_bcn)(struct rtw_phl_com_t *phl_com,
		struct hal_info_t *hal, struct rtw_bcn_entry *bcn_entry);
	enum rtw_hal_status (*upt_bcn)(struct rtw_phl_com_t *phl_com,
		struct hal_info_t *hal, struct rtw_bcn_entry *bcn_entry);
#endif

	enum rtw_hal_status (*pkt_ofld)(struct hal_info_t *hal, u8 *id, u8 op,
							u8 *pkt, u16 *len);
	enum rtw_hal_status (*pkt_update_ids)(struct hal_info_t *hal,
						struct pkt_ofld_entry *entry);
};

struct hal_info_t {
	struct rtw_hal_com_t *hal_com;
	_os_atomic hal_mac_mem;

	struct hal_trx_ops *trx_ops;
	struct hal_ops_t hal_ops;
#ifdef CONFIG_PCI_HCI
	void *txch_map;
#endif
	void *rpr_cfg;

	void *mac; /*halmac*/
	void *bb;
	void *rf;
	void *btc;
	void *efuse;
	enum rtw_rx_fltr_mode rx_fltr_mode;
	u8 monitor_mode; /* default: 0 */
};

struct hal_c2h_hdl {
	u8 cat;
	u8 cls_min;
	u8 cls_max;
	u32 (*c2h_hdl)(void *hal, struct rtw_c2h_info *c2h);
};


#ifdef CONFIG_PHL_CHANNEL_INFO

struct chinfo_bbcr_cfg {
	bool	ch_i_phy0_en;
	bool	ch_i_phy1_en;
	bool	ch_i_data_src;
	bool	ch_i_cmprs;
	u8	ch_i_grp_num_non_he;
	u8	ch_i_grp_num_he;
	u8	ch_i_blk_start_idx;
	u8	ch_i_blk_end_idx;
	u32	ch_i_ele_bitmap;
	bool	ch_i_type;
	u8	ch_i_seg_len;
};

struct ch_rpt_hdr_info {
	u16 total_len_l; /*header(16byte) + Raw data length(Unit: byte)*/
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8 total_len_m:1;
	u8 total_seg_num:7;
	#else
	u8 total_seg_num:7;
	u8 total_len_m:1;
	#endif
	u8 avg_noise_pow;
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8 is_pkt_end:1;
	u8 set_valid:1;
	u8 n_rx:3;
	u8 n_sts:3;
	#else
	u8 n_sts:3;
	u8 n_rx:3;
	u8 set_valid:1;
	u8 is_pkt_end:1;
	#endif
	u8 segment_size; /*unit (8Byte)*/
	u8 evm[2];
};

struct phy_info_rpt {
	u8	rssi[2];
	u16	rsvd_0;
	u8	rssi_avg;
	#if (PLATFOM_IS_LITTLE_ENDIAN)
	u8	rxsc:4;
	u8	rsvd_1:4;
	#else
	u8	rsvd_1:4;
	u8	rxsc:4;
	#endif
	u16	rsvd_2;
};


struct ch_info_drv_rpt {
	u32 raw_data_len;
	u8 seg_idx_curr;
};

#endif /* CONFIG_PHL_CHANNEL_INFO */
#endif /*_HAL_STRUCT_H_*/
