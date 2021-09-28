/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/

/**
******************************************************************************
* @file ihevce_entropy_structs.h
*
* @brief
*  This file contains encoder entropy context related structures and
*  interface prototypes
*
* @author
*  Ittiam
******************************************************************************
*/

#ifndef _IHEVCE_ENTROPY_STRUCTS_H_
#define _IHEVCE_ENTROPY_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/**
******************************************************************************
 *  @brief  defines maximum transform depth in HEVC (32 to 4)
******************************************************************************
 */
#define MAX_TFR_DEPTH 5

/**
******************************************************************************
 *  @brief  defines maximum qp delta to be coded as truncated unary code
******************************************************************************
 */
#define TU_MAX_QP_DELTA_ABS 5

/**
******************************************************************************
 *  @brief  defines maximum value of context increment used for qp delta encode
******************************************************************************
 */
#define CTXT_MAX_QP_DELTA_ABS 1

/**
******************************************************************************
 *  @brief  header length in the compressed scan coeff buffer of a TU
******************************************************************************
 */
#define COEFF_BUF_HEADER_LEN 4

/**
******************************************************************************
 *  @brief   extracts the "bitpos" bit of a input variable x
******************************************************************************
 */
#define EXTRACT_BIT(val, x, bitpos)                                                                \
    {                                                                                              \
        val = ((((x) >> (bitpos)) & 0x1));                                                         \
    }

/**
******************************************************************************
 *  @brief   inserts bit y in "bitpos' position of input varaible x
******************************************************************************
 */
#define INSERT_BIT(x, bitpos, y) ((x) |= ((y) << (bitpos)))

/**
******************************************************************************
 *  @brief   sets n bits starting from "bitpos' position of input varaible x
******************************************************************************
 */
#define SET_BITS(x, bitpos, n) ((x) |= (((1 << (n)) - 1) << (bitpos)))

/**
******************************************************************************
 *  @brief   clears n bits starting from "bitpos' position of input varaible x
******************************************************************************
 */
#define CLEAR_BITS(x, bitpos, n) ((x) &= (~(((1 << (n)) - 1) << (bitpos))))

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      Enumeration for memory records requested by entropy module
******************************************************************************
 */
typedef enum
{
    ENTROPY_CTXT = 0,
    ENTROPY_TOP_SKIP_FLAGS,
    ENTROPY_TOP_CU_DEPTH,
    ENTROPY_DUMMY_OUT_BUF,

    /* should always be the last entry */
    NUM_ENTROPY_MEM_RECS

} IHEVCE_ENTROPY_MEM_TABS_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      Entropy context for encoder
******************************************************************************
 */
