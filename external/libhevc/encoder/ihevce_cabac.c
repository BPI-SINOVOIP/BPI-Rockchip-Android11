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
* @file ihevce_cabac.c
*
* @brief
*  This file contains function definitions related to bitstream generation
*
* @author
*  ittiam
*
* @List of Functions
*  ihevce_cabac_reset()
*  ihevce_cabac_init()
*  ihevce_cabac_put_byte()
*  ihevce_cabac_encode_bin()
*  ihevce_cabac_encode_bypass_bin()
*  ihevce_cabac_encode_terminate()
*  ihevce_cabac_encode_tunary()
*  ihevce_cabac_encode_tunary_bypass()
*  ihevce_cabac_encode_bypass_bins()
*  ihevce_cabac_encode_egk()
*  ihevce_cabac_encode_trunc_rice()
*  ihevce_cabac_encode_trunc_rice_ctxt()
*  ihevce_cabac_flush()
*  ihevce_cabac_ctxt_backup()
*  ihevce_cabac_ctxt_row_init()
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_debug.h"
#include "ihevc_macros.h"
#include "ihevc_platform_macros.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"

#define TEST_CABAC_BITESTIMATE 0

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Resets the encoder cabac engine
*
*  @par   Description
*  This routine needs to be called at start of dependent slice encode
*
*  @param[inout]   ps_cabac_ctxt
*  pointer to cabac context (handle)
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   e_cabac_op_mode
*  opertaing mode of cabac; put bits / compute bits mode @sa CABAC_OP_MODE
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32
    ihevce_cabac_reset(cab_ctxt_t *ps_cabac, bitstrm_t *ps_bitstrm, CABAC_OP_MODE e_cabac_op_mode)
{
    /* Sanity checks */
    ASSERT(ps_cabac != NULL);
    ASSERT(
        (e_cabac_op_mode == CABAC_MODE_ENCODE_BITS) ||
        (e_cabac_op_mode == CABAC_MODE_COMPUTE_BITS));

    ps_cabac->e_cabac_op_mode = e_cabac_op_mode;

    if(CABAC_MODE_ENCODE_BITS == e_cabac_op_mode)
    {
        ASSERT(ps_bitstrm != NULL);

        /* Bitstream context initialization */
        ps_cabac->pu1_strm_buffer = ps_bitstrm->pu1_strm_buffer;
        ps_cabac->u4_max_strm_size = ps_bitstrm->u4_max_strm_size;
        /* When entropy sync is enabled start form fixed offset from point
         * where slice header extension has ended to handle emulation prevention
         *  bytes during insertion of slice offset at end of frame */
        if(1 == ps_cabac->i1_entropy_coding_sync_enabled_flag)
        {
            ps_cabac->u4_strm_buf_offset = ps_cabac->u4_first_slice_start_offset;
        }
        else
        {
            ps_cabac->u4_strm_buf_offset = ps_bitstrm->u4_strm_buf_offset;
        }
        ps_cabac->i4_zero_bytes_run = ps_bitstrm->i4_zero_bytes_run;

        /* cabac engine initialization */
        ps_cabac->u4_low = 0;
        ps_cabac->u4_range = 510;
        ps_cabac->u4_bits_gen = 0;
        ps_cabac->u4_out_standing_bytes = 0;
    }
    else /* (CABAC_MODE_COMPUTE_BITS == e_cabac_op_mode) */
    {
        /* reset the bits estimated */
        ps_cabac->u4_bits_estimated_q12 = 0;

        /* reset the texture bits estimated */
        ps_cabac->u4_texture_bits_estimated_q12 = 0;

        /* Setting range to 0 switches off AEV_TRACE in compute bits mode */
        ps_cabac->u4_range = 0;
    }

    return (IHEVCE_SUCCESS);
}

