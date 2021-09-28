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
*
* @file ihevce_cabac.h
*
* @brief
*  This file contains encoder cabac engine related structures and
*  interface prototypes
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_CABAC_H_
#define _IHEVCE_CABAC_H_

#include "ihevc_debug.h"
#include "ihevc_macros.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/**
*******************************************************************************
@brief Bit precision of cabac engine;
*******************************************************************************
 */
#define CABAC_BITS 9

/**
*******************************************************************************
@brief q format to account for the fractional bits encoded in cabac
*******************************************************************************
 */
#define CABAC_FRAC_BITS_Q 12

/**
*******************************************************************************
@brief Enables bit-efficient chroma cbf signalling by peeking into cbfs of
       children nodes
*******************************************************************************
 */
#define CABAC_BIT_EFFICIENT_CHROMA_PARENT_CBF 1

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
*******************************************************************************
@brief converts floating point number to CABAC_FRAC_BITS_Q q format and
       rounds the results to 16 bit integer
*******************************************************************************
 */
#define ROUND_Q12(x) ((UWORD16)(((x) * (1 << CABAC_FRAC_BITS_Q)) + 0.5))

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/**
*******************************************************************************
@brief Enums for controlling the operating mode of cabac engine
*******************************************************************************
 */
typedef enum
{
    /** in this mode, bits are encoded in the bit stream buffer */
    CABAC_MODE_ENCODE_BITS = 0,

    /** in this mode, only num bits gen are computed but not put in the stream */
    CABAC_MODE_COMPUTE_BITS = 1

} CABAC_OP_MODE;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      Cabac context for encoder
******************************************************************************
 */
typedef struct cab_ctxt
{
    /**
     * indicates if cabac encode works in put bits mode or bit compute mode
     * In puts bits mode, bitstream and cabac engine fields L,R etc are used
     * In bit compute mode, bitstream and cabac engine fields are not used
     */
    CABAC_OP_MODE e_cabac_op_mode;

    /**
     * total bits estimated (for a cu) when mode is CABAC_MODE_COMPUTE_BITS
     * This is in q12 format to account for the fractional bits as well
     */
    UWORD32 u4_bits_estimated_q12;

    /**
     * total texture bits estimated (for a cu) when mode is CABAC_MODE_COMPUTE_BITS
     * This is in q12 format to account for the fractional bits as well
     */
    UWORD32 u4_texture_bits_estimated_q12;

    /**
     * total header bits estimated (for a cu) when mode is CABAC_MODE_COMPUTE_BITS
     * This is in q12 format to account for the fractional bits as well
     */
    UWORD32 u4_header_bits_estimated_q12;

    UWORD32 u4_cbf_bits_q12;

    UWORD32 u4_true_tu_split_flag_q12;
    /*********************************************************************/
    /*  CABAC ENGINE related fields; not used in CABAC_MODE_COMPUTE_BITS */
    /*********************************************************************/
    /** cabac interval range R  */
    UWORD32 u4_range;

    /** cabac interval start L  */
    UWORD32 u4_low;

    /** bits generated during renormalization
     *  A byte is put to stream/u4_out_standing_bytes from u4_low(L) when
     *  u4_bits_gen exceeds 8
     */
    UWORD32 u4_bits_gen;

    /** bytes_outsanding; number of 0xFF bits that occur during renorm
     *  These  will be accumulated till the carry bit is knwon
     */
    UWORD32 u4_out_standing_bytes;

    /*************************************************************************/
    /*  OUTPUT Bitstream related fields; not used in CABAC_MODE_COMPUTE_BITS */
    /*************************************************************************/
    /** points to start of stream buffer.    */
    UWORD8 *pu1_strm_buffer;

    /**
     *  max bitstream size (in bytes).
     *  Encoded stream shall not exceed this size.
     */
    UWORD32 u4_max_strm_size;

    /**
    `*  byte offset (w.r.t pu1_strm_buffer) where next byte would be written
     *  Bitstream engine makes sure it would not corrupt data beyond
     *  u4_max_strm_size bytes
     */
    UWORD32 u4_strm_buf_offset;

    /**
     *  signifies the number of consecutive zero bytes propogated from previous
     *  word. It is used for emulation prevention byte insertion in the stream
     */
    WORD32 i4_zero_bytes_run;

    /*********************************************************************/
    /*  CABAC context models                                             */
    /*********************************************************************/
    /** All Context models stored in packed form pState[bits6-1] | MPS[bit0] */
    UWORD8 au1_ctxt_models[IHEVC_CAB_CTXT_END];

    /**
     *Cabac context for start of every row which is same as top right ctxt
     */
    UWORD8 au1_ctxt_models_top_right[IHEVC_CAB_CTXT_END];

    /**
     * copy of enable entropy coding sync flag in pps
     */
    WORD8 i1_entropy_coding_sync_enabled_flag;

    /**
     * store the bitstream offset from which first slice data is generated by cabac
     */
    UWORD32 u4_first_slice_start_offset;

} cab_ctxt_t;

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern UWORD16 gau2_ihevce_cabac_bin_to_bits[64 * 2];

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
WORD32
    ihevce_cabac_reset(cab_ctxt_t *ps_cabac, bitstrm_t *ps_bitstrm, CABAC_OP_MODE e_cabac_op_mode);

