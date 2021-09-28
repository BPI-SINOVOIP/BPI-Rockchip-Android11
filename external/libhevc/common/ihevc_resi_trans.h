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
*******************************************************************************
* @file
*  ihevc_resi_trans.h
*
* @brief
*  Functions declarations for residue and forward transform
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/
#ifndef _IHEVC_RESI_TRANS_H_
#define _IHEVC_RESI_TRANS_H_

typedef UWORD32 ihevc_resi_trans_4x4_ttype1_ft(UWORD8 *pu1_src,
                                    UWORD8 *pu1_pred,
                                    WORD32 *pi4_temp,
                                    WORD16 *pi2_dst,
                                    WORD32 src_strd,
                                    WORD32 pred_strd,
                                    WORD32 dst_strd,
                                    CHROMA_PLANE_ID_T e_chroma_plane);

typedef UWORD32 ihevc_hbd_resi_trans_4x4_ttype1_ft(UWORD16 *pu2_src,
                                    UWORD16 *pu2_pred,
                                    WORD32 *pi4_temp,
                                    WORD16 *pi2_dst,
                                    WORD32 src_strd,
                                    WORD32 pred_strd,
                                    WORD32 dst_strd,
                                    CHROMA_PLANE_ID_T e_chroma_plane,
                                    UWORD8 bit_depth);

typedef UWORD32 ihevc_resi_trans_4x4_ft(UWORD8 *pu1_src,
                             UWORD8 *pu1_pred,
                             WORD32 *pi4_temp,
                             WORD16 *pi2_dst,
                             WORD32 src_strd,
                             WORD32 pred_strd,
                             WORD32 dst_strd,
                             CHROMA_PLANE_ID_T e_chroma_plane);

typedef UWORD32 ihevc_hbd_resi_trans_4x4_ft
    (
    UWORD16 *pu2_src,
    UWORD16 *pu2_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd_chr_flag,
    UWORD8 bit_depth
    );

typedef UWORD32 ihevc_resi_trans_8x8_ft(UWORD8 *pu1_src,
                             UWORD8 *pu1_pred,
                             WORD32 *pi4_temp,
                             WORD16 *pi2_dst,
                             WORD32 src_strd,
                             WORD32 pred_strd,
                             WORD32 dst_strd,
                             CHROMA_PLANE_ID_T e_chroma_plane);

typedef UWORD32 ihevc_hbd_resi_trans_8x8_ft
    (
    UWORD16 *pu2_src,
    UWORD16 *pu2_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd_chr_flag,
    UWORD8 bit_depth
    );


typedef UWORD32 ihevc_resi_trans_16x16_ft(UWORD8 *pu1_src,
                               UWORD8 *pu1_pred,
                               WORD32 *pi4_temp,
                               WORD16 *pi2_dst,
                               WORD32 src_strd,
                               WORD32 pred_strd,
                               WORD32 dst_strd,
                               CHROMA_PLANE_ID_T e_chroma_plane);

typedef UWORD32 ihevc_hbd_resi_trans_16x16_ft(UWORD16 *pu2_src,
                               UWORD16 *pu2_pred,
                               WORD32 *pi4_temp,
                               WORD16 *pi2_dst,
                               WORD32 src_strd,
                               WORD32 pred_strd,
                               WORD32 dst_strd,
                               CHROMA_PLANE_ID_T e_chroma_plane,
                               UWORD8 bit_depth);

typedef UWORD32 ihevc_resi_trans_32x32_ft(UWORD8 *pu1_src,
                               UWORD8 *pu1_pred,
                               WORD32 *pi4_temp,
                               WORD16 *pi2_dst,
                               WORD32 src_strd,
                               WORD32 pred_strd,
                               WORD32 dst_strd,
                               CHROMA_PLANE_ID_T e_chroma_plane);

typedef UWORD32 ihevc_hbd_resi_trans_32x32_ft(UWORD16 *pu2_src,
                               UWORD16 *pu2_pred,
                               WORD32 *pi4_temp,
                               WORD16 *pi2_dst,
                               WORD32 src_strd,
                               WORD32 pred_strd,
                               WORD32 dst_strd,
                               CHROMA_PLANE_ID_T e_chroma_plane,
                               UWORD8 bit_depth);


typedef void ihevc_resi_trans_4x4_16bit_ft(WORD16 *pi2_src,
                          UWORD8 *pu1_pred,
                          WORD16 *pi2_tmp,
                          WORD16 *pi2_dst,
                          WORD32 src_strd,
                          WORD32 pred_strd,
                          WORD32 dst_strd);

typedef void ihevc_resi_trans_8x8_16bit_ft(WORD16 *pi2_src,
                          UWORD8 *pu1_pred,
                          WORD16 *pi2_tmp,
                          WORD16 *pi2_dst,
                          WORD32 src_strd,
                          WORD32 pred_strd,
                          WORD32 dst_strd);

typedef void ihevc_resi_trans_16x16_16bit_ft(WORD16 *pi2_src,
                            UWORD8 *pu1_pred,
                            WORD16 *pi2_tmp,
                            WORD16 *pi2_dst,
                            WORD32 src_strd,
                            WORD32 pred_strd,
                            WORD32 dst_strd);