/**
******************************************************************************
*
*  @brief Initializes the encoder cabac engine
*
*  @par   Description
*  This routine needs to be called at start of slice/frame encode
*
*  @param[inout]   ps_cabac_ctxt
*  pointer to cabac context (handle)
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   qp
*  current slice qp
*
*  @param[in]   cabac_init_idc
*  current slice init idc (range - [0- 2])*
*
*  @param[in]   e_cabac_op_mode
*  opertaing mode of cabac; put bits / compute bits mode @sa CABAC_OP_MODE
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_init(
    cab_ctxt_t *ps_cabac,
    bitstrm_t *ps_bitstrm,
    WORD32 slice_qp,
    WORD32 cabac_init_idc,
    CABAC_OP_MODE e_cabac_op_mode)
{
    /* Sanity checks */
    ASSERT(ps_cabac != NULL);
    ASSERT((slice_qp >= 0) && (slice_qp < IHEVC_MAX_QP));
    ASSERT((cabac_init_idc >= 0) && (cabac_init_idc < 3));
    ASSERT(
        (e_cabac_op_mode == CABAC_MODE_ENCODE_BITS) ||
        (e_cabac_op_mode == CABAC_MODE_COMPUTE_BITS));

    ps_cabac->e_cabac_op_mode = e_cabac_op_mode;

    if(CABAC_MODE_ENCODE_BITS == e_cabac_op_mode)
    {
        ASSERT(ps_bitstrm != NULL);

        /* Bitstream context initialization */
        ps_cabac->pu1_strm_buffer = ps_bitstrm->pu1_strm_buffer;
        ps_cabac->u4_max_strm_size = ps_bitstrm->u4_max_strm_size;
        /* When entropy sync is enabled start form fixed offset from point
         * where slice header extension has ended to handle emulation prevention
         *  bytes during insertion of slice offset at end of frame */
        if(1 == ps_cabac->i1_entropy_coding_sync_enabled_flag)
        {
            ps_cabac->u4_strm_buf_offset = ps_cabac->u4_first_slice_start_offset;
        }
        else
        {
            ps_cabac->u4_strm_buf_offset = ps_bitstrm->u4_strm_buf_offset;
        }
        ps_cabac->i4_zero_bytes_run = ps_bitstrm->i4_zero_bytes_run;

        /* cabac engine initialization */
        ps_cabac->u4_low = 0;
        ps_cabac->u4_range = 510;
        ps_cabac->u4_bits_gen = 0;
        ps_cabac->u4_out_standing_bytes = 0;

        /* reset the bits estimated */
        ps_cabac->u4_bits_estimated_q12 = 0;

        /* reset the texture bits estimated */
        ps_cabac->u4_texture_bits_estimated_q12 = 0;
    }
    else /* (CABAC_MODE_COMPUTE_BITS == e_cabac_op_mode) */
    {
        /* reset the bits estimated */
        ps_cabac->u4_bits_estimated_q12 = 0;

        /* reset the texture bits estimated */
        ps_cabac->u4_texture_bits_estimated_q12 = 0;

        /* Setting range to 0 switches off AEV_TRACE in compute bits mode */
        ps_cabac->u4_range = 0;
    }

    /* cabac context initialization based on init idc and slice qp */
    COPY_CABAC_STATES(
        ps_cabac->au1_ctxt_models,
        &gau1_ihevc_cab_ctxts[cabac_init_idc][slice_qp][0],
        IHEVC_CAB_CTXT_END);

    return (IHEVCE_SUCCESS);
}