WORD32 ihevce_cabac_init(
    cab_ctxt_t *ps_cabac,
    bitstrm_t *ps_bitstrm,
    WORD32 slice_qp,
    WORD32 cabac_init_idc,
    CABAC_OP_MODE e_cabac_op_mode);

WORD32 ihevce_cabac_put_byte(cab_ctxt_t *ps_cabac);

/**
******************************************************************************
*
*  @brief Codes a bin based on probablilty and mps packed context model
*
*  @par   Description
*  1. Apart from encoding bin, context model is updated as per state transition
*  2. Range and Low renormalization is done based on bin and original state
*  3. After renorm bistream is updated (if required)
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @param[in]   bin
*  bin(boolean) to be encoded
*
*  @param[in]   ctxt_index
*  index of cabac context model containing pState[bits6-1] | MPS[bit0]
*
*  @return      success or failure error code
*
******************************************************************************
*/
static INLINE WORD32 ihevce_cabac_encode_bin(cab_ctxt_t *ps_cabac, WORD32 bin, WORD32 ctxt_index)
{
    UWORD32 u4_range = ps_cabac->u4_range;
    UWORD32 u4_low = ps_cabac->u4_low;
    UWORD32 u4_rlps;
    UWORD8 *pu1_ctxt_model = &ps_cabac->au1_ctxt_models[ctxt_index];
    WORD32 state_mps = *pu1_ctxt_model;
    WORD32 shift;

    /* Sanity checks */
    ASSERT((bin == 0) || (bin == 1));
    ASSERT((ctxt_index >= 0) && (ctxt_index < IHEVC_CAB_CTXT_END));
    ASSERT(state_mps < 128);

    if(CABAC_MODE_ENCODE_BITS == ps_cabac->e_cabac_op_mode)
    {
        ASSERT((u4_range >= 256) && (u4_range < 512));

        /* Get the lps range from LUT based on quantized range and state */
        u4_rlps = gau1_ihevc_cabac_rlps[state_mps >> 1][(u4_range >> 6) & 0x3];

        u4_range -= u4_rlps;

        /* check if bin is mps or lps */
        if((state_mps & 0x1) ^ bin)
        {
            /* lps path;  L= L + R; R = RLPS */
            u4_low += u4_range;
            u4_range = u4_rlps;
        }

        /*Compute bit always to populate the trace*/
        /* increment bits generated based on state and bin encoded */
        ps_cabac->u4_bits_estimated_q12 += gau2_ihevce_cabac_bin_to_bits[state_mps ^ bin];

        /* update the context model from state transition LUT */
        *pu1_ctxt_model = gau1_ihevc_next_state[(state_mps << 1) | bin];

        /*****************************************************************/
        /* Renormalization; calculate bits generated based on range(R)   */
        /* Note : 6 <= R < 512; R is 2 only for terminating encode       */
        /*****************************************************************/
        GETRANGE(shift, u4_range);
        shift = 9 - shift;
        u4_low <<= shift;
        u4_range <<= shift;

        /* bits to be inserted in the bitstream */
        ps_cabac->u4_bits_gen += shift;
        ps_cabac->u4_range = u4_range;
        ps_cabac->u4_low = u4_low;

        /* generate stream when a byte is ready */
        if(ps_cabac->u4_bits_gen > CABAC_BITS)
        {
            return (ihevce_cabac_put_byte(ps_cabac));
        }
    }
    else /* (CABAC_MODE_COMPUTE_BITS == e_cabac_op_mode) */
    {
        /* increment bits generated based on state and bin encoded */
        ps_cabac->u4_bits_estimated_q12 += gau2_ihevce_cabac_bin_to_bits[state_mps ^ bin];

        /* update the context model from state transition LUT */
        *pu1_ctxt_model = gau1_ihevc_next_state[(state_mps << 1) | bin];
    }

    return (IHEVCE_SUCCESS);
}

