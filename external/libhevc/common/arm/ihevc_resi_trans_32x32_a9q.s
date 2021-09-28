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
@/**
@ *******************************************************************************
@ * @file
@ *  ihevc_resi_trans_32x32.s
@ *
@ * @brief
@ *  Contains function definitions for forward transform 32x32
@ *
@ * @author
@ * Mohit
@ *
@ * @par List of Functions:
@ *  - ihevc_resi_trans_32x32()
@ *
@ * @remarks
@ *  None
@ *
@ *******************************************************************************
@*/
@*/
.text
.p2align 2

.extern g_ai2_ihevc_trans_32
.extern g_ai4_ihevc_trans_32

g_ai2_ihevc_trans_32_addr_1:
.long g_ai2_ihevc_trans_32 - ulbl1 - 8

g_ai2_ihevc_trans_32_addr_2:
.long g_ai2_ihevc_trans_32 - ulbl2 - 8

g_ai4_ihevc_trans_32_addr:
.long g_ai4_ihevc_trans_32 - ulbl3 - 8

@*/
@*/
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
@*/  Input 32x32 pixels
@*/
@*/ @param[in] pu1_pred
@*/  Prediction data
@*/
@*/ @param[in] pi2_tmp
@*/  Temporary buffer of size 16x16
@*/
@*/ @param[out] pi2_dst
@*/  Output 32x32 coefficients
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
  .global ihevc_resi_trans_32x32_a9q
ihevc_resi_trans_32x32_a9q:

.equ TMP_STRIDE_32      ,  128              @16*4, Stride of tmp register
.equ SHIFT_32           ,  15               @shift = 15; // log2(iWidth) - 1 + g_uiBitIncrement

.equ COFF_STD_2B_32     ,  64               @Stride for g_ai2_ihevc_trans_32 in bytes
.equ COFF_STD_W_32      ,  64               @Stride for g_ai4_ihevc_trans_32 in bytes