/**
******************************************************************************
*
*  @brief Puts new byte (and outstanding bytes) into bitstream after cabac
*         renormalization
*
*  @par   Description
*  1. Extract the leading byte of low(L)
*  2. If leading byte=0xff increment outstanding bytes and return
*     (as the actual bits depend on carry propogation later)
*  3. If leading byte is not 0xff check for any carry propogation
*  4. Insert the carry (propogated in previous byte) along with outstanding
*     bytes (if any) and leading byte
*
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_put_byte(cab_ctxt_t *ps_cabac)
{
    UWORD32 u4_low = ps_cabac->u4_low;
    UWORD32 u4_bits_gen = ps_cabac->u4_bits_gen;
    WORD32 lead_byte = u4_low >> (u4_bits_gen + CABAC_BITS - 8);

    /* Sanity checks */
    ASSERT((ps_cabac->u4_range >= 256) && (ps_cabac->u4_range < 512));
    ASSERT((u4_bits_gen >= 8));

    /* update bits generated and low after extracting leading byte */
    u4_bits_gen -= 8;
    ps_cabac->u4_low &= ((1 << (CABAC_BITS + u4_bits_gen)) - 1);
    ps_cabac->u4_bits_gen = u4_bits_gen;

    /************************************************************************/
    /* 1. Extract the leading byte of low(L)                                */
    /* 2. If leading byte=0xff increment outstanding bytes and return       */
    /*      (as the actual bits depend on carry propogation later)          */
    /* 3. If leading byte is not 0xff check for any carry propogation       */
    /* 4. Insert the carry (propogated in previous byte) along with         */
    /*    outstanding bytes (if any) and leading byte                       */
    /************************************************************************/
    if(lead_byte == 0xff)
    {
        /* actual bits depend on carry propogration     */
        ps_cabac->u4_out_standing_bytes++;
        return (IHEVCE_SUCCESS);
    }
    else
    {
        /* carry = 1 => putbit(1); carry propogated due to L renorm */
        WORD32 carry = (lead_byte >> 8) & 0x1;
        UWORD8 *pu1_strm_buf = ps_cabac->pu1_strm_buffer;
        UWORD32 u4_strm_buf_offset = ps_cabac->u4_strm_buf_offset;
        WORD32 zero_run = ps_cabac->i4_zero_bytes_run;
        UWORD32 u4_out_standing_bytes = ps_cabac->u4_out_standing_bytes;

        /*********************************************************************/
        /* Bitstream overflow check                                          */
        /* NOTE: corner case of epb bytes (max 2 for 32bit word) not handled */
        /*********************************************************************/
        if((u4_strm_buf_offset + u4_out_standing_bytes + 1) >= ps_cabac->u4_max_strm_size)
        {
            /* return without corrupting the buffer beyond its size */
            return (IHEVCE_BITSTREAM_BUFFER_OVERFLOW);
        }

        /*********************************************************************/
        /*        Insert the carry propogated in previous byte               */
        /*                                                                   */
        /* Note : Do not worry about corruption into slice header align byte */
        /*        This is because the first bin cannot result in overflow    */
        /*********************************************************************/
        if(carry)
        {
            /* CORNER CASE: if the previous data is 0x000003, then EPB will be inserted
            and the data will become 0x00000303 and if the carry is present, it will
            be added with the last byte and it will become 0x00000304 which is not correct
            as per standard*/
            /* so check for previous four bytes and if it is equal to 0x00000303
            then subtract u4_strm_buf_offset by 1 */
            if(pu1_strm_buf[u4_strm_buf_offset - 1] == 0x03 &&
               pu1_strm_buf[u4_strm_buf_offset - 2] == 0x03 &&
               pu1_strm_buf[u4_strm_buf_offset - 3] == 0x00 &&
               pu1_strm_buf[u4_strm_buf_offset - 4] == 0x00)
            {
                u4_strm_buf_offset -= 1;
            }
            /* previous byte carry add will not result in overflow to        */
            /* u4_strm_buf_offset - 2 as we track 0xff as outstanding bytes  */
            pu1_strm_buf[u4_strm_buf_offset - 1] += carry;
            zero_run = 0;
        }

        /*        Insert outstanding bytes (if any)         */
        while(u4_out_standing_bytes)
        {
            UWORD8 u1_0_or_ff = carry ? 0 : 0xFF;

            PUTBYTE_EPB(pu1_strm_buf, u4_strm_buf_offset, u1_0_or_ff, zero_run);

            u4_out_standing_bytes--;
        }
        ps_cabac->u4_out_standing_bytes = 0;

        /*        Insert the leading byte                   */
        lead_byte &= 0xFF;
        PUTBYTE_EPB(pu1_strm_buf, u4_strm_buf_offset, lead_byte, zero_run);

        /* update the state variables and return success */
        ps_cabac->u4_strm_buf_offset = u4_strm_buf_offset;
        ps_cabac->i4_zero_bytes_run = zero_run;
        return (IHEVCE_SUCCESS);
    }
}

