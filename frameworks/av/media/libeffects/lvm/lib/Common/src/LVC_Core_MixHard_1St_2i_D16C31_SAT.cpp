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
#include "LVM_Macros.h"
#include "ScalarArithmetic.h"

/**********************************************************************************
   FUNCTION LVC_Core_MixHard_1St_2i_D16C31_SAT
***********************************************************************************/
void LVC_Core_MixHard_1St_2i_D16C31_SAT( LVMixer3_FLOAT_st        *ptrInstance1,
                                         LVMixer3_FLOAT_st        *ptrInstance2,
                                         const LVM_FLOAT    *src,
                                         LVM_FLOAT          *dst,
                                         LVM_INT16          n)
{
    LVM_FLOAT  Temp;
    LVM_INT16 ii;
    Mix_Private_FLOAT_st  *pInstance1 = (Mix_Private_FLOAT_st *)(ptrInstance1->PrivateParams);
    Mix_Private_FLOAT_st  *pInstance2 = (Mix_Private_FLOAT_st *)(ptrInstance2->PrivateParams);
    for (ii = n; ii != 0; ii--)
    {
        Temp = ((LVM_FLOAT)*(src++) * (LVM_FLOAT)pInstance1->Current);
        if (Temp > 1.0f)
            *dst++ = 1.0f;
        else if (Temp < -1.0f)
            *dst++ = -1.0f;
        else
            *dst++ = (LVM_FLOAT)Temp;

        Temp = ((LVM_FLOAT)*(src++) * (LVM_FLOAT)pInstance2->Current);
        if (Temp > 1.0f)
            *dst++ = 1.0f;
        else if (Temp < -1.0f)
            *dst++ = -1.0f;
        else
            *dst++ = (LVM_FLOAT)Temp;
    }

}
#ifdef SUPPORT_MC
void LVC_Core_MixHard_1St_MC_float_SAT (Mix_Private_FLOAT_st **ptrInstance,
                                         const LVM_FLOAT      *src,
                                         LVM_FLOAT            *dst,
                                         LVM_INT16            NrFrames,
                                         LVM_INT16            NrChannels)
{
    LVM_FLOAT  Temp;
    LVM_INT16 ii, jj;
    for (ii = NrFrames; ii != 0; ii--)
    {
        for (jj = 0; jj < NrChannels; jj++)
        {
            Mix_Private_FLOAT_st  *pInstance1 = (Mix_Private_FLOAT_st *)(ptrInstance[jj]);
            Temp = ((LVM_FLOAT)*(src++) * (LVM_FLOAT)pInstance1->Current);
            if (Temp > 1.0f)
                *dst++ = 1.0f;
            else if (Temp < -1.0f)
                *dst++ = -1.0f;
            else
                *dst++ = (LVM_FLOAT)Temp;
        }
    }
}
#endif
/**********************************************************************************/
