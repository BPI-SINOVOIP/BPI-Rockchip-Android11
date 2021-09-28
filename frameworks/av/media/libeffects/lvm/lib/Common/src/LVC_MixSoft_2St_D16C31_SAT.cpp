/*
 * Copyright (C) 2004-2010 NXP Software
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**********************************************************************************
   INCLUDE FILES
***********************************************************************************/

#include "LVC_Mixer_Private.h"
#include "VectorArithmetic.h"

/**********************************************************************************
   FUNCTION LVC_MixSoft_2St_D16C31_SAT.c
***********************************************************************************/
void LVC_MixSoft_2St_D16C31_SAT(LVMixer3_2St_FLOAT_st *ptrInstance,
                                const LVM_FLOAT       *src1,
                                const LVM_FLOAT       *src2,
                                      LVM_FLOAT       *dst,
                                      LVM_INT16       n)
{
    Mix_Private_FLOAT_st  *pInstance1 = \
                             (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[0].PrivateParams);
    Mix_Private_FLOAT_st  *pInstance2 = \
                             (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[1].PrivateParams);

    if(n <= 0)    return;

    /******************************************************************************
       SOFT MIXING
    *******************************************************************************/
    if ((pInstance1->Current == pInstance1->Target) && (pInstance1->Current == 0)){
        LVC_MixSoft_1St_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[1]),
                                    src2, dst, n);
    }
    else if ((pInstance2->Current == pInstance2->Target) && (pInstance2->Current == 0)){
        LVC_MixSoft_1St_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[0]),
                                    src1, dst, n);
    }
    else if ((pInstance1->Current != pInstance1->Target) || \
                                    (pInstance2->Current != pInstance2->Target))
    {
        LVC_MixSoft_1St_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[0]),
                                   src1, dst, n);
        LVC_MixInSoft_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[1]),
                                  src2, dst, n);
    }
    else{
        /******************************************************************************
           HARD MIXING
        *******************************************************************************/
        LVC_Core_MixHard_2St_D16C31_SAT( &ptrInstance->MixerStream[0],
                                         &ptrInstance->MixerStream[1],
                                         src1, src2, dst, n);
    }
}

#ifdef SUPPORT_MC
/*
 * FUNCTION:       LVC_MixSoft_2Mc_D16C31_SAT
 *
 * DESCRIPTION:
 *  2 stream Mixer function with support for processing multichannel input
 *
 * PARAMETERS:
 *  ptrInstance    Instance pointer
 *  src1           First multichannel source
 *  src2           Second multichannel source
 *  dst            Destination
 *  NrFrames       Number of frames
 *  NrChannels     Number of channels
 *
 * RETURNS:
 *  void
 *
 */
void LVC_MixSoft_2Mc_D16C31_SAT(LVMixer3_2St_FLOAT_st *ptrInstance,
                                const LVM_FLOAT       *src1,
                                const LVM_FLOAT       *src2,
                                      LVM_FLOAT       *dst,
                                      LVM_INT16       NrFrames,
                                      LVM_INT16       NrChannels)
{
    Mix_Private_FLOAT_st  *pInstance1 = \
                             (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[0].PrivateParams);
    Mix_Private_FLOAT_st  *pInstance2 = \
                             (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[1].PrivateParams);

    if (NrFrames <= 0)    return;

    /******************************************************************************
       SOFT MIXING
    *******************************************************************************/
    if ((pInstance1->Current == pInstance1->Target) && (pInstance1->Current == 0)) {
        LVC_MixSoft_Mc_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[1]),
                                    src2, dst, NrFrames, NrChannels);
    }
    else if ((pInstance2->Current == pInstance2->Target) && (pInstance2->Current == 0)) {
        LVC_MixSoft_Mc_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[0]),
                                    src1, dst, NrFrames, NrChannels);
    }
    else if ((pInstance1->Current != pInstance1->Target) || \
                                    (pInstance2->Current != pInstance2->Target))
    {
        LVC_MixSoft_Mc_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[0]),
                                   src1, dst, NrFrames, NrChannels);
        LVC_MixInSoft_Mc_D16C31_SAT((LVMixer3_1St_FLOAT_st *)(&ptrInstance->MixerStream[1]),
                                   src2, dst, NrFrames, NrChannels);
    }
    else{
        /******************************************************************************
           HARD MIXING
        *******************************************************************************/
        LVC_Core_MixHard_2St_D16C31_SAT(&ptrInstance->MixerStream[0],
                                        &ptrInstance->MixerStream[1],
                                        src1, src2, dst, NrFrames * NrChannels);
    }
}
#endif

/**********************************************************************************/
