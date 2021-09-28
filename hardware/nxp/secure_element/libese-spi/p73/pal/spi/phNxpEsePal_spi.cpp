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

/*
 * DAL spi port implementation for linux
 *
 * Project: Trusted ESE Linux
 *
 */
#define LOG_TAG "NxpEseHal"
#include <log/log.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ese_config.h>
#include <hardware/nfc.h>
#include <phEseStatus.h>
#include <phNxpEsePal.h>
#include <phNxpEsePal_spi.h>
#include <string.h>
#include "NfcAdaptation.h"
#include "hal_nxpese.h"
#include "phNxpEse_Api.h"

#define MAX_RETRY_CNT 10
#define HAL_NFC_SPI_DWP_SYNC 21
#define RF_ON 1

extern int omapi_status;
extern bool ese_debug_enabled;

static int rf_status;
unsigned long int configNum1, configNum2;
// Default max retry count for SPI CLT write blocked in secs
static const uint8_t DEFAULT_MAX_SPI_WRITE_RETRY_COUNT_RF_ON = 10;
static const uint8_t MAX_SPI_WRITE_RETRY_COUNT_HW_ERR = 3;
/*******************************************************************************
**
** Function         phPalEse_spi_close
**
** Description      Closes PN547 device
**
** Parameters       pDevHandle - device handle
**
** Returns          None
**
*******************************************************************************/
void phPalEse_spi_close(void* pDevHandle) {
  ese_nxp_IoctlInOutData_t inpOutData;
  static uint8_t cmd_omapi_concurrent[] = {0x2F, 0x01, 0x01, 0x00};
  int retval;
  ALOGD_IF(ese_debug_enabled, "halimpl close enter.");

  NfcAdaptation& pNfcAdapt = NfcAdaptation::GetInstance();
  pNfcAdapt.Initialize();
  // nxpesehal_ctrl.p_ese_stack_cback = p_cback;
  // nxpesehal_ctrl.p_ese_stack_data_cback = p_data_cback;
  memset(&inpOutData, 0x00, sizeof(ese_nxp_IoctlInOutData_t));
  inpOutData.inp.data.nxpCmd.cmd_len = sizeof(cmd_omapi_concurrent);
  inpOutData.inp.data_source = 1;
  memcpy(inpOutData.inp.data.nxpCmd.p_cmd, cmd_omapi_concurrent,
         sizeof(cmd_omapi_concurrent));
  retval = pNfcAdapt.HalIoctl(HAL_NFC_SPI_DWP_SYNC, &inpOutData);
  ALOGD_IF(ese_debug_enabled, "_spi_close() status %x", retval);

  if (NULL != pDevHandle) {
    close((intptr_t)pDevHandle);
  }
  ALOGD_IF(ese_debug_enabled, "halimpl close exit.");
  return;
}
ESESTATUS phNxpEse_spiIoctl(uint64_t ioctlType, void* p_data) {
  if (!p_data) {
    ALOGD_IF(ese_debug_enabled, "%s:p_data is null ioctltyp: %ld", __FUNCTION__,
             (long)ioctlType);
    return ESESTATUS_FAILED;
  }
  ese_nxp_IoctlInOutData_t* inpOutData = (ese_nxp_IoctlInOutData_t*)p_data;
  rf_status = inpOutData->inp.data.nxpCmd.p_cmd[0];
  if (rf_status == 1) {
    ALOGD_IF(ese_debug_enabled,
             "******************RF IS ON*************************************");
  } else {
    ALOGD_IF(
        ese_debug_enabled,
        "******************RF IS OFF*************************************");
  }
  return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phPalEse_spi_open_and_configure
**
** Description      Open and configure pn547 device
**
** Parameters       pConfig     - hardware information
**                  pLinkHandle - device handle
**
** Returns          ESE status:
**                  ESESTATUS_SUCCESS            - open_and_configure operation
*success
**                  ESESTATUS_INVALID_DEVICE     - device open operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_spi_open_and_configure(pphPalEse_Config_t pConfig) {
  int nHandle;
  int retryCnt = 0, nfc_access_retryCnt = 0;
  int retval;
  ese_nxp_IoctlInOutData_t inpOutData;
  NfcAdaptation& pNfcAdapt = NfcAdaptation::GetInstance();
  pNfcAdapt.Initialize();
  static uint8_t cmd_omapi_concurrent[] = {0x2F, 0x01, 0x01, 0x01};

  if (EseConfig::hasKey(NAME_NXP_SOF_WRITE)) {
    configNum1 = EseConfig::getUnsigned(NAME_NXP_SOF_WRITE);
    ALOGD_IF(ese_debug_enabled, "NXP_SOF_WRITE value from config file = %ld",
             configNum1);
  }

  if (EseConfig::hasKey(NAME_NXP_SPI_WRITE_TIMEOUT)) {
    configNum2 = EseConfig::getUnsigned(NAME_NXP_SPI_WRITE_TIMEOUT);
    ALOGD_IF(ese_debug_enabled,
             "NXP_SPI_WRITE_TIMEOUT value from config file = %ld", configNum2);
  }
  ALOGD_IF(ese_debug_enabled, "halimpl open enter.");
  memset(&inpOutData, 0x00, sizeof(ese_nxp_IoctlInOutData_t));
  inpOutData.inp.data.nxpCmd.cmd_len = sizeof(cmd_omapi_concurrent);
  inpOutData.inp.data_source = 1;
  memcpy(inpOutData.inp.data.nxpCmd.p_cmd, cmd_omapi_concurrent,
         sizeof(cmd_omapi_concurrent));

retry_nfc_access:
  omapi_status = ESESTATUS_FAILED;
  retval = pNfcAdapt.HalIoctl(HAL_NFC_SPI_DWP_SYNC, &inpOutData);
  if (omapi_status != 0) {
    ALOGD_IF(ese_debug_enabled, "omapi_status return failed.");
    nfc_access_retryCnt++;
    phPalEse_sleep(2000000);
    if (nfc_access_retryCnt < 5) goto retry_nfc_access;
    return ESESTATUS_FAILED;
  }

  ALOGD_IF(ese_debug_enabled, "Opening port=%s\n", pConfig->pDevName);
/* open port */

retry:
  nHandle = open((char const*)pConfig->pDevName, O_RDWR);
  if (nHandle < 0) {
    ALOGE("%s : failed errno = 0x%x", __FUNCTION__, errno);
    if (errno == -EBUSY || errno == EBUSY) {
      retryCnt++;
      ALOGE("Retry open eSE driver, retry cnt : %d", retryCnt);
      if (retryCnt < MAX_RETRY_CNT) {
        phPalEse_sleep(1000000);
        goto retry;
      }
    }
    ALOGE("_spi_open() Failed: retval %x", nHandle);
    pConfig->pDevHandle = NULL;
    return ESESTATUS_INVALID_DEVICE;
  }
  ALOGD_IF(ese_debug_enabled, "eSE driver opened :: fd = [%d]", nHandle);
  pConfig->pDevHandle = (void*)((intptr_t)nHandle);
  return ESESTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function         phPalEse_spi_read
**
** Description      Reads requested number of bytes from pn547 device into given
*buffer
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToRead   - number of bytes requested to be read
**
** Returns          numRead   - number of successfully read bytes
**                  -1        - read operation failure
**
*******************************************************************************/
int phPalEse_spi_read(void* pDevHandle, uint8_t* pBuffer, int nNbBytesToRead) {
  int ret = -1;
  ALOGD_IF(ese_debug_enabled, "%s Read Requested %d bytes", __FUNCTION__,
           nNbBytesToRead);
  ret = read((intptr_t)pDevHandle, (void*)pBuffer, (nNbBytesToRead));
  ALOGD_IF(ese_debug_enabled, "Read Returned = %d", ret);
  return ret;
}

/*******************************************************************************
**
** Function         phPalEse_spi_write
**
** Description      Writes requested number of bytes from given buffer into
*pn547 device
**
** Parameters       pDevHandle       - valid device handle
**                  pBuffer          - buffer for read data
**                  nNbBytesToWrite  - number of bytes requested to be written
**
** Returns          numWrote   - number of successfully written bytes
**                  -1         - write operation failure
**
*******************************************************************************/
int phPalEse_spi_write(void* pDevHandle, uint8_t* pBuffer,
                       int nNbBytesToWrite) {
  int ret = -1;
  int numWrote = 0;
  unsigned long int retryCount = 0;
  if (NULL == pDevHandle) {
    return -1;
  }

  if (configNum1 == 1) {
    /* Appending SOF for SPI write */
    pBuffer[0] = SEND_PACKET_SOF;
  } else {
    /* Do Nothing */
  }

  unsigned int maxRetryCount = 0, retryDelay = 0;
  while (numWrote < nNbBytesToWrite) {
    // usleep(5000);
    if (rf_status != RF_ON) {
      ret = write((intptr_t)pDevHandle, pBuffer + numWrote,
                  nNbBytesToWrite - numWrote);
    } else {
      ret = -1;
    }
    if (ret > 0) {
      numWrote += ret;
    } else if (ret == 0) {
      ALOGE("_spi_write() EOF");
      return -1;
    } else {
      ALOGE("_spi_write() errno : %x", errno);

      if (rf_status == RF_ON) {
        maxRetryCount = (configNum2 > 0)
                            ? configNum2
                            : DEFAULT_MAX_SPI_WRITE_RETRY_COUNT_RF_ON;
        retryDelay = 1000 * WRITE_WAKE_UP_DELAY;
        ALOGD_IF(ese_debug_enabled, "spi_Write failed as RF is ON.");
      } else {
        maxRetryCount = MAX_SPI_WRITE_RETRY_COUNT_HW_ERR;
        retryDelay = WRITE_WAKE_UP_DELAY;
        ALOGD_IF(ese_debug_enabled, "spi_write failed");
      }

      if (retryCount < maxRetryCount) {
        retryCount++;
        /*wait for eSE wake up*/
        phPalEse_sleep(retryDelay);
        ALOGE("_spi_write() failed. Going to retry, counter:%ld !", retryCount);
        continue;
      }
      return -1;
    }
  }
  return numWrote;
}

/*******************************************************************************
**
** Function         phPalEse_spi_ioctl
**
** Description      Exposed ioctl by p61 spi driver
**
** Parameters       pDevHandle     - valid device handle
**                  level          - reset level
**
** Returns           0   - ioctl operation success
**                  -1   - ioctl operation failure
**
*******************************************************************************/
ESESTATUS phPalEse_spi_ioctl(phPalEse_ControlCode_t eControlCode,
                             void* pDevHandle, long level) {
  ESESTATUS ret = ESESTATUS_IOCTL_FAILED;
  ALOGD_IF(ese_debug_enabled, "phPalEse_spi_ioctl(), ioctl %x , level %lx",
           eControlCode, level);
  ese_nxp_IoctlInOutData_t inpOutData;
  inpOutData.inp.level = level;
  NfcAdaptation& pNfcAdapt = NfcAdaptation::GetInstance();
  if (NULL == pDevHandle) {
    return ESESTATUS_IOCTL_FAILED;
  }
  switch (eControlCode) {
    // Nfc Driver communication part
    case phPalEse_e_ChipRst:
      ret = pNfcAdapt.HalIoctl(HAL_NFC_SET_SPM_PWR, &inpOutData);
      break;

    case phPalEse_e_SetPowerScheme:
      // ret = sendIoctlData(p, HAL_NFC_SET_POWER_SCHEME, &inpOutData);
      ret = ESESTATUS_SUCCESS;
      break;

    case phPalEse_e_GetSPMStatus:
      // ret = sendIoctlData(p, HAL_NFC_GET_SPM_STATUS, &inpOutData);
      ret = ESESTATUS_SUCCESS;
      break;

    case phPalEse_e_GetEseAccess:
      // ret = sendIoctlData(p, HAL_NFC_GET_ESE_ACCESS, &inpOutData);
      ret = ESESTATUS_SUCCESS;
      break;
#ifdef NXP_ESE_JCOP_DWNLD_PROTECTION
    case phPalEse_e_SetJcopDwnldState:
      // ret = sendIoctlData(p, HAL_NFC_SET_DWNLD_STATUS, &inpOutData);
      ret = ESESTATUS_SUCCESS;
      break;
#endif
    case phPalEse_e_DisablePwrCntrl:
      ret = pNfcAdapt.HalIoctl(HAL_NFC_INHIBIT_PWR_CNTRL, &inpOutData);
      break;
    default:
      ret = ESESTATUS_IOCTL_FAILED;
      break;
  }
  return ret;
}
