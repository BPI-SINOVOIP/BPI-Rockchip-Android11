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

/****************************************************************************************/
/*                                                                                      */
/*  Includes                                                                            */
/*                                                                                      */
/****************************************************************************************/

#include "LVEQNB.h"
#include "LVEQNB_Private.h"
#include "VectorArithmetic.h"
#include "BIQUAD.h"

#include <log/log.h>

/****************************************************************************************/
/*                                                                                      */
/*  Defines                                                                             */
/*                                                                                      */
/****************************************************************************************/

#define SHIFT       13

/****************************************************************************************/
/*                                                                                      */
/* FUNCTION:                LVEQNB_Process                                              */
/*                                                                                      */
/* DESCRIPTION:                                                                         */
/*  Process function for the N-Band Equaliser module.                                   */
/*                                                                                      */
/* PARAMETERS:                                                                          */
/*  hInstance               Instance handle                                             */
/*  pInData                 Pointer to the input data                                   */
/*  pOutData                Pointer to the output data                                  */
/*  NumSamples              Number of samples in the input buffer                       */
/*                                                                                      */
/* RETURNS:                                                                             */
/*  LVEQNB_SUCCESS          Succeeded                                                   */
/*  LVEQNB_NULLADDRESS      When hInstance, pInData or pOutData are NULL                */
/*  LVEQNB_ALIGNMENTERROR   When pInData or pOutData are not 32-bit aligned             */
/*  LVEQNB_TOOMANYSAMPLES   NumSamples was larger than the maximum block size           */
/*                                                                                      */
/* NOTES:                                                                               */
/*                                                                                      */
/****************************************************************************************/
LVEQNB_ReturnStatus_en LVEQNB_Process(LVEQNB_Handle_t       hInstance,
                                      const LVM_FLOAT       *pInData,
                                      LVM_FLOAT             *pOutData,
                                      const LVM_UINT16      NrFrames)
{                                     // updated to use samples = frames * channels.
    LVEQNB_Instance_t   *pInstance = (LVEQNB_Instance_t  *)hInstance;

#ifdef SUPPORT_MC
    // Mono passed in as stereo
    const LVM_INT32 NrChannels = pInstance->Params.NrChannels == 1
        ? 2 : pInstance->Params.NrChannels;
#else
    const LVM_INT32 NrChannels = 2; // FCC_2
#endif
    const LVM_INT32 NrSamples = NrChannels * NrFrames;

     /* Check for NULL pointers */
    if((hInstance == LVM_NULL) || (pInData == LVM_NULL) || (pOutData == LVM_NULL))
    {
        return LVEQNB_NULLADDRESS;
    }

    /* Check if the input and output data buffers are 32-bit aligned */
    if ((((uintptr_t)pInData % 4) != 0) || (((uintptr_t)pOutData % 4) != 0))
    {
        return LVEQNB_ALIGNMENTERROR;
    }

    LVM_FLOAT * const pScratch = (LVM_FLOAT *)pInstance->pFastTemporary;

    /*
    * Check the number of frames is not too large
    */
    if (NrFrames > pInstance->Capabilities.MaxBlockSize)
    {
        return LVEQNB_TOOMANYSAMPLES;
    }

    if (pInstance->Params.OperatingMode == LVEQNB_ON)
    {
        /*
         * Copy input data in to scratch buffer
         */
        Copy_Float(pInData,     /* Source */
                   pScratch,    /* Destination */
                   (LVM_INT16)NrSamples);

        /*
         * For each section execte the filter unless the gain is 0dB
         */
        if (pInstance->NBands != 0)
        {
            for (LVM_UINT16 i = 0; i < pInstance->NBands; i++)
            {
                /*
                 * Check if band is non-zero dB gain
                 */
                if (pInstance->pBandDefinitions[i].Gain != 0)
                {
                    /*
                     * Get the address of the biquad instance
                     */
                    Biquad_FLOAT_Instance_t *pBiquad = &pInstance->pEQNB_FilterState_Float[i];

                    /*
                     * Select single or double precision as required
                     */
                    switch (pInstance->pBiquadType[i])
                    {
                        case LVEQNB_SinglePrecision_Float:
                        {
#ifdef SUPPORT_MC
                            PK_Mc_D32F32C14G11_TRC_WRA_01(pBiquad,
                                                          pScratch,
                                                          pScratch,
                                                          (LVM_INT16)NrFrames,
                                                          (LVM_INT16)NrChannels);
#else
                            PK_2I_D32F32C14G11_TRC_WRA_01(pBiquad,
                                                          pScratch,
                                                          pScratch,
                                                          (LVM_INT16)NrFrames);
#endif
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }

        if(pInstance->bInOperatingModeTransition == LVM_TRUE){
#ifdef SUPPORT_MC
            LVC_MixSoft_2Mc_D16C31_SAT(&pInstance->BypassMixer,
                                       pScratch,
                                       pInData,
                                       pScratch,
                                       (LVM_INT16)NrFrames,
                                       (LVM_INT16)NrChannels);
#else
            LVC_MixSoft_2St_D16C31_SAT(&pInstance->BypassMixer,
                                       pScratch,
                                       pInData,
                                       pScratch,
                                       (LVM_INT16)NrSamples);
#endif
            // duplicate with else clause(s)
            Copy_Float(pScratch,                         /* Source */
                       pOutData,                         /* Destination */
                       (LVM_INT16)NrSamples);            /* All channel samples */
        }
        else{
            Copy_Float(pScratch,              /* Source */
                       pOutData,              /* Destination */
                       (LVM_INT16)NrSamples); /* All channel samples */
        }
    }
    else
    {
        /*
         * Mode is OFF so copy the data if necessary
         */
        if (pInData != pOutData)
        {
            Copy_Float(pInData,                          /* Source */
                       pOutData,                         /* Destination */
                       (LVM_INT16)NrSamples);            /* All channel samples */
        }
    }
    return LVEQNB_SUCCESS;

}
