/******************************************************************************
 *
 *  Copyright (C) 2018 NXP Semiconductors
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
#ifndef ANDROID_HARDWARE_HAL_NXPESE_V1_0_H
#define ANDROID_HARDWARE_HAL_NXPESE_V1_0_H

#define ESE_NXPNFC_HARDWARE_MODULE_ID "ese_nxp.pn54x"
#define MAX_IOCTL_TRANSCEIVE_CMD_LEN 256
#define MAX_IOCTL_TRANSCEIVE_RESP_LEN 256
#define MAX_ATR_INFO_LEN 128
enum {
  HAL_ESE_IOCTL_P61_IDLE_MODE = 0,
  HAL_ESE_IOCTL_P61_WIRED_MODE,
  HAL_ESE_IOCTL_P61_PWR_MODE,
  HAL_ESE_IOCTL_P61_DISABLE_MODE,
  HAL_ESE_IOCTL_P61_ENABLE_MODE,
  HAL_ESE_IOCTL_SET_BOOT_MODE,
  HAL_ESE_IOCTL_GET_CONFIG_INFO,
  HAL_ESE_IOCTL_CHECK_FLASH_REQ,
  HAL_ESE_IOCTL_FW_DWNLD,
  HAL_ESE_IOCTL_FW_MW_VER_CHECK,
  HAL_ESE_IOCTL_DISABLE_HAL_LOG,
  HAL_ESE_IOCTL_NXP_TRANSCEIVE,
  HAL_ESE_IOCTL_P61_GET_ACCESS,
  HAL_ESE_IOCTL_P61_REL_ACCESS,
  HAL_ESE_IOCTL_ESE_CHIP_RST,
  HAL_ESE_IOCTL_REL_SVDD_WAIT,
  HAL_ESE_IOCTL_SET_JCP_DWNLD_ENABLE,
  HAL_ESE_IOCTL_SET_JCP_DWNLD_DISABLE,
  HAL_ESE_IOCTL_SET_ESE_SERVICE_PID,
  HAL_ESE_IOCTL_REL_DWP_WAIT,
  HAL_ESE_IOCTL_GET_FEATURE_LIST,
  HAL_ESE_IOCTL_RF_STATUS_UPDATE
};

enum {
  HAL_NFC_IOCTL_P61_IDLE_MODE = 0,
  HAL_NFC_IOCTL_P61_WIRED_MODE,
  HAL_NFC_IOCTL_P61_PWR_MODE,
  HAL_NFC_IOCTL_P61_DISABLE_MODE,
  HAL_NFC_IOCTL_P61_ENABLE_MODE,
  HAL_NFC_IOCTL_SET_BOOT_MODE,
  HAL_NFC_IOCTL_GET_CONFIG_INFO,
  HAL_NFC_IOCTL_CHECK_FLASH_REQ,
  HAL_NFC_IOCTL_FW_DWNLD,
  HAL_NFC_IOCTL_FW_MW_VER_CHECK,
  HAL_NFC_IOCTL_DISABLE_HAL_LOG,
  HAL_NFC_IOCTL_NCI_TRANSCEIVE,
  HAL_NFC_IOCTL_P61_GET_ACCESS,
  HAL_NFC_IOCTL_P61_REL_ACCESS,
  HAL_NFC_IOCTL_ESE_CHIP_RST,
  HAL_NFC_IOCTL_REL_SVDD_WAIT,
  HAL_NFC_IOCTL_SET_JCP_DWNLD_ENABLE,
  HAL_NFC_IOCTL_SET_JCP_DWNLD_DISABLE,
  HAL_NFC_IOCTL_SET_NFC_SERVICE_PID,
  HAL_NFC_IOCTL_REL_DWP_WAIT,
  HAL_NFC_IOCTL_GET_FEATURE_LIST,
  HAL_NFC_IOCTL_SPI_DWP_SYNC, /*21*/
  HAL_NFC_IOCTL_RF_STATUS_UPDATE,
  HAL_NFC_SET_SPM_PWR,
  HAL_NFC_SET_POWER_SCHEME,
  HAL_NFC_GET_SPM_STATUS,
  HAL_NFC_GET_ESE_ACCESS,
  HAL_NFC_SET_DWNLD_STATUS,
  HAL_NFC_INHIBIT_PWR_CNTRL
};
/*
 * Data structures provided below are used of Hal Ioctl calls
 */