/**
******************************************************************************
*
*  @brief Codes a bypass bin (equi probable 0 / 1)
*
*  @par   Description
*  After encoding bypass bin, bits gen incremented by 1 and bitstream generated
*
*  @param[inout]  ps_cabac : pointer to cabac context (handle)
*
*  @param[in]   bin :  bypass bin(0/1) to be encoded
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_bypass_bin(cab_ctxt_t *ps_cabac, WORD32 bin)
{
    UWORD32 u4_range = ps_cabac->u4_range;
    UWORD32 u4_low = ps_cabac->u4_low;

    if(CABAC_MODE_ENCODE_BITS == ps_cabac->e_cabac_op_mode)
    {
        /* Sanity checks */
        ASSERT((u4_range >= 256) && (u4_range < 512));
        ASSERT((bin == 0) || (bin == 1));

        /*Compute bit always to populate the trace*/
        /* increment bits generated by 1 */
        ps_cabac->u4_bits_estimated_q12 += (1 << CABAC_FRAC_BITS_Q);

        u4_low <<= 1;
        /* add range if bin is 1 */
        if(bin)
        {
            u4_low += u4_range;
        }

        /* 1 bit to be inserted in the bitstream */
        ps_cabac->u4_bits_gen++;
        ps_cabac->u4_low = u4_low;

        /* generate stream when a byte is ready */
        if(ps_cabac->u4_bits_gen > CABAC_BITS)
        {
            return (ihevce_cabac_put_byte(ps_cabac));
        }
    }
    else /* (CABAC_MODE_COMPUTE_BITS == e_cabac_op_mode) */
    {
        /* increment bits generated by 1 */
        ps_cabac->u4_bits_estimated_q12 += (1 << CABAC_FRAC_BITS_Q);
    }

    return (IHEVCE_SUCCESS);
}

/**
******************************************************************************
*
*  @brief Codes a terminate bin (1:terminate 0:do not terminate)
*
*  @par   Description
*  After encoding bypass bin, bits gen incremented by 1 and bitstream generated
*
*  @param[inout]  ps_cabac : pointer to cabac context (handle)
*
*  @param[in]   term_bin : (1:terminate 0:do not terminate)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32
    ihevce_cabac_encode_terminate(cab_ctxt_t *ps_cabac, WORD32 term_bin, WORD32 i4_end_of_sub_strm)
{
    UWORD32 u4_range = ps_cabac->u4_range;
    UWORD32 u4_low = ps_cabac->u4_low;
    UWORD32 u4_rlps;
    WORD32 shift;
    WORD32 error = IHEVCE_SUCCESS;

    /* Sanity checks */
    ASSERT((u4_range >= 256) && (u4_range < 512));
    ASSERT((term_bin == 0) || (term_bin == 1));

    /*  term_bin = 1 has lps range = 2 */
    u4_rlps = 2;
    u4_range -= u4_rlps;

    /* if terminate L is incremented by curR and R=2 */
    if(term_bin)
    {
        /* lps path;  L= L + R; R = RLPS */
        u4_low += u4_range;
        u4_range = u4_rlps;
    }

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
        error = ihevce_cabac_put_byte(ps_cabac);
    }

    if(term_bin)
    {
        ihevce_cabac_flush(ps_cabac, i4_end_of_sub_strm);
    }

    /*Compute bit always to populate the trace*/
    ps_cabac->u4_bits_estimated_q12 += gau2_ihevce_cabac_bin_to_bits[(62 << 1) | term_bin];

    return (error);
}

/**
******************************************************************************
*
*  @brief Encodes a truncated unary symbol associated with context model(s)
*
*  @par   Description
*  Does binarization of tunary symbol as per sec 9.3.2.2 and does the cabac
*  encoding of each bin. This is used for computing symbols like qp_delta,
*  last_sig_coeff_prefix_x, last_sig_coeff_prefix_y.
*
*  The context models associated with each bin is computed as :
*   current bin context = "base context idx" + (bin_idx >> shift)
*  where
*   1. "base context idx" is the base index for the syntax element
*   2. "bin_idx" is the current bin index of the unary code
*   3. "shift" is the shift factor associated with this syntax element
*
*  @param[inout]ps_cabac
*   pointer to cabac context (handle)
*
*  @param[in]   sym
*   syntax element to be coded as truncated unary bins
*
*  @param[in]   c_max
*   maximum value of sym (required for tunary binarization)
*
*  @param[in]   ctxt_index
*   base context model index for this syntax element
*
*  @param[in]   ctxt_shift
*   shift factor for context increments associated with this syntax element
*
*  @param[in]   ctxt_inc_max
*   max value of context increment beyond which all bins will use same ctxt
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_tunary(
    cab_ctxt_t *ps_cabac,
    WORD32 sym,
    WORD32 c_max,
    WORD32 ctxt_index,
    WORD32 ctxt_shift,
    WORD32 ctxt_inc_max)
{
    WORD32 bin_ctxt, i;
    WORD32 error = IHEVCE_SUCCESS;

    /* Sanity checks */
    ASSERT(c_max > 0);
    ASSERT((sym <= c_max) && (sym >= 0));
    ASSERT((ctxt_index >= 0) && (ctxt_index < IHEVC_CAB_CTXT_END));
    ASSERT((ctxt_index + (c_max >> ctxt_shift)) < IHEVC_CAB_CTXT_END);

    /* Special case of sym= 0 */
    if(0 == sym)
    {
        return (ihevce_cabac_encode_bin(ps_cabac, 0, ctxt_index));
    }

    /* write '1' bins  */
    for(i = 0; i < sym; i++)
    {
        /* TODO: encode bin to be inlined later */
        bin_ctxt = ctxt_index + MIN((i >> ctxt_shift), ctxt_inc_max);
        error |= ihevce_cabac_encode_bin(ps_cabac, 1, bin_ctxt);
    }

    /* write terminating 0 bin */
    if(sym < c_max)
    {
        /* TODO: encode bin to be inlined later */
        bin_ctxt = ctxt_index + MIN((i >> ctxt_shift), ctxt_inc_max);
        error |= ihevce_cabac_encode_bin(ps_cabac, 0, bin_ctxt);
    }

    return (error);
}

