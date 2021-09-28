@/******************************************************************************
@ *
@ * Copyright (C) 2018 The Android Open Source Project
@ *
@ * Licensed under the Apache License, Version 2.0 (the "License");
@ * you may not use this file except in compliance with the License.
@ * You may obtain a copy of the License at:
@ *
@ * http://www.apache.org/licenses/LICENSE-2.0
@ *
@ * Unless required by applicable law or agreed to in writing, software
@ * distributed under the License is distributed on an "AS IS" BASIS,
@ * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
@ * See the License for the specific language governing permissions and
@ * limitations under the License.
@ *
@ *****************************************************************************
@ * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
@*/

.text
.align 4

@/**
@/*******************************************************************************
@/*
@/* @brief
@/*  Residue calculation and Forward Transform for 4x4 block with 8-bit input
@/*
@/* @par Description:
@/*  Performs residue calculation by subtracting source and  prediction and
@/*  followed by forward transform
@/*
@/* @param[in] pu1_src
@/*  Input 4x4 pixels
@/*
@/* @param[in] pu1_pred
@/*  Prediction data
@/*
@/* @param[in] pi4_tmp
@/*  Temporary buffer of size 4x4
@/*
@/* @param[out] pi2_dst
@/*  Output 4x4 coefficients
@/*
@/* @param[in] src_strd
@/*  Input stride
@/*
@/* @param[in] pred_strd
@/*  Prediction Stride
@/*
@/* @param[in] dst_strd_chr_flag
@/*  Output Stride and Chroma Flag packed in the MS and LS 16-bit
@/*
@/* @returns  Void
@/*
@/* @remarks
@/*  None
@/*
@/*******************************************************************************
@/*/

@/**************Variables Vs Registers*****************************************
@    r0 => *pu1_src
@    r1 => *pu1_pred
@    r2 => *pi4_temp
@    r3 => *pi2_dst
@    r4 => src_strd
@    r5 => pred_strd
@    r6 => dst_strd_chr_flag

    .global ihevc_resi_trans_4x4_a9q

ihevc_resi_trans_4x4_a9q:

    STMFD          sp!, {r4-r7, r14}   @ store all the register components from caller function to memory
    LDR            r4, [sp,#20]        @ r4 contains src_strd
    LDR            r5, [sp,#24]        @ r5 contains pred_strd
    LDR            r6, [sp,#28]        @ r6 contains dst_strd_chr_flag

    ANDS           r7, r6, #1          @check for chroma flag, if present interleaved data
    CMP            r7, #0
    BEQ            NON_INTERLEAVE_LOAD @if flag == 0, use non-interleaving loads

    VLD1.64        d0, [r0], r4        @ load row 0 src
    VLD1.64        d4, [r0], r4        @ load row 1 src
    VLD1.64        d1, [r0], r4        @ load row 2 src
    VLD1.64        d5, [r0], r4        @ load row 3 src
    VUZP.8         d0, d4              @ de-interleaving unzip instruction to get luma data of pu1_src in d0
    VUZP.8         d1, d5              @ de-interleaving unzip instruction to get luma data of pu1_src in d1

    VLD1.64        d2, [r1], r5        @ load row 0 pred
    VLD1.64        d6, [r1], r5        @ load row 1 pred
    VLD1.64        d3, [r1], r5        @ load row 2 pred
    VLD1.64        d7, [r1], r5        @ load row 3 pred
    VUZP.8         d2, d6              @ de-interleaving unzip instruction to get luma data of pu1_pred in d2
    VUZP.8         d3, d7              @ de-interleaving unzip instruction to get luma data of pu1_pred in d3

    B LOAD_END

NON_INTERLEAVE_LOAD:
    VLD1.U32     d0[0], [r0], r4       @ load row 0 src
    VLD1.U32     d0[1], [r0], r4       @ load row 1 src
    VLD1.U32     d1[0], [r0], r4       @ load row 2 src
    VLD1.U32     d1[1], [r0], r4       @ load row 3 src

    VLD1.U32     d2[0], [r1], r5       @ load row 0 pred
    VLD1.U32     d2[1], [r1], r5       @ load row 1 pred
    VLD1.U32     d3[0], [r1], r5       @ load row 2 pred
    VLD1.U32     d3[1], [r1], r5       @ load row 3 pred

LOAD_END:
    @ Finding the residue
    VSUBL.U8    q2, d0, d2             @ q2 contains 1st 16-bit 8 residues
    VSUBL.U8    q3, d1, d3             @ q3 contains 2nd 16-bit 8 residues

    @ SAD caculation
    VABDL.U8    q12, d0, d2            @ q12 contains absolute differences
    VABAL.U8    q12, d1, d3            @ q12 accumulates absolute differences
    VADD.U16    d26, d24, d25          @ add d-registers of q12
    VPADDL.U16  d27, d26               @ d27 contains 2 32-bit values that have to be added
    VPADDL.U32  d28, d27               @ d28 contains 64-bit SAD, only LSB important
    VMOV.32     r0, d28[0]             @ SAD stored in r0 for return
    @ SAD caculation ends

    @ Forward transform - step 1
    VMOV.I16    d2, #64                @ generate immediate constant in d2 for even row multiplication
    VTRN.16     d4, d5                 @ 3-step transpose of residue matrix starts
    VTRN.16     d6, d7                 @ 2nd step of the 3-step matrix transpose
    VMOV.I16    d0, #83                @ generate immediate constant in d0 for odd row multiplication
    VTRN.32     q2, q3                 @ Final step of matrix transpose

    VMOV.I16    d1, #36                @ generate immediate constant in d1 for odd row multiplication
    VSWP        d6, d7                 @ vector swap to allow even and odd row calculation using Q registers
    VADD.S16    q10, q2, q3            @ q4 has the even array
    VSUB.S16    q11, q2, q3            @ q5 has the odd array
    VMULL.S16   q12, d20, d2           @ e[0]*64
    VMLAL.S16   q12, d21, d2[0]        @ row 1 of results: e[0]*64 + e[1]*64
    VMULL.S16   q13, d20, d2           @ e[0]*64
    VMLSL.S16   q13, d21, d2[0]        @ row 3 of results: e[0]*64 - e[1]*64
    VMULL.S16   q8, d22, d0            @ o[0]*83
    VMLAL.S16   q8, d23, d1[0]         @ row 2 of results: o[0]*83 + o[1]*36
    VMULL.S16   q9, d22, d1            @ o[0]*36
    VMLSL.S16   q9, d23, d0[0]         @ row 4 of results: o[0]*36 - o[1]*83

    @ Forward transform - step 2
    VMOV.I32    d2, #64                @ generate immediate constant in d2 for even row multiplication
    VMOV.I32    d0, #83                @ generate immediate constant in d0 for odd row multiplication
    VTRN.32     q12, q8                @ 4-step transpose of residue matrix starts
    VTRN.32     q13, q9                @ 2nd step of the 4-step matrix transpose

    VMOV.I32    d1, #36                @ generate immediate constant in d1 for odd row multiplication
    VSWP        d25, d26               @ 3rd step of the 4-step matrix transpose
    VSWP        d17, d18               @ 4th step of the 4-step matrix transpose
    VADD.S32    q2, q12, q9            @ e[0]
    VADD.S32    q3, q8, q13            @ e[1]
    VSUB.S32    q10, q12, q9           @ o[0]
    VSUB.S32    q11, q8, q13           @ o[1]

    VMUL.S32    q12, q2, d2[0]         @ e[0]*64
    VMLA.S32    q12, q3, d2[0]         @ row 1 of results: e[0]*64 + e[1]*64
    VMUL.S32    q13, q2, d2[0]         @ e[1]*64
    VMLS.S32    q13, q3, d2[0]         @ row 3 of results: e[0]*64 - e[1]*64
    VMUL.S32    q8, q10, d0[0]         @ o[0]*83
    VMLA.S32    q8, q11, d1[0]         @ row 2 of results: o[0]*83 + o[1]*36
    VMUL.S32    q9, q10, d1[0]         @ o[0]*36
    VMLS.S32    q9, q11, d0[0]         @ row 4 of results: o[0]*36 - o[1]*83

    VRSHRN.S32  d0, q12, #9            @ (row1 + 256)/512
    VRSHRN.S32  d1, q8, #9             @ (row2 + 256)/512
    VRSHRN.S32  d2, q13, #9            @ (row3 + 256)/512
    VRSHRN.S32  d3, q9, #9             @ (row4 + 256)/512

    LSR         r7, r6, #15            @ r7 = 2*dst_strd, as pi2_dst contains 2-bit integers
    VST1.U16    d0, [r3], r7           @ store 1st row of result
    VST1.U16    d1, [r3], r7           @ store 2nd row of result
    VST1.U16    d2, [r3], r7           @ store 3rd row of result
    VST1.U16    d3, [r3], r7           @ store 4th row of result

    LDMFD       sp!,{r4-r7,r15}        @ Reload the registers from SP

    @ Function End

@/**
@*******************************************************************************
@*
@* @brief
@*  This function performs residue calculation and forward  transform type 1
@*  on input pixels
@*
@* @description
@*  Performs residue calculation by subtracting source and  prediction and
@*  followed by forward transform
@*
@* @param[in] pu1_src
@*  Input 4x4 pixels
@*
@* @param[in] pu1_pred
@*  Prediction data
@*
@* @param[in] pi2_tmp
@*  Temporary buffer of size 4x4
@*
@* @param[out] pi2_dst
@*  Output 4x4 coefficients
@*
@* @param[in] src_strd
@*  Input stride
@*
@* @param[in] pred_strd
@*  Prediction Stride
@*
@* @param[in] dst_strd_chr_flag
@*  Output Stride and Chroma Flag packed in the MS and LS 16-bit
@*
@* @returns void
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*/
@ UWORD32 ihevc_resi_trans_4x4_ttype1(UWORD8 *pu1_src,
@                                     UWORD8 *pu1_pred,
@                                        WORD32 *pi4_temp,
@                                     WORD16 *pi2_dst,
@                                     WORD32 src_strd,
@                                     WORD32 pred_strd,
@                                       WORD32 dst_strd_chr_flag);
@
@**************Variables Vs Registers*******************************************
@
@ r0 - pu1_src
@ r1 - pu1_pred
@ r2 - pi4_temp
@ r3 - pi2_dst
@
@ [sp]   - src_strd
@ [sp+4] - pred_strd
@ [sp+8] - dst_strd_chr_flag
@
@*******************************************************************************

    .global ihevc_resi_trans_4x4_ttype1_a9q