/*
 * ese_nxp_ExtnCmd_t shall contain data for commands used for transceive command
 * in ioctl
 */
typedef struct {
  uint16_t cmd_len;
  uint8_t p_cmd[MAX_IOCTL_TRANSCEIVE_CMD_LEN];
} ese_nxp_ExtnCmd_t;

/*
 * ese_nxp_ExtnRsp_t shall contain response for command sent in transceive
 * command
 */
typedef struct {
  uint16_t rsp_len;
  uint8_t p_rsp[MAX_IOCTL_TRANSCEIVE_RESP_LEN];
} ese_nxp_ExtnRsp_t;
/*
 * InputData_t :ioctl has multiple subcommands
 * Each command has corresponding input data which needs to be populated in this
 */
typedef union {
  uint16_t bootMode;
  uint8_t halType;
  ese_nxp_ExtnCmd_t nxpCmd;
  uint32_t timeoutMilliSec;
  long eseServicePid;
} eseInputData_t;
/*
 * ese_nxp_ExtnInputData_t :Apart from InputData_t, there are context data
 * which is required during callback from stub to proxy.
 * To avoid additional copy of data while propagating from libese to Adaptation
 * and Esestub to nxphal, common structure is used. As a sideeffect, context
 * data is exposed to libese (Not encapsulated).
 */
typedef struct {
  /*context to be used/updated only by users of proxy & stub of Ese.hal
   * i.e, EseAdaptation & hardware/interface/Ese.
   */
  void* context;
  eseInputData_t data;
  uint8_t data_source;
  long level;
} ese_nxp_ExtnInputData_t;

/*
 * outputData_t :ioctl has multiple commands/responses
 * This contains the output types for each ioctl.
 */
typedef union {
  uint32_t status;
  ese_nxp_ExtnRsp_t nxpRsp;
  uint8_t nxpNciAtrInfo[MAX_ATR_INFO_LEN];
  uint32_t p61CurrentState;
  uint16_t fwUpdateInf;
  uint16_t fwDwnldStatus;
  uint16_t fwMwVerStatus;
  uint8_t chipType;
} eseOutputData_t;

/*
 * ese_nxp_ExtnOutputData_t :Apart from outputData_t, there are other
 * information which is required during callback from stub to proxy. For ex
 * (context, result of the operation , type of ioctl which was completed). To
 * avoid additional copy of data while propagating from libese to Adaptation and
 * Esestub to nxphal, common structure is used. As a sideeffect, these data is
 * exposed(Not encapsulated).
 */
typedef struct {
  /*ioctlType, result & context to be used/updated only by users of
   * proxy & stub of Ese.hal.
   * i.e, EseAdaptation & hardware/interface/Ese
   * These fields shall not be used by libese or halimplementation*/
  uint64_t ioctlType;
  uint32_t result;
  void* context;
  eseOutputData_t data;
} ese_nxp_ExtnOutputData_t;

/*
 * ese_nxp_IoctlInOutData_t :data structure for input & output
 * to be sent for ioctl command. input is populated by client/proxy side
 * output is provided from server/stub to client/proxy
 */
typedef struct {
  ese_nxp_ExtnInputData_t inp;
  ese_nxp_ExtnOutputData_t out;
} ese_nxp_IoctlInOutData_t;

#endif  // ANDROID_HARDWARE_HAL_NXPESE_V1_0_H
