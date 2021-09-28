/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/**
 * \addtogroup SPI_Power_Management
 *
 * @{ */

#ifndef _PHNXPESE_SPM_H
#define _PHNXPESE_SPM_H

#include <phEseStatus.h>
#include <phNxpEseFeatures.h>
/*! SPI Power Manager (SPM) possible error codes */
typedef enum spm_power {
  SPM_POWER_ENABLE = 0,
  SPM_POWER_DISABLE,     /*!< SPM power disable */
  SPM_POWER_RESET,       /*!< SPM Reset pwer */
  SPM_POWER_PRIO_ENABLE, /*!< SPM prio mode enable */
  SPM_POWER_PRIO_DISABLE /*!< SPM prio mode disable */
} spm_power_t;

typedef enum spm_state {
  SPM_STATE_INVALID = 0x0000,     /*!< Nfc i2c driver misbehaving */
  SPM_STATE_IDLE = 0x0100,        /*!< ESE is free to use */
  SPM_STATE_WIRED = 0x0200,       /*!< p61 is being accessed by DWP (NFCC)*/
  SPM_STATE_SPI = 0x0400,         /*!< ESE is being accessed by SPI */
  SPM_STATE_DWNLD = 0x0800,       /*!< NFCC fw download is in progress */
  SPM_STATE_SPI_PRIO = 0x1000,    /*!< Start of p61 access by SPI on priority*/
  SPM_STATE_SPI_PRIO_END = 0x2000 /*!< End of p61 access by SPI on priority*/
#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
  ,
  SPM_STATE_JCOP_DWNLD = 0x8000 /*!< P73 state JCOP Download*/
#endif
} spm_state_t;

ESESTATUS phNxpEse_SPM_Init(void* pDevHandle);

ESESTATUS phNxpEse_SPM_DeInit(void);

ESESTATUS phNxpEse_SPM_ConfigPwr(spm_power_t arg);

ESESTATUS phNxpEse_SPM_EnablePwr(void);

ESESTATUS phNxpEse_SPM_DisablePwr(void);

ESESTATUS phNxpEse_SPM_GetState(spm_state_t* current_state);

#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
ESESTATUS phNxpEse_SPM_SetState(long arg);
#endif

ESESTATUS phNxpEse_SPM_RelAccess(void);

ESESTATUS phNxpEse_SPM_SetPwrScheme(long arg);

ESESTATUS phNxpEse_SPM_DisablePwrControl(unsigned long arg);
#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
ESESTATUS phNxpEse_SPM_SetJcopDwnldState(long arg);
#endif
#endif /*  _PHNXPESE_SPM_H    */
/** @} */