ihevc_resi_trans_4x4_ttype1_a9q:

    PUSH {r4}
    vpush {d8 - d15}

    LDR r2,[sp,#68]                 @ r2 = src_strd
    LDR r4,[sp,#72]                 @ r4 = pred_strd

    VLD1.32 d2[0],[r0],r2           @ Row 1 of source in d2[0]
    VLD1.32 d3[0],[r1],r4           @ Row 1 of prediction in d3[0]
    VLD1.32 d2[1],[r0],r2           @ Row 2 of source in d2[1]
    VLD1.32 d3[1],[r1],r4           @ Row 2 of prediction in d3[1]

    VLD1.32 d8[0],[r0],r2           @ Row 3 of source in d8[0]
    VABDL.U8 q0,d2,d3               @ Absolute differences of rows 1 and 2 in d0
                                    @ R2:[d11[3] d11[2] d11[1] d11[0]] => Row 2 of residue
    VLD1.32 d9[0],[r1],r4           @ Row 3 of prediction in d9[0]
    VSUBL.U8 q5,d2,d3               @ R1:[d10[3] d10[2] d10[1] d10[0]] => Row 1 of residue
    VLD1.32 d8[1],[r0]              @ Row 4 of source in d8[1]
    VTRN.16 d10,d11                 @ Transpose step 1
    VLD1.32 d9[1],[r1]              @ Row 4 of prediction in d9[1]

    VSUBL.U8 q6,d8,d9               @ R3:[d12[3] d12[2] d12[1] d12[0]] => Row 3 of residue
                                    @ R4:[d13[3] d13[2] d13[1] d13[0]] => Row 4 of residue
    VABAL.U8 q0,d8,d9               @ Absolute differences of rows 3 and 4 in d1
    VTRN.16 d12,d13                 @ Transpose step 2
    VTRN.32 q5,q6                   @ Transpose step 3, Residue block transposed
                                    @ Columns are in C1:d10, C2:d11, C3:d12 and C4:d13
    VADD.S16 d23,d11,d13            @ d23 = C2 + C4
    VMOV.I32 d6,#55                 @ Constant used for multiplication
    VADD.S16 d22,d10,d13            @ d22 = C1 + C4
    VADD.U16 d0,d1,d0               @ Accumulating SAD step 1
    VMOV.I32 d7,#84                 @ Constant used for multiplication
    VMULL.S16 q7,d23,d6[0]          @ q7  = 55*C2 + 55*C4
    VMOV.I32 d4,#74                 @ Constant used for multiplication
    VMULL.S16 q9,d22,d7[0]          @ q9  = 84*C1 + 84*C4
    VADD.S16 d16,d10,d11            @ d16 = C1 + C2
    VMUL.S16 d12,d12,d4[0]          @ d12 = 74*C3
    VMOV.I32 d5,#29                 @ Constant used for multiplication
    VPADDL.U16 d0,d0                @ Accumulating SAD step 2
    VSUB.S16 d16,d16,d13            @ d16 = C1 + C2 - C4
    VMLAL.S16 q7,d22,d5[0]          @ q7  = 29*C1 + 55*C2 + 84*C4
    VMLSL.S16 q9,d23,d5[0]          @ q9  = 84*C1 - 29*C2 + 55*C4
    VMULL.S16 q8,d16,d4[0]          @ q8  = 74*C1 + 74*C2 - 74*C4
    VPADDL.U32 d0,d0                @ Accumulating SAD step 3, SAD in d0
    VSUB.S32 q10,q9,q7              @ q10 = q9 - q7 = 55*C1 - 84*C2 - 29*C4
    VMOV.32 r0,d0[0]                @ Return SAD value
    VRSHR.S32 q8,q8,#1              @ Truncating the 1 bit in q8

    VADDW.S16 q7,q7,d12             @ q7  = 29*C1 + 55*C2 + 74*C3 + 84*C4
    VSUBW.S16 q9,q9,d12             @ q9  = 84*C1 - 29*C2 - 74*C3 + 55*C4
    VADDW.S16 q10,q10,d12           @ q10 = 55*C1 - 84*C2 + 74*C3 - 29*C4

    VRSHR.S32 q7,q7,#1              @ Truncating the 1 bit in q7
    VRSHR.S32 q9,q9,#1              @ Truncating the 1 bit in q9
    VRSHR.S32 q10,q10,#1            @ Truncating the 1 bit in q10
                                    @ Transform stage 1 is in P1:q7, P2:q8, P3:q9 and P4:q10
    VTRN.32 q7,q8
    VTRN.32 q9,q10
    VSWP d15,d18
    VSWP d17,d20                    @ Residue block transposed
                                    @ Corresponding columns are in S1:q7, S2:q8, S3:q9 and S4:q10
    VADD.S32 q13,q7,q8              @ q13 = S1 + S2
    VADD.S32 q1,q7,q10              @ q1 = S1 + S4
    VADD.S32 q4,q8,q10              @ q4 = S2 + S4
    VSUB.S32 q13,q13,q10            @ q13 = S1 + S2 - S4
    VMUL.S32 q12,q1,d5[0]           @ q12 = 29*S1 + 29*S4
    VMUL.S32 q14,q1,d7[0]           @ q14 = 84*S1 + 84*S4
    VMUL.S32 q13,q13,d4[0]          @ q13 = 74*S1 + 74*S2 - 74*S4

    VMLA.S32 q12,q4,d6[0]           @ q12 = 29*S1 + 55*S2 + 84*S4
    VMLS.S32 q14,q4,d5[0]           @ q14 = 84*S1 - 29*S2 + 55*S4
    VMUL.S32 q9,q9,d4[0]            @ q9 = 74*S3

    LDR r4,[sp,#76]                 @ r4 = dst_strd_chr_flag
    ASR r4,r4,#16                   @ r4 = dst_strd
    LSL r4,r4,#1                    @ r4 = 2*dst_strd

    VRSHRN.S32 d26,q13,#8
    VSUB.S32 q15,q14,q12            @ q15 = q14 - q12 = 55*S1 - 84*S2 - 29*S4

    VADD.S32 q12,q12,q9             @ q12 = 29*S1 + 55*S2 + 74*S3 + 84*S4
    VSUB.S32 q14,q14,q9             @ q14 = 84*S1 - 29*S2 - 74*S3 + 55*S4
    VADD.S32 q15,q15,q9             @ q15 = 55*S1 - 84*S2 + 74*S3 - 29*S4

    VRSHRN.S32 d24,q12,#8
    VRSHRN.S32 d28,q14,#8
    VRSHRN.S32 d30,q15,#8           @ Truncating the last 8 bits
                                    @ Transform stage 2 is in U1:d24, U2:d26, U3:d28 and U4:d30
    VST1.64 d24,[r3],r4             @ Storing row 1 of transform stage 2
    VST1.64 d26,[r3],r4             @ Storing row 2 of transform stage 2
    VST1.64 d28,[r3],r4             @ Storing row 3 of transform stage 2
    VST1.64 d30,[r3]                @ Storing row 4 of transform stage 2

    vpop {d8 - d15}
    POP {r4}
    MOV pc,lr

@/**
@*******************************************************************************
@*
@* @brief
@*  This function performs residue calculation and DCT integer forward transform
@*  on 8x8 block
@*
@* @description
@*  Performs residue calculation by subtracting source and prediction and
@*  followed by DCT integer forward transform
@*
@* @param[in] pu1_src
@*  Input 4x4 pixels
@*
@* @param[in] pu1_pred
@*  Prediction data
@*
@* @param[in] pi2_tmp
@*  Temporary buffer of size 8x8
@*
@* @param[out] pi2_dst
@*  Output 8x8 coefficients
@*
@* @param[in] src_strd
@*  Input stride
@*
@* @param[in] pred_strd
@*  Prediction Stride
@*
@* @param[in] dst_strd_chr_flag
@*  Output Stride and Chroma Flag packed in the MS and LS 16-bit
@*
@* @returns void
@*
@* @remarks
@*  None
@*
@*******************************************************************************
@*/
@ UWORB32 ihevc_resi_trans_8x8(UWORD8 *pu1_src,
@                              UWORD8 *pu1_pred,
@                              WORB32 *pi4_temp,
@                              WORB16 *pi2_dst,
@                              WORB32 src_strd,
@                              WORB32 pred_strd,
@                              WORB32 dst_strd_chr_flag);
@
@**************Variables Vs Registers*******************************************
@
@ r0 - pu1_src
@ r1 - pu1_pred
@ r2 - pi4_temp
@ r3 - pi2_dst
@
@ [sp]   - src_strd
@ [sp+4] - pred_strd
@ [sp+8] - dst_strd_chr_flag
@
@*******************************************************************************

    .global ihevc_resi_trans_8x8_a9q

ihevc_resi_trans_8x8_a9q:

    PUSH {r4,r5}
    vpush {d8 - d15}

    @ Loading Prediction and Source blocks of sixe 8x8

    LDR r4,[sp,#80]                 @ r4 = dst_strd_chr_flag
    AND r4,r4,#1                    @ r4 = chr_flag
    CMP r4,#1
    BNE CHROMA_LOAD

LUMA_LOAD:

    LDR r5,[sp,#72]                 @ r5 = src_strd
    LDR r4,[sp,#76]                 @ r4 = pred_strd

    VLD2.8 {d0,d2},[r1],r4          @ Row 1 of prediction in d0
    VLD2.8 {d1,d3},[r0],r5          @ Row 1 of source in d1

    VABDL.U8 q15,d1,d0              @ Row 1 of absolute difference in q15
    VLD2.8 {d2,d4},[r1],r4          @ Row 2 of prediction in d2
    VSUBL.U8 q0,d1,d0               @ Row 1 of residue in q0
    VLD2.8 {d3,d5},[r0],r5          @ Row 2 of source in d3

    VABDL.U8 q9,d3,d2               @ Row 2 of absolute difference in q9
    VLD2.8 {d4,d6},[r1],r4          @ Row 3 of prediction in d4
    VSUBL.U8 q1,d3,d2               @ Row 2 of residue in q1
    VLD2.8 {d5,d7},[r0],r5          @ Row 3 of source in d5

    VABAL.U8 q15,d5,d4              @ Row 3 of absolute difference accumulated in q15
    VLD2.8 {d6,d8},[r1],r4          @ Row 4 of prediction in d6
    VSUBL.U8 q2,d5,d4               @ Row 3 of residue in q2
    VLD2.8 {d7,d9},[r0],r5          @ Row 4 of source in d7

    VABAL.U8 q9,d7,d6               @ Row 4 of absolute difference accumulated in q9
    VLD2.8 {d8,d10},[r1],r4         @ Row 5 of prediction in d8
    VSUBL.U8 q3,d7,d6               @ Row 4 of residue in q3
    VLD2.8 {d9,d11},[r0],r5         @ Row 5 of source in d9

    VABDL.U8 q10,d9,d8              @ Row 5 of absolute difference in q10
    VLD2.8 {d10,d12},[r1],r4        @ Row 6 of prediction in d10
    VSUBL.U8 q4,d9,d8               @ Row 5 of residue in q4
    VLD2.8 {d11,d13},[r0],r5        @ Row 6 of source in d11

    VABAL.U8 q15,d11,d10            @ Row 6 of absolute difference accumulated in q15
    VLD2.8 {d12,d14},[r1],r4        @ Row 7 of prediction in d12
    VSUBL.U8 q5,d11,d10             @ Row 6 of residue in q5
    VLD2.8 {d13,d15},[r0],r5        @ Row 7 of source in d13

    VABAL.U8 q9,d13,d12             @ Row 7 of absolute difference accumulated in q9
    VLD2.8 {d14,d16},[r1]           @ Row 8 of prediction in d14
    VSUBL.U8 q6,d13,d12             @ Row 7 of residue in q6
    VLD2.8 {d15,d17},[r0]           @ Row 8 of source in d15

    B CHROMA_LOAD_END

CHROMA_LOAD:

    LDR r5,[sp,#72]                 @ r5 = src_strd
    LDR r4,[sp,#76]                 @ r4 = pred_strd

    VLD1.64 d0,[r1],r4              @ Row 1 of prediction in d0
    VLD1.64 d1,[r0],r5              @ Row 1 of source in d1

    VABDL.U8 q15,d1,d0              @ Row 1 of absolute difference in q15
    VLD1.64 d2,[r1],r4              @ Row 2 of prediction in d2
    VSUBL.U8 q0,d1,d0               @ Row 1 of residue in q0
    VLD1.64 d3,[r0],r5              @ Row 2 of source in d3

    VABDL.U8 q9,d3,d2               @ Row 2 of absolute difference in q9
    VLD1.64 d4,[r1],r4              @ Row 3 of prediction in d4
    VSUBL.U8 q1,d3,d2               @ Row 2 of residue in q1
    VLD1.64 d5,[r0],r5              @ Row 3 of source in d5

    VABAL.U8 q15,d5,d4              @ Row 3 of absolute difference accumulated in q15
    VLD1.64 d6,[r1],r4              @ Row 4 of prediction in d6
    VSUBL.U8 q2,d5,d4               @ Row 3 of residue in q2
    VLD1.64 d7,[r0],r5              @ Row 4 of source in d7

    VABAL.U8 q9,d7,d6               @ Row 4 of absolute difference accumulated in q9
    VLD1.64 d8,[r1],r4              @ Row 5 of prediction in d8
    VSUBL.U8 q3,d7,d6               @ Row 4 of residue in q3
    VLD1.64 d9,[r0],r5              @ Row 5 of source in d9

    VABDL.U8 q10,d9,d8              @ Row 5 of absolute difference in q10
    VLD1.64 d10,[r1],r4             @ Row 6 of prediction in d10
    VSUBL.U8 q4,d9,d8               @ Row 5 of residue in q4
    VLD1.64 d11,[r0],r5             @ Row 6 of source in d11

    VABAL.U8 q15,d11,d10            @ Row 6 of absolute difference accumulated in q15
    VLD1.64 d12,[r1],r4             @ Row 7 of prediction in d12
    VSUBL.U8 q5,d11,d10             @ Row 6 of residue in q5
    VLD1.64 d13,[r0],r5             @ Row 7 of source in d13

    VABAL.U8 q9,d13,d12             @ Row 7 of absolute difference accumulated in q9
    VLD1.64 d14,[r1]                @ Row 8 of prediction in d14
    VSUBL.U8 q6,d13,d12             @ Row 7 of residue in q6
    VLD1.64 d15,[r0]                @ Row 8 of source in d15

CHROMA_LOAD_END:

    @ Transform stage 1
    @ Transposing residue matrix

    VABAL.U8 q10,d15,d14            @ Row 8 of absolute difference accumulated in q10
    VTRN.16 q0,q1                   @ Transpose residue matrix step (1a)
    VSUBL.U8 q7,d15,d14             @ Row 8 of residue in q7
    VTRN.16 q2,q3                   @ Transpose residue matrix step (1b)

    VTRN.16 q4,q5                   @ Transpose residue matrix step (1c)
    VTRN.16 q6,q7                   @ Transpose residue matrix step (1d)
    VTRN.32 q0,q2                   @ Transpose residue matrix step (2a)
    VTRN.32 q1,q3                   @ Transpose residue matrix step (2b)

    VADD.U16 q8,q15,q9              @ SAD calculation (1)
    VTRN.32 q4,q6                   @ Transpose residue matrix step (2c)
    VTRN.32 q5,q7                   @ Transpose residue matrix step (2d)

    VADD.U16 q8,q8,q10              @ SAD calculation (2)
    VSWP d1,d8                      @ Transpose residue matrix step (3a)
    VSWP d3,d10                     @ Transpose residue matrix step (3b)

    VADD.U16 d16,d16,d17            @ SAD calculation (3)
    VSWP d7,d14                     @ Transpose residue matrix step (3c)
    VSWP d5,d12                     @ Transpose residue matrix step (3d)
                                    @ Columns of residue C0-C7 (8x8 matrix) in q0-q7
    VPADDL.U16 d16,d16              @ SAD calculation (4)

    @ Evaluating first step in Butterfly diagram

    VADD.S16 q10,q0,q7              @ q10 = C0 + C7
    VADD.S16 q11,q1,q6              @ q11 = C1 + C6
    VPADDL.U32 d16,d16              @ SAD calculation (5)
    VADD.S16 q12,q2,q5              @ q12 = C2 + C5
    VADD.S16 q13,q3,q4              @ q13 = C3 + C4

    VSUB.S16 q4,q3,q4               @ q4  = C3 - C4
    VSUB.S16 q5,q2,q5               @ q5  = C2 - C5
    VSUB.S16 q6,q1,q6               @ q6  = C1 - C6
    VSUB.S16 q7,q0,q7               @ q7  = C0 - C7

    @ Calculating F0, F2, F4 and F6

    VADD.S16 q1,q11,q12             @ q1  = C1 + C2 + C5 + C6
    VADD.S16 q2,q10,q13             @ q2  = C0 + C3 + C4 + C7

    MOV r4,#50
    LSL r4,r4,#16
    ADD r4,r4,#18
    MOV r5,#89
    LSL r5,r5,#16
    ADD r5,r5,#75
    VMOV d0,r4,r5                   @ 16-bit aligned, d0[3] = 89, d0[2] = 75, d0[1] = 50, d0[0]=18

    MOV r4,#83
    LSL r4,r4,#16
    ADD r4,r4,#36
    VMOV d1,r4,r4                   @ 16-bit aligned, d1[3] = 83, d1[2] = 36, d1[1] = 83, d1[0]=36

    VSUB.S16 q10,q10,q13            @ q10 = C0 - C3 - C4 + C7
    VSUB.S16 q11,q11,q12            @ q11 = C1 - C2 - C5 + C6
    VMOV.32 r0,d16[0]               @ SAD calculation (6) : Return value = SAD

    VSUB.S16 q3,q2,q1               @ q3 = C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7
    VADD.S16 q2,q2,q1               @ q2 = C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7

    VMULL.S16 q14,d20,d1[1]         @ q14 = [0] of 83*(C0 - C3 - C4 + C7)
    VMULL.S16 q15,d21,d1[1]         @ q15 = [1] of 83*(C0 - C3 - C4 + C7)
    VMULL.S16 q9,d20,d1[0]          @ q9  = [0] of 36*(C0 - C3 - C4 + C7)
    VMULL.S16 q10,d21,d1[0]         @ q10 = [1] of 36*(C0 - C3 - C4 + C7)

    VMLAL.S16 q14,d22,d1[0]         @ q14 = F2[0] = 83*(C0 - C3 - C4 + C7) + 36*(C1 - C2 - C5 + C6)
    VSHLL.S16 q13,d6,#6             @ q13 = F4[0] = 64*(C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7)
    VMLAL.S16 q15,d23,d1[0]         @ q15 = F2[1] = 83*(C0 - C3 - C4 + C7) + 36*(C1 - C2 - C5 + C6)
    VSHLL.S16 q3,d7,#6              @ q3  = F4[1] = 64*(C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7)
    VMLSL.S16 q9,d22,d1[1]          @ q9  = F6[0] = 36*(C0 - C3 - C4 + C7) - 83*(C1 - C2 - C5 + C6)
    VSHLL.S16 q12,d4,#6             @ q12 = F0[0] = 64*(C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7)
    VMLSL.S16 q10,d23,d1[1]         @ q10 = F6[1] = 36*(C0 - C3 - C4 + C7) - 83*(C1 - C2 - C5 + C6)
    VSHLL.S16 q2,d5,#6              @ q2  = F0[1] = 64*(C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7)

    @ Calculating F1, F3, F5 and F7

    MOV r4,#48
    VST1.64 {d24,d25},[r2]!         @ Row 1 of transform stage 1 F0[0] stored
    VST1.64 {d4,d5},[r2],r4         @ Row 1 of transform stage 1 F0[1] stored
    VST1.64 {d28,d29},[r2]!         @ Row 3 of transform stage 1 F2[0] stored
    VST1.64 {d30,d31},[r2],r4       @ Row 3 of transform stage 1 F2[1] stored

    VST1.64 {d26,d27},[r2]!         @ Row 5 of transform stage 1 F4[0] stored
    VMULL.S16 q1,d14,d0[3]          @ q1  = [0] of 89*(C0 - C7)
    VMULL.S16 q8,d15,d0[3]          @ q8  = [1] of 89*(C0 - C7)
    VST1.64 {d6,d7},[r2],r4         @ Row 5 of transform stage 1 F4[1] stored
    VMULL.S16 q11,d14,d0[2]         @ q11 = [0] of 75*(C0 - C7)
    VMULL.S16 q13,d15,d0[2]         @ q13 = [1] of 75*(C0 - C7)
    VST1.64 {d18,d19},[r2]!         @ Row 7 of transform stage 1 F6[0] stored
    VMULL.S16 q3,d14,d0[1]          @ q3  = [0] of 50*(C0 - C7)
    VMULL.S16 q9,d15,d0[1]          @ q9  = [1] of 50*(C0 - C7)
    VST1.64 {d20,d21},[r2]          @ Row 7 of transform stage 1 F6[1] stored
    VMULL.S16 q10,d14,d0[0]         @ q10 = [0] of 18*(C0 - C7)
    VMULL.S16 q7,d15,d0[0]          @ q7  = [1] of 18*(C0 - C7)

    VMLAL.S16 q1,d12,d0[2]          @ q1  = [0] of 89*(C0 - C7) + 75*(C1 - C6)
    VMLAL.S16 q8,d13,d0[2]          @ q8  = [1] of 89*(C0 - C7) + 75*(C1 - C6)
    VMLSL.S16 q11,d12,d0[0]         @ q11 = [0] of 75*(C0 - C7) - 18*(C1 - C6)
    VMLSL.S16 q13,d13,d0[0]         @ q13 = [1] of 75*(C0 - C7) - 18*(C1 - C6)
    VMLSL.S16 q3,d12,d0[3]          @ q3  = [0] of 50*(C0 - C7) - 89*(C1 - C6)
    VMLSL.S16 q9,d13,d0[3]          @ q9  = [1] of 50*(C0 - C7) - 89*(C1 - C6)
    VMLSL.S16 q10,d12,d0[1]         @ q10 = [0] of 18*(C0 - C7) - 50*(C1 - C6)
    VMLSL.S16 q7,d13,d0[1]          @ q7  = [1] of 18*(C0 - C7) - 50*(C1 - C6)

    VMLAL.S16 q1,d10,d0[1]          @ q1  = [0] of 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5)
    VMLAL.S16 q8,d11,d0[1]          @ q8  = [1] of 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5)
    VMLSL.S16 q11,d10,d0[3]         @ q11 = [0] of 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5)
    VMLSL.S16 q13,d11,d0[3]         @ q13 = [1] of 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5)
    VMLAL.S16 q3,d10,d0[0]          @ q3  = [0] of 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5)
    VMLAL.S16 q9,d11,d0[0]          @ q9  = [1] of 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5)
    VMLAL.S16 q10,d10,d0[2]         @ q10 = [0] of 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5)
    VMLAL.S16 q7,d11,d0[2]          @ q7  = [1] of 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5)

    VMLAL.S16 q1,d8,d0[0]           @ q1  = F1[0] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5) + 18*(C3 - C4)
    VMLAL.S16 q8,d9,d0[0]           @ q8  = F1[1] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5) + 18*(C3 - C4)
    VMLSL.S16 q11,d8,d0[1]          @ q11 = F3[0] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5) - 50*(C3 - C4)
    VMLSL.S16 q13,d9,d0[1]          @ q13 = F3[1] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5) - 50*(C3 - C4)
    SUB r2,r2,#176                  @ r2 now points to the second row
    VMLAL.S16 q3,d8,d0[2]           @ q3  = F5[0] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5) + 75*(C3 - C4)
    VMLAL.S16 q9,d9,d0[2]           @ q9  = F5[1] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5) + 75*(C3 - C4)
    VST1.64 {d2,d3},[r2]!           @ Row 2 of transform stage 1 F1[0] stored
    VMLSL.S16 q10,d8,d0[3]          @ q10 = F7[0] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5) - 89*(C3 - C4)
    VMLSL.S16 q7,d9,d0[3]           @ q7  = F7[1] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5) - 89*(C3 - C4)

    VST1.64 {d16,d17},[r2],r4       @ Row 2 of transform stage 1 F1[1] stored
    VST1.64 {d22,d23},[r2]!         @ Row 4 of transform stage 1 F3[0] stored
    VST1.64 {d26,d27},[r2],r4       @ Row 4 of transform stage 1 F3[1] stored
    VST1.64 {d6,d7},[r2]!           @ Row 6 of transform stage 1 F5[0] stored
    VST1.64 {d18,d19},[r2],r4       @ Row 6 of transform stage 1 F5[1] stored
    VST1.64 {d20,d21},[r2]!         @ Row 8 of transform stage 1 F7[0] stored
    VST1.64 {d14,d15},[r2]          @ Row 8 of transform stage 1 F7[1] stored

    @ Transform stage 2 (for rows 1-4 of transform stage 1)
    @ Transposing the 4 rows (F0, F1, F2, F3)
    @ F0 = {q2,q12},  F1 = {q8,q1}, F2 = {q15,q14} and F3 = {q13,q11}

    VTRN.32 q12,q1                  @ Transposing first half of transform stage 1 (1a)
    VTRN.32 q14,q11                 @ Transposing first half of transform stage 1 (1b)
    VSWP d25,d28                    @ Transposing first half of transform stage 1 (2a)
    VSWP d22,d3                     @ Transposing first half of transform stage 1 (2b)

    VTRN.32 q2,q8                   @ Transposing first half of transform stage 1 (3a)
    VTRN.32 q15,q13                 @ Transposing first half of transform stage 1 (3b)
    VSWP d5,d30                     @ Transposing first half of transform stage 1 (4a)
    VSWP d26,d17                    @ Transposing first half of transform stage 1 (4b)
                                    @ B0:q12, B1:q1, B2:q14, B3:q11, B4:q2, B5:q8, B6:q15 and B7:q13

    @ Evaluating first step in Butterfly diagram

    VADD.S32 q0,q12,q13             @ q0  = B0 + B7
    VADD.S32 q5,q11,q2              @ q5  = B3 + B4
    VADD.S32 q3,q1,q15              @ q3  = B1 + B6
    VADD.S32 q4,q14,q8              @ q4  = B2 + B5

    VSUB.S32 q7,q14,q8              @ q7  = B2 - B5
    VSUB.S32 q8,q1,q15              @ q8  = B1 - B6
    VSUB.S32 q6,q11,q2              @ q6  = B3 - B4
    VSUB.S32 q9,q12,q13             @ q9  = B0 - B7

    @ Calculating G0, G2, G4 and G6

    MOV r4,#18
    MOV r5,#50
    VMOV d2,r4,r5                   @ 32-bit aligned, d2[1] = 50, d2[0] = 18
    VSUB.S32 q2,q0,q5               @ q2  = B0 - B3 - B4 + B7

    MOV r4,#75
    MOV r5,#89
    VMOV d3,r4,r5                   @ 32-bit aligned, d3[1] = 89, d3[0] = 75
    VADD.S32 q10,q0,q5              @ q10 = B0 + B3 + B4 + B7

    MOV r4,#36
    MOV r5,#83
    VMOV d0,r4,r5                   @ 32-bit aligned, d0[1] = 83, d0[0] = 36
    VSUB.S32 q11,q3,q4              @ q11 = B1 - B2 - B5 + B6
    VADD.S32 q3,q3,q4               @ q3  = B1 + B2 + B5 + B6

    VMUL.S32 q12,q2,d0[1]           @ q12 = 83*(B0 - B3 - B4 + B7)
    VMUL.S32 q2,q2,d0[0]            @ q2  = 36*(B0 - B3 - B4 + B7)
    VMUL.S32 q5,q9,d3[1]            @ q5 = 89*(B0 - B7)
    VADD.S32 q14,q10,q3             @ q14 = B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7
    VMUL.S32 q4,q9,d3[0]            @ q4 = 75*(B0 - B7)
    VSUB.S32 q15,q10,q3             @ q15 = B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7