@LOAD the function
    STMFD       SP!,{r4-r12,LR}     @stack store values of the arguments
    vpush       {d8 - d15}
    SUB         SP,SP,#32

    LDR         R4,[SP,#136]            @get src_strd
    LDR         R5,[SP,#140]            @get pred_strd
    LDR         R6,[SP,#144]            @get dst_strd_chr_flag

    MOV R8,#0                       @Set loop counter
    LDR R9,g_ai2_ihevc_trans_32_addr_1    @get 16 bit transform matrix
ulbl1:
    ADD R9, R9, PC

    @Read [0 0] [8 0] [16 0] [24 0],[0 1] [8 1] [16 1] [24 1] values of g_ai2_ihevc_trans_32
    @and write to stack
    MOV R12,#COFF_STD_2B_32
    LSL R12,#3

    VLD1.S32 D30[0],[R9],R12
    VLD1.S32 D30[1],[R9],R12        @ D30 - [0 0] [0 1] [8 0] [8 1]
    VLD1.S32 D31[0],[R9],R12
    VLD1.S32 D31[1],[R9],R12        @ D31 - [16 0] [16 1] [24 0] [24 1]

    VTRN.S32 D30,D31                @ D30 - [0 0] [0 1] [16 0] [16 1]
    VTRN.S16 D30,D31                @ D31 - [8 0] [8 1] [24 0] [24 1]
    VST1.S16 {D30,D31},[SP]

    LDR R9,g_ai2_ihevc_trans_32_addr_2    @get 16 bit transform matrix
ulbl2:
    ADD R9, R9, PC

    MOV R7,#TMP_STRIDE_32
@   AND R14,R6,#0x1

    VMOV.S32 Q14,#0

@R0     pu1_src
@R1     pu1_pred
@R2     pi4_tmp
@R3     pi2_dst
@R4     src_strd - 16
@R5     pred_strd - 16
@R6     dst_strd_chr_flag
@R7     tmp_dst Nx4 block stride
@R8     loop cntr
@R9     g_ai2_ihevc_trans_32
@R10    tmp_dst Nx4 block offset
@R11    tmp register
@R12    ------
@R14    ------.
@q14    shift 32 bit
@q15    add 32 bit

    SUB R4, R4, #16
    SUB R5, R5, #16
CORE_LOOP_32X32_HORIZ:

    VLD1.U8 {D0,D1},[R0]!           @LOAD 1-16 src row 1

    VLD1.U8 {D4,D5},[R1]!           @LOAD 1-16 pred row 1

    VLD1.U8 {D2,D3},[R0],R4         @LOAD 17-32 src row 1
    @ Residue calculation
    VSUBL.U8 Q8,D0,D4           @ Get residue 1-8 row 1 -- dual issued with prev. instr. 2nd cycle

    VLD1.U8 {D6,D7},[R1],R5         @LOAD 17-32 pred row 1
    VSUBL.U8 Q9,D1,D5           @ Get residue 9-16 row 1 -- dual issue

    VLD1.U8 {D8,D9},[R0]!           @ LOAD 1-16 src row 2
    VSUBL.U8 Q10,D2,D6          @ Get residue 17-24 row 1 -- dual issue

    VLD1.U8 {D12,D13},[R1]!         @ LOAD 1-16 pred row 2
    VSUBL.U8 Q11,D3,D7          @ Get residue 25-32 row 1 -- dual issue

    VLD1.U8 {D10,D11},[R0],R4           @ LOAD 17-32 src row 2
    @ Residue - Row 2
    VSUBL.U8 Q12,D8,D12         @ Get residue 1-8 row 2 -- dual issue

    VLD1.U8 {D14,D15},[R1],R5           @ LOAD 17-32 pred row 2
    VSUBL.U8 Q13,D9,D13         @ Get residue 9-16 row 2 -- dual issue
    @ Get blk sads
    VABDL.U8 Q15,D0,D4
    VABAL.U8 Q15,D1,D5
    VABAL.U8 Q15,D2,D6
    VABAL.U8 Q15,D3,D7
    VABAL.U8 Q15,D8,D12
    VABAL.U8 Q15,D9,D13
    VABAL.U8 Q15,D10,D14
    VABAL.U8 Q15,D11,D15
    VADDW.S16 Q14,Q14,D30
    VADDW.S16 Q14,Q14,D31
    @ SAD Ends

    VREV64.S16 Q10,Q10          @ Rev 17-24 row 1 -- dual issue
    VSUBL.U8 Q2,D10,D14         @ Get residue 17-24 row 2
    VREV64.S16 Q11,Q11          @ Rev 25-32 row 1 -- dual issue
    VSUBL.U8 Q3,D11,D15         @ Get residue 25-32 row 2

    VSWP D20,D21                @ Q10: 24 23 22 21 20 19 18 17 row 1
    VSWP D22,D23                @ Q11: 32 31 30 29 28 27 26 25 row 1

    VREV64.S16 Q2,Q2            @ Rev 17-24 row 2
    VADD.S16 Q5, Q9,Q10         @ e[k] = resi_tmp_1 + resi_tmp_2  k ->9-16 row 1 -- dual issue
    VREV64.S16 Q3,Q3            @ Rev 25-32 row 2
    VADD.S16 Q4, Q8,Q11         @ e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-8 row 1 -- dual issue
    VSWP D4,D5                  @ Q2: 24 23 22 21 20 19 18 17 row 2
    VSUB.S16 Q6, Q8,Q11         @ o[k] = resi_tmp_1 - resi_tmp_2  k -> 1-8 row 1 -- dual issue
    VSWP D6,D7                  @ Q3: 32 31 30 29 28 27 26 25 row 2
    VSUB.S16 Q7, Q9,Q10         @ o[k] = resi_tmp_1 - resi_tmp_2  k ->9-16 row 1 -- dual issue

    VREV64.16 Q5, Q5            @ Rev 9-16 of e[k], row 1
    VADD.S16 Q9, Q13,Q2         @ e[k] = resi_tmp_1 + resi_tmp_2  k ->9-16 row 2 -- dual issue
    VADD.S16 Q8, Q12,Q3         @ e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-8 row 2
    VSWP D10, D11               @ Q5: e[16] e[15] e[14] e[13] e[12] e[11] e[10] e[9]
    VSUB.S16 Q10, Q12,Q3        @ o[k] = resi_tmp_1 - resi_tmp_2  k -> 1-8 row 2 -- dual issue
    VREV64.16 Q9, Q9            @ Rev 9-16 of e[k], row 2
    VSUB.S16 Q11, Q13,Q2        @ o[k] = resi_tmp_1 - resi_tmp_2  k ->9-16 row 2 -- dual issue

    VADD.S16 Q0, Q4, Q5         @ ee[k] = e[k] + e[16-k] k->1-8 row 1
    VSWP D18, D19               @ Q9: e[16] e[15] e[14] e[13] e[12] e[11] e[10] e[9]
    VSUB.S16 Q1, Q4, Q5         @ eo[k] = e[k] - e[16-k] k->1-8 row 1 -- dual issue

    VREV64.S16  D8,D1           @ rev ee[k] k-> 4-7 row 1
    VADD.S16 Q2, Q8, Q9         @ ee[k] = e[k] + e[16-k] k->1-8 row 2 -- dual issue
    VSUB.S16 Q3, Q8, Q9         @ eo[k] = e[k] - e[16-k] k->1-8 row 2
    VMOV.S16    D1,D4
    VREV64.S16  D9,D5           @ rev ee[k] k-> 4-7 row 2

    @ arrangement OF DATA
    @ Q0    A1 A2 A3 A4 B1 B2 B3 B4
    @ Q4    A8 A7 A6 A5 B8 B7 B6 B5
    @---------------Process EEO starts--------------------
    MOV R12,#COFF_STD_2B_32     @Get stride of coeffs

    ADD R11,R9,R12,LSL #2       @Load address of g_ai2_ihevc_trans_32[4]
    LSL R12,R12,#3

    VADD.S16 Q13, Q0, Q4        @ eee[k] = ee[k] + ee[7 - k] row 1 & 2
    VLD1.S16 D24,[R11],R12      @ LOAD g_ai2_ihevc_trans_32[4][0-4]
    VSUB.S16 Q0, Q0 ,Q4         @ eeo[k] = ee[k] - ee[7 - k] row 1 & 2  -- dual issue

    @ D26 R1eee[0] R1eee[1] R1eee[2] R1eee[3]
    @ D27 R2eee[0] R2eee[1] R2eee[2] R2eee[3]
    VTRN.S32 D26,D27
    @ D26 R1eee[0] R1eee[1] R2eee[0] R2eee[1]
    @ D27 R1eee[2] R1eee[3] R2eee[2] R2eee[3]
    VREV32.16 D4,D27
    @ D26 R1eee[0] R1eee[1] R2eee[0] R2eee[1]
    @ D4 R1eee[3] R1eee[2] R2eee[3] R2eee[2]
    VMOV.S16 D27,D26
    VNEG.S16 D5,D4

    @ Q13 R1eee[0] R1eee[1] R2eee[0] R2eee[1]  R1eee[0]  R1eee[1]  R2eee[0]  R2eee[1]
    @ Q2  R1eee[3] R1eee[2] R2eee[3] R2eee[2] -R1eee[3] -R1eee[2] -R2eee[3] -R2eee[2]
    @ 1- cycle stall?
    VADD.S16 Q2,Q13,Q2
    @ Q2 R1eeee[0] R1eeee[1] R2eeee[0] R2eeee[1] R1eeeo[0] R1eeeo[1] R2eeeo[0] R2eeeo[1]

    @ Q2  R1eeee[0] R1eeee[1] R2eeee[0] R2eeee[1]
    @    R1eeeo[0] R1eeeo[1] R2eeeo[0] R2eeeo[1]
    VMULL.S16 Q15,D24,D0            @g_ai2_ihevc_trans_32[4][0-4] * eeo[0-4]    R1 -- dual issue
    VTRN.S16 D4,D5
    @ Q2  R1eeee[0] R1eeeo[0] R2eeee[0] R2eeeo[0]
    @    R1eeee[1] R1eeeo[1] R2eeee[1] R2eeeo[1]
    @ 1-cycle stall?
    VDUP.S32 D8,D4[0]               @ R1eeee[0] R1eeeo[0] R1eeee[0] R1eeeo[0]
    VDUP.S32 D9,D4[1]               @ R2eeee[0] R2eeeo[0] R2eeee[0] R2eeeo[0]
    VDUP.S32 D10,D5[0]              @ R1eeee[1] R1eeeo[1] R1eeee[1] R1eeeo[1]
    VDUP.S32 D11,D5[1]              @ R2eeee[1] R2eeeo[1] R2eeee[1] R2eeeo[1]

    @D4 : [0 0] [8 0] [16 0] [24 0]
    @D5 : [0 1] [8 1] [16 1] [24 1]
    VLD1.S16 {D4,D5},[SP]               @   [0 0] [8 0] [16 0] [24 0] [0 1] [8 1] [16 1] [24 1]
    VMULL.S16 Q8,D8,D4              @   g_ai2_ihevc_trans_32 * R1eeee[0] R1eeeo[0] R1eeee[0] R1eeeo[0] -- dual issue 2nd cycle
    VMLAL.S16 Q8,D10,D5             @ + g_ai2_ihevc_trans_32 * R1eeee[1] R1eeeo[1] R1eeee[1] R1eeeo[1]
    VLD1.S16 D27,[R11],R12          @LOAD g_ai2_ihevc_trans_32[12][0-4] -- 1st cycle dual issue with prev. MLAL
    VMULL.S16 Q9,D9,D4              @   g_ai2_ihevc_trans_32 * R2eeee[0] R2eeeo[0] R2eeee[0] R2eeeo[0] -- dual issue 2nd cycle
    VMLAL.S16 Q9,D11,D5             @ + g_ai2_ihevc_trans_32 * R2eeee[1] R2eeeo[1] R2eeee[1] R2eeeo[1]

    VMULL.S16 Q4,D24,D1             @g_ai2_ihevc_trans_32[4][0-4] * eeo[0-4]    R2

    VMULL.S16 Q5,D27,D0             @g_ai2_ihevc_trans_32[12][0-4] * eeo[0-4]   R1
    VZIP.S32 Q8,Q9                  @ 3-cycle instruction -- 1st cycle dual issued
    @These values must go to 0 8 16 24 rows hence we need stride *8
    LSL R10,R7,#3
    VMULL.S16 Q12,D27,D1            @g_ai2_ihevc_trans_32[12][0-4] * eeo[0-4]   R2
    VST1.32 D16,[R2],R10            @ -- dual issued

    VST1.32 D17,[R2],R10

    VLD1.S16 D26,[R11],R12          @LOAD g_ai2_ihevc_trans_32[20][0-4]

    VMULL.S16 Q8,D26,D1             @g_ai2_ihevc_trans_32[20][0-4] * eeo[0-4] R2
    VST1.32 D18,[R2],R10            @ -- dual issued

    VST1.32 D19,[R2],R10

    SUB R2,R2,R10,LSL #2
    @----------------------------Process EEEO ends----------------------------------------

    VLD1.S16 D27,[R11],R12          @LOAD g_ai2_ihevc_trans_32[28][0-4]
    VMULL.S16 Q9,D26,D0             @g_ai2_ihevc_trans_32[20][0-4] * eeo[0-4] R1

    VMULL.S16 Q2,D27,D1             @g_ai2_ihevc_trans_32[28][0-4] * eeo[0-4] R2
    @transpose the 4x4 matrix row1
    VTRN.32 Q15, Q5                 @R1 transpose1  -- dual issue
    VMULL.S16 Q13,D27,D0            @g_ai2_ihevc_trans_32[28][0-4] * eeo[0-4] R1

    @transpose the 4x4 matrix row2
    VTRN.32 Q4,Q12                  @R2 transpose1
    VTRN.32 Q8,Q2                   @R2 transpose1

    @-----------------------Processing EO ----------------------------
    MOV R12,#COFF_STD_2B_32         @Get coeffs stride
    ADD R11,R9,R12,LSL #1           @Load address of g_ai2_ihevc_trans_32[2]
    LSL R12,R12,#2
    VLD1.S16 {D0,D1},[R11],R12          @g_ai2_ihevc_trans_32[2][0-7]

    VSWP    D4,D25                  @R2 transpose2
    VSWP    D16,D9                  @R2 transpose2

    VADD.S32 Q4,Q4,Q12              @R2 add -- dual issue 1st cycle
    VTRN.32 Q9, Q13                 @R1 transpose1
    VADD.S32 Q8,Q8,Q2               @R2 add -- dual issue 2nd cycle

    VSWP    D18,D31                 @R1 transpose2
    VMULL.S16 Q2,D2,D0              @eo[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q2,D3,D1              @eo[0][4-7]*  R1

    VSWP    D26,D11                 @R1 transpose2
    VADD.S32 Q8,Q4,Q8               @R2 add -- dual issue

    VADD.S32 Q15,Q15,Q9             @R1 add
    VADD.S32 Q5,Q5,Q13              @R1 add
    VMULL.S16 Q4,D6,D0              @eo[0][0-3]*  R2
    VMLAL.S16 Q4,D7,D1              @eo[0][4-7]*  R2
    VADD.S32 Q15,Q15,Q5             @R1 add

    VLD1.S16 {D0,D1},[R11],R12          @g_ai2_ihevc_trans_32[6][0-7]

    VMULL.S16 Q5,D2,D0              @eo[1][0-3]*  R1
    VMLAL.S16 Q5,D3,D1              @eo[1][4-7]*  R1


    VZIP.S32 Q15,Q8                 @ 3-cycle instruction
    VMULL.S16 Q13,D6,D0             @eo[1][0-3]*  R2 -- dual issue
    VMLAL.S16 Q13,D7,D1             @eo[1][4-7]*  R2

    VLD1.S16 {D0,D1},[R11],R12          @g_ai2_ihevc_trans_32[10][0-7] -- dual issue with prev. MLAL

    @write to memory
    @this should go to 4 12 20 28
    LSL R10,R7,#3
    ADD R2,R2,R7,LSL #2             @move to fifth row
    VST1.32 D30,[R2],R10
    VMULL.S16 Q9,D2,D0              @eo[2][0-3]*  R1 -- dual issue
    VMLAL.S16 Q9,D3,D1              @eo[2][4-7]*  R1
    VST1.32 D31,[R2],R10            @ 1st cycle dual issued with MLAL

    VST1.32 D16,[R2],R10
    VMULL.S16 Q12,D6,D0             @eo[2][0-3]*  R2 -- dual issue
    VMLAL.S16 Q12,D7,D1             @eo[2][4-7]*  R2
    VST1.32 D17,[R2],R10            @ 1st cycle dual issued with MLAL

    SUB R2,R2,R10,LSL #2
    SUB R2,R2,R7,LSL #2
    @--------------------Done procrssing EEO -------------------------

    VLD1.S16 {D0,D1},[R11],R12  @g_ai2_ihevc_trans_32[14][0-7]

    VMULL.S16 Q8,D2,D0      @eo[3][0-3]*  R1
    VMLAL.S16 Q8,D3,D1      @eo[3][4-7]*  R1

    @transpose the 4x4 matrix R1
    VTRN.32 Q2, Q5          @
    VMULL.S16 Q15,D6,D0     @eo[3][0-3]*  R2 -- dual issued with 2nd cycle of TRN
    VMLAL.S16 Q15,D7,D1     @eo[3][4-7]*  R2
    VTRN.32 Q9, Q8          @ 1st cycle dual issued
    @transpose the 4x4 matrix R2
    VTRN.32 Q4,Q13

    VSWP    D18, D5         @ R1
    VSWP    D16, D11        @ R1
    VADD.S32 Q2, Q2, Q5     @ R1
    VADD.S32 Q9, Q9, Q8     @ R1
    VTRN.32 Q12,Q15         @ R2 -- dual issue
    VADD.S32 Q9, Q2, Q9     @ R1

    VSWP    D24,D9          @ R2
    VSWP    D30,D27         @ R2

    VLD1.S16 {D4,D5},[R11],R12  @g_ai2_ihevc_trans_32[18][0-7]

    VADD.S32 Q4, Q4, Q13    @ R2
    VADD.S32 Q12, Q12, Q15  @ R2
    VMULL.S16 Q0,D2,D4      @eo[4][0-3]*  R1
    VMLAL.S16 Q0,D3,D5      @eo[4][4-7]*  R1
    VADD.S32 Q12, Q4, Q12   @ R2

    VZIP.S32 Q9,Q12         @ 3-cycle
    VMULL.S16 Q4,D6,D4      @eo[0][0-3]*  R2  -- dual issue
    VMLAL.S16 Q4,D7,D5      @eo[0][4-7]*  R2

    VLD1.S16 {D4,D5},[R11],R12  @g_ai2_ihevc_trans_32[22][0-7] -- 1st cycle dual issued with prev. instr

    @write to memory
    @this should go to 2 6 10 14
    ADD R2,R2,R7, LSL #1
    LSL R7,R7,#2
    VST1.32 D18,[R2],R7
    VMULL.S16 Q5,D2,D4      @eo[5][0-3]*  R1  -- dual issue
    VMLAL.S16 Q5,D3,D5      @eo[5][4-7]*  R1
    VST1.32 D19,[R2],R7     @ 1st cycle dual issued with prev. instr

    VST1.32 D24,[R2],R7
    VMULL.S16 Q8,D6,D4      @eo[0][0-3]*  R2  -- dual issue
    VMLAL.S16 Q8,D7,D5      @eo[0][4-7]*  R2
    VST1.32 D25,[R2],R7     @ 1st cycle dual issued with prev. instr


    VLD1.S16 {D4,D5},[R11],R12  @g_ai2_ihevc_trans_32[26][0-7]
    VMULL.S16 Q9,D2,D4      @eo[6][0-3]*  R1
    VMLAL.S16 Q9,D3,D5      @eo[6][4-7]*  R1
    VMULL.S16 Q12,D6,D4     @eo[0][0-3]*  R2
    VMLAL.S16 Q12,D7,D5     @eo[0][4-7]*  R2

    VLD1.S16 {D4,D5},[R11],R12  @g_ai2_ihevc_trans_32[30][0-7]
    VMULL.S16 Q13,D2,D4     @eo[7][0-3]*  R1
    VMLAL.S16 Q13,D3,D5     @eo[7][4-7]*  R1
    VMULL.S16 Q15,D6,D4     @eo[0][0-3]*  R2
    VMLAL.S16 Q15,D7,D5     @eo[0][4-7]*  R2

    @-----------------------Processing O ----------------------------
    MOV R12,#COFF_STD_2B_32 @Get coeffs stride
    LSL R12,R12,#1
    ADD R11,R9,#COFF_STD_2B_32  @Get address of g_ai2_ihevc_trans_32[1]
    SUB R12, R12, #16

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[1][0-7]

    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[1][8-15]
    VMULL.S16 Q1,D20,D4     @o[0][0-3]*  R2 -- dual issue
    VMLAL.S16 Q1,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q1,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q1,D23,D7     @o[0][12-15]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q5          @ R1
    VTRN.32 Q9,Q13          @ R1
    @transpose the 4x4 matrix R2
    VTRN.32 Q4,Q8           @ R2
    VSWP    D18, D1         @ R1
    VSWP    D26, D11        @ R1
    VTRN.32 Q12,Q15         @ R2
    VADD.S32 Q0, Q0, Q5     @ R1 -- dual issue
    VADD.S32 Q9, Q9, Q13    @ R1

    VSWP    D24,D9          @ R2
    VSWP    D30,D17         @ R2
    VADD.S32 Q9, Q0, Q9     @ R1 -- dual issue

    VMULL.S16 Q0,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q0,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q0,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q0,D15,D7     @o[0][12-15]*  R1

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[3][0-7]
    VADD.S32 Q4, Q4, Q8     @ R2 -- dual issue
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[3][8-15]
    VADD.S32 Q12, Q12, Q15  @ R2 -- dual issue

    VMULL.S16 Q5,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q5,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q5,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q5,D23,D7     @o[0][12-15]*  R2
    VADD.S32 Q12, Q4, Q12   @ R2

    VZIP.S32 Q9,Q12
    VMULL.S16 Q4,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q4,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q4,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q4,D15,D7     @o[0][12-15]*  R1

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[5][0-7] -- 1st cycle dual issued with prev. instr

    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[5][8-15]
    VMULL.S16 Q8,D12,D4     @o[0][0-3]*  R1 -- dual issue with 2nd cycle
    VMLAL.S16 Q8,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q8,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q8,D15,D7     @o[0][12-15]*  R1
    @this should go to 18 22 26 30
    VST1.32 D18,[R2],R7     @1st cycle dual issue

    VST1.32 D19,[R2],R7

    VST1.32 D24,[R2],R7
    VMULL.S16 Q9,D20,D4     @o[0][0-3]*  R2 -- dual issue with 2nd cycle
    VMLAL.S16 Q9,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q9,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q9,D23,D7     @o[0][12-15]*  R2

    VST1.32 D25,[R2],R7     @ 1st cycle dual issue

    SUB R2,R2,R7, LSL #3
    LSR R7,R7,#2
    SUB R2,R2,R7, LSL #1
    @--------------------Done Processing EO--------------------------


    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[7][0-7]

    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[7][8-15]
    VMULL.S16 Q12,D12,D4    @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q12,D13,D5    @o[0][4-7]*  R1 -- dual issue
    VMLAL.S16 Q12,D14,D6    @o[0][8-11]*  R1
    VMLAL.S16 Q12,D15,D7    @o[0][12-15]*  R1
    VMULL.S16 Q13,D20,D4    @o[0][0-3]*  R2
    VMLAL.S16 Q13,D21,D5    @o[0][4-7]*  R2
    VMLAL.S16 Q13,D22,D6    @o[0][8-11]*  R2
    VMLAL.S16 Q13,D23,D7    @o[0][12-15]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q4          @ R1
    VTRN.32 Q8, Q12         @ R1
    @transpose the 4x4 matrix R2
    VTRN.32 Q1, Q5          @ R2
    VSWP    D16, D1         @ R1
    VSWP    D24, D9         @ R1

    VTRN.32 Q9, Q13         @ R2
    VADD.S32 Q0, Q0, Q4     @ R1 -- dual issue
    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[9][0-7]
    VADD.S32 Q8, Q8, Q12    @ R1 -- dual issue

    VSWP    D18, D3         @ R2
    VSWP    D26, D11        @ R2
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[9][8-15]
    VADD.S32 Q8, Q0, Q8     @ R1 -- dual issue

    VADD.S32 Q1, Q1, Q5     @ R2
    VADD.S32 Q9, Q9, Q13    @ R2

    VMULL.S16 Q0,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q0,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q0,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q0,D15,D7     @o[0][12-15]*  R1
    VADD.S32 Q9, Q1, Q9     @ R2

    VMULL.S16 Q1,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q1,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q1,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q1,D23,D7     @o[0][12-15]*  R2

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[11][0-7] -- 1st cycle dual issue
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[11][8-15]

    VZIP.S32 Q8, Q9

    @write to memory
    @this should go to 1 3 5 7
    ADD R2,R2,R7
    LSL R7,R7,#1
    VST1.32 D16, [R2], R7

    VST1.32 D17, [R2], R7
    VMULL.S16 Q4,D12,D4     @o[0][0-3]*  R1 -- dual issued with 2nd cycle
    VMLAL.S16 Q4,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q4,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q4,D15,D7     @o[0][12-15]*  R1

    VST1.32 D18, [R2], R7   @ 1st cycle dual issued
    VMULL.S16 Q5,D20,D4     @o[0][0-3]*  R2 -- dual issue with 2nd cycle
    VMLAL.S16 Q5,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q5,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q5,D23,D7     @o[0][12-15]*  R2

    VST1.32 D19, [R2], R7   @ 1st cycle dual issued


    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[13][0-7]

    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[13][8-15]
    VMULL.S16 Q8,D12,D4     @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q8,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q8,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q8,D15,D7     @o[0][12-15]*  R1
    VMULL.S16 Q9,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q9,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q9,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q9,D23,D7     @o[0][12-15]*  R2

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[15][0-7] - 1st cycle dual issue
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[15][8-15]
    VMULL.S16 Q12,D12,D4    @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q12,D13,D5    @o[0][4-7]*  R1
    VMLAL.S16 Q12,D14,D6    @o[0][8-11]*  R1
    VMLAL.S16 Q12,D15,D7    @o[0][12-15]*  R1
    VMULL.S16 Q13,D20,D4    @o[0][0-3]*  R2
    VMLAL.S16 Q13,D21,D5    @o[0][4-7]*  R2
    VMLAL.S16 Q13,D22,D6    @o[0][8-11]*  R2
    VMLAL.S16 Q13,D23,D7    @o[0][12-15]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q4          @ R1 1st cycle dual issue
    VTRN.32 Q8, Q12         @ R1
    @transpose the 4x4 matrix R2
    VTRN.32 Q1, Q5          @ R2
    VSWP    D16, D1         @ R1
    VSWP    D24, D9         @ R1

    VTRN.32 Q9, Q13         @ R2
    VADD.S32 Q0, Q0, Q4     @ R1 -- dual issue
    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[17][0-7]
    VADD.S32 Q8, Q8, Q12    @ R1 -- dual issue

    VSWP    D18, D3         @ R2
    VSWP    D26, D11        @ R2
    VADD.S32 Q8, Q0, Q8     @ R1 -- dual issue with 1st cycle
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[17][8-15]


    VADD.S32 Q1, Q1, Q5     @ R2 -- dual issue with 2nd cycle
    VADD.S32 Q9, Q9, Q13    @ R2

    VMULL.S16 Q0,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q0,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q0,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q0,D15,D7     @o[0][12-15]*  R1
    VADD.S32 Q9, Q1, Q9     @ R2

    VMULL.S16 Q1,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q1,D21,D5     @o[0][4-7]*  R2
    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[19][0-7]
    VMLAL.S16 Q1,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q1,D23,D7     @o[0][12-15]*  R2
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[19][8-15]

    VZIP.S32 Q8, Q9

    @write to memory
    @this should go to 9 11 13 15
    VST1.32 D16, [R2], R7
    VMULL.S16 Q4,D12,D4     @o[0][0-3]*  R1 -- dual issued with 2nd cycle
    VMLAL.S16 Q4,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q4,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q4,D15,D7     @o[0][12-15]*  R1

    VST1.32 D17, [R2], R7   @ 1st cycle dual issued
    VMULL.S16 Q5,D20,D4     @o[0][0-3]*  R2 -- dual issue with 2nd cycle
    VMLAL.S16 Q5,D21,D5     @o[0][4-7]*  R2
    VST1.32 D18, [R2], R7   @1st cycle dual issued
    VMLAL.S16 Q5,D22,D6     @o[0][8-11]*  R2 -- dual issued with 2nd cycle
    VMLAL.S16 Q5,D23,D7     @o[0][12-15]*  R2

    VST1.32 D19, [R2], R7   @ 1st cycle dual issue


    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[21][0-7]
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[21][8-15]
    VMULL.S16 Q8,D12,D4     @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q8,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q8,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q8,D15,D7     @o[0][12-15]*  R1
    VMULL.S16 Q9,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q9,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q9,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q9,D23,D7     @o[0][12-15]*  R2

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[23][0-7]
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[23][8-15]
    VMULL.S16 Q12,D12,D4    @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q12,D13,D5    @o[0][4-7]*  R1 -- dual issue
    VMLAL.S16 Q12,D14,D6    @o[0][8-11]*  R1
    VMLAL.S16 Q12,D15,D7    @o[0][12-15]*  R1
    VMULL.S16 Q13,D20,D4    @o[0][0-3]*  R2
    VMLAL.S16 Q13,D21,D5    @o[0][4-7]*  R2
    VMLAL.S16 Q13,D22,D6    @o[0][8-11]*  R2
    VMLAL.S16 Q13,D23,D7    @o[0][12-15]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q4          @ R1
    VTRN.32 Q8, Q12         @ R1
    @transpose the 4x4 matrix R2
    VTRN.32 Q1, Q5          @ R2
    VSWP    D16, D1         @ R1
    VSWP    D24, D9         @ R1

    VTRN.32 Q9, Q13         @ R2
    VADD.S32 Q0, Q0, Q4     @ R1 -- dual issue
    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[25][0-7]
    VADD.S32 Q8, Q8, Q12    @ R1 -- dual issue

    VSWP    D18, D3         @ R2
    VSWP    D26, D11        @ R2
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[25][8-15]
    VADD.S32 Q8, Q0, Q8     @ R1 -- dual issue

    VADD.S32 Q1, Q1, Q5     @ R2
    VADD.S32 Q9, Q9, Q13    @ R2

    VMULL.S16 Q0,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q0,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q0,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q0,D15,D7     @o[0][12-15]*  R1
    VADD.S32 Q9, Q1, Q9     @ R2

    VMULL.S16 Q1,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q1,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q1,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q1,D23,D7     @o[0][12-15]*  R2

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[27][0-7]
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[27][8-15]

    VZIP.S32 Q8, Q9
    VMULL.S16 Q4,D12,D4     @o[0][0-3]*  R1
    VMLAL.S16 Q4,D13,D5     @o[0][4-7]*  R1
    VMLAL.S16 Q4,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q4,D15,D7     @o[0][12-15]*  R1
    @write to memory
    @this should go to 17 19 21 23
    VST1.32 D16, [R2], R7
    VMULL.S16 Q5,D20,D4     @o[0][0-3]*  R2 -- dual issue
    VST1.32 D17, [R2], R7
    VMLAL.S16 Q5,D21,D5     @o[0][4-7]*  R2 -- dual issue
    VST1.32 D18, [R2], R7
    VMLAL.S16 Q5,D22,D6     @o[0][8-11]*  R2 -- dual issue
    VST1.32 D19, [R2], R7
    VMLAL.S16 Q5,D23,D7     @o[0][12-15]*  R2 -- dual issue

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[29][0-7]
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[29][8-15]
    VMULL.S16 Q8,D12,D4     @o[0][0-3]*  R1 -- dual issue
    VMLAL.S16 Q8,D13,D5     @o[0][4-7]*  R1  -- dual issue
    VMLAL.S16 Q8,D14,D6     @o[0][8-11]*  R1
    VMLAL.S16 Q8,D15,D7     @o[0][12-15]*  R1
    VMULL.S16 Q9,D20,D4     @o[0][0-3]*  R2
    VMLAL.S16 Q9,D21,D5     @o[0][4-7]*  R2
    VMLAL.S16 Q9,D22,D6     @o[0][8-11]*  R2
    VMLAL.S16 Q9,D23,D7     @o[0][12-15]*  R2

    VLD1.S16 {D4,D5},[R11]!     @g_ai2_ihevc_trans_32[31][0-7]
    VLD1.S16 {D6,D7},[R11],R12  @g_ai2_ihevc_trans_32[31][8-15]
    VMULL.S16 Q12,D12,D4    @o[0][0-3]*  R1 -- dual issued
    VMLAL.S16 Q12,D13,D5    @o[0][4-7]*  R1 -- dual issued
    VMLAL.S16 Q12,D14,D6    @o[0][8-11]*  R1
    VMLAL.S16 Q12,D15,D7    @o[0][12-15]*  R1
    VMULL.S16 Q13,D20,D4    @o[0][0-3]*  R2
    VMLAL.S16 Q13,D21,D5    @o[0][4-7]*  R2
    VMLAL.S16 Q13,D22,D6    @o[0][8-11]*  R2
    VMLAL.S16 Q13,D23,D7    @o[0][12-15]*  R2

    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q4          @ R1
    VTRN.32 Q8, Q12         @ R1
    @transpose the 4x4 matrix R2
    VTRN.32 Q1, Q5          @ R2
    VSWP    D16, D1         @ R1
    VSWP    D24, D9         @ R1

    VTRN.32 Q9, Q13         @ R2
    VADD.S32 Q0, Q0, Q4     @ R1 -- dual issue
    VADD.S32 Q8, Q8, Q12    @ R1
    @ 1-cycle stall?
    VADD.S32 Q0, Q0, Q8     @ R1
    VSWP    D18, D3         @ R2
    VSWP    D26, D11        @ R2
    VADD.S32 Q1, Q1, Q5     @ R2
    VADD.S32 Q9, Q9, Q13    @ R2
    @ 1-cycle stall?
    VADD.S32 Q1, Q1, Q9     @ R2
    @ 2-cycle stall?
    VZIP.S32 Q0, Q1         @ 3-cycle instruction

    @ 1-cycle stall?
    @write to memory
    @this should go to 25 27 29 31
    VST1.32 D0, [R2], R7
    VST1.32 D1, [R2], R7
    VST1.32 D2, [R2], R7
    VST1.32 D3, [R2], R7
    @------------------Processing O ends-------------------------------

    SUB R2,R2,R7,LSL #4
    LSR R7,R7,#1
    SUB R2,R2,R7

    ADD R2,R2,#8                @MOVE TO NEXT to next COLUMN - pi4_tmp

    ADD R8,R8,#2                @increment loop cntr
    CMP R8,#32                  @check loop cntr
    BNE CORE_LOOP_32X32_HORIZ   @jump acc


@*****************Vertical transform************************************

@Initialization for vert transform
@pi4_tmp will be the new src
@tmp stride will be new src stride
@dst will be new pi4_tmp
@dst stride will be new tmp stride
@trans table will be of 32 bit

    LDR R9,g_ai4_ihevc_trans_32_addr    @get 32 bit transform matrix
ulbl3:
    ADD R9, R9, PC

    SUB R0,R2,#128                  @set tmp as src [-32 to move back to orgin]
    MOV R2,R3                       @set dst as tmp
    MOV R4,#TMP_STRIDE_32           @set tmp stride as src stride
    SUB R4,#112                     @Adjust stride for 7 previous loads
    LSR R7,R6,#15                   @Set dst stride as tmp stride


    @Block SAD
    VADD.S32 D28,D28,D29
    VPADD.S32 D28,D28,D29
    VMOV.S32 R3,D28[0]

    @Read [0 0] [8 0] [16 0] [24 0],[0 1] [8 1] [16 1] [24 1]
    @values of g_ai4_ihevc_trans_32 and write to stack
    MOV R12,#COFF_STD_W_32
    LSL R12,R12,#3
    VLD1.S32 D28,[R9],R12           @ D28: [0 0] [0 1]
    VLD1.S32 D29,[R9],R12           @ D29: [8 0] [8 1]
    VLD1.S32 D30,[R9],R12           @ D30: [16 0] [16 1]
    VLD1.S32 D31,[R9],R12           @ D31: [24 0] [24 1]
    SUB R9,R9,R12,LSL #2

    VREV64.32 Q15,Q15               @ Q15: [16 1] [16 0] [24 1] [24 0]
    VTRN.S32 Q14,Q15                @ Q14: [0 0] [16 1] [8 0] [24 1]
                                    @ Q15: [0 1] [16 0] [8 1] [24 0]
    VST1.S32 {Q14-Q15},[SP]

@   VMOV.U32 Q14,#RADD              ;get the round factor to q14
@   VMOV.U32 Q15,#SHIFT             ;Get the shift to neon

    MOV R8,#0                   @INIT LOOP

CORE_LOOP_32X32_VERT:

    VLD1.S32 {D0,D1},[R0]!          @LOAD 1-4 src R1
    VLD1.S32 {D2,D3},[R0]!          @LOAD 5-8 src R1
    VLD1.S32 {D4,D5},[R0]!          @LOAD 9-12 src R1
    VLD1.S32 {D6,D7},[R0]!          @LOAD 13-16 src R1
    VLD1.S32 {D8,D9},[R0]!          @LOAD 17-20 src R1
    VREV64.S32 Q4,Q4            @Rev 17-20 R1
    VLD1.S32 {D10,D11},[R0]!            @LOAD 21-24 src R1
    VREV64.S32 Q5,Q5            @Rev 21-24 R1
    VLD1.S32 {D12,D13},[R0]!            @LOAD 25-28 src R1
    VREV64.S32 Q6,Q6            @Rev 25-28 R1
    VLD1.S32 {D14,D15},[R0],R4          @LOAD 29-32 src R1
    VREV64.S32 Q7,Q7            @Rev 29-32 R1

    VSWP D8,D9                  @ Q4: 20 19 18 17
    VADD.S32 Q11, Q3, Q4        @e[k] = resi_tmp_1 + resi_tmp_2  k -> 13-16  R1-- dual issue
    VSWP D10,D11                @ Q5: 24 23 22 21
    VADD.S32 Q10, Q2, Q5        @e[k] = resi_tmp_1 + resi_tmp_2  k -> 9-12  R1-- dual issue
    VSWP D12,D13                @ Q6: 28 27 26 25
    VADD.S32 Q9, Q1, Q6         @e[k] = resi_tmp_1 + resi_tmp_2  k -> 5-8  R1 -- dual issue
    VSWP D14,D15                @ Q7: 32 31 30 29

    VADD.S32 Q8, Q0, Q7         @e[k] = resi_tmp_1 + resi_tmp_2  k -> 1-4  R1 -- dual issue
    VREV64.S32 Q11, Q11         @rev e[k] k-> 13-16 R1 -- dual issue
    VSUB.S32 Q12, Q0, Q7        @o[k] = resi_tmp_1 - resi_tmp_2  k -> 1-4  R1
    VREV64.S32 Q10, Q10         @rev e[k] k-> 9-12 R1 -- dual issue
    VSUB.S32 Q13, Q1, Q6        @o[k] = resi_tmp_1 - resi_tmp_2  k -> 5-8  R1
    VSWP D22, D23               @Q11: e[16] e[15] e[14] e[13] -- dual issue
    VSUB.S32 Q14, Q2, Q5        @o[k] = resi_tmp_1 - resi_tmp_2  k -> 9-12  R1
    VSWP D20, D21               @Q10: e[12] e[11] e[10] e[9] -- dual issue
    VSUB.S32 Q15, Q3, Q4        @o[k] = resi_tmp_1 - resi_tmp_2  k -> 13-16  R1

    VADD.S32 Q1, Q9, Q10        @ee[k] = e[k] + e[15- k] row R1, k-> 4-7
    VADD.S32 Q0, Q8, Q11        @ee[k] = e[k] + e[15- k] row R1, k-> 0-3

    VSUB.S32 Q2, Q8, Q11        @eo[k] = e[k] - e[15 - k] row R1, k-> 0-3
    VSUB.S32 Q3, Q9, Q10        @eo[k] = e[k] - e[15 - k] row R1, k-> 4-7
    VREV64.S32 Q1, Q1           @Q1: ee[5] ee[4] ee[7] ee[6] -- dual issue

    VSWP D2, D3                 @Q1: ee[7] ee[6] ee[5]  ee[4]

    VADD.S32 Q4, Q0, Q1         @eee[k] = ee[k] + ee[7-k] row R1, k-> 0-3
    VSUB.S32 Q5, Q0, Q1         @eeo[k] = ee[k] - ee[7-k] row R1, k-> 0-3

    @D8: eee[0] eee[1]
    VLD1.S32 {Q10,Q11},[SP]     @Load g_ai4_ihevc_trans_32[xx]->  Q10 : [0 0] [16 1] [8 0] [24 1]  Q11 : [0 1] [16 0] [8 1] [24 0]
    VREV64.S32 D9, D9           @D9: eee[3] eee[2]

    @-----------------------Processing EEO ----------------------------
                                @Q5 :R1eeo[0] R1eeo[1] R1eeo[2] R1eeo[3]
    MOV R12,#COFF_STD_W_32
    ADD R11,R9,R12,LSL #2       @Get to the 4th row of src
    LSL R12,R12,#3

    VADD.S32 D12, D8, D9        @eeee[0] eeee[1] -- dual issue in 1st cycle
    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai4_ihevc_trans_32[4][0-4] -> 4G0 4G1 4G2 4G3
    VSUB.S32 D13, D8, D9        @eeeo[0] eeeo[1] -- dual issue in 2nd cycle

    VMUL.S32  Q0,Q5,Q7          @4G0 4G1 4G2 4G3 * R1eeo[0] R1eeo[1] R1eeo[2] R1eeo[3]

    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai4_ihevc_trans_32[12][0-4] -- 1st cycle dual issue
    VREV64.S32 Q8,Q6            @Q6 : eeee[0] eeee[1] eeeo[0] eeeo[1] R1   ->   ;Q8 : eeee[1] eeee[0] eeeo[1] eeeo[0] R1

    VMUL.S32 Q4,Q6,Q10          @g_ai4_ihevc_trans_32 * eeee[0] eeee[1] eeeo[0] eeeo[1] R1 -- dual issue
    VMLA.S32 Q4,Q8,Q11          @g_ai4_ihevc_trans_32 * eeee[1] eeee[0] eeeo[1] eeeo[0] R1

    VMUL.S32  Q9,Q5,Q7          @g_ai4_ihevc_trans_32[6][0-4] * eeo[0-4]

    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai4_ihevc_trans_32[20][0-4] - 1st cycle dual issue
    VRSHRN.S32 D8,Q4,#SHIFT_32  @ROUND NARROW R1 -- dual issued in 2nd cycle
                                @ D8: 0 16 8 24
    @WRITE INTO MEM the values or wait to be shuffled
    @These values must go to 0 8 16 24 colums
    LSL R10,R7,#3
    VST1.S16 D8[0],[R2],R10
    VMUL.S32  Q10,Q5,Q7         @g_ai4_ihevc_trans_32[10][0-4] * eeo[0-4] -- dual issued

    VLD1.S32  {D14,D15},[R11],R12       @LOAD g_ai4_ihevc_trans_32[28][0-4]

    VST1.S16 D8[2],[R2],R10
    VMUL.S32  Q11,Q5,Q7         @g_ai4_ihevc_trans_32[14][0-4] * eeo[0-4] -- dual issue
    @transpose the 4x4 matrix R1
    VTRN.32 Q0, Q9
    @-----------------------Processing EO ----------------------------
    MOV R12,#COFF_STD_W_32
    ADD R11,R9,R12,LSL #1       @Get 1ST row
    LSL R12,R12,#2

    VLD1.S32 {Q6,Q7},[R11],R12  @g_ai4_ihevc_trans_16[2][0-7]

    VMUL.S32 Q8,Q6,Q2           @g_ai4_ihevc_trans_16[2][0-3]*eo[0][0-3] R1
    VTRN.32 Q10, Q11            @ dual issue
    VMLA.S32 Q8,Q7,Q3           @g_ai4_ihevc_trans_16[2][4-7]*eo[0][4-7] R1


    VSWP    D20, D1
    VSWP    D22, D19

    VST1.S16 D8[1],[R2],R10
    VADD.S32 Q0, Q0, Q9         @ dual issue
    VST1.S16 D8[3],[R2],R10
    VADD.S32 Q10, Q10, Q11      @ dual issue
    SUB R2,R2,R10, LSL #2
    @-----------------------Processing EEEO complete-------------------

    VLD1.S32 {Q4,Q5},[R11],R12  @g_ai4_ihevc_trans_16[6][0-7]
    VADD.S32 Q0, Q0, Q10        @ dual issue

    VMUL.S32 Q7,Q4,Q2           @eo[0][0-3]
    VMLA.S32 Q7,Q5,Q3           @eo[0][4-7]
    VRSHRN.S32 D0,Q0,#SHIFT_32  @ Shift by SHIFT and Round the result

    VLD1.S32 {Q9,Q10},[R11],R12 @g_ai4_ihevc_trans_16[10][0-7]
    VADD.S32 D12,D16,D17        @g_ai4_ihevc_trans_16[2][k]*eo[0][k]+g_ai4_ihevc_trans_16[2][7-k]*eo[0][7-k] R1 -- dual issue


    VMUL.S32 Q8,Q9,Q2           @eo[0][0-3]
    VMLA.S32 Q8,Q10,Q3          @eo[0][4-7]

    @this should go to 4  12  20  28
    LSL R10,R7,#3
    ADD R2,R2,R7,LSL #2         @move to fifth row
    VST1.S16 D0[0], [R2], R10
    VADD.S32 D13,D14,D15        @ -- dual issue--
    VST1.S16 D0[1], [R2], R10
    VADD.S32 D10,D16,D17        @ -- dual issue --

    VLD1.S32 {Q7,Q8},[R11],R12  @g_ai4_ihevc_trans_16[14][0-7]

    VST1.S16 D0[2], [R2], R10
    VMUL.S32 Q9,Q7,Q2           @eo[0][0-3] -- dual issue
    VST1.S16 D0[3], [R2], R10
    VMLA.S32 Q9,Q8,Q3           @eo[0][4-7] -- dual issue
    SUB R2,R2,R10,LSL #2        @go back to orgin
    SUB R2,R2,R7,LSL #2
    @----------------------Processing EEO  complete-------------------

    VLD1.S32 {Q0,Q1},[R11],R12  @g_ai4_ihevc_trans_16[18][0-7]

    VMUL.S32 Q4,Q0,Q2           @g_ai4_ihevc_trans_16[18][0-3]*eo[0][0-3] R1
    VMLA.S32 Q4,Q1,Q3           @g_ai4_ihevc_trans_16[18][4-7]*eo[0][4-7] R1

    VLD1.S32 {Q0,Q1},[R11],R12  @g_ai4_ihevc_trans_16[22][0-7]
    VADD.S32 D11,D18,D19        @ dual issue

    @Q5 Q6
    VMUL.S32 Q7,Q0,Q2           @eo[0][0-3]
    VMLA.S32 Q7,Q1,Q3           @eo[0][4-7]

    VPADD.S32 D16,D12,D13
    VPADD.S32 D17,D10,D11

    VADD.S32 D12,D8,D9          @g_ai4_ihevc_trans_16[18][k]*eo[0][k]+g_ai4_ihevc_trans_16[18][7-k]*eo[0][7-k] R1
    VADD.S32 D13,D14,D15

    VRSHRN.S32 D14,Q8,#SHIFT_32
    VLD1.S32 {Q0,Q1},[R11],R12  @g_ai4_ihevc_trans_16[26][0-7]
    VLD1.S32 {Q10,Q11},[R11],R12    @g_ai4_ihevc_trans_16[30][0-7]
    @write to memory this should go to 2 6 10 14
    ADD R2,R2,R7,LSL #1
    LSL R7,R7,#2
    VST1.S16 D14[0],[R2],R7
    VMUL.S32 Q8,Q0,Q2           @eo[0][0-3] -- dual issue
    VST1.S16 D14[1],[R2],R7
    VMLA.S32 Q8,Q1,Q3           @eo[0][4-7] -- dual issue
    VST1.S16 D14[2],[R2],R7
    VMUL.S32 Q9,Q10,Q2          @eo[0][0-3] -- dual issue
    VST1.S16 D14[3],[R2],R7
    VMLA.S32 Q9,Q11,Q3          @eo[0][4-7] -- dual issue

    VADD.S32 D10,D16,D17
    @---------------Processing O Row 1-----------------------------------------------
    MOV R12,#COFF_STD_W_32
    ADD R11,R9,R12              @Get 1ST row
    LSL R12,R12,#1
    SUB R12, R12, #32
    VLD1.S32 {Q0,Q1},[R11]!

    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[1][0-15]
    VADD.S32 D11,D18,D19        @ dual issue in 2nd cycle

    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[1][0-3]*o[0][0-3] R1
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[1][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[1][8-11]*o[0][8-11] R1
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[1][12-15]*o[0][12-15] R1

    @Q5 Q6
    VPADD.S32 D16,D12,D13
    VPADD.S32 D17,D10,D11


    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[3][0-15]
    VRSHRN.S32 D16,Q8,#SHIFT_32 @ dual issue

    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[3][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[3][4-7]*o[0][4-7] R1
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[3][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[3][12-15]*o[0][12-15] R1

    @write to memory this should go to 2 6 10 14
    VST1.S16 D16[0],[R2],R7
    VADD.S32 D10,D8,D9          @g_ai4_ihevc_trans_32[1][0-3]*o[0][0-3]+g_ai4_ihevc_trans_32[1][4-7]*o[0][4-7]+g_ai4_ihevc_trans_32[1][8-11]*o[0][8-11]+g_ai4_ihevc_trans_32[1][12-15]*o[0][12-15]
    VLD1.S32 {Q0,Q1},[R11]!
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[5][0-15]
    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[5][0-3]*o[0][0-3] R1 -- dual issue
    VST1.S16 D16[1],[R2],R7
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[5][4-7]*o[0][4-7] R1 -- dual issue
    VST1.S16 D16[2],[R2],R7
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[5][8-11]*o[0][8-11] R1 -- dual issue
    VST1.S16 D16[3],[R2],R7
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[5][12-15]*o[0][12-15] R1
    SUB R2,R2,R7, LSL #3
    LSR R7,R7,#2
    SUB R2,R2,R7, LSL #1

    @--------------------Done Processing EO--------------------------

    VLD1.S32 {Q0,Q1},[R11]!
    VADD.S32 D11,D14,D15        @ dual issued
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[7][0-15]
    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[7][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[7][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[7][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[7][12-15]*o[0][12-15] R1


    VADD.S32 D12,D8,D9          @ dual issued
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[9][0-15]
    @Q5 Q6
    VPADD.S32 D16,D10,D11
    VADD.S32 D13,D14,D15

    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[9][0-3]*o[0][0-3] R1
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[9][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[9][8-11]*o[0][8-11] R1
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[9][12-15]*o[0][12-15] R1
    VPADD.S32 D17,D12,D13


    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[11][0-15]
    VRSHRN.S32 D16,Q8,#SHIFT_32 @ duall issue

    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[11][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[11][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[11][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[11][12-15]*o[0][12-15] R1
    VADD.S32 D10,D8,D9          @g_ai4_ihevc_trans_32[9][0-3]*o[0][0-3]+g_ai4_ihevc_trans_32[9][4-7]*o[0][4-7]+g_ai4_ihevc_trans_32[9][8-11]*o[0][8-11]+g_ai4_ihevc_trans_32[9][12-15]*o[0][12-15]
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[13][0-15]
    @write to memory this should go to 1 3 5 7
    ADD R2,R2,R7
    LSL R7,R7,#1
    VST1.S16 D16[0],[R2],R7
    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[13][0-3]*o[0][0-3] R1
    VST1.S16 D16[1],[R2],R7
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[13][4-7]*o[0][4-7] R1
    VST1.S16 D16[2],[R2],R7
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[13][8-11]*o[0][8-11] R1
    VST1.S16 D16[3],[R2],R7
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[13][12-15]*o[0][12-15] R1

    VLD1.S32 {Q0,Q1},[R11]!
    VADD.S32 D11,D14,D15        @ dual issue
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[15][0-15]
    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[15][0-3]*o[0][0-3] R1 -- dual issue--
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[15][4-7]*o[0][4-7] R1
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[15][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[15][12-15]*o[0][12-15] R1

    VLD1.S32 {Q0,Q1},[R11]!
    VADD.S32 D12,D8,D9          @ dual issued

    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[17][0-15]
    @Q5 Q6
    VPADD.S32 D16,D10,D11
    VADD.S32 D13,D14,D15

    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[17][0-3]*o[0][0-3] R1
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[17][4-7]*o[0][4-7] R1
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[17][8-11]*o[0][8-11] R1
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[17][12-15]*o[0][12-15] R1
    VPADD.S32 D17,D12,D13

    VLD1.S32 {Q0,Q1},[R11]!
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[19][0-15]
    VRSHRN.S32 D16,Q8,#SHIFT_32 @ dual issue

    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[19][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[19][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[19][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[19][12-15]*o[0][12-15] R1
    VADD.S32 D10,D8,D9          @g_ai4_ihevc_trans_32[17][0-3]*o[0][0-3]+g_ai4_ihevc_trans_32[17][4-7]*o[0][4-7]+g_ai4_ihevc_trans_32[17][8-11]*o[0][8-11]+g_ai4_ihevc_trans_32[17][12-15]*o[0][12-15]
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[21][0-15]
    @write to memory this should go to 9 11 13 15
    VST1.S16 D16[0],[R2],R7
    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[21][0-3]*o[0][0-3] R1
    VST1.S16 D16[1],[R2],R7
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[21][4-7]*o[0][4-7] R1
    VST1.S16 D16[2],[R2],R7
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[21][8-11]*o[0][8-11] R1
    VST1.S16 D16[3],[R2],R7
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[21][12-15]*o[0][12-15] R1


    VLD1.S32 {Q0,Q1},[R11]!
    VADD.S32 D11,D14,D15        @ dual issue
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[23][0-15]
    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[23][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[23][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[23][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[23][12-15]*o[0][12-15] R1
    VADD.S32 D12,D8,D9          @ dual issued
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[25][0-15]

    @Q5 Q6
    VPADD.S32 D16,D10,D11
    VADD.S32 D13,D14,D15

    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[25][0-3]*o[0][0-3] R1
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[25][4-7]*o[0][4-7] R1
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[25][8-11]*o[0][8-11] R1
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[25][12-15]*o[0][12-15] R1
    VPADD.S32 D17,D12,D13

    VLD1.S32 {Q0,Q1},[R11]!
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[27][0-15]
    VRSHRN.S32 D16,Q8,#SHIFT_32

    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[27][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[27][4-7]*o[0][4-7] R1
    VLD1.S32 {Q0,Q1},[R11]!
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[27][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[27][12-15]*o[0][12-15] R1
    VADD.S32 D10,D8,D9          @g_ai4_ihevc_trans_32[25][0-3]*o[0][0-3]+g_ai4_ihevc_trans_32[25][4-7]*o[0][4-7]+g_ai4_ihevc_trans_32[25][8-11]*o[0][8-11]+g_ai4_ihevc_trans_32[25][12-15]*o[0][12-15]
    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[29][0-15]
    @write to memory this should go to 17 19 21 23
    VST1.S16 D16[0],[R2],R7
    VMUL.S32 Q4,Q0,Q12          @g_ai4_ihevc_trans_32[29][0-3]*o[0][0-3] R1
    VST1.S16 D16[1],[R2],R7
    VMLA.S32 Q4,Q1,Q13          @g_ai4_ihevc_trans_32[29][4-7]*o[0][4-7] R1
    VST1.S16 D16[2],[R2],R7
    VMLA.S32 Q4,Q2,Q14          @g_ai4_ihevc_trans_32[29][8-11]*o[0][8-11] R1
    VST1.S16 D16[3],[R2],R7
    VMLA.S32 Q4,Q3,Q15          @g_ai4_ihevc_trans_32[29][12-15]*o[0][12-15] R1

    VADD.S32 D11,D14,D15        @ dual issue
    VLD1.S32 {Q0,Q1},[R11]!

    VLD1.S32 {Q2,Q3},[R11],R12  @g_ai4_ihevc_trans_32[31][0-15]
    VMUL.S32 Q7,Q0,Q12          @g_ai4_ihevc_trans_32[31][0-3]*o[0][0-3] R1
    VMLA.S32 Q7,Q1,Q13          @g_ai4_ihevc_trans_32[31][4-7]*o[0][4-7] R1
    VMLA.S32 Q7,Q2,Q14          @g_ai4_ihevc_trans_32[31][8-11]*o[0][8-11] R1
    VMLA.S32 Q7,Q3,Q15          @g_ai4_ihevc_trans_32[31][12-15]*o[0][12-15] R1


    VADD.S32 D12,D8,D9
    @Q5 Q6
    VPADD.S32 D16,D10,D11

    VADD.S32 D13,D14,D15


    VPADD.S32 D17,D12,D13

    VRSHRN.S32 D16,Q8,#SHIFT_32

    @write to memory this should go to 25 27 29 31
    VST1.S16 D16[0],[R2],R7
    VST1.S16 D16[1],[R2],R7
    VST1.S16 D16[2],[R2],R7
    VST1.S16 D16[3],[R2],R7

    SUB R2,R2,R7,LSL #4
    LSR R7,R7,#1
    SUB R2,R2,R7

    ADD R2,R2,#2                @MOVE TO NEXT to next COLUMN

    ADD R8,R8,#1                @increment loop cntr by 2 since we process loop as 2 cols
    CMP R8,#32                  @check loop cntr
    BNE CORE_LOOP_32X32_VERT    @jump acc

    MOV R0,R3

    ADD SP,SP,#32
    vpop {d8 - d15}
    LDMFD       sp!,{r4-r12,PC}     @stack store values of the arguments


    .section    .note.GNU-stack,"",%progbits
