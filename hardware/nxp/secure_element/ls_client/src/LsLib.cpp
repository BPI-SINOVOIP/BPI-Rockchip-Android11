/*******************************************************************************
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
#define LOG_TAG "LSClient"
#include <LsClient.h>
#include <LsLib.h>
#include <errno.h>
#include <log/log.h>
#include <stdlib.h>
#include <string.h>

extern bool ese_debug_enabled;

static int32_t gsTransceiveTimeout = 120000;
static uint8_t gsCmd_Buffer[64 * 1024];
static int32_t gsCmd_count = 0;
static bool gsIslastcmdLoad;
static bool gsSendBack_cmds = false;
static uint8_t* gspBuffer;
static uint8_t gsStoreData[22];
static uint8_t gsTag42Arr[17];
static uint8_t gsTag45Arr[9];
static uint8_t gsLsExecuteResp[4];
static int32_t gsResp_len = 0;

LSCSTATUS(*Applet_load_seqhandler[])
(Lsc_ImageInfo_t* pContext, LSCSTATUS status, Lsc_TranscieveInfo_t* pInfo) = {
    LSC_OpenChannel, LSC_ResetChannel, LSC_SelectLsc,
    LSC_StoreData,   LSC_loadapplet,   NULL};

/*******************************************************************************
**
** Function:        Perform_LSC
**
** Description:     Performs the LSC download sequence
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS Perform_LSC(const char* name, const char* dest, const uint8_t* pdata,
                      uint16_t len, uint8_t* respSW) {
  static const char fn[] = "Perform_LSC";
  ALOGD_IF(ese_debug_enabled, "%s: enter; sha-len=%d", fn, len);
  if ((pdata == NULL) || (len == 0x00)) {
    ALOGE("%s: Invalid SHA-data", fn);
    return LSCSTATUS_FAILED;
  }
  gsStoreData[0] = STORE_DATA_TAG;
  gsStoreData[1] = len;
  memcpy(&gsStoreData[2], pdata, len);
  LSCSTATUS status = LSC_update_seq_handler(Applet_load_seqhandler, name, dest);
  if ((status != LSCSTATUS_SUCCESS) && (gsLsExecuteResp[2] == 0x90) &&
      (gsLsExecuteResp[3] == 0x00)) {
    gsLsExecuteResp[2] = LS_ABORT_SW1;
    gsLsExecuteResp[3] = LS_ABORT_SW2;
  }
  memcpy(&respSW[0], &gsLsExecuteResp[0], 4);
  ALOGD_IF(ese_debug_enabled, "%s: lsExecuteScript Response SW=%2x%2x", fn,
           gsLsExecuteResp[2], gsLsExecuteResp[3]);

  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x0%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_update_seq_handler
**
** Description:     Performs the LSC update sequence handler sequence
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_update_seq_handler(
    LSCSTATUS (*seq_handler[])(Lsc_ImageInfo_t* pContext, LSCSTATUS status,
                               Lsc_TranscieveInfo_t* pInfo),
    const char* name, const char* dest) {
  static const char fn[] = "LSC_update_seq_handler";
  Lsc_ImageInfo_t update_info;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  memset(&update_info, 0, sizeof(Lsc_ImageInfo_t));
  if (dest != NULL) {
    strlcat(update_info.fls_RespPath, dest, sizeof(update_info.fls_RespPath));
    ALOGD_IF(ese_debug_enabled,
             "%s: Loader Service response data path/destination: %s", fn, dest);
    update_info.bytes_wrote = 0xAA;
  } else {
    update_info.bytes_wrote = 0x55;
  }
  if ((LSC_UpdateExeStatus(LS_DEFAULT_STATUS)) != true) {
    return LSCSTATUS_FAILED;
  }
  // memcpy(update_info.fls_path, (char*)Lsc_path, sizeof(Lsc_path));
  strlcat(update_info.fls_path, name, sizeof(update_info.fls_path));
  ALOGD_IF(ese_debug_enabled, "Selected applet to install is: %s",
           update_info.fls_path);

  uint16_t seq_counter = 0;
  LSCSTATUS status = LSCSTATUS_FAILED;
  Lsc_TranscieveInfo_t trans_info;
  memset(&trans_info, 0, sizeof(Lsc_TranscieveInfo_t));
  while ((seq_handler[seq_counter]) != NULL) {
    status = (*(seq_handler[seq_counter]))(&update_info, status, &trans_info);
    if (LSCSTATUS_SUCCESS != status) {
      ALOGE("%s: exiting; status=0x0%X", fn, status);
      break;
    }

    if ((seq_counter == 0x00) &&
        update_info.Channel_Info[update_info.channel_cnt - 1].isOpend) {
      update_info.initChannelNum =
          update_info.Channel_Info[update_info.channel_cnt - 1].channel_id;
    }
    seq_counter++;
  }

  LSC_CloseChannel(&update_info, LSCSTATUS_FAILED, &trans_info);
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_OpenChannel
**
** Description:     Creates the logical channel with lsc
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_OpenChannel(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                          Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_OpenChannel";

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
  cmdApdu.len = (int32_t)sizeof(OpenChannel);
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  memcpy(cmdApdu.p_data, OpenChannel, cmdApdu.len);

  ALOGD_IF(ese_debug_enabled, "%s: Calling Secure Element Transceive", fn);
  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if (eseStat != ESESTATUS_SUCCESS && (rspApdu.len < 0x03)) {
    if (rspApdu.len == 0x02)
      memcpy(&gsLsExecuteResp[2], &rspApdu.p_data[rspApdu.len - 2], 2);
    status = LSCSTATUS_FAILED;
    ALOGE("%s: SE transceive failed status = 0x%X", fn, status);
  } else if (((rspApdu.p_data[rspApdu.len - 2] != 0x90) &&
              (rspApdu.p_data[rspApdu.len - 1] != 0x00))) {
    memcpy(&gsLsExecuteResp[2], &rspApdu.p_data[rspApdu.len - 2], 2);
    status = LSCSTATUS_FAILED;
    ALOGE("%s: invalid response = 0x%X", fn, status);
  } else {
    uint8_t cnt = Os_info->channel_cnt;
    Os_info->Channel_Info[cnt].channel_id = rspApdu.p_data[rspApdu.len - 3];
    Os_info->Channel_Info[cnt].isOpend = true;
    Os_info->channel_cnt++;
    status = LSCSTATUS_SUCCESS;
  }

  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x%x", fn, status);
  return status;
}
/*******************************************************************************
**
** Function:        LSC_ResetChannel
**
** Description:     Reset(Open & Close) next available logical channel
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_ResetChannel(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                           Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_ResetChannel";

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }

  ESESTATUS eseStat = ESESTATUS_FAILED;
  bool bResetCompleted = false;
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
  cmdApdu.len = (int32_t)sizeof(OpenChannel);
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  memcpy(cmdApdu.p_data, OpenChannel, cmdApdu.len);

  do {
    ALOGD_IF(ese_debug_enabled, "%s: Calling Secure Element Transceive", fn);
    eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);
    if (eseStat != ESESTATUS_SUCCESS && (rspApdu.len < 0x03)) {
      status = LSCSTATUS_FAILED;
      ALOGE("%s: SE transceive failed status = 0x%X", fn, status);
    } else if (((rspApdu.p_data[rspApdu.len - 2] != 0x90) &&
                (rspApdu.p_data[rspApdu.len - 1] != 0x00))) {
      status = LSCSTATUS_FAILED;
      ALOGE("%s: invalid response = 0x%X", fn, status);
    } else if (!bResetCompleted) {
      /*close the previously opened channel*/
      uint8_t xx = 0;
      cmdApdu.p_data[xx++] = rspApdu.p_data[rspApdu.len - 3]; /*channel id*/
      cmdApdu.p_data[xx++] = 0x70;
      cmdApdu.p_data[xx++] = 0x80;
      cmdApdu.p_data[xx++] = rspApdu.p_data[rspApdu.len - 3];
      cmdApdu.p_data[xx++] = 0x00;
      cmdApdu.len = 5;
      bResetCompleted = true;
      phNxpEse_free(rspApdu.p_data);
      status = LSCSTATUS_SUCCESS;
    } else {
      ALOGD_IF(ese_debug_enabled, "%s: Channel reset success", fn);
      status = LSCSTATUS_SUCCESS;
      break;
    }
  } while (status == LSCSTATUS_SUCCESS);

  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_SelectLsc