@    VSHL.S32 q14,q14,#6             ; q14 = G0 = 64*(B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7)
@    VSHL.S32 q15,q15,#6             ; q15 = G4 = 64*(B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7)

    VMLA.S32 q12,q11,d0[0]          @ q12 = G2 = 83*(B0 - B3 - B4 + B7) + 36*(B1 - B2 - B5 + B6)
    VRSHRN.I32 d28,q14,#5           @ Truncating last 11 bits in G0
    VMLS.S32 q2,q11,d0[1]           @ q2  = G6 = 36*(B0 - B3 - B4 + B7) - 83*(B1 - B2 - B5 + B6)
    VRSHRN.I32 d30,q15,#5           @ Truncating last 11 bits in G4

    LDR r4,[sp,#80]                 @ r4 = dst_strd_chr_flag
    ASR r4,r4,#16                   @ r4 = dst_strd
    LSL r4,r4,#2                    @ r4 = 2*dst_strd*2

    VMUL.S32 q3,q9,d2[1]            @ q3 = 50*(B0 - B7)
    VRSHRN.I32 d24,q12,#11          @ Truncating last 11 bits in G2
    VMUL.S32 q9,q9,d2[0]            @ q9 = 18*(B0 - B7)
    VRSHRN.I32 d4,q2,#11            @ Truncating last 11 bits in G6

    VMLA.S32 q5,q8,d3[0]            @ q5 = 89*(B0 - B7) + 75*(B1 - B6)
    VST1.64 d28,[r3],r4             @ First half-row of row 1 of transform stage 2 (G0) stored
    VMLS.S32 q4,q8,d2[0]            @ q4 = 75*(B0 - B7) - 18*(B1 - B6)

    VMLS.S32 q3,q8,d3[1]            @ q3 = 50*(B0 - B7) - 89*(B1 - B6)
    VST1.64 d24,[r3],r4             @ First half-row of row 3 of transform stage 2 (G2) stored
    VMLS.S32 q9,q8,d2[1]            @ q9 = 18*(B0 - B7) - 50*(B1 - B6)

    VMLA.S32 q5,q7,d2[1]            @ q5 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5)
    VST1.64 d30,[r3],r4             @ First half-row of row 5 of transform stage 2 (G4) stored
    VMLS.S32 q4,q7,d3[1]            @ q4 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5)

    VMLA.S32 q3,q7,d2[0]            @ q3 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5)
    VST1.64 d4,[r3]                 @ First half-row of row 7 of transform stage 2 (G6) stored
    VMLA.S32 q9,q7,d3[0]            @ q9 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5)

    VMLA.S32 q5,q6,d2[0]            @ q5 = G1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) + 18*(B3 - B4)
    VMLS.S32 q4,q6,d2[1]            @ q4 = G3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) - 50*(B3 - B4)
    VMLA.S32 q3,q6,d3[0]            @ q3 = G5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) + 75*(B3 - B4)
    VMLS.S32 q9,q6,d3[1]            @ q9 = G7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) - 89*(B3 - B4)

    SUB r3,r3,r4,LSL #1
    SUB r3,r3,r4,ASR #1             @ r3 = r3 - 5*dst_strd*2
                                    @ r3 is moved from row 7 to row 2
    VRSHRN.I32 d10,q5,#11           @ Truncating last 11 bits in G1
    VRSHRN.I32 d8,q4,#11            @ Truncating last 11 bits in G3
    VRSHRN.I32 d6,q3,#11            @ Truncating last 11 bits in G5
    VST1.64 d10,[r3],r4             @ First half-row of row 2 of transform stage 2 (G1) stored
    VRSHRN.I32 d18,q9,#11           @ Truncating last 11 bits in G7

    VST1.64 d8,[r3],r4              @ First half-row of row 4 of transform stage 2 (G3) stored
    VST1.64 d6,[r3],r4              @ First half-row of row 6 of transform stage 2 (G5) stored
    VST1.64 d18,[r3]!               @ First half-row of row 8 of transform stage 2 (G7) stored

    @ Transform stage 2 (for rows 5-8 of transform stage 1)
    @ Loading the 4 rows (F4, F5, F6, F7)

    SUB r2,r2,#112                  @ r2 jumps from row 8 to row 5 in temporary memory
    VLD1.64 {d20,d21},[r2]!         @ q10 = F4[0]
    VLD1.64 {d22,d23},[r2]!         @ q11 = F4[1]
    VLD1.64 {d8,d9},[r2]!           @ q4  = F5[0]
    @ Transposing the 4 rows
    @ F0 = {q11,q10}, F1 = {q5,q4}, F2 = {q3,q2} and F3 = {q13,q12}

    VTRN.32 q10,q4                  @ Transposing second half of transform stage 1 (1a)
    VLD1.64 {d10,d11},[r2]!         @ q5  = F5[1]
    VLD1.64 {d4,d5},[r2]!           @ q2  = F6[0]
    VLD1.64 {d6,d7},[r2]!           @ q3  = F6[1]
    VLD1.64 {d24,d25},[r2]!         @ q12 = F7[0]
    VTRN.32 q2,q12                  @ Transposing second half of transform stage 1 (1b)
    VLD1.64 {d26,d27},[r2]          @ q13 = F7[1]

    VSWP d21,d4                     @ Transposing second half of transform stage 1 (2a)
    VSWP d24,d9                     @ Transposing second half of transform stage 1 (2b)

    VTRN.32 q11,q5                  @ Transposing second half of transform stage 1 (3a)
    VTRN.32 q3,q13                  @ Transposing second half of transform stage 1 (3b)
    VSWP d26,d11                    @ Transposing second half of transform stage 1 (4b)
    VSWP d23,d6                     @ Transposing second half of transform stage 1 (4a)
                                    @ B0:q10, B1:q4, B2:q2, B3:q12, B4:q11, B5:q5, B6:q3 and B7:q13

    @ Evaluating first step in Butterfly diagram

    VADD.S32 q0,q10,q13             @ q0  = B0 + B7
    VADD.S32 q15,q12,q11            @ q15 = B3 + B4
    VADD.S32 q1,q4,q3               @ q1  = B1 + B6
    VADD.S32 q14,q2,q5              @ q14 = B2 + B5

    VSUB.S32 q9,q10,q13             @ q9  = B0 - B7
    VSUB.S32 q6,q12,q11             @ q6  = B3 - B4
    VSUB.S32 q7,q2,q5               @ q7  = B2 - B5
    VSUB.S32 q8,q4,q3               @ q8  = B1 - B6

    @ Calculating H0, H2, H4 and H6

    VADD.S32 q3,q1,q14              @ q3 = B1 + B2 + B5 + B6
    VSUB.S32 q5,q1,q14              @ q5 = B1 - B2 - B5 + B6

    MOV r4,#18
    MOV r5,#50
    VSUB.S32 q4,q0,q15              @ q4 = B0 - B3 - B4 + B7
    VMOV d2,r4,r5                   @ 32-bit aligned, d2[1] = 50, d2[0] = 18

    MOV r4,#75
    MOV r5,#89
    VADD.S32 q2,q0,q15              @ q2 = B0 + B3 + B4 + B7
    VMOV d3,r4,r5                   @ 32-bit aligned, d3[1] = 89, d3[0] = 75

    MOV r4,#36
    MOV r5,#83

    @ Calculating H1, H3, H5 and H7

    VMUL.S32 q10,q9,d3[1]           @ q10 = 89*(B0 - B7)
    VMOV d0,r4,r5                   @ 32-bit aligned, d0[1] = 83, d0[0] = 36

    VMUL.S32 q13,q9,d3[0]           @ q13 = 75*(B0 - B7)

    VMUL.S32 q12,q4,d0[1]           @ q12 = 83*(B0 - B3 - B4 + B7)
    VADD.S32 q14,q2,q3              @ q14 = B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7
    VMUL.S32 q4,q4,d0[0]            @ q4  = 36*(B0 - B3 - B4 + B7)
    VSUB.S32 q2,q2,q3               @ q2  = B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7


    VMLA.S32 q12,q5,d0[0]           @ q12 = H2 = 83*(B0 - B3 - B4 + B7) + 36*(B1 - B2 - B5 + B6)
