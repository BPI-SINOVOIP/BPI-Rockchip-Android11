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
#ifndef _PHNXPSPILIB_H_
#define _PHNXPSPILIB_H_

#include <phNxpEseFeatures.h>
#include <phNxpEse_Api.h>

/* Macro to enable SPM Module */
#define SPM_INTEGRATED
//#undef SPM_INTEGRATED
#ifdef SPM_INTEGRATED
#include "../spm/phNxpEse_Spm.h"
#endif

/********************* Definitions and structures *****************************/

typedef enum {
  ESE_STATUS_CLOSE = 0x00,
  ESE_STATUS_BUSY,
  ESE_STATUS_RECOVERY,
  ESE_STATUS_IDLE,
  ESE_STATUS_OPEN,
} phNxpEse_LibStatus;

typedef enum {
  PN67T_POWER_SCHEME = 0x01,
  PN80T_LEGACY_SCHEME,
  PN80T_EXT_PMU_SCHEME,
} phNxpEse_PowerScheme;

/* Macros definition */
#define MAX_DATA_LEN 260
#define SECOND_TO_MILLISECOND(X) X * 1000
#define CONVERT_TO_PERCENTAGE(X, Y) X* Y / 100
#define ADDITIONAL_SECURE_TIME_PERCENTAGE 5
#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
#define ESE_JCOP_OS_DWNLD_RETRY_CNT \
  10 /* Maximum retry count for ESE JCOP OS Dwonload*/
#endif
#ifdef NXP_NFCC_SPI_FW_DOWNLOAD_SYNC
#define ESE_FW_DWNLD_RETRY_CNT 10 /* Maximum retry count for FW Dwonload*/
#endif

/* Secure timer values F1, F2, F3 */
typedef struct phNxpEse_SecureTimer {
  unsigned int secureTimer1;
  unsigned int secureTimer2;
  unsigned int secureTimer3;
} phNxpEse_SecureTimer_t;

/* JCOP download states */
typedef enum jcop_dwnld_state {
#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
  JCP_DWNLD_IDLE = SPM_STATE_JCOP_DWNLD, /* jcop dwnld is not ongoing*/
#endif
  JCP_DWNLD_INIT = 0x8010,         /* jcop dwonload init state*/
  JCP_DWNLD_START = 0x8020,        /* download started */
  JCP_SPI_DWNLD_COMPLETE = 0x8040, /* jcop download complete in spi interface*/
  JCP_DWP_DWNLD_COMPLETE = 0x8080, /* jcop download complete */
} phNxpEse_JcopDwnldState;

/* SPI Control structure */
typedef struct phNxpEse_Context {
  phNxpEse_LibStatus EseLibStatus; /* Indicate if Ese Lib is open or closed */
  void* pDevHandle;

  uint8_t p_read_buff[MAX_DATA_LEN];
  uint16_t cmd_len;
  uint8_t p_cmd_data[MAX_DATA_LEN];

  bool spm_power_state;
  uint8_t pwr_scheme;
  phNxpEse_initParams initParams;
  phNxpEse_SecureTimer_t secureTimerParams;
} phNxpEse_Context_t;

ESESTATUS phNxpEse_WriteFrame(uint32_t data_len, const uint8_t* p_data);
ESESTATUS phNxpEse_read(uint32_t* data_len, uint8_t** pp_data);

#endif /* _PHNXPSPILIB_H_ */