typedef struct entropy_context
{
    /** cabac engine context                                        */
    cab_ctxt_t s_cabac_ctxt;

    /** bitstream context                                           */
    bitstrm_t s_bit_strm;

    /**
     * duplicate bitstream to generate entry offset
     * to support entropy sync
     */
    bitstrm_t s_dup_bit_strm_ent_offset;

    /** pointer to top row cu skip flags (1 bit per 8x8CU)          */
    UWORD8 *pu1_skip_cu_top;

    /** pointer to top row cu depth buffer (1 byte per 8x8CU)       */
    UWORD8 *pu1_cu_depth_top;

    /** pointer to parent coded block flags based on trasform depth */
    UWORD8 *apu1_cbf_cb[2];

    /** pointer to parent coded block flags based on trasform depth */
    UWORD8 *apu1_cbf_cr[2];

    /** left cu skip flags  (max of 8) (1 bit per 8x8)              */
    UWORD32 u4_skip_cu_left;

    /** array of left cu skip flags  (max of 8) (1 byte per 8x8)    */
    UWORD8 au1_cu_depth_left[8];

    /** scratch array of cb coded block flags for tu recursion      */
    UWORD8 au1_cbf_cb[2][MAX_TFR_DEPTH + 1];

    /** scratch array of cr coded block flags for tu recursion      */
    UWORD8 au1_cbf_cr[2][MAX_TFR_DEPTH + 1];

    /** current ctb x offset w.r.t frame start */
    WORD32 i4_ctb_x;

    /** current ctb y offset w.r.t frame start */
    WORD32 i4_ctb_y;

    //These values are never consumed apart from test-bench. Observed on June16 2014.
    /** current slice first ctb x offset w.r.t frame start */
    /** current slice first ctb y offset w.r.t frame start */
    WORD32 i4_ctb_slice_x;
    WORD32 i4_ctb_slice_y;

    /** Address of first CTB of next slice segment. In ctb unit */
    WORD32 i4_next_slice_seg_x;

    /** Address of first CTB of next slice segment. In ctb unit */
    WORD32 i4_next_slice_seg_y;

    /** sracth place holder for cu index of a ctb in context */
    WORD32 i4_cu_idx;

    /** sracth place holder for tu index of a cu in context  */
    WORD32 i4_tu_idx;

    /** pcm not supported currently; this parameter shall be 0 */
    WORD8 i1_ctb_num_pcm_blks;

    /** indicates if qp delta is to be coded in trasform unit of a cu */
    WORD8 i1_encode_qp_delta;

    /** place holder for current qp of a cu */
    WORD8 i1_cur_qp;

    /** log2ctbsize  indicated in SPS */
    WORD8 i1_log2_ctb_size;

    /**************************************************************************/
    /* Following are shared structures with the encoder loop                  */
    /* entropy context is not the owner of these and hence not allocated here */
    /**************************************************************************/
    /** pointer to current vps parameters    */
    vps_t *ps_vps;

    /** pointer to current sps parameters    */
    sps_t *ps_sps;

    /** pointer to current pps parameters    */
    pps_t *ps_pps;

    /** pointer to current sei parameters    */
    sei_params_t *ps_sei;

    /** pointer to current slice header parameters    */
    slice_header_t *ps_slice_hdr;

    /** pointer to frame level ctb structures prepared by main encode loop   */
    ctb_enc_loop_out_t *ps_frm_ctb;

    /**
     * array to store cu level qp for entire 64x64 ctb
     */
    WORD32 ai4_8x8_cu_qp[64];

    /**
     * flag to check if cbf all tu in a given cu is zero
     */
    WORD32 i4_is_cu_cbf_zero;

    /**
     * flag to enable / disbale residue encoding (used for RD opt bits estimate mode)
     */
    WORD32 i4_enable_res_encode;

    /*  flag to enable/disable insertion of SPS, VPS, PPS at CRA pictures   */
    WORD32 i4_sps_at_cdr_enable;

    /* quantization group position variables which stores the aligned position */
    WORD32 i4_qg_pos_x;

    WORD32 i4_qg_pos_y;

    void *pv_tile_params_base;

    s_pic_level_acc_info_t *ps_pic_level_info;

    void *pv_sys_api;

    /*  Flag to control dependent slices.
    0: Disable all slice segment limits
    1: Enforce max number of CTBs (not supported)
    2: Enforce max number of bytes */
    WORD32 i4_slice_segment_mode;

    /* Max number of CTBs/bytes in encoded slice. Will be used only when
    i4_slice_mode_enable is set to 1 or 2 in configuration file. This parameter is
    used for limiting the size of encoded slice under user-configured value */
    WORD32 i4_slice_segment_max_length;

    /* Accumulated number of CTBs/bytes in current slice */
    WORD32 i4_slice_seg_len;

    /** Number of slice segments generated per picture
        this parameter is to track the number of slices generated
        and comapre aganist MAX NUM VCL Nals allowed at a given level */
    WORD32 i4_num_slice_seg;

    /** Codec Level */
    WORD32 i4_codec_level;

    /**
    * number of neigbour cus coded as skips; Cannot exceed 2 (1 left, 1 top)
    */
    WORD32 i4_num_nbr_skip_cus;

    void *pv_dummy_out_buf;

    WORD32 i4_bitstream_buf_size;
} entropy_context_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
WORD32 ihevce_encode_transform_tree(
    entropy_context_t *ps_entropy_ctxt,
    WORD32 x0_ctb,
    WORD32 y0_ctb,
    WORD32 log2_tr_size,
    WORD32 tr_depth,
    WORD32 blk_num,
    cu_enc_loop_out_t *ps_enc_cu);

WORD32 ihevce_cabac_residue_encode(
    entropy_context_t *ps_entropy_ctxt, void *pv_coeff, WORD32 log2_tr_size, WORD32 is_luma);

WORD32 ihevce_cabac_residue_encode_rdopt(
    entropy_context_t *ps_entropy_ctxt,
    void *pv_coeff,
    WORD32 log2_tr_size,
    WORD32 is_luma,
    WORD32 perform_rdoq);

WORD32 ihevce_cabac_residue_encode_rdoq(
    entropy_context_t *ps_entropy_ctxt,
    void *pv_coeff,
    WORD32 log2_tr_size,
    WORD32 is_luma,
    void *ps_rdoq_ctxt_1,
    LWORD64 *pi8_tu_coded_dist,
    LWORD64 *pi8_not_coded_dist,
    WORD32 perform_sbh);

WORD32 ihevce_find_new_last_csb(
    WORD32 *pi4_subBlock2csbfId_map,
    WORD32 cur_last_csb_pos,
    void *pv_rdoq_ctxt,
    UWORD8 *pu1_trans_table,
    UWORD8 *pu1_csb_table,
    WORD16 *pi2_coeffs,
    WORD32 shift_value,
    WORD32 mask_value,
    UWORD8 **ppu1_addr);

WORD32 ihevce_code_all_sig_coeffs_as_0_explicitly(
    void *pv_rdoq_ctxt,
    WORD32 i,
    UWORD8 *pu1_trans_table,
    WORD32 is_luma,
    WORD32 scan_type,
    WORD32 infer_coeff,
    WORD32 nbr_csbf,
    cab_ctxt_t *ps_cabac);

void ihevce_copy_backup_ctxt(
    void *pv_dest, void *pv_src, void *pv_backup_ctxt_dest, void *pv_backup_ctxt_src);

WORD32 ihevce_cabac_encode_coding_unit(
    entropy_context_t *ps_entropy_ctxt,
    cu_enc_loop_out_t *ps_enc_cu,
    WORD32 cu_depth,
    WORD32 top_avail,
    WORD32 left_avail);

WORD32 ihevce_encode_slice_data(
    entropy_context_t *ps_entropy_ctxt,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 *pi4_end_of_slice_flag);

WORD32 ihevce_cabac_encode_sao(
    entropy_context_t *ps_entropy_ctxt, ctb_enc_loop_out_t *ps_ctb_enc_loop_out);

#endif /* _IHEVCE_ENTROPY_STRUCTS_H_ */