@    VSHL.S32 q14,q14,#6             ; q14 = H0 = 64*(B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7)
    VMLS.S32 q4,q5,d0[1]            @ q4 = H6 = 36*(B0 - B3 - B4 + B7) - 83*(B1 - B2 - B5 + B6)
@    VSHL.S32 q2,q15,#6              ; q2 = H4 = 64*(B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7)

    VMUL.S32 q11,q9,d2[1]           @ q11 = 50*(B0 - B7)
    VRSHRN.I32 d28,q14,#5           @ Truncating last 11 bits in H0
    VMUL.S32 q9,q9,d2[0]            @ q9  = 18*(B0 - B7)
    VRSHRN.I32 d24,q12,#11          @ Truncating last 11 bits in H2

    VMLA.S32 q10,q8,d3[0]           @ q10 = 89*(B0 - B7) + 75*(B1 - B6)
    VRSHRN.I32 d4,q2,#5             @ Truncating last 11 bits in H4
    VMLS.S32 q13,q8,d2[0]           @ q13 = 75*(B0 - B7) - 18*(B1 - B6)
    VRSHRN.I32 d8,q4,#11            @ Truncating last 11 bits in H6

    LDR r4,[sp,#80]                 @ r4 = dst_strd_chr_flag
    ASR r4,r4,#16                   @ r4 = dst_strd
    LSL r4,r4,#2                    @ r4 = 2*dst_strd*2

    SUB r3,r3,r4,LSL #2
    ADD r3,r3,r4,ASR #1             @ r3 = r3 - 7*dst_strd*2
                                    @ r3 is moved from row 8 to row 1
    VMLS.S32 q11,q8,d3[1]           @ q11 = 50*(B0 - B7) - 89*(B1 - B6)
    VST1.64 d28,[r3],r4             @ Second half-row of row 1 of transform stage 2 (H0) stored
    VMLS.S32 q9,q8,d2[1]            @ q9  = 18*(B0 - B7) - 50*(B1 - B6)

    VMLA.S32 q10,q7,d2[1]           @ q10 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5)
    VST1.64 d24,[r3],r4             @ Second half-row of row 3 of transform stage 2 (H2) stored
    VMLS.S32 q13,q7,d3[1]           @ q13 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5)

    VMLA.S32 q11,q7,d2[0]           @ q11 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5)
    VST1.64 d4,[r3],r4              @ Second half-row of row 5 of transform stage 2 (H4) stored
    VMLA.S32 q9,q7,d3[0]            @ q9  = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5)

    VMLA.S32 q10,q6,d2[0]           @ q10 = H1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) + 18*(B3 - B4)
    VST1.64 d8,[r3]                 @ Second half-row of row 7 of transform stage 2 (H6) stored
    VMLS.S32 q13,q6,d2[1]           @ q13 = H3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) - 50*(B3 - B4)

    VMLA.S32 q11,q6,d3[0]           @ q11 = H5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) + 75*(B3 - B4)
    VMLS.S32 q9,q6,d3[1]            @ q9  = H7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) - 89*(B3 - B4)

    SUB r3,r3,r4,LSL #1
    SUB r3,r3,r4,ASR #1             @ r3 = r3 - 5*dst_strd
                                    @ r3 is moved from row 7 to row 2
    VRSHRN.I32 d20,q10,#11          @ Truncating last 11 bits in H1
    VRSHRN.I32 d26,q13,#11          @ Truncating last 11 bits in H3
    VRSHRN.I32 d22,q11,#11          @ Truncating last 11 bits in H5
    VST1.64 d20,[r3],r4             @ Second half-row of row 2 of transform stage 2 (H1) stored
    VRSHRN.I32 d18,q9,#11           @ Truncating last 11 bits in H7

    VST1.64 d26,[r3],r4             @ Second half-row of row 4 of transform stage 2 (H3) stored
    VST1.64 d22,[r3],r4             @ Second half-row of row 6 of transform stage 2 (H5) stored
    VST1.64 d18,[r3]                @ Second half-row of row 8 of transform stage 2 (H7) stored

    vpop {d8 - d15}
    POP {r4,r5}
    MOV pc,lr