/**
******************************************************************************
*
*  @brief Encodes a syntax element as truncated unary bypass bins
*
*  @par   Description
*  Does binarization of tunary symbol as per sec 9.3.2.2 and does the cabac
*  encoding of each bin. This is used for computing symbols like merge_idx,
*  mpm_idx etc
*
*  @param[inout]ps_cabac
*   pointer to cabac context (handle)
*
*  @param[in]   sym
*   syntax element to be coded as truncated unary bins
*
*  @param[in]   c_max
*   maximum value of sym (required for tunary binarization)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_tunary_bypass(cab_ctxt_t *ps_cabac, WORD32 sym, WORD32 c_max)
{
    WORD32 error = IHEVCE_SUCCESS;
    WORD32 length;
    WORD32 u4_bins;

    /* Sanity checks */
    ASSERT(c_max > 0);
    ASSERT((sym <= c_max) && (sym >= 0));

    if(sym < c_max)
    {
        /* unary code with (sym) '1's and terminating '0' bin */
        length = (sym + 1);
        u4_bins = (1 << length) - 2;
    }
    else
    {
        /* tunary code with (sym) '1's */
        length = sym;
        u4_bins = (1 << length) - 1;
    }

    /* Encode the tunary binarized code as bypass bins */
    error = ihevce_cabac_encode_bypass_bins(ps_cabac, u4_bins, length);

    return (error);
}

/**
******************************************************************************
*
*  @brief Encodes a syntax element as kth order Exp-Golomb code (EGK)
*
*  @par   Description
*  Does binarization of symbol as per sec 9.3.2.4  kth order Exp-Golomb(EGk)
*  process and encodes the resulting bypass bins
*
*  @param[inout]ps_cabac
*   pointer to cabac context (handle)
*
*  @param[in]   u4_sym
*   syntax element to be coded as EGK
*
*  @param[in]   k
*   order of EGk
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_egk(cab_ctxt_t *ps_cabac, UWORD32 u4_sym, WORD32 k)
{
    WORD32 num_bins, unary_length;
    UWORD32 u4_sym_shiftk_plus1, u4_egk, u4_unary_bins;

    WORD32 error = IHEVCE_SUCCESS;

    /* Sanity checks */
    ASSERT((k >= 0));
    /* ASSERT(u4_sym >= (UWORD32)(1 << k)); */

    /************************************************************************/
    /* shift symbol by k bits to find unary code prefix (111110)            */
    /* Use GETRANGE to elminate the while loop in sec 9.3.2.4 of HEVC spec  */
    /************************************************************************/
    u4_sym_shiftk_plus1 = (u4_sym >> k) + 1;
    /* GETRANGE(unary_length, (u4_sym_shiftk_plus1 + 1)); */
    GETRANGE(unary_length, u4_sym_shiftk_plus1);

    /* unary code with (unary_length-1) '1's and terminating '0' bin */
    u4_unary_bins = (1 << unary_length) - 2;

    /* insert the symbol prefix of (unary lenght - 1)  bins */
    u4_egk = (u4_unary_bins << (unary_length - 1)) |
             (u4_sym_shiftk_plus1 & ((1 << (unary_length - 1)) - 1));

    /* insert last k bits of symbol in the end */
    u4_egk = (u4_egk << k) | (u4_sym & ((1 << k) - 1));

    /* length of the code = 2 *(unary_length - 1) + 1 + k */
    num_bins = (2 * unary_length) - 1 + k;

    /* Encode the egk binarized code as bypass bins */
    error = ihevce_cabac_encode_bypass_bins(ps_cabac, u4_egk, num_bins);

    return (error);
}