typedef void ihevc_resi_trans_32x32_16bit_ft(WORD16 *pi2_src,
                            UWORD8 *pu1_pred,
                            WORD16 *pi2_tmp,
                            WORD16 *pi2_dst,
                            WORD32 src_strd,
                            WORD32 pred_strd,
                            WORD32 dst_strd);

ihevc_resi_trans_4x4_ttype1_ft ihevc_resi_trans_4x4_ttype1;
ihevc_resi_trans_4x4_ft ihevc_resi_trans_4x4;
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8;
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32;
ihevc_resi_trans_4x4_16bit_ft ihevc_resi_trans_4x4_16bit;
ihevc_resi_trans_8x8_16bit_ft ihevc_resi_trans_8x8_16bit;
ihevc_resi_trans_16x16_16bit_ft ihevc_resi_trans_16x16_16bit;
ihevc_resi_trans_32x32_16bit_ft ihevc_resi_trans_32x32_16bit;

ihevc_resi_trans_4x4_ttype1_ft ihevc_resi_trans_4x4_ttype1_sse42;
ihevc_resi_trans_4x4_ft ihevc_resi_trans_4x4_sse42;
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8_sse42;
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16_sse42;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32_sse42;
ihevc_resi_trans_4x4_16bit_ft ihevc_resi_trans_4x4_16bit_sse42;
ihevc_resi_trans_8x8_16bit_ft ihevc_resi_trans_8x8_16bit_sse42;
ihevc_resi_trans_16x16_16bit_ft ihevc_resi_trans_16x16_16bit_sse42;
ihevc_resi_trans_32x32_16bit_ft ihevc_resi_trans_32x32_16bit_sse42;


ihevc_resi_trans_4x4_ttype1_ft ihevc_resi_trans_4x4_ttype1_avx;
ihevc_resi_trans_4x4_ft ihevc_resi_trans_4x4_avx;
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8_avx;
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16_avx;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32_avx;
ihevc_resi_trans_4x4_16bit_ft ihevc_resi_trans_4x4_16bit_avx;
ihevc_resi_trans_8x8_16bit_ft ihevc_resi_trans_8x8_16bit_avx;

#ifndef  DISABLE_AVX2
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8_avx2;
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16_avx2;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32_avx2;
#endif

ihevc_hbd_resi_trans_4x4_ttype1_ft ihevc_hbd_resi_trans_4x4_ttype1;
ihevc_hbd_resi_trans_4x4_ft ihevc_hbd_resi_trans_4x4;
ihevc_hbd_resi_trans_8x8_ft ihevc_hbd_resi_trans_8x8;
ihevc_hbd_resi_trans_16x16_ft ihevc_hbd_resi_trans_16x16;
ihevc_hbd_resi_trans_32x32_ft ihevc_hbd_resi_trans_32x32;

ihevc_hbd_resi_trans_4x4_ttype1_ft ihevc_hbd_resi_trans_4x4_ttype1_sse42;
ihevc_hbd_resi_trans_4x4_ft ihevc_hbd_resi_trans_4x4_sse42;
ihevc_hbd_resi_trans_8x8_ft ihevc_hbd_resi_trans_8x8_sse42;
ihevc_hbd_resi_trans_16x16_ft ihevc_hbd_resi_trans_16x16_sse42;
ihevc_hbd_resi_trans_32x32_ft ihevc_hbd_resi_trans_32x32_sse42;


ihevc_hbd_resi_trans_4x4_ttype1_ft ihevc_hbd_resi_trans_4x4_ttype1_avx;
ihevc_hbd_resi_trans_4x4_ft ihevc_hbd_resi_trans_4x4_avx;
ihevc_hbd_resi_trans_8x8_ft ihevc_hbd_resi_trans_8x8_avx;
ihevc_hbd_resi_trans_16x16_ft ihevc_hbd_resi_trans_16x16_avx;
ihevc_hbd_resi_trans_32x32_ft ihevc_hbd_resi_trans_32x32_avx;

/* AVX2 declarations */
ihevc_hbd_resi_trans_8x8_ft ihevc_hbd_resi_trans_8x8_avx2;
ihevc_hbd_resi_trans_16x16_ft ihevc_hbd_resi_trans_16x16_avx2;
ihevc_hbd_resi_trans_32x32_ft ihevc_hbd_resi_trans_32x32_avx2;

/*A9 declarations*/
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16_a9q;
ihevc_resi_trans_4x4_ft ihevc_resi_trans_4x4_a9q;
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8_a9q;
ihevc_resi_trans_4x4_ttype1_ft ihevc_resi_trans_4x4_ttype1_a9q;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32_a9q;
ihevc_resi_trans_4x4_ft ihevc_resi_trans_4x4_neon;
ihevc_resi_trans_4x4_ttype1_ft ihevc_resi_trans_4x4_ttype1_neon;
ihevc_resi_trans_8x8_ft ihevc_resi_trans_8x8_neon;
ihevc_resi_trans_16x16_ft ihevc_resi_trans_16x16_neon;
ihevc_resi_trans_32x32_ft ihevc_resi_trans_32x32_neon;

#endif /*_IHEVC_RESI_TRANS_H_*/