@/**
@*/ *******************************************************************************
@*/
@*/@brief
@*/  This function performs residue calculation and forward  transform on
@*/ input pixels
@*/
@*/@par Description:
@*/ Performs residue calculation by subtracting source and  prediction and
@*/ followed by forward transform
@*/
@*/ @param[in] pu1_src
@*/  Input 16x16 pixels
@*/
@*/ @param[in] pu1_pred
@*/  Prediction data
@*/
@*/ @param[in] pi2_tmp
@*/  Temporary buffer of size 16x16
@*/
@*/ @param[out] pi2_dst
@*/  Output 16x16 coefficients
@*/
@*/ @param[in] src_strd
@*/  Input stride
@*/
@*/ @param[in] pred_strd
@*/  Prediction Stride
@*/
@*/ @param[in] dst_strd_chr_flag
@*/  Output Stride and Chroma Flag packed in the MS and LS 16-bit
@*/
@*/ @returns  Void
@*/
@*/ @remarks
@*/  None
@*/
@*/*******************************************************************************
@*/

.extern g_ai2_ihevc_trans_16
.extern g_ai4_ihevc_trans_16

g_ai2_ihevc_trans_16_addr_1:
.long g_ai2_ihevc_trans_16 - ulbl1 - 8

g_ai2_ihevc_trans_16_addr_2:
.long g_ai2_ihevc_trans_16 - ulbl2 - 8

g_ai4_ihevc_trans_16_addr:
.long g_ai4_ihevc_trans_16 - ulbl3 - 8

    .global ihevc_resi_trans_16x16_a9q

ihevc_resi_trans_16x16_a9q:

.equ TMP_STRIDE        ,  64            @16*4, Stride of tmp register
.equ SHIFT             ,  13            @shift = 13; // log2(iWidth) - 1 + g_uiBitIncrement
.equ RADD              ,  4096          @1 << (shift - 1);

.equ COFF_STD_2B       ,  32            @Stride for g_ai2_ihevc_trans_16 in bytes
.equ COFF_STD_W        ,  32            @Stride for g_ai4_ihevc_trans_16 in bytes