/**
******************************************************************************
*
*  @brief Encodes a syntax element as truncated rice code (TR)
*
*  @par   Description
*  Does binarization of symbol as per sec 9.3.2.3 Truncated Rice(TR)
*  binarization process and encodes the resulting bypass bins
*  This function ise used for coeff_abs_level_remaining coding when
*  level is less than c_rice_max
*
*  @param[inout]ps_cabac
*   pointer to cabac context (handle)
*
*  @param[in]   u4_sym
*   syntax element to be coded as truncated rice code
*
*  @param[in]   c_rice_param
*    shift factor for truncated unary prefix coding of (u4_sym >> c_rice_param)
*
*  @param[in]   c_rice_max
*    max symbol val below which a suffix is coded as (u4_sym%(1<<c_rice_param))
*    This is currently (4 << c_rice_param) for coeff_abs_level_remaining
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_trunc_rice(
    cab_ctxt_t *ps_cabac, UWORD32 u4_sym, WORD32 c_rice_param, WORD32 c_rice_max)
{
    WORD32 num_bins, unary_length, u4_unary_bins;
    UWORD32 u4_tr;

    WORD32 error = IHEVCE_SUCCESS;

    (void)c_rice_max;
    /* Sanity checks */
    ASSERT((c_rice_param >= 0));
    ASSERT((UWORD32)c_rice_max > u4_sym);

    /************************************************************************/
    /* shift symbol by c_rice_param bits to find unary code prefix (111.10) */
    /************************************************************************/
    unary_length = (u4_sym >> c_rice_param) + 1;

    /* unary code with (unary_length-1) '1's and terminating '0' bin */
    u4_unary_bins = (1 << unary_length) - 2;

    /* insert last c_rice_param bits of symbol in the end */
    u4_tr = (u4_unary_bins << c_rice_param) | (u4_sym & ((1 << c_rice_param) - 1));

    /* length of the code */
    num_bins = unary_length + c_rice_param;

    /* Encode the tr binarized code as bypass bins */
    error = ihevce_cabac_encode_bypass_bins(ps_cabac, u4_tr, num_bins);

    return (error);
}

