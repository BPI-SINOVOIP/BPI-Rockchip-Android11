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
#include "ScalarArithmetic.h"

/**********************************************************************************
   DEFINITIONS
***********************************************************************************/

#define TRUE          1
#define FALSE         0

/**********************************************************************************
   FUNCTION LVMixer3_MIXSOFT_1ST_D16C31_SAT
***********************************************************************************/
void LVC_MixSoft_1St_D16C31_SAT( LVMixer3_1St_FLOAT_st *ptrInstance,
                                  const LVM_FLOAT             *src,
                                        LVM_FLOAT             *dst,
                                        LVM_INT16             n)
{
    char        HardMixing = TRUE;
    LVM_FLOAT   TargetGain;
    Mix_Private_FLOAT_st  *pInstance = \
                          (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[0].PrivateParams);

    if(n <= 0)    return;

    /******************************************************************************
       SOFT MIXING
    *******************************************************************************/
    if (pInstance->Current != pInstance->Target)
    {
        if(pInstance->Delta == 1.0f){
            pInstance->Current = pInstance->Target;
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(&(ptrInstance->MixerStream[0]), TargetGain);
        }else if (Abs_Float(pInstance->Current - pInstance->Target) < pInstance->Delta){
            pInstance->Current = pInstance->Target; /* Difference is not significant anymore. \
                                                       Make them equal. */
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(&(ptrInstance->MixerStream[0]), TargetGain);
        }else{
            /* Soft mixing has to be applied */
            HardMixing = FALSE;
            LVC_Core_MixSoft_1St_D16C31_WRA(&(ptrInstance->MixerStream[0]), src, dst, n);
        }
    }

    /******************************************************************************
       HARD MIXING
    *******************************************************************************/

    if (HardMixing){
        if (pInstance->Target == 0)
            LoadConst_Float(0.0, dst, n);
        else {
            if ((pInstance->Target) != 1.0f)
                Mult3s_Float(src, (pInstance->Target), dst, n);
            else if(src != dst)
                Copy_Float(src, dst, n);
        }

    }

    /******************************************************************************
       CALL BACK
    *******************************************************************************/

    if (ptrInstance->MixerStream[0].CallbackSet){
        if (Abs_Float(pInstance->Current - pInstance->Target) < pInstance->Delta){
            pInstance->Current = pInstance->Target; /* Difference is not significant anymore. \
                                                       Make them equal. */
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(ptrInstance->MixerStream, TargetGain);
            ptrInstance->MixerStream[0].CallbackSet = FALSE;
            if (ptrInstance->MixerStream[0].pCallBack != 0){
                (*ptrInstance->MixerStream[0].pCallBack) ( \
                                                ptrInstance->MixerStream[0].pCallbackHandle,
                                                ptrInstance->MixerStream[0].pGeneralPurpose,
                                                ptrInstance->MixerStream[0].CallbackParam );
            }
        }
    }
}
#ifdef SUPPORT_MC
/*
 * FUNCTION:       LVC_MixSoft_Mc_D16C31_SAT
 *
 * DESCRIPTION:
 *  Mixer function with support for processing multichannel input
 *
 * PARAMETERS:
 *  ptrInstance    Instance pointer
 *  src            Source
 *  dst            Destination
 *  NrFrames       Number of Frames
 *  NrChannels     Number of channels
 *
 * RETURNS:
 *  void
 *
 */
void LVC_MixSoft_Mc_D16C31_SAT(LVMixer3_1St_FLOAT_st *ptrInstance,
                                  const LVM_FLOAT      *src,
                                        LVM_FLOAT      *dst,
                                        LVM_INT16      NrFrames,
                                        LVM_INT16      NrChannels)
{
    char        HardMixing = TRUE;
    LVM_FLOAT   TargetGain;
    Mix_Private_FLOAT_st  *pInstance = \
                          (Mix_Private_FLOAT_st *)(ptrInstance->MixerStream[0].PrivateParams);

    if (NrFrames <= 0)    return;

    /******************************************************************************
       SOFT MIXING
    *******************************************************************************/
    if (pInstance->Current != pInstance->Target)
    {
        if (pInstance->Delta == 1.0f) {
            pInstance->Current = pInstance->Target;
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(&(ptrInstance->MixerStream[0]), TargetGain);
        }else if (Abs_Float(pInstance->Current - pInstance->Target) < pInstance->Delta) {
            pInstance->Current = pInstance->Target; /* Difference is not significant anymore. \
                                                       Make them equal. */
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(&(ptrInstance->MixerStream[0]), TargetGain);
        }else{
            /* Soft mixing has to be applied */
            HardMixing = FALSE;
            LVC_Core_MixSoft_Mc_D16C31_WRA(&(ptrInstance->MixerStream[0]),
                                           src,
                                           dst,
                                           NrFrames,
                                           NrChannels);
        }
    }

    /******************************************************************************
       HARD MIXING
    *******************************************************************************/

    if (HardMixing) {
        if (pInstance->Target == 0)
            LoadConst_Float(0.0, dst, NrFrames * NrChannels);
        else {
            if ((pInstance->Target) != 1.0f)
                Mult3s_Float(src, (pInstance->Target), dst, NrFrames * NrChannels);
            else if (src != dst)
                Copy_Float(src, dst, NrFrames * NrChannels);
        }

    }

    /******************************************************************************
       CALL BACK
    *******************************************************************************/

    if (ptrInstance->MixerStream[0].CallbackSet) {
        if (Abs_Float(pInstance->Current - pInstance->Target) < pInstance->Delta) {
            pInstance->Current = pInstance->Target; /* Difference is not significant anymore. \
                                                       Make them equal. */
            TargetGain = pInstance->Target;
            LVC_Mixer_SetTarget(ptrInstance->MixerStream, TargetGain);
            ptrInstance->MixerStream[0].CallbackSet = FALSE;
            if (ptrInstance->MixerStream[0].pCallBack != 0) {
                (*ptrInstance->MixerStream[0].pCallBack) (\
                                                ptrInstance->MixerStream[0].pCallbackHandle,
                                                ptrInstance->MixerStream[0].pGeneralPurpose,
                                                ptrInstance->MixerStream[0].CallbackParam);
            }
        }
    }
}

#endif

/**********************************************************************************/