@;LOAD the fucntion
    STMFD          SP!,{r4-r12,LR}      @stack store values of the arguments
    vpush          {d8 - d15}
    SUB            SP,SP,#32

    LDR             R4,[SP,#136]            @get src_strd
    LDR             R5,[SP,#140]         @get pred_strd
    LDR             R6,[SP,#144]         @get dst_strd_chr_flag

    MOV R8,#0                           @Set loop counter
    LDR R9,g_ai2_ihevc_trans_16_addr_1    @get 16 bit transform matrix
ulbl1:
    ADD R9, R9, PC
    @Read [0 0] [4 0] [8 0] [12 0],[0 1] [4 1] [8 1] [12 1] values of g_ai2_ihevc_trans_16
    @and write to stack
    MOV R12,#COFF_STD_2B
    LSL R12,#2

    VLD1.S32 D30[0],[R9],R12
    VLD1.S32 D30[1],[R9],R12
    VLD1.S32 D31[0],[R9],R12
    VLD1.S32 D31[1],[R9],R12

    VTRN.S32 D30,D31
    VTRN.S16 D30,D31
    VST1.S16 {d30,d31},[SP]

    LDR R9,g_ai2_ihevc_trans_16_addr_2      @get back 16 bit transform matrix
ulbl2:
    ADD R9, R9, PC

    MOV R7,#TMP_STRIDE
    AND R14,R6,#0x1

    VMOV.S32 Q14,#0

@R0         pu1_src
@R1         pu1_pred
@R2         pi4_tmp
@R3         pi2_dst
@R4         src_strd
@R5         pred_strd
@R6         dst_strd_chr_flag
@R7         tmp_dst Nx4 block stride
@R8         loop cntr
@R9         g_ai2_ihevc_trans_16
@R10        tmp_dst Nx4 block offset
@R11        tmp register
@R12        ------
@R14        ------.
@q14        shift 32 bit
@q15        add 32 bit

CORE_LOOP_16X16_HORIZ:

    CMP R14,#1
    BEQ INTERLEAVED_LOAD_S1

    VLD1.U8 {d0,d1},[R0],R4             @LOAD 1-16 src row 1
    VLD1.U8 {d2,d3},[R1],R5             @LOAD 1-16 pred row 1
    VLD1.U8 {d4,d5},[R0],R4             @LOAD 1-16 src row 2
    VLD1.U8 {d6,d7},[R1],R5             @LOAD 1-16 pred row 2
    B    LOAD_DONE

INTERLEAVED_LOAD_S1:

    VLD2.U8 {Q0,Q1},[R0],R4             @LOAD 1-16 src row 1
    VLD2.U8 {Q1,Q2},[R1],R5             @LOAD 1-16 pred row 1
    VLD2.U8 {Q2,Q3},[R0],R4             @LOAD 1-16 src row 2
    VLD2.U8 {Q3,Q4},[R1],R5             @LOAD 1-16 pred row 2
LOAD_DONE:

    VSUBL.U8 Q4,D0,D2                   @Get residue 1-8 row 1
    VSUBL.U8 Q5,D1,D3                   @Get residue 9-16 row 1
    VSUBL.U8 Q6,D4,D6                   @Get residue 1-8 row 2
    VSUBL.U8 Q7,D5,D7                   @Get residue 9-16 row 2

    @Get blk sads
    VABDL.U8 Q15,D0,D2
    VABAL.U8 Q15,D1,D3
    VABAL.U8 Q15,D4,D6
    VABAL.U8 Q15,D5,D7
    VADDW.S16 Q14,Q14,D30
    VADDW.S16 Q14,Q14,D31

    VREV64.S16 Q5,Q5                    @Rev row 1
    VREV64.S16 Q7,Q7                    @Rev row 2
    VSWP D10,D11
    VSWP D14,D15

    VADD.S16 Q8 ,Q4,Q5                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-8 row 1
    VSUB.S16 Q9 ,Q4,Q5                  @o[k] = resi_tmp_1 - resi_tmp_2     k ->9-16 row 1
    VADD.S16 Q10,Q6,Q7                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-8 row 2
    VSUB.S16 Q11,Q6,Q7                  @o[k] = resi_tmp_1 - resi_tmp_2     k ->9-16 row 2

    VREV64.S16    D24,D17               @rev e[k] k-> 4-7 row 1
    VREV64.S16    D25,D21               @rev e[k] k-> 4-7 row 2
    VMOV.S16    D17,D20

    @arrangement OF DATA
    @Q8     A1 A2 A3 A4 B1 B2 B3 B4
    @Q12    A8 A7 A6 A5 B8 B7 B6 B5

    VADD.S16 Q13,Q8,Q12                 @ee[k] = e[k] + e[7 - k] row 1 & 2
    VSUB.S16 Q0,Q8,Q12                  @eo[k] = e[k] - e[7 - k] row 1 & 2

    @D26 R1ee[0] R1ee[1] R1ee[2] R1ee[3]
    @D27 R2ee[0] R2ee[1] R2ee[2] R2ee[3]
    VTRN.S32 D26,D27                    @1-cycle stall before it?
    @D26 R1ee[0] R1ee[1] R2ee[0] R2ee[1]
    @D27 R1ee[2] R1ee[3] R2ee[2] R2ee[3]
    VREV32.16 D2,D27                    @1-cycle stall before it?
    @D26 R1ee[0] R1ee[1] R2ee[0] R2ee[1]
    @D2 R1ee[3] R1ee[2] R2ee[3] R2ee[2]
    VMOV.S16 D27,D26
    VNEG.S16 D3,D2
    @Q13 R1ee[0] R1ee[1] R2ee[0] R2ee[1]  R1ee[0]  R1ee[1]  R2ee[0]  R2ee[1]
    @Q1  R1ee[3] R1ee[2] R2ee[3] R2ee[2] -R1ee[3] -R1ee[2] -R2ee[3] -R2ee[2]

    @D8 : [0 0] [4 0] [8 0] [12 0]
    @D9 : [0 1] [4 1] [8 1] [12 1]
    VLD1.S16 {d8,d9},[SP]               @[0 0] [4 0] [8 0] [12 0] [0 1] [4 1] [8 1] [12 1]
    VADD.S16 Q1,Q13,Q1                  @ 1-cycle stall before it?
    @Q15 R1eee[0] R1eee[1] R2eee[0] R2eee[1] R1eeo[0] R1eeo[1] R2eeo[0] R2eeo[1]

    @Q1  R1eee[0] R1eee[1] R2eee[0] R2eee[1]
    @    R1eeo[0] R1eeo[1] R2eeo[0] R2eeo[1]
    VTRN.S16 D2,D3                      @2-cycle stall before it?
    @Q1  R1eee[0] R1eeo[0] R2eee[0] R2eeo[0]
    @     R1eee[1] R1eeo[1] R2eee[1] R2eeo[1]

    VDUP.S32 D4,D2[0]    @R1eee[0] R1eeo[0] R1eee[0] R1eeo[0]    ;1-cycle stall?
    VDUP.S32 D5,D2[1]    @R2eee[0] R2eeo[0] R2eee[0] R2eeo[0]
    VDUP.S32 D6,D3[0]    @R1eee[1] R1eeo[1] R1eee[1] R1eeo[1]
    VDUP.S32 D7,D3[1]    @R2eee[1] R2eeo[1] R2eee[1] R2eeo[1]

    @---------------Process EO--------------------
    @ Early start to avoid stalls
    MOV R12,#COFF_STD_2B                @Get stride of coeffs

    VMULL.S16 Q5,D4,D8                  @   g_ai2_ihevc_trans_16 * R1eee[0] R1eeo[0] R1eee[0] R1eeo[0]
    VMLAL.S16 Q5,D6,D9                  @ + g_ai2_ihevc_trans_16 * R1eee[1] R1eeo[1] R1eee[1] R1eeo[1]
    VMULL.S16 Q6,D5,D8                  @   g_ai2_ihevc_trans_16 * R2eee[0] R2eeo[0] R2eee[0] R2eeo[0]
    VMLAL.S16 Q6,D7,D9                  @ + g_ai2_ihevc_trans_16 * R2eee[1] R2eeo[1] R2eee[1] R2eeo[1]

    ADD R11,R9,R12,LSL #1               @Load address of g_ai2_ihevc_trans_16[2]
    LSL R12,R12,#2

    VLD1.S16 D26,[R11],R12              @LOAD g_ai2_ihevc_trans_16[2][0-4]]

    VLD1.S16 D27,[R11],R12              @LOAD g_ai2_ihevc_trans_16[6][0-4]
    VMULL.S16 Q1,D26,D0                 @g_ai2_ihevc_trans_16[2][0-4] * eo[0-4]    R1

    VMULL.S16 Q2,D26,D1                 @g_ai2_ihevc_trans_16[2][0-4] * eo[0-4]    R2

    VZIP.S32 Q5,Q6                      @3-cycle instruction
    VMULL.S16 Q3,D27,D0                 @g_ai2_ihevc_trans_16[6][0-4] * eo[0-4]    R1


    VLD1.S16 D26,[R11],R12              @LOAD g_ai2_ihevc_trans_16[10][0-4]
    VMULL.S16 Q4,D27,D1                 @g_ai2_ihevc_trans_16[6][0-4] * eo[0-4]    R2

    @These values must go to 0 4 8 12 colums hence we need stride *4
    LSL R10,R7,#2

    VLD1.S16 D27,[R11],R12              @LOAD g_ai2_ihevc_trans_16[14][0-4]

    VST1.32 D10,[R2],R10
    VMULL.S16 Q8,D27,D1                 @g_ai2_ihevc_trans_16[14][0-4] * eo[0-4] R2

    VST1.32 D11,[R2],R10
    VMULL.S16 Q7,D27,D0                 @g_ai2_ihevc_trans_16[14][0-4] * eo[0-4] R1

    VST1.32 D12,[R2],R10
    VMULL.S16 Q5,D26,D0                 @g_ai2_ihevc_trans_16[10][0-4] * eo[0-4] R1

    VST1.32 D13,[R2],R10
    VMULL.S16 Q6,D26,D1                 @g_ai2_ihevc_trans_16[10][0-4] * eo[0-4] R2

    SUB R2,R2,R10,LSL #2

    @transpose the 4x4 matrix row1
    VTRN.32 Q1, Q3                      @R1 transpose1 -- 2 cycles

    @transpose the 4x4 matrix row2
    VTRN.32 Q2,Q4                       @R2 transpose1 -- 2 cycles

    VTRN.32 Q5, Q7                      @R1 transpose1 -- 2 cycles

    VTRN.32 Q6,Q8                       @R2 transpose1 -- 2 cycles

    VSWP    D10,D3                      @R1 transpose2
    VSWP    D14,D7                      @R1 transpose2

    VSWP    D12,D5                      @R2 transpose2
    VSWP    D16,D9                      @R2 transpose2

    VADD.S32 Q5,Q5,Q1                   @R1 add
    VADD.S32 Q3,Q3,Q7                   @R1 add

    VADD.S32 Q2,Q2,Q4                   @R2 add
    VADD.S32 Q6,Q6,Q8                   @R2 add

    VADD.S32 Q5,Q5,Q3                   @R1 add

    VADD.S32 Q4,Q6,Q2                   @R2 add

    @-----------------------Processing O ----------------------------
    @ Early start to avoid stalls
    MOV R12,#COFF_STD_2B                @Get coeffs stride
    LSL R12,R12,#1
    ADD R11,R9,#COFF_STD_2B             @Get address of g_ai2_ihevc_trans_16[1]

    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[1][0-7] -- 2 cycles

    VZIP.S32 Q5,Q4                      @ 3 cycle instruction
    VMULL.S16 Q6,D18,D4                 @o[0][0-3]*  R1


    VMLAL.S16 Q6,D19,D5                 @o[0][4-7]*  R1     ; follows MULL instruction: Multiplier accumulator forwarding
    @write to memory
    @this should go to 2 6 10 14
    LSL R10,R7,#2
    ADD R2,R2,R7,LSL #1                 @move to third row
    VST1.32 D10,[R2],R10
    VMULL.S16 Q7,D22,D4                 @o[0][0-3]*  R2

    VST1.32 D11,[R2],R10
    VMLAL.S16 Q7,D23,D5                 @o[0][4-7]*  R2

    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[3][0-7]

    VST1.32 D8,[R2],R10
    VMULL.S16 Q8,D18,D4                 @o[1][0-3]*  R1

    VST1.32 D9,[R2],R10
    VMLAL.S16 Q8,D19,D5                 @o[1][4-7]*  R1
    SUB R2,R2,R10,LSL #2
    SUB R2,R2,R7,LSL #1

    @--------------------Done procrssing EO -------------------------

    @ -----------------Processing O continues------------------------

    VMULL.S16 Q10,D22,D4                @o[1][0-3]*  R2
    VMLAL.S16 Q10,D23,D5                @o[1][4-7]*  R2

    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[5][0-7]

    VLD1.S16 {d6,d7},[R11],R12          @g_ai2_ihevc_trans_16[7][0-7]
    VMULL.S16 Q12,D18,D4                @o[2][0-3]*  R1

    VMLAL.S16 Q12,D19,D5                @o[2][4-7]*  R1
    VMULL.S16 Q0,D18,D6                 @o[3][0-3]*  R1
    VMLAL.S16 Q0,D19,D7                 @o[3][4-7]*  R1

    VMULL.S16 Q13,D22,D4                @o[2][0-3]*  R2
    VMLAL.S16 Q13,D23,D5                @o[2][4-7]*  R2
    VMULL.S16 Q1,D22,D6                 @o[3][0-3]*  R2
    VMLAL.S16 Q1,D23,D7                 @o[3][4-7]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q6, Q8                      @ 2-cycle instruction

    VTRN.32 Q12,Q0                      @ 2-cycle instruction

    @transpose the 4x4 matrix R2
    VTRN.32 Q7,Q10                      @ 2-cycle instruction

    VTRN.32 Q13,Q1                      @ 2-cycle instruction

    VSWP    D24,D13
    VSWP    D0, D17

    VSWP     D26,D15
    VSWP    D2,D21

    VADD.S32 Q8 ,Q8 ,Q6
    VADD.S32 Q12,Q12,Q0

    VADD.S32 Q10,Q10,Q7
    VADD.S32 Q13,Q13,Q1

    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[9][0-7]
    VADD.S32 Q12 ,Q12 ,Q8

    VADD.S32 Q13,Q13,Q10
    VMULL.S16 Q3,D18,D4                 @o[4][0-3]*  R1
    VMLAL.S16 Q3,D19,D5                 @o[4][4-7]*  R1

    VZIP.S32 Q12,Q13
    VMULL.S16 Q4,D22,D4                 @o[0][0-3]*  R2


    VMLAL.S16 Q4,D23,D5                 @o[0][4-7]*  R2
    @write to memory
    @this should go to 1 3 5 7
    ADD R2,R2,R7
    LSL R7,R7,#1
    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[11][0-7]

    VST1.32 D24,[R2],R7
    VMULL.S16 Q5,D18,D4                 @o[5][0-3]*  R1

    VST1.32 D25,[R2],R7
    VMLAL.S16 Q5,D19,D5                 @o[5][4-7]*  R1

    VST1.32 D26,[R2],R7
    VMULL.S16 Q6,D22,D4                 @o[0][0-3]*  R2

    VST1.32 D27,[R2],R7
    VMLAL.S16 Q6,D23,D5                 @o[0][4-7]*  R2

    VLD1.S16 {d4,d5},[R11],R12          @g_ai2_ihevc_trans_16[13][0-7]

    VLD1.S16 {d2,d3},[R11],R12          @g_ai2_ihevc_trans_16[15][0-7]
    VMULL.S16 Q7,D18,D4                 @o[6][0-3]*  R1

    VMLAL.S16 Q7,D19,D5                 @o[6][4-7]*  R1
    VMULL.S16 Q10,D18,D2                @o[7][0-3]*  R1
    VMLAL.S16 Q10,D19,D3                @o[7][4-7]*  R1

    VMULL.S16 Q8,D22,D4                 @o[0][0-3]*  R2
    VMLAL.S16 Q8,D23,D5                 @o[0][4-7]*  R2
    VMULL.S16 Q12,D22,D2                @o[0][0-3]*  R2
    VMLAL.S16 Q12,D23,D3                @o[0][4-7]*  R2


    @transpose the 4x4 matrix R1
    VTRN.32 Q3 ,Q5                      @ 2-cycle instruction

    VTRN.32 Q7 ,Q10                     @ transpose step 2 R1 , 2-cycle instruction

    @transpose the 4x4 matrix R2
    VTRN.32 Q4 ,Q6                      @ 2-cycle instruction

    VTRN.32 Q8 ,Q12                     @ transpose step 2 R2 , 2-cycle instruction

    VSWP    D14,D7                      @ transpose step 3, R1
    VSWP    D20,D11                     @ transpose step 4, R1
    VSWP    D16,D9                      @ transpose step 3, R2
    VSWP    D24,D13                     @ transpose step 4, R2

    VADD.S32 Q5 ,Q5 ,Q3
    VADD.S32 Q10,Q10,Q7
    VADD.S32 Q6 ,Q6 ,Q4
    VADD.S32 Q12,Q12,Q8
    VADD.S32 Q10,Q10,Q5
    VADD.S32 Q12,Q12,Q6

    @ 2-cycle stall
    VZIP.S32 Q10,Q12                    @ 3-cycle instruction

    @ 2-cycle stall
    @this should go to 9 11 13 15
    VST1.32 D20,[R2],R7

    VST1.32 D21,[R2],R7

    VST1.32 D24,[R2],R7

    VST1.32 D25,[R2],R7

    SUB R2,R2,R7,LSL #3
    LSR R7,R7,#1
    SUB R2,R2,R7

    ADD R2,R2,#8                        @MOVE TO NEXT to next COLUMN - pi4_tmp

    ADD R8,R8,#2                        @increment loop cntr
    CMP R8,#16                          @check lllop cntr
    BNE CORE_LOOP_16X16_HORIZ           @jump acc


@*****************Vertical transform************************************

@Initialization for vert transform
@pi4_tmp will be the new src
@tmp stride will be new src stride
@dst will be new pi4_tmp
@dst stride will be new tmp stride
@trans table will be of 32 bit

    LDR R9,g_ai4_ihevc_trans_16_addr        @get 32 bit transform matrix
ulbl3:
    ADD R9, R9, PC

    SUB R0,R2,#64                       @set tmp as src [-32 to move back to orgin]
    MOV R2,R3                           @set dst as tmp
    MOV R4,#TMP_STRIDE                  @set tmp stride as src stride
    LSR R7,R6,#15                       @Set dst stride as tmp stride
    SUB R4,#48                          @Adjust stride 3 previous loads

    @Block SAD
    VADD.S32 D28,D28,D29
    VPADD.S32 D28,D28,D29
    VMOV.S32 R3,D28[0]
    @ SAD calculation ends -- final value in R3.

    @Read [0 0] [4 0] [8 0] [12 0],[0 1] [4 1] [8 1] [12 1]
    @values of g_ai4_ihevc_trans_16 and write to stack
    MOV R12,#COFF_STD_W
    LSL R12,R12,#2
    VLD1.S32 D28,[R9],R12
    VLD1.S32 D29,[R9],R12
    VLD1.S32 D30,[R9],R12
    VLD1.S32 D31,[R9],R12
    SUB R9,R9,R12,LSL #2

    VREV64.32 Q15,Q15
    VTRN.S32 Q14,Q15
    VST1.S32 {Q14-Q15},[SP]

    VMOV.U32 Q14,#RADD                  @get the round factor to q14
    VMOV.U32 Q15,#SHIFT                 @Get the shift to neon

    MOV R8,#0                           @INIT LOOP

CORE_LOOP_16X16_VERT:

    VLD1.S32 {D0,D1},[R0]!              @LOAD 1-4 src R1
    VLD1.S32 {D2,D3},[R0]!              @LOAD 5-8 pred R1
    VLD1.S32 {D4,D5},[R0]!              @LOAD 9-12 src R1
    VLD1.S32 {D6,D7},[R0],R4            @LOAD 12-16 pred R1

    VLD1.S32 {D8,D9},[R0]!              @LOAD 1-4 src R2
    VLD1.S32 {D10,D11},[R0]!            @LOAD 5-8 pred R2
    VLD1.S32 {D12,D13},[R0]!            @LOAD 9-12 src R2
    VLD1.S32 {D14,D15},[R0],R4          @LOAD 12-16 pred R2

    VREV64.S32 Q2,Q2                    @Rev 9-12 R1
    VREV64.S32 Q3,Q3                    @Rev 12-16 R1
    VREV64.S32 Q6,Q6                    @Rev 9-12 R2
    VREV64.S32 Q7,Q7                    @Rev 12-16 R2

    VSWP D6,D7
    VSWP D4,D5
    VADD.S32 Q8 ,Q0,Q3                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-4  R1
    VSWP D12,D13                        @ dual issued with prev. instruction
    VADD.S32 Q9 ,Q1,Q2                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 5-8  R1
    VSWP D14,D15                        @ dual issued with prev. instruction
    VSUB.S32 Q10,Q0,Q3                  @o[k] = resi_tmp_1 - resi_tmp_2  k -> 1-4  R1
    VSUB.S32 Q11,Q1,Q2                  @o[k] = resi_tmp_1 - resi_tmp_2  k -> 5-8  R1

    VADD.S32 Q12,Q4,Q7                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-4  R2
    VREV64.S32    Q9 ,Q9                @rev e[k] k-> 4-7 R1, dual issued with prev. instruction
    VADD.S32 Q13,Q5,Q6                  @e[k] = resi_tmp_1 + resi_tmp_2  k -> 5-8  R2
    VSUB.S32 Q0 ,Q4,Q7                  @o[k] = resi_tmp_1 - resi_tmp_2  k -> 1-4  R2
    VSWP D18,D19                        @ dual issued with prev. instruction
    VSUB.S32 Q1 ,Q5,Q6                  @o[k] = resi_tmp_1 - resi_tmp_2  k -> 5-8  R2
    VREV64.S32    Q13,Q13               @rev e[k] k-> 4-7 R2, dual issued with prev. instruction

    VADD.S32 Q2,Q8,Q9                   @ee[k] = e[k] + e[7 - k] row R1
    VSUB.S32 Q3,Q8,Q9                   @eo[k] = e[k] - e[7 - k] row R1
    VSWP D26,D27


    VADD.S32 Q4,Q12,Q13                 @ee[k] = e[k] + e[7 - k] row R2
    VSUB.S32 Q5,Q12,Q13                 @eo[k] = e[k] - e[7 - k] row R2
    VREV64.S32 D5,D5                    @rev ee[k] 4-7 R1, dual issued with prev. instruction

    VADD.S32 D12,D4,D5                  @eee[0] eee[1]    R1
    VSUB.S32 D13,D4,D5                  @eeo[0] eeo[1]    R1
    VREV64.S32 D9,D9                    @rev ee[k] 4-7 R2, dual issued with prev. instruction


    VADD.S32 D14,D8,D9                  @eee[0] eee[1]    R2
    VSUB.S32 D15,D8,D9                  @eeo[0] eeo[1]    R2

    VLD1.S32 {Q12,Q13},[SP]             @Load g_ai2_ihevc_trans_16[xx]->  Q12 : [0 0] [8 0] [4 0] [12 0]  Q13 : [0 1] [8 1] [4 1] [12 1]
    VREV64.S32 Q8,Q6                    @Q6 : eee[0] eee[1] eeo[0] eeo[1] R1   ->     ;Q8 : eee[1] eee[0] eeo[1] eeo[0] R1

    VREV64.S32 Q9,Q7                    @Q7 : eee[0] eee[1] eeo[0] eeo[1] R2     ->    ;Q9 : eee[1] eee[0] eeo[1] eeo[0] R2


    VMUL.S32 Q4,Q6,Q12                  @g_ai2_ihevc_trans_16 * eee[0] eee[1] eeo[0] eeo[1]    R1
    VMLA.S32 Q4,Q8,Q13                  @g_ai2_ihevc_trans_16 * eee[1] eee[0] eeo[1] eeo[0]    R1

    VMUL.S32 Q6,Q7,Q12                  @g_ai2_ihevc_trans_16 * eee[0] eee[1] eeo[0] eeo[1]    R2
    VMLA.S32 Q6,Q9,Q13                  @g_ai2_ihevc_trans_16 * eee[1] eee[0] eeo[1] eeo[0] R2

                                        @Q3    :R1E00 R1E01 R1E02 R1E03
                                        @Q5    :R2E00 R2E01 R2E02 R2E03
    VSWP D7,D10                         @ dual issued with prev. instruction
                                        @Q3    :R1E00 R1E01 R2E00 R2E01
                                        @Q5    :R1E02 R1E03 R2E02 R2E03
    VSWP D7,D11
                                        @Q3    :R1E00 R1E01 R2E02 R2E03
                                        @Q5    :R1E02 R1E03 R2E00 R2E01

    MOV R12,#COFF_STD_W
    ADD R11,R9,R12,LSL #1               @Get to the 2nd row of src
    LSL R12,R12,#2

    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai2_ihevc_trans_16[2][0-4] -> 2G0 2G1 2G2 2G3, 2-cycle instr.

    VADD.S32  Q4,Q4,Q14                 @ROUND  R1
    VMUL.S32  Q12,Q3,Q7                 @2G0 2G1 2G2 2G3 * R1E00 R1E01 R2E02 R2E03, 4-cycle instruction
    VSWP      D14,D15                   @2G0 2G1 2G2 2G3 -> 2G2 2G3 2G0 2G1, dual issued with prev. instruction

    VADD.S32 Q6,Q6,Q14                  @ROUND  R2

    VSHRN.S32 D8,Q4,#SHIFT              @NARROW R1

    VLD1.S32  {D16,D17},[R11],R12       @LOAD g_ai2_ihevc_trans_16[6][0-4]
    VSHRN.S32 D9,Q6,#SHIFT              @NARROW R2, dual issued in 2nd cycle

    VMUL.S32  Q2,Q3,Q8                  @g_ai2_ihevc_trans_16[6][0-4] * eo[0-4], 4-cycle instruction
    VSWP      D16,D17                   @dual issued with prev. instr.

    VZIP.S16 D8,D9                      @INTERLEAVE R1 R2 R1 R2 R1 R2 to write
    VMLA.S32  Q12,Q5,Q7                 @2G2 2G3 2G0 2G1 * R1E02 R1E03 R2E00 R2E01, 4-cycle instruction


    @WRITE INTO MEM the values or wait to be shuffled
    @These values must go to 0 4 8 12 colums
    LSL R10,R7,#2
    VST1.S32 D8[0],[R2],R10

    VST1.S32 D9[0],[R2],R10

    VST1.S32 D8[1],[R2],R10
    VPADD.S32 D18,D24,D25               @D18[0] -> 2G0*R1E00+2G1*R1E01 2G2*R2E02+2G3*R2E03
                                        @D18[1] -> 2G2*R1E02+2G3*R1E03 2G0*R2E00+*2G1R2E01

    VST1.S32 D9[1],[R2],R10
    VMLA.S32  Q2,Q5,Q8                  @g_ai2_ihevc_trans_16[2][0-4] * eo[0-4]
    LSL R10,R10,#2
    SUB R2,R2,R10

    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai2_ihevc_trans_16[10][0-4]

    VMUL.S32  Q6,Q3,Q7                  @g_ai2_ihevc_trans_16[10][0-4] * eo[0-4]
    VSWP      D14,D15                   @ dual issued with prev. instruction
    VPADD.S32 D19,D4,D5

    VLD1.S32  {D16,D17},[R11],R12       @LOAD g_ai2_ihevc_trans_16[14][0-4]
    VMUL.S32  Q2,Q3,Q8                  @g_ai2_ihevc_trans_16[14][0-4] * eo[0-4]
    VSWP      D16,D17

    VMLA.S32  Q6,Q5,Q7                  @g_ai2_ihevc_trans_16[2][0-4] * eo[0-4]
    VADD.S32 Q9,Q9,Q14                  @Round by RADD R1
    VMLA.S32  Q2,Q5,Q8                  @g_ai2_ihevc_trans_16[2][0-4] * eo[0-4]
    VSHRN.S32 D8,Q9,#SHIFT              @Shift by SHIFT
    VPADD.S32 D24,D12,D13
    @---------------Processing O, Row 1 and Row 2--------------------------------------
    @ Early start to avoid stalls
    MOV R12,#COFF_STD_W
    ADD R11,R9,R12                      @Get 1ST row
    LSL R12,R12,#1

    LSL R10,R7,#2
    ADD R2,R2,R7,LSL #1                 @move to third row
    @this should go to 2  6 10 14
    VST1.S32 D8[0],[R2],R10

    VST1.S32 D8[1],[R2],R10
    VPADD.S32 D25,D4,D5                 @ dual issued with prev. instruction in 2nd cycle

    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[1][0-7]
    VADD.S32 Q12,Q12,Q14                @Round by RADD R2, dual issued with prev. instruction in 2nd cycle
    VMUL.S32 Q6,Q2,Q0                   @g_ai2_ihevc_trans_16[1][0-3]*o[0][0-3] R2
    VMLA.S32 Q6,Q3,Q1                   @g_ai2_ihevc_trans_16[1][4-7]*o[0][4-7] R2
    VSHRN.S32 D9,Q12,#SHIFT             @Shift by SHIFT

    VMUL.S32 Q2,Q2,Q10                  @g_ai2_ihevc_trans_16[1][0-3]*o[0][0-3] R1
    VMLA.S32 Q2,Q3,Q11                  @g_ai2_ihevc_trans_16[1][4-7]*o[0][4-7] R1
    VADD.S32 D11,D12,D13                @g_ai2_ihevc_trans_16[1][k]*o[0][k]+g_ai2_ihevc_trans_16[0][7-k]*o[0][7-k] R2, dual issued with prev. instr.
    VST1.S32 D9[0],[R2],R10

    VST1.S32 D9[1],[R2],R10
    VADD.S32 D10,D4,D5                  @g_ai2_ihevc_trans_16[1][k]*o[0][k]+g_ai2_ihevc_trans_16[0][7-k]*o[0][7-k] R1, dual issued with prev. instr.
    LSL R10,R10,#2                      @go back to orgin
    SUB R2,R2,R10
    SUB R2,R2,R7,LSL #1

    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[3][0-7]

    VMUL.S32 Q7,Q2,Q10                  @o[0][0-3]
    VMLA.S32 Q7,Q3,Q11                  @o[0][4-7]
    VMUL.S32 Q8,Q2,Q0                   @o[0][0-3]
    VMLA.S32 Q8,Q3,Q1                   @o[0][4-7]

    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[5][0-7]
    VADD.S32 D18,D14,D15
    VMUL.S32 Q12,Q2,Q10                 @o[0][0-3]
    VMLA.S32 Q12,Q3,Q11                 @o[0][4-7]
    VADD.S32 D19,D16,D17
    VMUL.S32 Q4,Q2,Q0
    VMLA.S32 Q4,Q3,Q1
    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[7][0-7]
    VADD.S32 D26,D24,D25                @ dual issued with prev. instr.
    VMUL.S32 Q6,Q2,Q10                  @o[0][0-3]
    VMLA.S32 Q6,Q3,Q11                  @o[0][4-7]
    VADD.S32 D27,D8,D9
    VMUL.S32 Q4,Q2,Q0
    VMLA.S32 Q4,Q3,Q1
    VADD.S32 D12,D12,D13
    @Q5 Q9 Q13 Q6
    VPADD.S32 D14,D10,D11
    VPADD.S32 D15,D18,D19
    VPADD.S32 D16,D26,D27
    VADD.S32  D13,D8,D9
    VADD.S32 Q9,Q7,Q14
    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[0][0-7]
    VPADD.S32 D17,D12,D13               @ dual issued with prev. instr. in 2nd cycle

    VMUL.S32 Q4,Q2,Q10                  @o[0][0-3]
    VMLA.S32 Q4,Q3,Q11                  @o[0][4-7]

    VADD.S32 Q12,Q8,Q14

    VMUL.S32 Q6,Q2,Q0                   @o[0][0-3]
    VMLA.S32 Q6,Q3,Q1                   @o[0][4-7]

    VSHRN.S32 D26,Q9,#SHIFT
    VSHRN.S32 D27,Q12,#SHIFT
    VADD.S32 D10,D8,D9
    @write to memory this should go to 1 3 5 7
    ADD R2,R2,R7
    LSL R7,R7,#1
    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[1][0-7]
    VADD.S32 D11,D12,D13                @ dual issued with prev. instr.

    VST1.S32 D26[0],[R2],R7
    VMUL.S32 Q7,Q2,Q10                  @o[0][0-3]
    VMLA.S32 Q7,Q3,Q11                  @o[0][4-7]
    VST1.S32 D26[1],[R2],R7
    VMUL.S32 Q8,Q2,Q0                   @o[0][0-3]
    VMLA.S32 Q8,Q3,Q1                   @o[0][4-7]
    VST1.S32 D27[0],[R2],R7
    VADD.S32 D18,D14,D15
    VST1.S32 D27[1],[R2],R7

    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[2][0-7]
    VADD.S32 D19,D16,D17                @ dual issued with prev. instr.

    VMUL.S32 Q12,Q2,Q10                 @o[0][0-3]
    VMLA.S32 Q12,Q3,Q11                 @o[0][4-7]
    VMUL.S32 Q4,Q2,Q0
    VMLA.S32 Q4,Q3,Q1

    VLD1.S32 {Q2,Q3},[R11],R12          @g_ai2_ihevc_trans_16[3][0-7]
    VADD.S32 D26,D24,D25

    VMUL.S32 Q6,Q2,Q10                  @o[0][0-3]
    VMLA.S32 Q6,Q3,Q11                  @o[0][4-7]
    VADD.S32  D27,D8,D9

    VMUL.S32 Q4,Q2,Q0
    VMLA.S32 Q4,Q3,Q1
    VADD.S32 D12,D12,D13
    @Q5 Q9 Q13 Q6
    VPADD.S32 D14,D10,D11
    VPADD.S32 D15,D18,D19
    VPADD.S32 D16,D26,D27
    VADD.S32  D13,D8,D9
    VADD.S32 Q9,Q7,Q14
    @ 1- cycle stall?
    VPADD.S32 D17,D12,D13
    VSHRN.S32 D22,Q9,#SHIFT
    VADD.S32 Q10,Q8,Q14
    @ 2-cycle stall?
    VSHRN.S32 D23,Q10,#SHIFT

    @this should go to 9 11 13 15
    @LSL R11,R7,#1
    VST1.S32 D22[0],[R2],R7
    VST1.S32 D22[1],[R2],R7
    VST1.S32 D23[0],[R2],R7
    VST1.S32 D23[1],[R2],R7

    SUB R2,R2,R7,LSL #3
    LSR R7,R7,#1
    SUB R2,R2,R7

    ADD R2,R2,#4                        @MOVE TO NEXT to next COLUMN

    ADD R8,R8,#2                        @increment loop cntr by 2 since we process loop as 2 cols
    CMP R8,#16                          @check loop cntr
    BNE CORE_LOOP_16X16_VERT            @jump acc

    MOV R0,R3

    ADD SP,SP,#32
    vpop {d8 - d15}
    LDMFD          sp!,{r4-r12,PC}      @stack store values of the arguments