**
** Description:     Creates the logical channel with lsc
**                  Channel_id will be used for any communication with Lsc
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_SelectLsc(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                        Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_SelectLsc";

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }

  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;

  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

  /*p_data will have channel_id (1 byte) + SelectLsc APDU*/
  cmdApdu.len = (int32_t)(sizeof(SelectLsc) + 1);
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  cmdApdu.p_data[0] = Os_info->Channel_Info[0].channel_id;

  memcpy(&(cmdApdu.p_data[1]), SelectLsc, sizeof(SelectLsc));

  ALOGD_IF(ese_debug_enabled,
           "%s: Calling Secure Element Transceive with Loader service AID", fn);

  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if (eseStat != ESESTATUS_SUCCESS && (rspApdu.len == 0x00)) {
    status = LSCSTATUS_FAILED;
    ALOGE("%s: SE transceive failed status = 0x%X", fn, status);
  } else if (((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
              (rspApdu.p_data[rspApdu.len - 1] == 0x00))) {
    status = Process_SelectRsp(rspApdu.p_data, (rspApdu.len - 2));
    if (status != LSCSTATUS_SUCCESS) {
      ALOGE("%s: Select Lsc Rsp doesnt have a valid key; status = 0x%X", fn,
            status);
    }
  } else if (((rspApdu.p_data[rspApdu.len - 2] != 0x90))) {
    /*Copy the response SW in failure case*/
    memcpy(&gsLsExecuteResp[2], &(rspApdu.p_data[rspApdu.len - 2]), 2);
  } else {
    status = LSCSTATUS_FAILED;
  }
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_StoreData
**
** Description:     It is used to provide the LSC with an Unique
**                  Identifier of the Application that has triggered the LSC
*script.
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_StoreData(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                        Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_StoreData";
  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }

  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
  cmdApdu.len = (int32_t)(5 + sizeof(gsStoreData));
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));

  uint32_t xx = 0;
  int32_t len =
      gsStoreData[1] + 2;  //+2 offset is for tag value and length byte
  cmdApdu.p_data[xx++] = STORE_DATA_CLA | (Os_info->Channel_Info[0].channel_id);
  cmdApdu.p_data[xx++] = STORE_DATA_INS;
  cmdApdu.p_data[xx++] = 0x00;  // P1
  cmdApdu.p_data[xx++] = 0x00;  // P2
  cmdApdu.p_data[xx++] = len;
  memcpy(&(cmdApdu.p_data[xx]), gsStoreData, len);

  ALOGD_IF(ese_debug_enabled, "%s: Calling Secure Element Transceive", fn);
  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if ((eseStat != ESESTATUS_SUCCESS) && (rspApdu.len == 0x00)) {
    status = LSCSTATUS_FAILED;
    ALOGE("%s: SE transceive failed status = 0x%X", fn, status);
  } else if ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
             (rspApdu.p_data[rspApdu.len - 1] == 0x00)) {
    ALOGD_IF(ese_debug_enabled, "%s: STORE CMD is successful", fn);
    status = LSCSTATUS_SUCCESS;
  } else {
    /*Copy the response SW in failure case*/
    memcpy(&gsLsExecuteResp[2], &(rspApdu.p_data[rspApdu.len - 2]), 2);
    status = LSCSTATUS_FAILED;
  }
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_loadapplet
**
** Description:     Reads the script from the file and sent to Lsc
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_loadapplet(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                         Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_loadapplet";
  bool reachEOFCheck = false;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }
  if (Os_info->bytes_wrote == 0xAA) {
    Os_info->fResp = fopen(Os_info->fls_RespPath, "a+");
    if (Os_info->fResp == NULL) {
      ALOGE("%s: Error opening response recording file <%s> for reading: %s",
            fn, Os_info->fls_path, strerror(errno));
      return LSCSTATUS_FAILED;
    }
    ALOGD_IF(ese_debug_enabled,
             "%s: Response OUT FILE path is successfully created", fn);
  } else {
    ALOGD_IF(ese_debug_enabled,
             "%s: Response Out file is optional as per input", fn);
  }

  Os_info->fp = fopen(Os_info->fls_path, "r");
  if (Os_info->fp == NULL) {
    ALOGE("%s: Error opening OS image file <%s> for reading: %s", fn,
          Os_info->fls_path, strerror(errno));
    return LSCSTATUS_FAILED;
  }
  int wResult = fseek(Os_info->fp, 0L, SEEK_END);
  if (wResult) {
    ALOGE("%s: Error seeking end OS image file %s", fn, strerror(errno));
    goto exit;
  }
  Os_info->fls_size = ftell(Os_info->fp);
  if (Os_info->fls_size < 0) {
    ALOGE("%s: Error ftelling file %s", fn, strerror(errno));
    goto exit;
  }
  wResult = fseek(Os_info->fp, 0L, SEEK_SET);
  if (wResult) {
    ALOGE("%s: Error seeking start image file %s", fn, strerror(errno));
    goto exit;
  }

  Os_info->bytes_read = 0;
  status = LSC_Check_KeyIdentifier(Os_info, status, pTranscv_Info, NULL,
                                   LSCSTATUS_FAILED, 0);
  if (status != LSCSTATUS_SUCCESS) {
    goto exit;
  }

  uint8_t len_byte, offset;
  while (!feof(Os_info->fp) && (Os_info->bytes_read < Os_info->fls_size)) {
    len_byte = 0;
    offset = 0;

    uint8_t temp_buf[1024];
    memset(temp_buf, 0, sizeof(temp_buf));
    status = LSC_ReadScript(Os_info, temp_buf);
    if (status != LSCSTATUS_SUCCESS) {
      goto exit;
    }
    /*Reset the flag in case further commands exists*/
    reachEOFCheck = false;

    int32_t wLen = 0;
    LSCSTATUS tag40_found = LSCSTATUS_SUCCESS;
    if (temp_buf[offset] == TAG_LSC_CMD_ID) {
      /* start sending the packet to Lsc */
      offset = offset + 1;
      len_byte = Numof_lengthbytes(&temp_buf[offset], &wLen);
      /* If the len data not present or len is less than or equal to 32 */
      if ((len_byte == 0) || (wLen <= 32)) {
        ALOGE("%s: Invalid length zero", fn);
        goto exit;
      }

      tag40_found = LSCSTATUS_SUCCESS;
      offset = offset + len_byte;
      pTranscv_Info->sSendlength = wLen;
      memcpy(pTranscv_Info->sSendData, &temp_buf[offset], wLen);

      status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Comm);
      if (status != LSCSTATUS_SUCCESS) {
        /*When the switching of LS 6320 case*/
        if (status == LSCSTATUS_SELF_UPDATE_DONE) {
          status = LSC_CloseAllLogicalChannels(Os_info);
          if (status != LSCSTATUS_SUCCESS) {
            ALOGE("%s: CleanupLsUpdaterChannels failed", fn);
          }
          status = LSCSTATUS_SUCCESS;
          goto exit;
        }
        ALOGE("%s: Sending packet to lsc failed", fn);
        goto exit;
      }
    } else if ((temp_buf[offset] == (0x7F)) &&
               (temp_buf[offset + 1] == (0x21))) {
      ALOGD_IF(ese_debug_enabled,
               "%s: TAGID: Encountered again certificate tag 7F21", fn);
      if (tag40_found == LSCSTATUS_SUCCESS) {
        ALOGD_IF(ese_debug_enabled,
                 "%s: 2nd Script processing starts with reselect", fn);
        status = LSCSTATUS_FAILED;
        status = LSC_SelectLsc(Os_info, status, pTranscv_Info);
        if (status == LSCSTATUS_SUCCESS) {
          ALOGD_IF(ese_debug_enabled,
                   "%s: 2nd Script select success next store data command", fn);
          status = LSCSTATUS_FAILED;
          status = LSC_StoreData(Os_info, status, pTranscv_Info);
          if (status == LSCSTATUS_SUCCESS) {
            ALOGD_IF(ese_debug_enabled,
                     "%s: 2nd Script store data success next certificate "
                     "verification",
                     fn);
            offset = offset + 2;
            len_byte = Numof_lengthbytes(&temp_buf[offset], &wLen);
            status = LSC_Check_KeyIdentifier(Os_info, status, pTranscv_Info,
                                             temp_buf, LSCSTATUS_SUCCESS,
                                             wLen + len_byte + 2);
          }
        }
        /*If the certificate and signature is verified*/
        if (status == LSCSTATUS_SUCCESS) {
          /*If the certificate is verified for 6320 then new script starts*/
          tag40_found = LSCSTATUS_FAILED;
        } else {
          /*If the certificate or signature verification failed*/
          goto exit;
        }
      } else {
        /*Already certificate&Sginature verified previously skip 7f21& tag 60*/
        memset(temp_buf, 0, sizeof(temp_buf));
        status = LSC_ReadScript(Os_info, temp_buf);
        if (status != LSCSTATUS_SUCCESS) {
          ALOGE("%s: Next Tag has to TAG 60 not found", fn);
          goto exit;
        }
        if (temp_buf[offset] == TAG_JSBL_HDR_ID)
          continue;
        else
          goto exit;
      }
    } else {
      /*
       * Invalid packet received in between stop processing packet
       * return failed status
       */
      status = LSCSTATUS_FAILED;
      break;
    }
  }
  if (Os_info->bytes_wrote == 0xAA) {
    fclose(Os_info->fResp);
  }
  LSC_UpdateExeStatus(LS_SUCCESS_STATUS);
  wResult = fclose(Os_info->fp);
  ALOGD_IF(ese_debug_enabled, "%s: exit, status=0x%x", fn, status);
  return status;