WORD32 ihevce_cabac_encode_bypass_bin(cab_ctxt_t *ps_cabac, WORD32 bin);

WORD32
    ihevce_cabac_encode_terminate(cab_ctxt_t *ps_cabac, WORD32 term_bin, WORD32 i4_end_of_sub_strm);

/**
******************************************************************************
*
*  @brief Encodes a series of bypass bins (FLC bypass bins)
*
*  @par   Description
*  This function is more optimal than calling ihevce_cabac_encode_bypass_bin()
*  in a loop as cabac low, renorm and generating the stream (8bins at a time)
*  can be done in one operation
*
*  @param[inout]ps_cabac
*   pointer to cabac context (handle)
*
*  @param[in]   u4_sym
*   syntax element to be coded (as FLC bins)
*
*  @param[in]   num_bins
*   This is the FLC length for u4_sym
*
*
*  @return      success or failure error code
*
******************************************************************************
*/
static INLINE WORD32
    ihevce_cabac_encode_bypass_bins(cab_ctxt_t *ps_cabac, UWORD32 u4_bins, WORD32 num_bins)
{
    UWORD32 u4_range = ps_cabac->u4_range;
    WORD32 next_byte;
    WORD32 error = IHEVCE_SUCCESS;

    if(CABAC_MODE_ENCODE_BITS == ps_cabac->e_cabac_op_mode)
    {
        /* Sanity checks */
        ASSERT((num_bins < 33) && (num_bins > 0));
        ASSERT((u4_range >= 256) && (u4_range < 512));

        /*Compute bit always to populate the trace*/
        /* increment bits generated by num_bins */
        ps_cabac->u4_bits_estimated_q12 += (num_bins << CABAC_FRAC_BITS_Q);

        /* Encode 8bins at a time and put in the bit-stream */
        while(num_bins > 8)
        {
            num_bins -= 8;

            /* extract the leading 8 bins */
            next_byte = (u4_bins >> num_bins) & 0xff;

            /*  L = (L << 8) +  (R * next_byte) */
            ps_cabac->u4_low <<= 8;
            ps_cabac->u4_low += (next_byte * u4_range);
            ps_cabac->u4_bits_gen += 8;

            if(ps_cabac->u4_bits_gen > CABAC_BITS)
            {
                /*  insert the leading byte of low into stream */
                error |= ihevce_cabac_put_byte(ps_cabac);
            }
        }

        /* Update low with remaining bins and return */
        next_byte = (u4_bins & ((1 << num_bins) - 1));

        ps_cabac->u4_low <<= num_bins;
        ps_cabac->u4_low += (next_byte * u4_range);
        ps_cabac->u4_bits_gen += num_bins;

        if(ps_cabac->u4_bits_gen > CABAC_BITS)
        {
            /*  insert the leading byte of low into stream */
            error |= ihevce_cabac_put_byte(ps_cabac);
        }
    }
    else
    {
        /* increment bits generated by num_bins */
        ps_cabac->u4_bits_estimated_q12 += (num_bins << CABAC_FRAC_BITS_Q);
    }

    return (error);
}

WORD32 ihevce_cabac_encode_tunary(
    cab_ctxt_t *ps_cabac,
    WORD32 sym,
    WORD32 c_max,
    WORD32 ctxt_index,
    WORD32 ctxt_shift,
    WORD32 ctxt_inc_max);

WORD32 ihevce_cabac_encode_tunary_bypass(cab_ctxt_t *ps_cabac, WORD32 sym, WORD32 c_max);

WORD32 ihevce_cabac_encode_egk(cab_ctxt_t *ps_cabac, UWORD32 u4_sym, WORD32 k);

WORD32 ihevce_cabac_encode_trunc_rice(
    cab_ctxt_t *ps_cabac, UWORD32 u4_sym, WORD32 c_rice_param, WORD32 c_rice_max);

WORD32 ihevce_cabac_flush(cab_ctxt_t *ps_cabac, WORD32 i4_end_of_sub_strm);

WORD32 ihevce_cabac_ctxt_backup(cab_ctxt_t *ps_cabac);

WORD32 ihevce_cabac_ctxt_row_init(cab_ctxt_t *ps_cabac);

#endif /* _IHEVCE_CABAC_H_ */
