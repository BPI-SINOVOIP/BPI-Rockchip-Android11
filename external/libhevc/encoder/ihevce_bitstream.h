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
* @file
*  ihevce_bitstream.h
*
* @brief
*  This file contains encoder bitstream engine related structures and
*  interface prototypes
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_BITSTREAM_H_
#define _IHEVCE_BITSTREAM_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
*  @brief      defines the maximum number of bits in a bitstream word
******************************************************************************
*/
#define WORD_SIZE 32

/**
******************************************************************************
*  @brief  The number of consecutive zero bytes for emulation prevention check
******************************************************************************
*/
#define EPB_ZERO_BYTES 2

/**
******************************************************************************
*  @brief  Emulation prevention insertion byte
******************************************************************************
*/
#define EPB_BYTE 0x03

/**
******************************************************************************
*  @brief  Maximum number of NALs in a frame
******************************************************************************
*/
#define MAX_NALS_IN_AU 256

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
*  @brief   Macro to check if emulation prevention byte insertion is required
******************************************************************************
*/
#define INSERT_EPB(zero_run, next_byte) ((zero_run) == EPB_ZERO_BYTES) && (0 == ((next_byte)&0xFC))

/**
******************************************************************************
*  @brief   returns bits required to code a value
******************************************************************************
*/
#define UE_LENGTH(bits, x)                                                                         \
    {                                                                                              \
        UWORD32 r_bit;                                                                             \
        GETRANGE(r_bit, x + 1)                                                                     \
        bits = (((r_bit - 1) << 1) + 1);                                                           \
    }

/**
******************************************************************************
*  @brief  Inserts 1 byte and Emulation Prevention Byte(if any) into bitstream
*          Increments the stream offset and zero run correspondingly
******************************************************************************
*/
#define PUTBYTE_EPB(ptr, off, byte, zero_run)                                                      \
    {                                                                                              \
        if(INSERT_EPB(zero_run, byte))                                                             \
        {                                                                                          \
            ptr[off] = EPB_BYTE;                                                                   \
            off++;                                                                                 \
            zero_run = 0;                                                                          \
        }                                                                                          \
                                                                                                   \
        ptr[off] = byte;                                                                           \
        off++;                                                                                     \
        zero_run = byte ? 0 : zero_run + 1;                                                        \
    }

/**
******************************************************************************
*  @brief  Ensures Byte alignment of the slice header
******************************************************************************
*/

#define BYTE_ALIGNMENT(ps_bitstrm) ihevce_put_rbsp_trailing_bits(ps_bitstrm)

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
*  @brief      Bitstream context for encoder
******************************************************************************
*/
typedef struct bitstrm
{
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
   *  current bitstream word; It is a scratch word containing max of
   *  WORD_SIZE bits. Will be copied to stream buffer when the word is
   *  full
   */
    UWORD32 u4_cur_word;

    /**
   *  signifies number of bits available in u4_cur_word
   *  bits from msb to i4_bits_left_in_cw of u4_cur_word have already been
   *  inserted next bits would be inserted from pos [i4_bits_left_in_cw-1]
   *  Range of this variable [1 : WORD_SIZE]
   */
    WORD32 i4_bits_left_in_cw;

    /**
   *  signifies the number of consecutive zero bytes propogated from previous
   *  word. It is used for emulation prevention byte insertion in the stream
   */
    WORD32 i4_zero_bytes_run;

    /** Total number of NAL units in the output buffer; Shall not exceed
   * MAX_NALS_IN_AU */
    WORD32 i4_num_nal;

    /** Pointer to start of each NAL unit in the output buffer */
    UWORD8 *apu1_nal_start[MAX_NALS_IN_AU];

} bitstrm_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

IHEVCE_ERROR_T
    ihevce_bitstrm_init(bitstrm_t *ps_bitstrm, UWORD8 *pu1_bitstrm_buf, UWORD32 u4_max_bitstrm_size);

IHEVCE_ERROR_T ihevce_put_bits(bitstrm_t *ps_bitstrm, UWORD32 u4_code_val, WORD32 code_len);

IHEVCE_ERROR_T ihevce_put_bit(bitstrm_t *ps_bitstrm, UWORD32 u4_code_val);

IHEVCE_ERROR_T ihevce_put_rbsp_trailing_bits(bitstrm_t *ps_bitstrm);

IHEVCE_ERROR_T ihevce_put_uev(bitstrm_t *ps_bitstrm, UWORD32 u4_code_num);

IHEVCE_ERROR_T ihevce_put_sev(bitstrm_t *ps_bitstrm, WORD32 syntax_elem);

IHEVCE_ERROR_T
    ihevce_put_nal_start_code_prefix(bitstrm_t *ps_bitstrm, WORD32 insert_leading_zero_8bits);

#endif /* _IHEVCE_BITSTREAM_H_ */