exit:
  wResult = fclose(Os_info->fp);
  if (Os_info->bytes_wrote == 0xAA) {
    fclose(Os_info->fResp);
  }
  /*Script ends with SW 6320 and reached END OF FILE*/
  if (reachEOFCheck == true) {
    status = LSCSTATUS_SUCCESS;
    LSC_UpdateExeStatus(LS_SUCCESS_STATUS);
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit; status= 0x%X", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_Check_KeyIdentifier
**
** Description:     Checks and validates certificate
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_Check_KeyIdentifier(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                                  Lsc_TranscieveInfo_t* pTranscv_Info,
                                  uint8_t* temp_buf, LSCSTATUS flag,
                                  int32_t wNewLen) {
  static const char fn[] = "LSC_Check_KeyIdentifier";
  status = LSCSTATUS_FAILED;
  uint8_t read_buf[1024] = {0};
  uint16_t offset = 0, len_byte = 0;
  int32_t wLen;
  uint8_t certf_found = LSCSTATUS_FAILED;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  while (!feof(Os_info->fp) && (Os_info->bytes_read < Os_info->fls_size)) {
    offset = 0x00;
    wLen = 0;
    if (flag == LSCSTATUS_SUCCESS) {
      /*If the 7F21 TAG is already read: After TAG 40*/
      memcpy(read_buf, temp_buf, wNewLen);
      status = LSCSTATUS_SUCCESS;
      flag = LSCSTATUS_FAILED;
    } else {
      /*If the 7F21 TAG is not read: Before TAG 40*/
      status = LSC_ReadScript(Os_info, read_buf);
    }
    if (status != LSCSTATUS_SUCCESS) return status;
    if (LSCSTATUS_SUCCESS ==
        Check_Complete_7F21_Tag(Os_info, pTranscv_Info, read_buf, &offset)) {
      ALOGD_IF(ese_debug_enabled, "%s: Certificate is verified", fn);
      certf_found = LSCSTATUS_SUCCESS;
      break;
    }
    /*
     * The Loader Service Client ignores all subsequent commands starting by tag
     * 7F21 or tag 60 until the first command starting by tag 40 is found
     */
    else if (((read_buf[offset] == TAG_LSC_CMD_ID) &&
              (certf_found != LSCSTATUS_SUCCESS))) {
      ALOGE("%s: NOT FOUND Root entity identifier's certificate", fn);
      status = LSCSTATUS_FAILED;
      return status;
    }
  }
  memset(read_buf, 0, sizeof(read_buf));
  if (certf_found == LSCSTATUS_SUCCESS) {
    offset = 0x00;
    wLen = 0;
    status = LSC_ReadScript(Os_info, read_buf);
    if (status != LSCSTATUS_SUCCESS) return status;
    if ((read_buf[offset] == TAG_JSBL_HDR_ID) &&
        (certf_found != LSCSTATUS_FAILED)) {
      // TODO check the SElect cmd response and return status accordingly
      ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_JSBL_HDR_ID", fn);
      offset = offset + 1;
      len_byte = Numof_lengthbytes(&read_buf[offset], &wLen);
      offset = offset + len_byte;
      if (read_buf[offset] == TAG_SIGNATURE_ID) {
        offset = offset + 1;
        len_byte = Numof_lengthbytes(&read_buf[offset], &wLen);
        offset = offset + len_byte;
        ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_SIGNATURE_ID", fn);

        pTranscv_Info->sSendlength = wLen + 5;

        pTranscv_Info->sSendData[0] = 0x00;
        pTranscv_Info->sSendData[1] = 0xA0;
        pTranscv_Info->sSendData[2] = 0x00;
        pTranscv_Info->sSendData[3] = 0x00;
        pTranscv_Info->sSendData[4] = wLen;

        memcpy(&(pTranscv_Info->sSendData[5]), &read_buf[offset], wLen);
        ALOGD_IF(ese_debug_enabled, "%s: start transceive for length %ld", fn,
                 (long)pTranscv_Info->sSendlength);
        status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Sign);
        if (status != LSCSTATUS_SUCCESS) {
          return status;
        }
      }
    } else if (read_buf[offset] != TAG_JSBL_HDR_ID) {
      status = LSCSTATUS_FAILED;
    }
  } else {
    ALOGE("%s : Exit certificate verification failed", fn);
  }

  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_ReadScript
**
** Description:     Reads the current line if the script
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_ReadScript(Lsc_ImageInfo_t* Os_info, uint8_t* read_buf) {
  static const char fn[] = "LSC_ReadScript";
  int32_t wResult = 0, wCount, wIndex = 0;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  for (wCount = 0; (wCount < 2 && !feof(Os_info->fp)); wCount++, wIndex++) {
    wResult = FSCANF_BYTE(Os_info->fp, "%2X", (unsigned int*)&read_buf[wIndex]);
  }
  if (wResult == 0) return LSCSTATUS_FAILED;

  Os_info->bytes_read = Os_info->bytes_read + (wCount * 2);

  int32_t lenOff = 1;
  if ((read_buf[0] == 0x7f) && (read_buf[1] == 0x21)) {
    for (wCount = 0; (wCount < 1 && !feof(Os_info->fp)); wCount++, wIndex++) {
      wResult =
          FSCANF_BYTE(Os_info->fp, "%2X", (unsigned int*)&read_buf[wIndex]);
    }
    if (wResult == 0) {
      ALOGE("%s: Exit Read Script failed in 7F21 ", fn);
      return LSCSTATUS_FAILED;
    }
    /*Read_Script from wCount*2 to wCount*1 */
    Os_info->bytes_read = Os_info->bytes_read + (wCount * 2);
    lenOff = 2;
  } else if ((read_buf[0] == 0x40) || (read_buf[0] == 0x60)) {
    lenOff = 1;
  } else {
    /*If TAG is neither 7F21 nor 60 nor 40 then ABORT execution*/
    ALOGE("%s: Invalid TAG 0x%X found in the script", fn, read_buf[0]);
    return LSCSTATUS_FAILED;
  }

  uint8_t len_byte = 0;
  int32_t wLen;
  if (read_buf[lenOff] == 0x00) {
    ALOGE("%s: Invalid length zero", fn);
    len_byte = 0x00;
    return LSCSTATUS_FAILED;
  } else if ((read_buf[lenOff] & 0x80) == 0x80) {
    len_byte = read_buf[lenOff] & 0x0F;
    len_byte = len_byte + 1;  // 1 byte added for byte 0x81

    ALOGD_IF(ese_debug_enabled, "%s: Length byte Read from 0x80 is 0x%x ", fn,
             len_byte);

    if (len_byte == 0x02) {
      for (wCount = 0; (wCount < 1 && !feof(Os_info->fp)); wCount++, wIndex++) {
        wResult =
            FSCANF_BYTE(Os_info->fp, "%2X", (unsigned int*)&read_buf[wIndex]);
      }
      if (wResult == 0) {
        ALOGE("%s: Exit Read Script failed in length 0x02 ", fn);
        return LSCSTATUS_FAILED;
      }

      wLen = read_buf[lenOff + 1];
      Os_info->bytes_read = Os_info->bytes_read + (wCount * 2);
      ALOGD_IF(ese_debug_enabled,
               "%s: Length of Read Script in len_byte= 0x02 is 0x%x ", fn,
               wLen);
    } else if (len_byte == 0x03) {
      for (wCount = 0; (wCount < 2 && !feof(Os_info->fp)); wCount++, wIndex++) {
        wResult =
            FSCANF_BYTE(Os_info->fp, "%2X", (unsigned int*)&read_buf[wIndex]);
      }
      if (wResult == 0) {
        ALOGE("%s: Exit Read Script failed in length 0x03 ", fn);
        return LSCSTATUS_FAILED;
      }

      Os_info->bytes_read = Os_info->bytes_read + (wCount * 2);
      wLen = read_buf[lenOff + 1];  // Length of the packet send to LSC
      wLen = ((wLen << 8) | (read_buf[lenOff + 2]));
      ALOGD_IF(ese_debug_enabled,
               "%s: Length of Read Script in len_byte= 0x03 is 0x%x ", fn,
               wLen);
    } else {
      /*Need to provide the support if length is more than 2 bytes*/
      ALOGE("Length recived is greater than 3");
      return LSCSTATUS_FAILED;
    }
  } else {
    len_byte = 0x01;
    wLen = read_buf[lenOff];
    ALOGE("%s: Length of Read Script in len_byte= 0x01 is 0x%x ", fn, wLen);
  }

  for (wCount = 0; (wCount < wLen && !feof(Os_info->fp)); wCount++, wIndex++) {
    wResult = FSCANF_BYTE(Os_info->fp, "%2X", (unsigned int*)&read_buf[wIndex]);
  }

  if (wResult == 0) {
    ALOGE("%s: Exit Read Script failed in fscanf function ", fn);
    return LSCSTATUS_FAILED;
  }
  Os_info->bytes_read =
      Os_info->bytes_read + (wCount * 2) + 1;  // not sure why 2 added

  ALOGD_IF(ese_debug_enabled, "%s: exit: Num of bytes read=%d and index=%d", fn,
           Os_info->bytes_read, wIndex);

  return LSCSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function:        LSC_SendtoEse
**
** Description:     It is used to send the packet to p61
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_SendtoEse(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                        Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_SendtoEse";
  bool chanl_open_cmd = false;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  /* Bufferize_load_cmds function is implemented in JCOP */
  status = Bufferize_load_cmds(Os_info, status, pTranscv_Info);
  if (status != LSCSTATUS_FAILED) {
    if (pTranscv_Info->sSendData[1] == 0x70) {
      if (pTranscv_Info->sSendData[2] == 0x00) {
        chanl_open_cmd = true;
      } else {
        for (uint8_t cnt = 0; cnt < Os_info->channel_cnt; cnt++) {
          if (Os_info->Channel_Info[cnt].channel_id ==
              pTranscv_Info->sSendData[3]) {
            ALOGD_IF(ese_debug_enabled, "%s: channel 0%x closed", fn,
                     Os_info->Channel_Info[cnt].channel_id);
            Os_info->Channel_Info[cnt].isOpend = false;
          }
        }
      }
    }

    phNxpEse_data cmdApdu;
    phNxpEse_data rspApdu;
    phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
    phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

    cmdApdu.len = (int32_t)(pTranscv_Info->sSendlength);
    cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
    memcpy(cmdApdu.p_data, pTranscv_Info->sSendData, cmdApdu.len);

    ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

    if (eseStat != ESESTATUS_SUCCESS) {
      ALOGE("%s: Transceive failed; status=0x%X", fn, eseStat);
      status = LSCSTATUS_FAILED;
    } else {
      if (chanl_open_cmd && (rspApdu.len == 0x03) &&
          ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
           (rspApdu.p_data[rspApdu.len - 1] == 0x00))) {
        ALOGD_IF(ese_debug_enabled, "%s: open channel success", fn);
        uint8_t cnt = Os_info->channel_cnt;
        Os_info->Channel_Info[cnt].channel_id = rspApdu.p_data[rspApdu.len - 3];
        Os_info->Channel_Info[cnt].isOpend = true;
        Os_info->channel_cnt++;
      }
      memcpy(pTranscv_Info->sRecvData, rspApdu.p_data, rspApdu.len);
      status = Process_EseResponse(pTranscv_Info, rspApdu.len, Os_info);
      phNxpEse_free(cmdApdu.p_data);
      phNxpEse_free(rspApdu.p_data);
    }
  } else if (gsSendBack_cmds == false) {
    /* Workaround for issue in JCOP, send the fake response back */
    int32_t recvBufferActualSize = 0x03;
    pTranscv_Info->sRecvData[0] = 0x00;
    pTranscv_Info->sRecvData[1] = 0x90;
    pTranscv_Info->sRecvData[2] = 0x00;
    status = Process_EseResponse(pTranscv_Info, recvBufferActualSize, Os_info);
  } else {
    if (gsIslastcmdLoad == true) {
      status = Send_Backall_Loadcmds(Os_info, status, pTranscv_Info);
      gsSendBack_cmds = false;
    } else {
      memset(gsCmd_Buffer, 0, sizeof(gsCmd_Buffer));
      gsSendBack_cmds = false;
      status = LSCSTATUS_FAILED;
    }
  }

  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_SendtoLsc
**
** Description:     It is used to forward the packet to Lsc
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_SendtoLsc(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                        Lsc_TranscieveInfo_t* pTranscv_Info, Ls_TagType tType) {
  static const char fn[] = "LSC_SendtoLsc";

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  pTranscv_Info->sSendData[0] = (0x80 | Os_info->Channel_Info[0].channel_id);
  pTranscv_Info->timeout = gsTransceiveTimeout;
  pTranscv_Info->sRecvlength = 1024;

  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
  cmdApdu.len = pTranscv_Info->sSendlength;
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  memcpy(cmdApdu.p_data, pTranscv_Info->sSendData, cmdApdu.len);

  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if (eseStat != ESESTATUS_SUCCESS) {
    ALOGE("%s: Transceive failed; status=0x%X", fn, eseStat);
    status = LSCSTATUS_FAILED;
  } else {
    memcpy(pTranscv_Info->sRecvData, rspApdu.p_data, rspApdu.len);
    status = LSC_ProcessResp(Os_info, rspApdu.len, pTranscv_Info, tType);
  }
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_CloseChannel
**
** Description:     Closes the previously opened logical channel
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_CloseChannel(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                           Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "LSC_CloseChannel";
  status = LSCSTATUS_FAILED;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  if (Os_info == NULL || pTranscv_Info == NULL) {
    ALOGE("%s: Invalid parameter", fn);
    return LSCSTATUS_FAILED;
  }
  for (uint8_t cnt = 0; (cnt < Os_info->channel_cnt); cnt++) {
    phNxpEse_data cmdApdu;
    phNxpEse_data rspApdu;

    phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
    phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

    cmdApdu.len = 5;
    cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
    if (!Os_info->Channel_Info[cnt].isOpend) continue;
    uint8_t xx = 0;
    cmdApdu.p_data[xx++] = Os_info->Channel_Info[cnt].channel_id;
    cmdApdu.p_data[xx++] = 0x70;
    cmdApdu.p_data[xx++] = 0x80;
    cmdApdu.p_data[xx++] = Os_info->Channel_Info[cnt].channel_id;
    cmdApdu.p_data[xx++] = 0x00;

    ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

    if (eseStat != ESESTATUS_SUCCESS || rspApdu.len < 2) {
      ALOGD_IF(ese_debug_enabled, "%s: Transceive failed; status=0x%X", fn,
               eseStat);
    } else if ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
               (rspApdu.p_data[rspApdu.len - 1] == 0x00)) {
      ALOGD_IF(ese_debug_enabled, "%s: Close channel id = 0x0%x success", fn,
               Os_info->Channel_Info[cnt].channel_id);
      if (Os_info->Channel_Info[cnt].channel_id == Os_info->initChannelNum) {
        Os_info->initChannelNum = 0x00;
      }
      status = LSCSTATUS_SUCCESS;
    } else {
      ALOGD_IF(ese_debug_enabled, "%s: Close channel id = 0x0%x failed", fn,
               Os_info->Channel_Info[cnt].channel_id);
    }
    phNxpEse_free(cmdApdu.p_data);
    phNxpEse_free(rspApdu.p_data);
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit; status=0x0%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        LSC_ProcessResp
**
** Description:     Process the response packet received from Lsc
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS LSC_ProcessResp(Lsc_ImageInfo_t* image_info, int32_t recvlen,
                          Lsc_TranscieveInfo_t* trans_info, Ls_TagType tType) {
  static const char fn[] = "LSC_ProcessResp";
  uint8_t* RecvData = trans_info->sRecvData;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  if (RecvData == NULL && recvlen == 0x00) {
    ALOGE("%s: Invalid parameter.", fn);
    return LSCSTATUS_FAILED;
  } else if (recvlen < 2) {
    ALOGE("%s: Invalid response.", fn);
    return LSCSTATUS_FAILED;
  }

  char sw[2];
  sw[0] = RecvData[recvlen - 2];
  sw[1] = RecvData[recvlen - 1];
  ALOGD_IF(ese_debug_enabled, "%s: Process Response SW, status = 0x%2X%2X", fn,
           sw[0], sw[1]);

  /*Update the Global variable for storing response length*/
  gsResp_len = recvlen;
  if (sw[0] != 0x63) {
    gsLsExecuteResp[2] = sw[0];
    gsLsExecuteResp[3] = sw[1];
  }

  LSCSTATUS status = LSCSTATUS_FAILED;
  if ((recvlen == 0x02) && (sw[0] == 0x90) && (sw[1] == 0x00)) {
    status = Write_Response_To_OutFile(image_info, RecvData, recvlen, tType);
  } else if ((recvlen > 0x02) && (sw[0] == 0x90) && (sw[1] == 0x00)) {
    status = Write_Response_To_OutFile(image_info, RecvData, recvlen, tType);
  } else if ((recvlen > 0x02) && (sw[0] == 0x63) && (sw[1] == 0x10)) {
    static int32_t temp_len = 0;
    if (temp_len != 0) {
      memcpy((trans_info->sTemp_recvbuf + temp_len), RecvData, (recvlen - 2));
      trans_info->sSendlength = temp_len + (recvlen - 2);
      memcpy(trans_info->sSendData, trans_info->sTemp_recvbuf,
             trans_info->sSendlength);
      temp_len = 0;
    } else {
      memcpy(trans_info->sSendData, RecvData, (recvlen - 2));
      trans_info->sSendlength = recvlen - 2;
    }
    status = LSC_SendtoEse(image_info, status, trans_info);
  } else if ((recvlen > 0x02) && (sw[0] == 0x63) && (sw[1] == 0x20)) {
    /*In case of self update, status 0x6320 indicates script execution success
    and response data has new AID*/
    status = LSCSTATUS_SELF_UPDATE_DONE;
  } else if ((recvlen >= 0x02) &&
             ((sw[0] != 0x90) && (sw[0] != 0x63) && (sw[0] != 0x61))) {
    Write_Response_To_OutFile(image_info, RecvData, recvlen, tType);
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        Process_EseResponse
**
** Description:     It is used to process the received response packet from ESE
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS Process_EseResponse(Lsc_TranscieveInfo_t* pTranscv_Info,
                              int32_t recv_len, Lsc_ImageInfo_t* Os_info) {
  static const char fn[] = "Process_EseResponse";
  LSCSTATUS status = LSCSTATUS_SUCCESS;
  uint8_t xx = 0;
  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  pTranscv_Info->sSendData[xx++] =
      (CLA_BYTE | Os_info->Channel_Info[0].channel_id);
  pTranscv_Info->sSendData[xx++] = 0xA2;

  if (recv_len <= 0xFF) {
    pTranscv_Info->sSendData[xx++] = 0x80;
    pTranscv_Info->sSendData[xx++] = 0x00;
    pTranscv_Info->sSendData[xx++] = (uint8_t)recv_len;
    memcpy(&(pTranscv_Info->sSendData[xx]), pTranscv_Info->sRecvData, recv_len);
    pTranscv_Info->sSendlength = xx + recv_len;
    status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Comm);
  } else {
    while (recv_len > MAX_SIZE) {
      xx = PARAM_P1_OFFSET;
      pTranscv_Info->sSendData[xx++] = 0x00;
      pTranscv_Info->sSendData[xx++] = 0x00;
      pTranscv_Info->sSendData[xx++] = MAX_SIZE;
      recv_len = recv_len - MAX_SIZE;
      memcpy(&(pTranscv_Info->sSendData[xx]), pTranscv_Info->sRecvData,
             MAX_SIZE);
      pTranscv_Info->sSendlength = xx + MAX_SIZE;
      /*
       * Need not store Process eSE response's response in the out file so
       * LS_Comm = 0
       */
      status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Comm);
      if (status != LSCSTATUS_SUCCESS) {
        ALOGE("%s: Sending packet to Lsc failed: status=0x%x", fn, status);
        return status;
      }
    }
    xx = PARAM_P1_OFFSET;
    pTranscv_Info->sSendData[xx++] = LAST_BLOCK;
    pTranscv_Info->sSendData[xx++] = 0x01;
    pTranscv_Info->sSendData[xx++] = recv_len;
    memcpy(&(pTranscv_Info->sSendData[xx]), pTranscv_Info->sRecvData, recv_len);
    pTranscv_Info->sSendlength = xx + recv_len;
    status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Comm);
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}

/*******************************************************************************
**
** Function:        Process_SelectRsp
**
** Description:    It is used to process the received response for SELECT LSC
**                 cmd.
**
** Returns:         Success if ok.
**
*******************************************************************************/
LSCSTATUS Process_SelectRsp(uint8_t* Recv_data, int32_t Recv_len) {
  static const char fn[] = "Process_SelectRsp";
  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  if (Recv_len < 2) {
    ALOGE("%s: Invalid response length %d", fn, Recv_len);
    return LSCSTATUS_FAILED;
  }

  int i = 0;
  if (Recv_data[i] != TAG_SELECT_ID) {
    ALOGE("%s: Invalid FCI TAG = 0x%x", fn, Recv_data[i]);
    return LSCSTATUS_FAILED;
  }
  i++;
  int len = Recv_data[i++];
  if (Recv_len < len + 2) {
    ALOGE("%s: Invalid response length %d", fn, Recv_len);
    return LSCSTATUS_FAILED;
  }
  if (Recv_data[i] != TAG_LSC_ID) {
    ALOGE("%s: Invalid Loader Service AID TAG ID = 0x%x", fn, Recv_data[i]);
    return LSCSTATUS_FAILED;
  }
  i++;
  len = Recv_data[i];
  i = i + 1 + len;  // points to next tag name A5
  // points to TAG 9F08 for LS application version
  if ((Recv_data[i] != TAG_LS_VER1) || (Recv_data[i + 1] != TAG_LS_VER2)) {
    ALOGE("%s: Invalid LS Version = 0x%2X%2X", fn, Recv_data[i],
          Recv_data[i + 1]);
    return LSCSTATUS_FAILED;
  }
  uint8_t lsaVersionLen = 0;
  i = i + 2;
  lsaVersionLen = Recv_data[i];
  // points to TAG 9F08 LS application version
  i++;
  // points to Identifier of the Root Entity key set identifier
  i = i + lsaVersionLen;

  if (Recv_data[i] != TAG_RE_KEYID) {
    ALOGE("%s: Invalid Root entity key set TAG ID = 0x%x", fn, Recv_data[i]);
    return LSCSTATUS_FAILED;
  }

  i = i + 2;
  if (Recv_data[i] != TAG_LSRE_ID) {
    ALOGE("%s: Invalid Root entity for TAG 42 = 0x%x", fn, Recv_data[i]);
    return LSCSTATUS_FAILED;
  }
  i++;
  uint8_t tag42Len = Recv_data[i];
  // copy the data including length
  memcpy(gsTag42Arr, &Recv_data[i], tag42Len + 1);
  i = i + tag42Len + 1;
  ALOGD_IF(ese_debug_enabled, "%s: gsTag42Arr %s", fn, gsTag42Arr);
  if (Recv_data[i] != TAG_LSRE_SIGNID) {
    ALOGE("%s: Invalid Root entity for TAG 45 = 0x%x", fn, Recv_data[i]);
    return LSCSTATUS_FAILED;
  }
  uint8_t tag45Len = Recv_data[i + 1];
  memcpy(gsTag45Arr, &Recv_data[i + 1], tag45Len + 1);
  ALOGD_IF(ese_debug_enabled, "%s: Exiting", fn);
  return LSCSTATUS_SUCCESS;
}

LSCSTATUS Bufferize_load_cmds(__attribute__((unused)) Lsc_ImageInfo_t* Os_info,
                              __attribute__((unused)) LSCSTATUS status,
                              Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "Bufferize_load_cmds";

  if (gsCmd_count == 0x00) {
    if ((pTranscv_Info->sSendData[1] == INSTAL_LOAD_ID) &&
        (pTranscv_Info->sSendData[2] == PARAM_P1_OFFSET) &&
        (pTranscv_Info->sSendData[3] == 0x00)) {
      ALOGD_IF(ese_debug_enabled, "%s: BUffer: install for load", fn);
      gspBuffer[0] = pTranscv_Info->sSendlength;
      memcpy(&gspBuffer[1], &(pTranscv_Info->sSendData[0]),
             pTranscv_Info->sSendlength);
      gspBuffer = gspBuffer + pTranscv_Info->sSendlength + 1;
      gsCmd_count++;
      return LSCSTATUS_FAILED;
    }
    /* Do not buffer this cmd, Send to eSE */
    return LSCSTATUS_SUCCESS;
  } else {
    uint8_t Param_P2 = gsCmd_count - 1;
    if ((pTranscv_Info->sSendData[1] == LOAD_CMD_ID) &&
        (pTranscv_Info->sSendData[2] == LOAD_MORE_BLOCKS) &&
        (pTranscv_Info->sSendData[3] == Param_P2)) {
      ALOGD_IF(ese_debug_enabled, "%s: BUffer: load", fn);
      gspBuffer[0] = pTranscv_Info->sSendlength;
      memcpy(&gspBuffer[1], &(pTranscv_Info->sSendData[0]),
             pTranscv_Info->sSendlength);
      gspBuffer = gspBuffer + pTranscv_Info->sSendlength + 1;
      gsCmd_count++;
    } else if ((pTranscv_Info->sSendData[1] == LOAD_CMD_ID) &&
               (pTranscv_Info->sSendData[2] == LOAD_LAST_BLOCK) &&
               (pTranscv_Info->sSendData[3] == Param_P2)) {
      ALOGD_IF(ese_debug_enabled, "%s: BUffer: last load", fn);
      gsSendBack_cmds = true;
      gspBuffer[0] = pTranscv_Info->sSendlength;
      memcpy(&gspBuffer[1], &(pTranscv_Info->sSendData[0]),
             pTranscv_Info->sSendlength);
      gspBuffer = gspBuffer + pTranscv_Info->sSendlength + 1;
      gsCmd_count++;
      gsIslastcmdLoad = true;
    } else {
      ALOGD_IF(ese_debug_enabled, "%s: BUffer: Not a load cmd", fn);
      gsSendBack_cmds = true;
      gspBuffer[0] = pTranscv_Info->sSendlength;
      memcpy(&gspBuffer[1], &(pTranscv_Info->sSendData[0]),
             pTranscv_Info->sSendlength);
      gspBuffer = gspBuffer + pTranscv_Info->sSendlength + 1;
      gsIslastcmdLoad = false;
      gsCmd_count++;
    }
  }
  ALOGD_IF(ese_debug_enabled, "%s: exit", fn);
  return LSCSTATUS_FAILED;
}

LSCSTATUS Send_Backall_Loadcmds(Lsc_ImageInfo_t* Os_info, LSCSTATUS status,
                                Lsc_TranscieveInfo_t* pTranscv_Info) {
  static const char fn[] = "Send_Backall_Loadcmds";
  status = LSCSTATUS_FAILED;

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);
  gspBuffer = gsCmd_Buffer;  // Points to start of first cmd to send
  if (gsCmd_count == 0x00) {
    ALOGD_IF(ese_debug_enabled, "%s: No cmds stored to send to eSE", fn);
  } else {
    while (gsCmd_count-- > 0) {
      phNxpEse_data cmdApdu;
      phNxpEse_data rspApdu;
      phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
      phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

      cmdApdu.len = (int32_t)(gspBuffer[0]);
      cmdApdu.p_data =
          (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
      gspBuffer = gspBuffer + 1 + cmdApdu.len;

      memcpy(cmdApdu.p_data, &gspBuffer[1], cmdApdu.len);

      ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);
      memcpy(pTranscv_Info->sRecvData, rspApdu.p_data, rspApdu.len);
      int32_t recvBufferActualSize = rspApdu.len;
      phNxpEse_free(cmdApdu.p_data);
      phNxpEse_free(rspApdu.p_data);

      if (eseStat != ESESTATUS_SUCCESS || (recvBufferActualSize < 2)) {
        ALOGE("%s: Transceive failed; status=0x%X", fn, eseStat);
      } else if (gsCmd_count == 0x00) {
        // Last command in the buffer
        if (gsIslastcmdLoad == false) {
          status =
              Process_EseResponse(pTranscv_Info, recvBufferActualSize, Os_info);
        } else if ((recvBufferActualSize == 0x02) &&
                   (pTranscv_Info->sRecvData[recvBufferActualSize - 2] ==
                    0x90) &&
                   (pTranscv_Info->sRecvData[recvBufferActualSize - 1] ==
                    0x00)) {
          recvBufferActualSize = 0x03;
          pTranscv_Info->sRecvData[0] = 0x00;
          pTranscv_Info->sRecvData[1] = 0x90;
          pTranscv_Info->sRecvData[2] = 0x00;
          status =
              Process_EseResponse(pTranscv_Info, recvBufferActualSize, Os_info);
        } else {
          status =
              Process_EseResponse(pTranscv_Info, recvBufferActualSize, Os_info);
        }
      } else if ((recvBufferActualSize == 0x02) &&
                 (pTranscv_Info->sRecvData[0] == 0x90) &&
                 (pTranscv_Info->sRecvData[1] == 0x00)) {
        /*response ok without data, send next command in the buffer*/
      } else if ((recvBufferActualSize == 0x03) &&
                 (pTranscv_Info->sRecvData[0] == 0x00) &&
                 (pTranscv_Info->sRecvData[1] == 0x90) &&
                 (pTranscv_Info->sRecvData[2] == 0x00)) {
        /*response ok without data, send next command in the buffer*/
      } else if ((pTranscv_Info->sRecvData[recvBufferActualSize - 2] != 0x90) &&
                 (pTranscv_Info->sRecvData[recvBufferActualSize - 1] != 0x00)) {
        /*Error condition hence exiting the loop*/
        status =
            Process_EseResponse(pTranscv_Info, recvBufferActualSize, Os_info);
        /*If the sending of Load fails reset the count*/
        gsCmd_count = 0;
        break;
      }
    }
  }
  memset(gsCmd_Buffer, 0, sizeof(gsCmd_Buffer));
  gspBuffer = gsCmd_Buffer;  // point back to start of line
  gsCmd_count = 0x00;
  ALOGD_IF(ese_debug_enabled, "%s: exit: status=0x%x", fn, status);
  return status;
}
/*******************************************************************************
**
** Function:        Numof_lengthbytes
**
** Description:     Checks the number of length bytes and assigns
**                  length value to wLen.
**
** Returns:         Number of Length bytes
**
*******************************************************************************/
uint8_t Numof_lengthbytes(uint8_t* read_buf, int32_t* pLen) {
  static const char fn[] = "Numof_lengthbytes";
  uint8_t len_byte = 0;
  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  if (read_buf[0] == 0x00) {
    ALOGE("%s: Invalid length zero", fn);
    len_byte = 0x00;
  } else if ((read_buf[0] & 0x80) == 0x80) {
    len_byte = read_buf[0] & 0x0F;
    len_byte = len_byte + 1;  // 1 byte added for byte 0x81
  } else {
    len_byte = 0x01;
  }

  /* To get the length of the value field */
  int32_t wLen = 0;
  switch (len_byte) {
    case 0:
      wLen = read_buf[0];
      break;
    case 1:
      /*1st byte is the length*/
      wLen = read_buf[0];
      break;
    case 2:
      /*2nd byte is the length*/
      wLen = read_buf[1];
      break;
    case 3:
      /*1st and 2nd bytes are length*/
      wLen = read_buf[1];
      wLen = ((wLen << 8) | (read_buf[2]));
      break;
    case 4:
      /*3bytes are the length*/
      wLen = read_buf[1];
      wLen = ((wLen << 16) | (read_buf[2] << 8));
      wLen = (wLen | (read_buf[3]));
      break;
    default:
      ALOGE("%s: Invalid length %d.", fn, len_byte);
      break;
  }

  *pLen = wLen;
  ALOGD_IF(ese_debug_enabled, "%s: exit; len_bytes=0x0%x, Length=%d", fn,
           len_byte, *pLen);
  return len_byte;
}

/*******************************************************************************
**
** Function:        Write_Response_To_OutFile
**
** Description:     Write the response to Out file
**                  with length recvlen from buffer RecvData.
**
** Returns:         Success if OK
**
*******************************************************************************/
LSCSTATUS Write_Response_To_OutFile(Lsc_ImageInfo_t* image_info,
                                    uint8_t* RecvData, int32_t recvlen,
                                    Ls_TagType tType) {
  static const char fn[] = "Write_Response_to_OutFile";

  ALOGD_IF(ese_debug_enabled, "%s: Enter", fn);
  /*If the Response out file is NULL or Other than LS commands*/
  if ((image_info->bytes_wrote == 0x55) || (tType == LS_Default)) {
    return LSCSTATUS_SUCCESS;
  }

  uint8_t tag43Len = 1;
  /*Certificate TAG occupies 2 bytes*/
  if (tType == LS_Cert) {
    tag43Len = 2;
  }

  /* |TAG|LEN|                      VAL                      |
   * |61 |XX |TAG|LEN|    VAL   |TAG|    LEN    |     VAL    |
   *         |43 |1/2|7F21/60/40|44 |apduRespLen|apduResponse|
   */
  int32_t tag44Len = 0;
  uint8_t ucTag44[3] = {0x00, 0x00, 0x00};
  int32_t tag61Len = 0;
  uint8_t tag43off = 0;
  uint8_t tag44off = 0;
  uint8_t tagLen = 0;
  uint8_t tagBuffer[12] = {0x61, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  if (recvlen < 0x80) {
    tag44Len = 1;
    ucTag44[0] = recvlen;
    tag61Len = recvlen + 4 + tag43Len;

    if (tag61Len & 0x80) {
      tagBuffer[1] = 0x81;
      tagBuffer[2] = tag61Len;
      tag43off = 3;
      tag44off = 5 + tag43Len;
      tagLen = tag44off + 2;
    } else {
      tagBuffer[1] = tag61Len;
      tag43off = 2;
      tag44off = 4 + tag43Len;
      tagLen = tag44off + 2;
    }
  } else if ((recvlen >= 0x80) && (recvlen <= 0xFF)) {
    ucTag44[0] = 0x81;
    ucTag44[1] = recvlen;
    tag61Len = recvlen + 5 + tag43Len;
    tag44Len = 2;

    if ((tag61Len & 0xFF00) != 0) {
      tagBuffer[1] = 0x82;
      tagBuffer[2] = (tag61Len & 0xFF00) >> 8;
      tagBuffer[3] = (tag61Len & 0xFF);
      tag43off = 4;
      tag44off = 6 + tag43Len;
      tagLen = tag44off + 3;
    } else {
      tagBuffer[1] = 0x81;
      tagBuffer[2] = (tag61Len & 0xFF);
      tag43off = 3;
      tag44off = 5 + tag43Len;
      tagLen = tag44off + 3;
    }
  } else if ((recvlen > 0xFF) && (recvlen <= 0xFFFF)) {
    ucTag44[0] = 0x82;
    ucTag44[1] = (recvlen & 0xFF00) >> 8;
    ucTag44[2] = (recvlen & 0xFF);
    tag44Len = 3;

    tag61Len = recvlen + 6 + tag43Len;

    if ((tag61Len & 0xFF00) != 0) {
      tagBuffer[1] = 0x82;
      tagBuffer[2] = (tag61Len & 0xFF00) >> 8;
      tagBuffer[3] = (tag61Len & 0xFF);
      tag43off = 4;
      tag44off = 6 + tag43Len;
      tagLen = tag44off + 4;
    }
  }
  tagBuffer[tag43off] = 0x43;
  tagBuffer[tag43off + 1] = tag43Len;
  tagBuffer[tag44off] = 0x44;
  memcpy(&tagBuffer[tag44off + 1], &ucTag44[0], tag44Len);

  if (tType == LS_Cert) {
    tagBuffer[tag43off + 2] = 0x7F;
    tagBuffer[tag43off + 3] = 0x21;
  } else if (tType == LS_Sign) {
    tagBuffer[tag43off + 2] = 0x60;
  } else if (tType == LS_Comm) {
    tagBuffer[tag43off + 2] = 0x40;
  } else {
    /*Do nothing*/
  }

  uint8_t tempLen = 0;
  LSCSTATUS wStatus = LSCSTATUS_FAILED;
  int32_t status = 0;
  while (tempLen < tagLen) {
    status = fprintf(image_info->fResp, "%02X", tagBuffer[tempLen++]);
    if (status != 2) {
      ALOGE("%s: Invalid Response during fprintf; status=0x%x", fn, (status));
      wStatus = LSCSTATUS_FAILED;
      break;
    }
  }
  /*Updating the response data into out script*/
  int32_t respLen = 0;
  while (respLen < recvlen) {
    status = fprintf(image_info->fResp, "%02X", RecvData[respLen++]);
    if (status != 2) {
      ALOGE("%s: Invalid Response during fprintf; status=0x%x", fn, (status));
      wStatus = LSCSTATUS_FAILED;
      break;
    }
  }
  if (status == 2) {
    fprintf(image_info->fResp, "%s\n", "");
    ALOGD_IF(ese_debug_enabled,
             "%s: SUCCESS Response written to script out file", fn);
    wStatus = LSCSTATUS_SUCCESS;
  }
  fflush(image_info->fResp);
  return wStatus;
}

/*******************************************************************************
**
** Function:        Check_Certificate_Tag
**
** Description:     Check certificate Tag presence in script
**                  by 7F21 .
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_Certificate_Tag(uint8_t* read_buf, uint16_t* offset1) {
  static const char fn[] = "Check_Certificate_Tag";
  uint16_t offset = *offset1;

  if (((read_buf[offset] << 8 | read_buf[offset + 1]) == TAG_CERTIFICATE)) {
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_CERTIFICATE", fn);
    int32_t wLen;
    offset = offset + 2;
    uint16_t len_byte = Numof_lengthbytes(&read_buf[offset], &wLen);
    offset = offset + len_byte;
    *offset1 = offset;
    if (wLen <= MAX_CERT_LEN) return LSCSTATUS_SUCCESS;
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Check_SerialNo_Tag
**
** Description:     Check Serial number Tag presence in script
**                  by 0x93 .
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_SerialNo_Tag(uint8_t* read_buf, uint16_t* offset1) {
  static const char fn[] = "Check_SerialNo_Tag";
  uint16_t offset = *offset1;

  if (read_buf[offset] == TAG_SERIAL_NO) {
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_SERIAL_NO", fn);
    uint8_t serNoLen = read_buf[offset + 1];
    offset = offset + serNoLen + 2;
    *offset1 = offset;
    ALOGD_IF(ese_debug_enabled, "%s: TAG_LSROOT_ENTITY is %x", fn,
             read_buf[offset]);
    return LSCSTATUS_SUCCESS;
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Check_LSRootID_Tag
**
** Description:     Check LS root ID tag presence in script and compare with
**                  select response root ID value.
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_LSRootID_Tag(uint8_t* read_buf, uint16_t* offset1) {
  static const char fn[] = "Check_LSRootID_Tag";
  uint16_t offset = *offset1;

  if (read_buf[offset] == TAG_LSRE_ID) {
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_LSROOT_ENTITY", fn);
    if (gsTag42Arr[0] == read_buf[offset + 1]) {
      uint8_t tag42Len = read_buf[offset + 1];
      offset = offset + 2;
      if (!memcmp(&read_buf[offset], &gsTag42Arr[1], gsTag42Arr[0])) {
        ALOGD_IF(ese_debug_enabled, "%s : TAG 42 verified", fn);
        offset = offset + tag42Len;
        *offset1 = offset;
        return LSCSTATUS_SUCCESS;
      }
    }
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Check_CertHoldID_Tag
**
** Description:     Check certificate holder ID tag presence in script.
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_CertHoldID_Tag(uint8_t* read_buf, uint16_t* offset1) {
  static const char fn[] = "Check_CertHoldID_Tag";
  uint16_t offset = *offset1;

  if ((read_buf[offset] << 8 | read_buf[offset + 1]) == TAG_CERTFHOLD_ID) {
    uint8_t certfHoldIDLen = 0;
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_CERTFHOLD_ID", fn);
    certfHoldIDLen = read_buf[offset + 2];
    offset = offset + certfHoldIDLen + 3;
    if (read_buf[offset] == TAG_KEY_USAGE) {
      ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_KEY_USAGE", fn);
      uint8_t keyusgLen = read_buf[offset + 1];
      offset = offset + keyusgLen + 2;
      *offset1 = offset;
      return LSCSTATUS_SUCCESS;
    }
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Check_Date_Tag
**
** Description:     Check date tags presence in script.
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_Date_Tag(uint8_t* read_buf, uint16_t* offset1) {
  static const char fn[] = "Check_Date_Tag";
  LSCSTATUS status = LSCSTATUS_FAILED;
  uint16_t offset = *offset1;

  if ((read_buf[offset] << 8 | read_buf[offset + 1]) == TAG_EFF_DATE) {
    uint8_t effDateLen = read_buf[offset + 2];
    offset = offset + 3 + effDateLen;
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_EFF_DATE", fn);
    if ((read_buf[offset] << 8 | read_buf[offset + 1]) == TAG_EXP_DATE) {
      uint8_t effExpLen = read_buf[offset + 2];
      offset = offset + 3 + effExpLen;
      ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_EXP_DATE", fn);
      status = LSCSTATUS_SUCCESS;
    } else if (read_buf[offset] == TAG_LSRE_SIGNID) {
      status = LSCSTATUS_SUCCESS;
    }
  } else if ((read_buf[offset] << 8 | read_buf[offset + 1]) == TAG_EXP_DATE) {
    uint8_t effExpLen = read_buf[offset + 2];
    offset = offset + 3 + effExpLen;
    ALOGD_IF(ese_debug_enabled, "%s: TAGID: TAG_EXP_DATE", fn);
    status = LSCSTATUS_SUCCESS;
  } else if (read_buf[offset] == TAG_LSRE_SIGNID) {
    status = LSCSTATUS_SUCCESS;
  } else {
    /*LSCSTATUS_FAILED*/
  }
  *offset1 = offset;
  return status;
}

/*******************************************************************************
**
** Function:        Check_45_Tag
**
** Description:     Check 45 tags presence in script and compare the value
**                  with select response tag 45 value
**
** Returns:         Success if Tag found
**
*******************************************************************************/
LSCSTATUS Check_45_Tag(uint8_t* read_buf, uint16_t* offset1,
                       uint8_t* tag45Len) {
  static const char fn[] = "Check_45_Tag";
  uint16_t offset = *offset1;
  if (read_buf[offset] == TAG_LSRE_SIGNID) {
    *tag45Len = read_buf[offset + 1];
    offset = offset + 2;
    if (gsTag45Arr[0] == *tag45Len) {
      if (!memcmp(&read_buf[offset], &gsTag45Arr[1], gsTag45Arr[0])) {
        *offset1 = offset;
        ALOGD_IF(ese_debug_enabled,
                 "%s: LSC_Check_KeyIdentifier : TAG 45 verified", fn);
        return LSCSTATUS_SUCCESS;
      }
    }
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Certificate_Verification
**
** Description:     Perform the certificate verification by forwarding it to
**                  LS applet.
**
** Returns:         Success if certificate is verified
**
*******************************************************************************/
LSCSTATUS Certificate_Verification(Lsc_ImageInfo_t* Os_info,
                                   Lsc_TranscieveInfo_t* pTranscv_Info,
                                   uint8_t* read_buf, uint16_t* offset1,
                                   uint8_t* tag45Len) {
  static const char fn[] = "Certificate_Verification";

  pTranscv_Info->sSendData[0] = 0x80;
  pTranscv_Info->sSendData[1] = 0xA0;
  pTranscv_Info->sSendData[2] = 0x01;
  pTranscv_Info->sSendData[3] = 0x00;

  int32_t wCertfLen = (read_buf[2] << 8 | read_buf[3]);
  uint16_t offset = *offset1;
  /*If the certificate is less than 255 bytes*/
  if (wCertfLen <= 251) {
    uint8_t tag7f49Off = 0;
    uint8_t u7f49Len = 0;
    uint8_t tag5f37Len = 0;
    ALOGD_IF(ese_debug_enabled, "%s: Certificate is less than 255", fn);
    offset = offset + *tag45Len;
    ALOGD_IF(ese_debug_enabled, "%s: Before TAG_CCM_PERMISSION = %x", fn,
             read_buf[offset]);
    if (read_buf[offset] != TAG_CCM_PERMISSION) {
      return LSCSTATUS_FAILED;
    }
    int32_t tag53Len = 0;
    uint8_t len_byte = 0;
    offset = offset + 1;
    len_byte = Numof_lengthbytes(&read_buf[offset], &tag53Len);
    offset = offset + tag53Len + len_byte;
    ALOGD_IF(ese_debug_enabled, "%s: Verified TAG TAG_CCM_PERMISSION = 0x53",
             fn);
    if ((uint16_t)(read_buf[offset] << 8 | read_buf[offset + 1]) !=
        TAG_SIG_RNS_COMP) {
      return LSCSTATUS_FAILED;
    }
    tag7f49Off = offset;
    u7f49Len = read_buf[offset + 2];
    offset = offset + 3 + u7f49Len;
    if (u7f49Len != 64) {
      return LSCSTATUS_FAILED;
    }
    if ((uint16_t)(read_buf[offset] << 8 | read_buf[offset + 1]) != 0x7f49) {
      return LSCSTATUS_FAILED;
    }
    tag5f37Len = read_buf[offset + 2];
    if (read_buf[offset + 3] != 0x86 || (read_buf[offset + 4] != 65)) {
      return LSCSTATUS_FAILED;
    }
    uint8_t tag_len_byte = Numof_lengthbytes(&read_buf[2], &wCertfLen);
    pTranscv_Info->sSendData[4] = wCertfLen + 2 + tag_len_byte;
    pTranscv_Info->sSendlength = wCertfLen + 7 + tag_len_byte;
    memcpy(&(pTranscv_Info->sSendData[5]), &read_buf[0],
           wCertfLen + 2 + tag_len_byte);

    ALOGD_IF(ese_debug_enabled, "%s: start transceive for length %d", fn,
             pTranscv_Info->sSendlength);
    LSCSTATUS status = LSCSTATUS_FAILED;
    status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Cert);
    if (status == LSCSTATUS_SUCCESS) {
      ALOGD_IF(ese_debug_enabled, "%s: Certificate is verified", fn);
    }
    return status;
  } else {
    /*If the certificate is more than 255 bytes*/
    uint8_t tag7f49Off = 0;
    uint8_t u7f49Len = 0;
    uint8_t tag5f37Len = 0;
    ALOGD_IF(ese_debug_enabled, "%s: Certificate is greater than 255", fn);
    offset = offset + *tag45Len;
    ALOGD_IF(ese_debug_enabled, "%s: Before TAG_CCM_PERMISSION = %x", fn,
             read_buf[offset]);
    if (read_buf[offset] != TAG_CCM_PERMISSION) {
      return LSCSTATUS_FAILED;
    }
    int32_t tag53Len = 0;
    uint8_t len_byte = 0;
    offset = offset + 1;
    len_byte = Numof_lengthbytes(&read_buf[offset], &tag53Len);
    offset = offset + tag53Len + len_byte;
    ALOGD_IF(ese_debug_enabled, "%s: Verified TAG TAG_CCM_PERMISSION = 0x53",
             fn);
    if ((uint16_t)(read_buf[offset] << 8 | read_buf[offset + 1]) !=
        TAG_SIG_RNS_COMP) {
      return LSCSTATUS_FAILED;
    }
    tag7f49Off = offset;
    u7f49Len = read_buf[offset + 2];
    offset = offset + 3 + u7f49Len;
    if (u7f49Len != 64) {
      return LSCSTATUS_FAILED;
    }
    if ((uint16_t)(read_buf[offset] << 8 | read_buf[offset + 1]) != 0x7f49) {
      return LSCSTATUS_FAILED;
    }
    tag5f37Len = read_buf[offset + 2];
    if (read_buf[offset + 3] != 0x86 || (read_buf[offset + 4] != 65)) {
      return LSCSTATUS_FAILED;
    }
    pTranscv_Info->sSendData[4] = tag7f49Off;
    memcpy(&(pTranscv_Info->sSendData[5]), &read_buf[0], tag7f49Off);
    pTranscv_Info->sSendlength = tag7f49Off + 5;
    ALOGD_IF(ese_debug_enabled, "%s: start transceive for length %d", fn,
             pTranscv_Info->sSendlength);

    LSCSTATUS status = LSCSTATUS_FAILED;
    status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Default);
    if (status != LSCSTATUS_SUCCESS) {
      uint8_t* RecvData = pTranscv_Info->sRecvData;
      Write_Response_To_OutFile(Os_info, RecvData, gsResp_len, LS_Cert);
      return status;
    }

    pTranscv_Info->sSendData[2] = 0x00;
    pTranscv_Info->sSendData[4] = u7f49Len + tag5f37Len + 6;
    memcpy(&(pTranscv_Info->sSendData[5]), &read_buf[tag7f49Off],
           u7f49Len + tag5f37Len + 6);
    pTranscv_Info->sSendlength = u7f49Len + tag5f37Len + 11;
    ALOGD_IF(ese_debug_enabled, "%s: start transceive for length %d", fn,
             pTranscv_Info->sSendlength);

    status = LSC_SendtoLsc(Os_info, status, pTranscv_Info, LS_Cert);
    if (status == LSCSTATUS_SUCCESS) {
      ALOGD_IF(ese_debug_enabled, "Certificate is verified");
    }
    return status;
  }
  return LSCSTATUS_FAILED;
}

/*******************************************************************************
**
** Function:        Check_Complete_7F21_Tag
**
** Description:     Traverses the 7F21 tag for verification of each sub tag with
**                  in the 7F21 tag.
**
** Returns:         Success if all tags are verified
**
*******************************************************************************/
LSCSTATUS Check_Complete_7F21_Tag(Lsc_ImageInfo_t* Os_info,
                                  Lsc_TranscieveInfo_t* pTranscv_Info,
                                  uint8_t* read_buf, uint16_t* offset) {
  static const char fn[] = "Check_Complete_7F21_Tag";

  if (LSCSTATUS_SUCCESS != Check_Certificate_Tag(read_buf, offset)) {
    ALOGE("%s: FAILED in Check_Certificate_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  if (LSCSTATUS_SUCCESS != Check_SerialNo_Tag(read_buf, offset)) {
    ALOGE("%s: FAILED in Check_SerialNo_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  if (LSCSTATUS_SUCCESS != Check_LSRootID_Tag(read_buf, offset)) {
    ALOGE("%s: FAILED in Check_LSRootID_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  if (LSCSTATUS_SUCCESS != Check_CertHoldID_Tag(read_buf, offset)) {
    ALOGE("%s: FAILED in Check_CertHoldID_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  if (LSCSTATUS_SUCCESS != Check_Date_Tag(read_buf, offset)) {
    ALOGE("%s: FAILED in Check_CertHoldID_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  uint8_t tag45Len = 0;
  if (LSCSTATUS_SUCCESS != Check_45_Tag(read_buf, offset, &tag45Len)) {
    ALOGE("%s: FAILED in Check_CertHoldID_Tag", fn);
    return LSCSTATUS_FAILED;
  }
  if (LSCSTATUS_SUCCESS != Certificate_Verification(Os_info, pTranscv_Info,
                                                    read_buf, offset,
                                                    &tag45Len)) {
    ALOGE("%s: FAILED in Certificate_Verification", fn);
    return LSCSTATUS_FAILED;
  }
  return LSCSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function:        LSC_UpdateExeStatus
**
** Description:     Updates LSC status to a file
**
** Returns:         true if success else false
**
*******************************************************************************/
bool LSC_UpdateExeStatus(uint16_t status) {
  static const char fn[] = "LSC_UpdateExeStatus";

  ALOGD_IF(ese_debug_enabled, "%s: enter", fn);

  FILE* fLsStatus = fopen(LS_STATUS_PATH, "w+");
  if (fLsStatus == NULL) {
    ALOGE("%s: Error opening LS Status file for backup: %s", fn,
          strerror(errno));
    return false;
  }
  if ((fprintf(fLsStatus, "%04x", status)) != 4) {
    ALOGE("%s: Error updating LS Status backup: %s", fn, strerror(errno));
    fclose(fLsStatus);
    return false;
  }
  fclose(fLsStatus);
  ALOGD_IF(ese_debug_enabled, "%s: exit", fn);
  return true;
}

/*******************************************************************************
**
** Function:        Get_LsStatus
**
** Description:     Interface to fetch Loader service client status to JNI,
**                  Services
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS Get_LsStatus(uint8_t* pStatus) {
  static const char fn[] = "Get_LsStatus";

  FILE* fLsStatus = fopen(LS_STATUS_PATH, "r");
  if (fLsStatus == NULL) {
    ALOGE("%s: Error opening LS Status file for backup: %s", fn,
          strerror(errno));
    return LSCSTATUS_FAILED;
  }

  uint8_t lsStatus[2] = {0x63, 0x40};
  for (uint8_t loopcnt = 0; loopcnt < 2; loopcnt++) {
    if ((FSCANF_BYTE(fLsStatus, "%2x", &lsStatus[loopcnt])) == 0) {
      ALOGE("%s: Error updating LS Status backup: %s", fn, strerror(errno));
      fclose(fLsStatus);
      return LSCSTATUS_FAILED;
    }
  }
  ALOGD_IF(ese_debug_enabled, "%s: LS Status 0x%X 0x%X", fn, lsStatus[0],
           lsStatus[1]);
  memcpy(pStatus, lsStatus, 2);
  fclose(fLsStatus);
  return LSCSTATUS_SUCCESS;
}

/*******************************************************************************
**
** Function:        LSC_CloseAllLogicalChannels
**
** Description:     Close all opened logical channels
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS LSC_CloseAllLogicalChannels(Lsc_ImageInfo_t* Os_info) {
  ESESTATUS status = ESESTATUS_FAILED;
  LSCSTATUS lsStatus = LSCSTATUS_FAILED;
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;

  ALOGD_IF(ese_debug_enabled, "%s: Enter", __func__);
  for (uint8_t channelNumber = 0x01; channelNumber < 0x04; channelNumber++) {
    if (channelNumber == Os_info->initChannelNum) continue;
    phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
    phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
    cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(5 * sizeof(uint8_t));
    if (cmdApdu.p_data != NULL) {
      uint8_t xx = 0;

      cmdApdu.p_data[xx++] = channelNumber;
      cmdApdu.p_data[xx++] = 0x70;           // INS
      cmdApdu.p_data[xx++] = 0x80;           // P1
      cmdApdu.p_data[xx++] = channelNumber;  // P2
      cmdApdu.p_data[xx++] = 0x00;           // Lc
      cmdApdu.len = xx;

      status = phNxpEse_Transceive(&cmdApdu, &rspApdu);
    }
    if (status != ESESTATUS_SUCCESS) {
      lsStatus = LSCSTATUS_FAILED;
    } else if ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
               (rspApdu.p_data[rspApdu.len - 1] == 0x00)) {
      lsStatus = LSCSTATUS_SUCCESS;
    } else {
      lsStatus = LSCSTATUS_FAILED;
    }

    phNxpEse_free(cmdApdu.p_data);
    phNxpEse_free(rspApdu.p_data);
  }
  return lsStatus;
}

/*******************************************************************************
**
** Function:        LSC_SelectLsHash
**
** Description:     Selects LS Hash applet
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS LSC_SelectLsHash() {
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  LSCSTATUS lsStatus = LSCSTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "%s: Enter ", __func__);
  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

  cmdApdu.len = (int32_t)(sizeof(SelectLscSlotHash));
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  memcpy(cmdApdu.p_data, SelectLscSlotHash, sizeof(SelectLscSlotHash));

  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if ((eseStat != ESESTATUS_SUCCESS) ||
      ((rspApdu.p_data[rspApdu.len - 2] != 0x90) &&
       (rspApdu.p_data[rspApdu.len - 1] != 0x00))) {
    lsStatus = LSCSTATUS_FAILED;
  } else {
    lsStatus = LSCSTATUS_SUCCESS;
  }

  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  return lsStatus;
}
/*******************************************************************************
**
** Function:        LSC_ReadLsHash
**
** Description:     Read the LS SHA1 for the intended slot
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS LSC_ReadLsHash(uint8_t* hash, uint16_t* readHashLen, uint8_t slotId) {
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  LSCSTATUS lsStatus = LSCSTATUS_FAILED;

  lsStatus = LSC_SelectLsHash();
  if (lsStatus != LSCSTATUS_SUCCESS) {
    return lsStatus;
  }

  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(5 * sizeof(uint8_t));

  if (cmdApdu.p_data != NULL) {
    uint8_t xx = 0;
    cmdApdu.p_data[xx++] = 0x80;    // CLA
    cmdApdu.p_data[xx++] = 0x02;    // INS
    cmdApdu.p_data[xx++] = slotId;  // P1
    cmdApdu.p_data[xx++] = 0x00;    // P2
    cmdApdu.len = xx;

    ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

    if ((eseStat == ESESTATUS_SUCCESS) &&
        ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
         (rspApdu.p_data[rspApdu.len - 1] == 0x00))) {
      ALOGD_IF(ese_debug_enabled, "%s: rspApdu.len : %u", __func__,
               rspApdu.len);
      *readHashLen = rspApdu.len - 2;
      if (*readHashLen <= HASH_DATA_LENGTH) {
        memcpy(hash, rspApdu.p_data, *readHashLen);
        lsStatus = LSCSTATUS_SUCCESS;
      } else {
        ALOGE("%s:Invalid LS HASH data received", __func__);
        lsStatus = LSCSTATUS_FAILED;
      }
    } else {
      if ((rspApdu.p_data[rspApdu.len - 2] == 0x6A) &&
          (rspApdu.p_data[rspApdu.len - 1] == 0x86)) {
        ALOGD_IF(ese_debug_enabled, "%s: slot id is invalid", __func__);
        lsStatus = LSCSTATUS_HASH_SLOT_INVALID;
      } else if ((rspApdu.p_data[rspApdu.len - 2] == 0x6A) &&
                 (rspApdu.p_data[rspApdu.len - 1] == 0x83)) {
        ALOGD_IF(ese_debug_enabled, "%s: slot is empty", __func__);
        lsStatus = LSCSTATUS_HASH_SLOT_EMPTY;
      } else {
        lsStatus = LSCSTATUS_FAILED;
      }
    }
    phNxpEse_free(cmdApdu.p_data);
    phNxpEse_free(rspApdu.p_data);
  }
  return lsStatus;
}

/*******************************************************************************
**
** Function:        LSC_UpdateLsHash
**
** Description:     Updates the SHA1 for the intended slot
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS LSC_UpdateLsHash(uint8_t* hash, long hashLen, uint8_t slotId) {
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  LSCSTATUS lsStatus = LSCSTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "%s: Enter ", __func__);

  lsStatus = LSC_SelectLsHash();
  if (lsStatus != LSCSTATUS_SUCCESS) {
    return lsStatus;
  }

  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

  cmdApdu.len = (int32_t)(5 + hashLen);
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));

  if (cmdApdu.p_data != NULL) {
    uint8_t xx = 0;
    cmdApdu.p_data[xx++] = 0x80;
    cmdApdu.p_data[xx++] = 0x01;     // INS
    cmdApdu.p_data[xx++] = slotId;   // P1
    cmdApdu.p_data[xx++] = 0x00;     // P2
    cmdApdu.p_data[xx++] = hashLen;  // Lc
    memcpy(&cmdApdu.p_data[xx], hash, hashLen);

    ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

    if ((eseStat == ESESTATUS_SUCCESS) &&
        ((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
         (rspApdu.p_data[rspApdu.len - 1] == 0x00))) {
      lsStatus = LSCSTATUS_SUCCESS;
    } else {
      if ((rspApdu.p_data[rspApdu.len - 2] == 0x6A) &&
          (rspApdu.p_data[rspApdu.len - 1] == 0x86)) {
        ALOGD_IF(ese_debug_enabled, "%s: if slot id is invalid", __func__);
      }
      lsStatus = LSCSTATUS_FAILED;
    }
  }

  ALOGD_IF(ese_debug_enabled, "%s: Exit ", __func__);
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  return lsStatus;
}

/*******************************************************************************
**
** Function:        LSC_ReadLscInfo
**
** Description:     Read the state of LS applet
**
** Returns:         SUCCESS/FAILURE
**
*******************************************************************************/
LSCSTATUS LSC_ReadLscInfo(uint8_t* state, uint16_t* version) {
  static const char fn[] = "LSC_ReadLscInfo";
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;
  LSCSTATUS status = LSCSTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "%s: Enter ", __func__);

  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

  /*p_data will have channel_id (1 byte) + SelectLsc APDU*/
  cmdApdu.len = (int32_t)(sizeof(SelectLsc) + 1);
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(cmdApdu.len * sizeof(uint8_t));
  cmdApdu.p_data[0] = 0x00;  // fchannel 0

  memcpy(&(cmdApdu.p_data[1]), SelectLsc, sizeof(SelectLsc));

  ALOGD_IF(ese_debug_enabled, "%s: Selecting Loader service applet", fn);

  ESESTATUS eseStat = phNxpEse_Transceive(&cmdApdu, &rspApdu);

  if (eseStat != ESESTATUS_SUCCESS && (rspApdu.len == 0x00)) {
    status = LSCSTATUS_FAILED;
    ALOGE("%s: SE transceive failed status = 0x%X", fn, status);
  } else if (((rspApdu.p_data[rspApdu.len - 2] == 0x90) &&
              (rspApdu.p_data[rspApdu.len - 1] == 0x00))) {
    status = Process_SelectRsp(rspApdu.p_data, (rspApdu.len - 2));
    if (status != LSCSTATUS_SUCCESS) {
      ALOGE("%s: Select Lsc Rsp doesnt have a valid key; status = 0x%X", fn,
            status);
    } else {
      *state = rspApdu.p_data[18];
      *version = (rspApdu.p_data[22] << 8) | rspApdu.p_data[23];
    }
  } else if (rspApdu.p_data[rspApdu.len - 2] != 0x90) {
    ALOGE("%s: Selecting Loader service applet failed", fn);
    status = LSCSTATUS_FAILED;
  }

  ALOGD_IF(ese_debug_enabled, "%s: Exit ", __func__);
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);
  return status;
}