/**
******************************************************************************
*
*  @brief Flushes the cabac encoder engine as per section 9.3.4 figure 9-12
*
*  @par   Description
*
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_flush(cab_ctxt_t *ps_cabac, WORD32 i4_end_of_sub_strm)
{
    UWORD32 u4_low = ps_cabac->u4_low;
    UWORD32 u4_bits_gen = ps_cabac->u4_bits_gen;

    UWORD8 *pu1_strm_buf = ps_cabac->pu1_strm_buffer;
    UWORD32 u4_strm_buf_offset = ps_cabac->u4_strm_buf_offset;
    WORD32 zero_run = ps_cabac->i4_zero_bytes_run;
    UWORD32 u4_out_standing_bytes = ps_cabac->u4_out_standing_bytes;

    (void)i4_end_of_sub_strm;
    /************************************************************************/
    /* Insert the carry (propogated in previous byte) along with            */
    /* outstanding bytes (if any) and flush remaining bits                  */
    /************************************************************************/

    //TODO: Review this function
    {
        /* carry = 1 => putbit(1); carry propogated due to L renorm */
        WORD32 carry = (u4_low >> (u4_bits_gen + CABAC_BITS)) & 0x1;
        WORD32 last_byte;
        WORD32 bits_left;
        WORD32 rem_bits;

        /*********************************************************************/
        /* Bitstream overflow check                                          */
        /* NOTE: corner case of epb bytes (max 2 for 32bit word) not handled */
        /*********************************************************************/
        if((u4_strm_buf_offset + u4_out_standing_bytes + 1) >= ps_cabac->u4_max_strm_size)
        {
            /* return without corrupting the buffer beyond its size */
            return (IHEVCE_BITSTREAM_BUFFER_OVERFLOW);
        }

        if(carry)
        {
            /* CORNER CASE: if the previous data is 0x000003, then EPB will be inserted
            and the data will become 0x00000303 and if the carry is present, it will
            be added with the last byte and it will become 0x00000304 which is not correct
            as per standard*/
            /* so check for previous four bytes and if it is equal to 0x00000303
            then subtract u4_strm_buf_offset by 1 */
            if(pu1_strm_buf[u4_strm_buf_offset - 1] == 0x03 &&
               pu1_strm_buf[u4_strm_buf_offset - 2] == 0x03 &&
               pu1_strm_buf[u4_strm_buf_offset - 3] == 0x00 &&
               pu1_strm_buf[u4_strm_buf_offset - 4] == 0x00)
            {
                u4_strm_buf_offset -= 1;
            }
            /* previous byte carry add will not result in overflow to        */
            /* u4_strm_buf_offset - 2 as we track 0xff as outstanding bytes  */
            pu1_strm_buf[u4_strm_buf_offset - 1] += carry;
            zero_run = 0;
        }

        /*        Insert outstanding bytes (if any)         */
        while(u4_out_standing_bytes)
        {
            UWORD8 u1_0_or_ff = carry ? 0 : 0xFF;

            PUTBYTE_EPB(pu1_strm_buf, u4_strm_buf_offset, u1_0_or_ff, zero_run);

            u4_out_standing_bytes--;
        }

        /*  clear the carry in low */
        u4_low &= ((1 << (u4_bits_gen + CABAC_BITS)) - 1);

        /* extract the remaining bits;                                   */
        /* includes additional msb bit of low as per Figure 9-12      */
        bits_left = u4_bits_gen + 1;
        rem_bits = (u4_low >> (u4_bits_gen + CABAC_BITS - bits_left));

        if(bits_left >= 8)
        {
            last_byte = (rem_bits >> (bits_left - 8)) & 0xFF;
            PUTBYTE_EPB(pu1_strm_buf, u4_strm_buf_offset, last_byte, zero_run);
            bits_left -= 8;
        }

        /* insert last byte along with rbsp stop bit(1) and 0's in the end */
        last_byte = (rem_bits << (8 - bits_left)) | (1 << (7 - bits_left));
        last_byte &= 0xFF;
        PUTBYTE_EPB(pu1_strm_buf, u4_strm_buf_offset, last_byte, zero_run);

        /* update the state variables and return success */
        ps_cabac->u4_strm_buf_offset = u4_strm_buf_offset;
        ps_cabac->i4_zero_bytes_run = 0;
        return (IHEVCE_SUCCESS);
    }
}

/**
******************************************************************************
*
*  @brief API to backup cabac ctxt at end of 2nd CTB row which is used to init
*   context at start of every row
*
*  @par   Description
*         API to backup cabac ctxt at end of 2nd CTB row which is used to init
*         context at start of every row
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_ctxt_backup(cab_ctxt_t *ps_cabac)
{
    memcpy(
        ps_cabac->au1_ctxt_models_top_right,
        ps_cabac->au1_ctxt_models,
        sizeof(ps_cabac->au1_ctxt_models));
    return (IHEVCE_SUCCESS);
}

/**
******************************************************************************
*
*  @brief Init cabac ctxt at every row start
*
*  @par   Description
*         API to init cabac ctxt at start of every row when entropy sync is
*         enabled
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_ctxt_row_init(cab_ctxt_t *ps_cabac)
{
    /* cabac engine initialization */
    ps_cabac->u4_low = 0;
    ps_cabac->u4_range = 510;
    ps_cabac->u4_bits_gen = 0;
    ps_cabac->u4_out_standing_bytes = 0;
    ps_cabac->i4_zero_bytes_run = 0;

    /*copy top right context as init context when starting to encode a row*/
    COPY_CABAC_STATES(
        ps_cabac->au1_ctxt_models, ps_cabac->au1_ctxt_models_top_right, IHEVC_CAB_CTXT_END);

    return (IHEVCE_SUCCESS);
}
