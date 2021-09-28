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
#define LOG_TAG "NxpEseHal"
#include <log/log.h>
#include <phNxpEseProto7816_3.h>

extern bool ese_debug_enabled;

/******************************************************************************
\section Introduction Introduction

 * This module provide the 7816-3 protocol level implementation for ESE
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_ResetProtoParams(void);
static ESESTATUS phNxpEseProto7816_SendRawFrame(uint32_t data_len,
                                                uint8_t* p_data);
static ESESTATUS phNxpEseProto7816_GetRawFrame(uint32_t* data_len,
                                               uint8_t** pp_data);
static uint8_t phNxpEseProto7816_ComputeLRC(unsigned char* p_buff,
                                            uint32_t offset, uint32_t length);
static ESESTATUS phNxpEseProto7816_CheckLRC(uint32_t data_len, uint8_t* p_data);
static ESESTATUS phNxpEseProto7816_SendSFrame(sFrameInfo_t sFrameData);
static ESESTATUS phNxpEseProto7816_SendIframe(iFrameInfo_t iFrameData);
static ESESTATUS phNxpEseProto7816_sendRframe(rFrameTypes_t rFrameType);
static ESESTATUS phNxpEseProto7816_SetFirstIframeContxt(void);
static ESESTATUS phNxpEseProto7816_SetNextIframeContxt(void);
static ESESTATUS phNxpEseProro7816_SaveIframeData(uint8_t* p_data,
                                                  uint32_t data_len);
static ESESTATUS phNxpEseProto7816_ResetRecovery(void);
static ESESTATUS phNxpEseProto7816_RecoverySteps(void);
static ESESTATUS phNxpEseProto7816_DecodeFrame(uint8_t* p_data,
                                               uint32_t data_len);
static ESESTATUS phNxpEseProto7816_ProcessResponse(void);
static ESESTATUS TransceiveProcess(void);
static ESESTATUS phNxpEseProto7816_RSync(void);
static ESESTATUS phNxpEseProto7816_ResetProtoParams(void);

/******************************************************************************
 * Function         phNxpEseProto7816_SendRawFrame
 *
 * Description      This internal function is called send the data to ESE
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_SendRawFrame(uint32_t data_len,
                                                uint8_t* p_data) {
  ESESTATUS status = ESESTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  status = phNxpEse_WriteFrame(data_len, p_data);
  if (ESESTATUS_SUCCESS != status) {
    ALOGE("%s Error phNxpEse_WriteFrame\n", __FUNCTION__);
  } else {
    ALOGD_IF(ese_debug_enabled, "%s phNxpEse_WriteFrame Success \n",
             __FUNCTION__);
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_GetRawFrame
 *
 * Description      This internal function is called read the data from the ESE
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_GetRawFrame(uint32_t* data_len,
                                               uint8_t** pp_data) {
  ESESTATUS status = ESESTATUS_FAILED;

  status = phNxpEse_read(data_len, pp_data);
  if (ESESTATUS_SUCCESS != status) {
    ALOGE("%s phNxpEse_read failed , status : 0x%x", __FUNCTION__, status);
  }
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ComputeLRC
 *
 * Description      This internal function is called compute the LRC
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static uint8_t phNxpEseProto7816_ComputeLRC(unsigned char* p_buff,
                                            uint32_t offset, uint32_t length) {
  uint32_t LRC = 0, i = 0;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  for (i = offset; i < length; i++) {
    LRC = LRC ^ p_buff[i];
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return (uint8_t)LRC;
}

/******************************************************************************
 * Function         phNxpEseProto7816_CheckLRC
 *
 * Description      This internal function is called compute and compare the
 *                  received LRC of the received data
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_CheckLRC(uint32_t data_len,
                                            uint8_t* p_data) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  uint8_t calc_crc = 0;
  uint8_t recv_crc = 0;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  recv_crc = p_data[data_len - 1];

  /* calculate the CRC after excluding CRC  */
  calc_crc = phNxpEseProto7816_ComputeLRC(p_data, 1, (data_len - 1));
  ALOGD_IF(ese_debug_enabled, "Received LRC:0x%x Calculated LRC:0x%x", recv_crc,
           calc_crc);
  if (recv_crc != calc_crc) {
    status = ESESTATUS_FAILED;
    ALOGE("%s LRC failed", __FUNCTION__);
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SendSFrame
 *
 * Description      This internal function is called to send S-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_SendSFrame(sFrameInfo_t sFrameData) {
  ESESTATUS status = ESESTATUS_FAILED;
  uint32_t frame_len = 0;
  uint8_t* p_framebuff = NULL;
  uint8_t pcb_byte = 0;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  sFrameInfo_t sframeData = sFrameData;
  /* This update is helpful in-case a R-NACK is transmitted from the MW */
  phNxpEseProto7816_3_Var.lastSentNonErrorframeType = SFRAME;
  switch (sframeData.sFrameType) {
    case RESYNCH_REQ:
      frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
      p_framebuff = (uint8_t*)phNxpEse_memalloc(frame_len * sizeof(uint8_t));
      if (NULL == p_framebuff) {
        return ESESTATUS_FAILED;
      }
      p_framebuff[2] = 0;
      p_framebuff[3] = 0x00;

      pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
      pcb_byte |= PH_PROTO_7816_S_RESYNCH;
      break;
    case INTF_RESET_REQ:
      frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
      p_framebuff = (uint8_t*)phNxpEse_memalloc(frame_len * sizeof(uint8_t));
      if (NULL == p_framebuff) {
        return ESESTATUS_FAILED;
      }
      p_framebuff[2] = 0;
      p_framebuff[3] = 0x00;

      pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
      pcb_byte |= PH_PROTO_7816_S_RESET;
      break;
    case PROP_END_APDU_REQ:
      frame_len = (PH_PROTO_7816_HEADER_LEN + PH_PROTO_7816_CRC_LEN);
      p_framebuff = (uint8_t*)phNxpEse_memalloc(frame_len * sizeof(uint8_t));
      if (NULL == p_framebuff) {
        return ESESTATUS_FAILED;
      }
      p_framebuff[2] = 0;
      p_framebuff[3] = 0x00;

      pcb_byte |= PH_PROTO_7816_S_BLOCK_REQ; /* PCB */
      pcb_byte |= PH_PROTO_7816_S_END_OF_APDU;
      break;
    case WTX_RSP:
      frame_len = (PH_PROTO_7816_HEADER_LEN + 1 + PH_PROTO_7816_CRC_LEN);
      p_framebuff = (uint8_t*)phNxpEse_memalloc(frame_len * sizeof(uint8_t));
      if (NULL == p_framebuff) {
        return ESESTATUS_FAILED;
      }
      p_framebuff[2] = 0x01;
      p_framebuff[3] = 0x01;

      pcb_byte |= PH_PROTO_7816_S_BLOCK_RSP;
      pcb_byte |= PH_PROTO_7816_S_WTX;
      break;
    default:
      ALOGE("Invalid S-block");
      break;
  }
  if (NULL != p_framebuff) {
    /* frame the packet */
    p_framebuff[0] = 0x00;     /* NAD Byte */
    p_framebuff[1] = pcb_byte; /* PCB */

    p_framebuff[frame_len - 1] =
        phNxpEseProto7816_ComputeLRC(p_framebuff, 0, (frame_len - 1));
    ALOGD_IF(ese_debug_enabled, "S-Frame PCB: %x\n", p_framebuff[1]);
    status = phNxpEseProto7816_SendRawFrame(frame_len, p_framebuff);
    phNxpEse_free(p_framebuff);
  } else {
    ALOGE("Invalid S-block or malloc for s-block failed");
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_sendRframe
 *
 * Description      This internal function is called to send R-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_sendRframe(rFrameTypes_t rFrameType) {
  ESESTATUS status = ESESTATUS_FAILED;
  uint8_t recv_ack[4] = {0x00, 0x80, 0x00, 0x00};
  if (RNACK == rFrameType) /* R-NACK */
  {
    recv_ack[1] = 0x82;
  } else /* R-ACK*/
  {
    /* This update is helpful in-case a R-NACK is transmitted from the MW */
    phNxpEseProto7816_3_Var.lastSentNonErrorframeType = RFRAME;
  }
  recv_ack[1] |=
      ((phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo ^ 1)
       << 4);
  ALOGD_IF(ese_debug_enabled, "%s recv_ack[1]:0x%x", __FUNCTION__, recv_ack[1]);
  recv_ack[3] =
      phNxpEseProto7816_ComputeLRC(recv_ack, 0x00, (sizeof(recv_ack) - 1));
  status = phNxpEseProto7816_SendRawFrame(sizeof(recv_ack), recv_ack);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SendIframe
 *
 * Description      This internal function is called to send I-frame with all
 *                   updated 7816-3 headers
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_SendIframe(iFrameInfo_t iFrameData) {
  ESESTATUS status = ESESTATUS_FAILED;
  uint32_t frame_len = 0;
  uint8_t* p_framebuff = NULL;
  uint8_t pcb_byte = 0;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  if (0 == iFrameData.sendDataLen) {
    ALOGE("I frame Len is 0, INVALID");
    return ESESTATUS_FAILED;
  }
  /* This update is helpful in-case a R-NACK is transmitted from the MW */
  phNxpEseProto7816_3_Var.lastSentNonErrorframeType = IFRAME;
  frame_len = (iFrameData.sendDataLen + PH_PROTO_7816_HEADER_LEN +
               PH_PROTO_7816_CRC_LEN);

  p_framebuff = (uint8_t*)phNxpEse_memalloc(frame_len * sizeof(uint8_t));
  if (NULL == p_framebuff) {
    ALOGE("Heap allocation failed");
    return ESESTATUS_FAILED;
  }

  /* frame the packet */
  p_framebuff[0] = 0x00; /* NAD Byte */

  if (iFrameData.isChained) {
    /* make B6 (M) bit high */
    pcb_byte |= PH_PROTO_7816_CHAINING;
  }

  /* Update the send seq no */
  pcb_byte |=
      (phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo << 6);

  /* store the pcb byte */
  p_framebuff[1] = pcb_byte;
  /* store I frame length */
  p_framebuff[2] = iFrameData.sendDataLen;
  /* store I frame */
  phNxpEse_memcpy(&(p_framebuff[3]), iFrameData.p_data + iFrameData.dataOffset,
                  iFrameData.sendDataLen);

  p_framebuff[frame_len - 1] =
      phNxpEseProto7816_ComputeLRC(p_framebuff, 0, (frame_len - 1));

  status = phNxpEseProto7816_SendRawFrame(frame_len, p_framebuff);

  phNxpEse_free(p_framebuff);
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetNextIframeContxt
 *
 * Description      This internal function is called to set the context for next
 *I-frame.
 *                  Not applicable for the first I-frame of the transceive
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_SetFirstIframeContxt(void) {
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.dataOffset = 0;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo =
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo ^ 1;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;
  if (phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen >
      phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen) {
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = true;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen =
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen =
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen -
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen;
  } else {
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen =
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = false;
  }
  ALOGD_IF(ese_debug_enabled, "I-Frame Data Len: %d Seq. no:%d",
           phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen,
           phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo);
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetNextIframeContxt
 *
 * Description      This internal function is called to set the context for next
 *I-frame.
 *                  Not applicable for the first I-frame of the transceive
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_SetNextIframeContxt(void) {
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  /* Expecting to reach here only after first of chained I-frame is sent and
   * before the last chained is sent */
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_IFRAME;

  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo =
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo ^ 1;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.dataOffset =
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.dataOffset +
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data =
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.p_data;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen =
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;

  // if  chained
  if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen >
      phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen) {
    ALOGD_IF(ese_debug_enabled, "Process Chained Frame");
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = true;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen =
        phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen =
        phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen -
        phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen;
  } else {
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.isChained = false;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen =
        phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.totalDataLen;
  }
  ALOGD_IF(ese_debug_enabled, "I-Frame Data Len: %d",
           phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.sendDataLen);
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetRecovery
 *
 * Description      This internal function is called to do reset the recovery
 *pareameters
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProro7816_SaveIframeData(uint8_t* p_data,
                                                  uint32_t data_len) {
  ESESTATUS status = ESESTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  ALOGD_IF(ese_debug_enabled, "Data[0]=0x%x len=%d Data[%d]=0x%x", p_data[0],
           data_len, data_len - 1, p_data[data_len - 1]);
  if (ESESTATUS_SUCCESS != phNxpEse_StoreDatainList(data_len, p_data)) {
    ALOGE("%s - Error storing chained data in list", __FUNCTION__);
  } else {
    status = ESESTATUS_SUCCESS;
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetRecovery
 *
 * Description      This internal function is called to do reset the recovery
 *pareameters
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_ResetRecovery(void) {
  phNxpEseProto7816_3_Var.recoveryCounter = 0;
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEseProto7816_RecoverySteps
 *
 * Description      This internal function is called when 7816-3 stack failed to
 *recover
 *                  after PH_PROTO_7816_FRAME_RETRY_COUNT, and the interface has
 *to be
 *                  recovered
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_RecoverySteps(void) {
  if (phNxpEseProto7816_3_Var.recoveryCounter <=
      PH_PROTO_7816_FRAME_RETRY_COUNT) {
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
        INTF_RESET_REQ;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
    phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
        INTF_RESET_REQ;
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
        SEND_S_INTF_RST;
  } else { /* If recovery fails */
    phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
  }
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEseProto7816_DecodeSecureTimer
 *
 * Description      This internal function is to decode the secure timer.
 *                  value from the payload
 * Returns          void
 *
 ******************************************************************************/
static void phNxpEseProto7816_DecodeSecureTimer(uint8_t* frameOffset,
                                                unsigned int* secureTimer,
                                                uint8_t* p_data) {
  uint8_t byteCounter = 0;
  uint8_t dataLength = p_data[++(*frameOffset)]; /* To get the L of TLV */
  if (dataLength > 0) {
    /* V of TLV: Retrieve each byte(4 byte) and push it to get the secure timer
     * value (unsigned long) */
    for (byteCounter = 1; byteCounter <= dataLength; byteCounter++) {
      (*frameOffset)++;
      *secureTimer = (*secureTimer) << 8;
      *secureTimer |= p_data[(*frameOffset)];
    }
  } else {
    (*frameOffset)++; /* Goto the end of current marker if length is zero */
  }
  return;
}

/******************************************************************************
 * Function         phNxpEseProto7816_DecodeSFrameData
 *
 * Description      This internal function is to decode S-frame payload.
 * Returns          void
 *
 ******************************************************************************/
static void phNxpEseProto7816_DecodeSFrameData(uint8_t* p_data) {
  uint8_t maxSframeLen = 0, dataType = 0, frameOffset = 0;
  frameOffset = PH_PROPTO_7816_FRAME_LENGTH_OFFSET;
  maxSframeLen =
      p_data[frameOffset] +
      frameOffset; /* to be in sync with offset which starts from index 0 */
  while (maxSframeLen > frameOffset) {
    frameOffset += 1; /* To get the Type (TLV) */
    dataType = p_data[frameOffset];
    ALOGD_IF(ese_debug_enabled, "%s frameoffset=%d value=0x%x\n", __FUNCTION__,
             frameOffset, p_data[frameOffset]);
    switch (dataType) /* Type (TLV) */
    {
      case PH_PROPTO_7816_SFRAME_TIMER1:
        phNxpEseProto7816_DecodeSecureTimer(
            &frameOffset,
            &phNxpEseProto7816_3_Var.secureTimerParams.secureTimer1, p_data);
        break;
      case PH_PROPTO_7816_SFRAME_TIMER2:
        phNxpEseProto7816_DecodeSecureTimer(
            &frameOffset,
            &phNxpEseProto7816_3_Var.secureTimerParams.secureTimer2, p_data);
        break;
      case PH_PROPTO_7816_SFRAME_TIMER3:
        phNxpEseProto7816_DecodeSecureTimer(
            &frameOffset,
            &phNxpEseProto7816_3_Var.secureTimerParams.secureTimer3, p_data);
        break;
      default:
        frameOffset +=
            p_data[frameOffset + 1]; /* Goto the end of current marker */
        break;
    }
  }
  ALOGD_IF(ese_debug_enabled, "secure timer t1 = 0x%x t2 = 0x%x t3 = 0x%x",
           phNxpEseProto7816_3_Var.secureTimerParams.secureTimer1,
           phNxpEseProto7816_3_Var.secureTimerParams.secureTimer2,
           phNxpEseProto7816_3_Var.secureTimerParams.secureTimer3);
  return;
}

/******************************************************************************
 * Function         phNxpEseProto7816_DecodeFrame
 *
 * Description      This internal function is used to
 *                  1. Identify the received frame
 *                  2. If the received frame is I-frame with expected sequence
 number, store it or else send R-NACK
                    3. If the received frame is R-frame,
                       3.1 R-ACK with expected seq. number: Send the next
 chained I-frame
                       3.2 R-ACK with different sequence number: Sebd the R-Nack
                       3.3 R-NACK: Re-send the last frame
                    4. If the received frame is S-frame, send back the correct
 S-frame response.
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_DecodeFrame(uint8_t* p_data,
                                               uint32_t data_len) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  uint8_t pcb;
  phNxpEseProto7816_PCB_bits_t pcb_bits;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  ALOGD_IF(ese_debug_enabled, "Retry Counter = %d\n",
           phNxpEseProto7816_3_Var.recoveryCounter);
  pcb = p_data[PH_PROPTO_7816_PCB_OFFSET];
  // memset(&phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.rcvPcbBits, 0x00,
  // sizeof(struct PCB_BITS));
  phNxpEse_memset(&pcb_bits, 0x00, sizeof(phNxpEseProto7816_PCB_bits_t));
  phNxpEse_memcpy(&pcb_bits, &pcb, sizeof(uint8_t));

  if (0x00 == pcb_bits.msb) /* I-FRAME decoded should come here */
  {
    ALOGD_IF(ese_debug_enabled, "%s I-Frame Received", __FUNCTION__);
    phNxpEseProto7816_3_Var.wtx_counter = 0;
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = IFRAME;
    if (phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo !=
        pcb_bits.bit7)  //   != pcb_bits->bit7)
    {
      ALOGD_IF(ese_debug_enabled, "%s I-Frame lastRcvdIframeInfo.seqNo:0x%x",
               __FUNCTION__, pcb_bits.bit7);
      phNxpEseProto7816_ResetRecovery();
      phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo = 0x00;
      phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo |=
          pcb_bits.bit7;

      if (pcb_bits.bit6) {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.isChained =
            true;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
            NO_ERROR;
        status = phNxpEseProro7816_SaveIframeData(&p_data[3], data_len - 4);
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            SEND_R_ACK;
      } else {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.isChained =
            false;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        status = phNxpEseProro7816_SaveIframeData(&p_data[3], data_len - 4);
      }
    } else {
      phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
      if (phNxpEseProto7816_3_Var.recoveryCounter <
          PH_PROTO_7816_FRAME_RETRY_COUNT) {
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
            OTHER_ERROR;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            SEND_R_NACK;
        phNxpEseProto7816_3_Var.recoveryCounter++;
      } else {
        phNxpEseProto7816_RecoverySteps();
        phNxpEseProto7816_3_Var.recoveryCounter++;
      }
    }
  } else if ((0x01 == pcb_bits.msb) &&
             (0x00 == pcb_bits.bit7)) /* R-FRAME decoded should come here */
  {
    ALOGD_IF(ese_debug_enabled, "%s R-Frame Received", __FUNCTION__);
    phNxpEseProto7816_3_Var.wtx_counter = 0;
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = RFRAME;
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo =
        0;  // = 0;
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo |=
        pcb_bits.bit5;

    if ((pcb_bits.lsb == 0x00) && (pcb_bits.bit2 == 0x00)) {
      phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode =
          NO_ERROR;
      phNxpEseProto7816_ResetRecovery();
      if (phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.seqNo !=
          phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo) {
        status = phNxpEseProto7816_SetNextIframeContxt();
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            SEND_IFRAME;
      } else {
        // error handling.
      }
    } /* Error handling 1 : Parity error */
    else if (((pcb_bits.lsb == 0x01) && (pcb_bits.bit2 == 0x00)) ||
             /* Error handling 2: Other indicated error */
             ((pcb_bits.lsb == 0x00) && (pcb_bits.bit2 == 0x01))) {
      phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
      if ((pcb_bits.lsb == 0x00) && (pcb_bits.bit2 == 0x01))
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode =
            OTHER_ERROR;
      else
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode =
            PARITY_ERROR;
      if (phNxpEseProto7816_3_Var.recoveryCounter <
          PH_PROTO_7816_FRAME_RETRY_COUNT) {
        if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType == IFRAME) {
          phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                          &phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                          sizeof(phNxpEseProto7816_NextTx_Info_t));
          phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
              SEND_IFRAME;
          phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
        } else if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType ==
                   RFRAME) {
          /* Usecase to reach the below case:
          I-frame sent first, followed by R-NACK and we receive a R-NACK with
          last sent I-frame sequence number*/
          if ((phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo
                   .seqNo ==
               phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo) &&
              (phNxpEseProto7816_3_Var.lastSentNonErrorframeType == IFRAME)) {
            phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                            &phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                            sizeof(phNxpEseProto7816_NextTx_Info_t));
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
                SEND_IFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = IFRAME;
          }
          /* Usecase to reach the below case:
          R-frame sent first, followed by R-NACK and we receive a R-NACK with
          next expected I-frame sequence number*/
          else if ((phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo
                        .seqNo != phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx
                                      .IframeInfo.seqNo) &&
                   (phNxpEseProto7816_3_Var.lastSentNonErrorframeType ==
                    RFRAME)) {
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
                NO_ERROR;
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
                SEND_R_ACK;
          }
          /* Usecase to reach the below case:
          I-frame sent first, followed by R-NACK and we receive a R-NACK with
          next expected I-frame sequence number + all the other unexpected
          scenarios */
          else {
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
                OTHER_ERROR;
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
                SEND_R_NACK;
          }
        } else if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType ==
                   SFRAME) {
          /* Copy the last S frame sent */
          phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                          &phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                          sizeof(phNxpEseProto7816_NextTx_Info_t));
        }
        phNxpEseProto7816_3_Var.recoveryCounter++;
      } else {
        phNxpEseProto7816_RecoverySteps();
        phNxpEseProto7816_3_Var.recoveryCounter++;
      }
      // resend previously send I frame
    }
    /* Error handling 3 */
    else if ((pcb_bits.lsb == 0x01) && (pcb_bits.bit2 == 0x01)) {
      phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
      if (phNxpEseProto7816_3_Var.recoveryCounter <
          PH_PROTO_7816_FRAME_RETRY_COUNT) {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode =
            SOF_MISSED_ERROR;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx =
            phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
        phNxpEseProto7816_3_Var.recoveryCounter++;
      } else {
        phNxpEseProto7816_RecoverySteps();
        phNxpEseProto7816_3_Var.recoveryCounter++;
      }
    } else /* Error handling 4 */
    {
      phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
      if (phNxpEseProto7816_3_Var.recoveryCounter <
          PH_PROTO_7816_FRAME_RETRY_COUNT) {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdRframeInfo.errCode =
            UNDEFINED_ERROR;
        phNxpEseProto7816_3_Var.recoveryCounter++;
      } else {
        phNxpEseProto7816_RecoverySteps();
        phNxpEseProto7816_3_Var.recoveryCounter++;
      }
    }
  } else if ((0x01 == pcb_bits.msb) &&
             (0x01 == pcb_bits.bit7)) /* S-FRAME decoded should come here */
  {
    ALOGD_IF(ese_debug_enabled, "%s S-Frame Received", __FUNCTION__);
    int32_t frameType = (int32_t)(pcb & 0x3F); /*discard upper 2 bits */
    phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = SFRAME;
    if (frameType != WTX_REQ) {
      phNxpEseProto7816_3_Var.wtx_counter = 0;
    }
    switch (frameType) {
      case RESYNCH_REQ:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            RESYNCH_REQ;
        break;
      case RESYNCH_RSP:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            RESYNCH_RSP;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = UNKNOWN;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
      case IFSC_REQ:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            IFSC_REQ;
        break;
      case IFSC_RES:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            IFSC_RES;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = UNKNOWN;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
      case ABORT_REQ:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            ABORT_REQ;
        break;
      case ABORT_RES:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            ABORT_RES;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = UNKNOWN;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
      case WTX_REQ:
        phNxpEseProto7816_3_Var.wtx_counter++;
        ALOGD_IF(ese_debug_enabled, "%s Wtx_counter value - %lu", __FUNCTION__,
                 phNxpEseProto7816_3_Var.wtx_counter);
        ALOGD_IF(ese_debug_enabled, "%s Wtx_counter wtx_counter_limit - %lu",
                 __FUNCTION__, phNxpEseProto7816_3_Var.wtx_counter_limit);
        /* Previous sent frame is some S-frame but not WTX response S-frame */
        if (phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType !=
                WTX_RSP &&
            phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType ==
                SFRAME) { /* Goto recovery if it keep coming here for more than
                             recovery counter max. value */
          if (phNxpEseProto7816_3_Var.recoveryCounter <
              PH_PROTO_7816_FRAME_RETRY_COUNT) { /* Re-transmitting the previous
                                                    sent S-frame */
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx =
                phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
            phNxpEseProto7816_3_Var.recoveryCounter++;
          } else {
            phNxpEseProto7816_RecoverySteps();
            phNxpEseProto7816_3_Var.recoveryCounter++;
          }
        } else { /* Checking for WTX counter with max. allowed WTX count */
          if (phNxpEseProto7816_3_Var.wtx_counter ==
              phNxpEseProto7816_3_Var.wtx_counter_limit) {
            phNxpEseProto7816_3_Var.wtx_counter = 0;
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo
                .sFrameType = INTF_RESET_REQ;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
                INTF_RESET_REQ;
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
                SEND_S_INTF_RST;
            ALOGE("%s Interface Reset to eSE wtx count reached!!!",
                  __FUNCTION__);
          } else {
            phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
            phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo
                .sFrameType = WTX_REQ;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
                WTX_RSP;
            phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
                SEND_S_WTX_RSP;
          }
        }
        break;
      case WTX_RSP:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            WTX_RSP;
        break;
      case INTF_RESET_REQ:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            INTF_RESET_REQ;
        break;
      case INTF_RESET_RSP:
        phNxpEseProto7816_ResetProtoParams();
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            INTF_RESET_RSP;
        if (p_data[PH_PROPTO_7816_FRAME_LENGTH_OFFSET] > 0)
          phNxpEseProto7816_DecodeSFrameData(p_data);
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = UNKNOWN;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
      case PROP_END_APDU_REQ:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            PROP_END_APDU_REQ;
        break;
      case PROP_END_APDU_RSP:
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdSframeInfo.sFrameType =
            PROP_END_APDU_RSP;
        if (p_data[PH_PROPTO_7816_FRAME_LENGTH_OFFSET] > 0)
          phNxpEseProto7816_DecodeSFrameData(p_data);
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = UNKNOWN;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
      default:
        ALOGE("%s Wrong S-Frame Received", __FUNCTION__);
        break;
    }
  } else {
    ALOGE("%s Wrong-Frame Received", __FUNCTION__);
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ProcessResponse
 *
 * Description      This internal function is used to
 *                  1. Check the LRC
 *                  2. Initiate decoding of received frame of data.
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_ProcessResponse(void) {
  uint32_t data_len = 0;
  uint8_t* p_data = NULL;
  ESESTATUS status = ESESTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  status = phNxpEseProto7816_GetRawFrame(&data_len, &p_data);
  ALOGD_IF(ese_debug_enabled, "%s p_data ----> %p len ----> 0x%x", __FUNCTION__,
           p_data, data_len);
  if (ESESTATUS_SUCCESS == status) {
    /* Resetting the timeout counter */
    phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
    /* LRC check followed */
    status = phNxpEseProto7816_CheckLRC(data_len, p_data);
    if (status == ESESTATUS_SUCCESS) {
      /* Resetting the RNACK retry counter */
      phNxpEseProto7816_3_Var.rnack_retry_counter = PH_PROTO_7816_VALUE_ZERO;
      phNxpEseProto7816_DecodeFrame(p_data, data_len);
    } else {
      ALOGE("%s LRC Check failed", __FUNCTION__);
      if (phNxpEseProto7816_3_Var.rnack_retry_counter <
          phNxpEseProto7816_3_Var.rnack_retry_limit) {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
            PARITY_ERROR;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.seqNo =
            (!phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo)
            << 4;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            SEND_R_NACK;
        phNxpEseProto7816_3_Var.rnack_retry_counter++;
      } else {
        phNxpEseProto7816_3_Var.rnack_retry_counter = PH_PROTO_7816_VALUE_ZERO;
        /* Re-transmission failed completely, Going to exit */
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
      }
    }
  } else {
    ALOGE("%s phNxpEseProto7816_GetRawFrame failed", __FUNCTION__);
    if ((SFRAME == phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType) &&
        ((WTX_RSP ==
          phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType) ||
         (RESYNCH_RSP ==
          phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.SframeInfo.sFrameType))) {
      if (phNxpEseProto7816_3_Var.rnack_retry_counter <
          phNxpEseProto7816_3_Var.rnack_retry_limit) {
        phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = RFRAME;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.errCode =
            OTHER_ERROR;
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.RframeInfo.seqNo =
            (!phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo)
            << 4;
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            SEND_R_NACK;
        phNxpEseProto7816_3_Var.rnack_retry_counter++;
      } else {
        phNxpEseProto7816_3_Var.rnack_retry_counter = PH_PROTO_7816_VALUE_ZERO;
        /* Re-transmission failed completely, Going to exit */
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
      }
    } else {
      phNxpEse_Sleep(DELAY_ERROR_RECOVERY);
      /* re transmit the frame */
      if (phNxpEseProto7816_3_Var.timeoutCounter <
          PH_PROTO_7816_TIMEOUT_RETRY_COUNT) {
        phNxpEseProto7816_3_Var.timeoutCounter++;
        ALOGE("%s re-transmitting the previous frame", __FUNCTION__);
        phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx =
            phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx;
      } else {
        /* Re-transmission failed completely, Going to exit */
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
        ALOGE("%s calling phNxpEse_StoreDatainList", __FUNCTION__);
        phNxpEse_StoreDatainList(data_len, p_data);
      }
    }
  }
  ALOGD_IF(ese_debug_enabled, "Exit %s Status 0x%x", __FUNCTION__, status);
  return status;
}

/******************************************************************************
 * Function         TransceiveProcess
 *
 * Description      This internal function is used to
 *                  1. Send the raw data received from application after
 *computing LRC
 *                  2. Receive the the response data from ESE, decode, process
 *and
 *                     store the data.
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS TransceiveProcess(void) {
  ESESTATUS status = ESESTATUS_FAILED;
  sFrameInfo_t sFrameInfo;

  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  while (phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState !=
         IDLE_STATE) {
    ALOGD_IF(ese_debug_enabled, "%s nextTransceiveState %x", __FUNCTION__,
             phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState);
    switch (phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState) {
      case SEND_IFRAME:
        status = phNxpEseProto7816_SendIframe(
            phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo);
        break;
      case SEND_R_ACK:
        status = phNxpEseProto7816_sendRframe(RACK);
        break;
      case SEND_R_NACK:
        status = phNxpEseProto7816_sendRframe(RNACK);
        break;
      case SEND_S_RSYNC:
        sFrameInfo.sFrameType = RESYNCH_REQ;
        status = phNxpEseProto7816_SendSFrame(sFrameInfo);
        break;
      case SEND_S_INTF_RST:
        sFrameInfo.sFrameType = INTF_RESET_REQ;
        status = phNxpEseProto7816_SendSFrame(sFrameInfo);
        break;
      case SEND_S_EOS:
        sFrameInfo.sFrameType = PROP_END_APDU_REQ;
        status = phNxpEseProto7816_SendSFrame(sFrameInfo);
        break;
      case SEND_S_WTX_RSP:
        sFrameInfo.sFrameType = WTX_RSP;
        status = phNxpEseProto7816_SendSFrame(sFrameInfo);
        break;
      default:
        phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
            IDLE_STATE;
        break;
    }
    if (ESESTATUS_SUCCESS == status) {
      phNxpEse_memcpy(&phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx,
                      &phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx,
                      sizeof(phNxpEseProto7816_NextTx_Info_t));
      status = phNxpEseProto7816_ProcessResponse();
    } else {
      ALOGD_IF(ese_debug_enabled,
               "%s Transceive send failed, going to recovery!", __FUNCTION__);
      phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
          IDLE_STATE;
    }
  };
  ALOGD_IF(ese_debug_enabled, "Exit %s Status 0x%x", __FUNCTION__, status);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Transceive
 *
 * Description      This function is used to
 *                  1. Send the raw data received from application after
 *computing LRC
 *                  2. Receive the the response data from ESE, decode, process
 *and
 *                     store the data.
 *                  3. Get the final complete data and sent back to application
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_Transceive(phNxpEse_data* pCmd,
                                       phNxpEse_data* pRsp) {
  ESESTATUS status = ESESTATUS_FAILED;
  ESESTATUS wStatus = ESESTATUS_FAILED;
  phNxpEse_data pRes;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  if ((NULL == pCmd) || (NULL == pRsp) ||
      (phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState !=
       PH_NXP_ESE_PROTO_7816_IDLE))
    return status;
  phNxpEse_memset(&pRes, 0x00, sizeof(phNxpEse_data));
  /* Updating the transceive information to the protocol stack */
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data = pCmd->p_data;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.totalDataLen =
      pCmd->len;
  ALOGD_IF(ese_debug_enabled, "Transceive data ptr 0x%p len:%d", pCmd->p_data,
           pCmd->len);
  status = phNxpEseProto7816_SetFirstIframeContxt();
  status = TransceiveProcess();
  if (ESESTATUS_FAILED == status) {
    /* ESE hard reset to be done */
    ALOGE("Transceive failed, hard reset to proceed");
    wStatus = phNxpEse_GetData(&pRes.len, &pRes.p_data);
    if (ESESTATUS_SUCCESS == wStatus) {
      ALOGE(
          "%s Data successfully received at 7816, packaging to "
          "send upper layers: DataLen = %d",
          __FUNCTION__, pRes.len);
      /* Copy the data to be read by the upper layer via transceive api */
      pRsp->len = pRes.len;
      pRsp->p_data = pRes.p_data;
    }
  } else {
    // fetch the data info and report to upper layer.
    wStatus = phNxpEse_GetData(&pRes.len, &pRes.p_data);
    if (ESESTATUS_SUCCESS == wStatus) {
      ALOGD_IF(ese_debug_enabled,
               "%s Data successfully received at 7816, packaging to "
               "send upper layers: DataLen = %d",
               __FUNCTION__, pRes.len);
      /* Copy the data to be read by the upper layer via transceive api */
      pRsp->len = pRes.len;
      pRsp->p_data = pRes.p_data;
    } else
      status = ESESTATUS_FAILED;
  }
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_IDLE;
  ALOGD_IF(ese_debug_enabled, "Exit %s Status 0x%x", __FUNCTION__, status);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_RSync
 *
 * Description      This function is used to send the RSync command
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_RSync(void) {
  ESESTATUS status = ESESTATUS_FAILED;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
  /* send the end of session s-frame */
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
      RESYNCH_REQ;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_RSYNC;
  status = TransceiveProcess();
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_IDLE;
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_ResetProtoParams
 *
 * Description      This function is used to reset the 7816 protocol stack
 *instance
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
static ESESTATUS phNxpEseProto7816_ResetProtoParams(void) {
  unsigned long int tmpWTXCountlimit = PH_PROTO_7816_VALUE_ZERO;
  unsigned long int tmpRNACKCountlimit = PH_PROTO_7816_VALUE_ZERO;
  tmpWTXCountlimit = phNxpEseProto7816_3_Var.wtx_counter_limit;
  tmpRNACKCountlimit = phNxpEseProto7816_3_Var.rnack_retry_limit;
  phNxpEse_memset(&phNxpEseProto7816_3_Var, PH_PROTO_7816_VALUE_ZERO,
                  sizeof(phNxpEseProto7816_t));
  phNxpEseProto7816_3_Var.wtx_counter_limit = tmpWTXCountlimit;
  phNxpEseProto7816_3_Var.rnack_retry_limit = tmpRNACKCountlimit;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_IDLE;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = IDLE_STATE;
  phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdFrameType = INVALID;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = INVALID;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen =
      IFSC_SIZE_SEND;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.p_data = NULL;
  phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.FrameType = INVALID;
  phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.maxDataLen =
      IFSC_SIZE_SEND;
  phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.p_data = NULL;
  /* Initialized with sequence number of the last I-frame sent */
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.seqNo =
      PH_PROTO_7816_VALUE_ONE;
  /* Initialized with sequence number of the last I-frame received */
  phNxpEseProto7816_3_Var.phNxpEseRx_Cntx.lastRcvdIframeInfo.seqNo =
      PH_PROTO_7816_VALUE_ONE;
  /* Initialized with sequence number of the last I-frame received */
  phNxpEseProto7816_3_Var.phNxpEseLastTx_Cntx.IframeInfo.seqNo =
      PH_PROTO_7816_VALUE_ONE;
  phNxpEseProto7816_3_Var.recoveryCounter = PH_PROTO_7816_VALUE_ZERO;
  phNxpEseProto7816_3_Var.timeoutCounter = PH_PROTO_7816_VALUE_ZERO;
  phNxpEseProto7816_3_Var.wtx_counter = PH_PROTO_7816_VALUE_ZERO;
  /* This update is helpful in-case a R-NACK is transmitted from the MW */
  phNxpEseProto7816_3_Var.lastSentNonErrorframeType = UNKNOWN;
  phNxpEseProto7816_3_Var.rnack_retry_counter = PH_PROTO_7816_VALUE_ZERO;
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Reset
 *
 * Description      This function is used to reset the 7816 protocol stack
 *instance
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_Reset(void) {
  ESESTATUS status = ESESTATUS_FAILED;
  /* Resetting host protocol instance */
  phNxpEseProto7816_ResetProtoParams();
  /* Resynchronising ESE protocol instance */
  status = phNxpEseProto7816_RSync();
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Open
 *
 * Description      This function is used to open the 7816 protocol stack
 *instance
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_Open(phNxpEseProto7816InitParam_t initParam) {
  ESESTATUS status = ESESTATUS_FAILED;
  status = phNxpEseProto7816_ResetProtoParams();
  ALOGD_IF(ese_debug_enabled, "%s: First open completed, Congratulations",
           __FUNCTION__);
  /* Update WTX max. limit */
  phNxpEseProto7816_3_Var.wtx_counter_limit = initParam.wtx_counter_limit;
  phNxpEseProto7816_3_Var.rnack_retry_limit = initParam.rnack_retry_limit;
  if (initParam.interfaceReset) /* Do interface reset */
  {
    status = phNxpEseProto7816_IntfReset(initParam.pSecureTimerParams);
    if (ESESTATUS_SUCCESS == status) {
      phNxpEse_memcpy(initParam.pSecureTimerParams,
                      &phNxpEseProto7816_3_Var.secureTimerParams,
                      sizeof(phNxpEseProto7816SecureTimer_t));
    }
  } else /* Do R-Sync */
  {
    status = phNxpEseProto7816_RSync();
  }
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_Close
 *
 * Description      This function is used to close the 7816 protocol stack
 *instance
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_Close(
    phNxpEseProto7816SecureTimer_t* pSecureTimerParams) {
  ESESTATUS status = ESESTATUS_FAILED;
  if (phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState !=
      PH_NXP_ESE_PROTO_7816_IDLE)
    return status;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_DEINIT;
  phNxpEseProto7816_3_Var.recoveryCounter = 0;
  phNxpEseProto7816_3_Var.wtx_counter = 0;
  /* send the end of session s-frame */
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
      PROP_END_APDU_REQ;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState = SEND_S_EOS;
  status = TransceiveProcess();
  if (ESESTATUS_FAILED == status) {
    /* reset all the structures */
    ALOGE("%s TransceiveProcess failed ", __FUNCTION__);
  }
  phNxpEse_memcpy(pSecureTimerParams,
                  &phNxpEseProto7816_3_Var.secureTimerParams,
                  sizeof(phNxpEseProto7816SecureTimer_t));
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_IDLE;
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_IntfReset
 *
 * Description      This function is used to reset just the current interface
 *
 * Returns          On success return true or else false.
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_IntfReset(
    phNxpEseProto7816SecureTimer_t* pSecureTimerParam) {
  ESESTATUS status = ESESTATUS_FAILED;
  ALOGD_IF(ese_debug_enabled, "Enter %s ", __FUNCTION__);
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_TRANSCEIVE;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.FrameType = SFRAME;
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.SframeInfo.sFrameType =
      INTF_RESET_REQ;
  phNxpEseProto7816_3_Var.phNxpEseProto7816_nextTransceiveState =
      SEND_S_INTF_RST;
  status = TransceiveProcess();
  if (ESESTATUS_FAILED == status) {
    /* reset all the structures */
    ALOGE("%s TransceiveProcess failed ", __FUNCTION__);
  }
  phNxpEse_memcpy(pSecureTimerParam, &phNxpEseProto7816_3_Var.secureTimerParams,
                  sizeof(phNxpEseProto7816SecureTimer_t));
  phNxpEseProto7816_3_Var.phNxpEseProto7816_CurrentState =
      PH_NXP_ESE_PROTO_7816_IDLE;
  ALOGD_IF(ese_debug_enabled, "Exit %s ", __FUNCTION__);
  return status;
}

/******************************************************************************
 * Function         phNxpEseProto7816_SetIfscSize
 *
 * Description      This function is used to set the max T=1 data send size
 *
 * Returns          Always return true (1).
 *
 ******************************************************************************/
ESESTATUS phNxpEseProto7816_SetIfscSize(uint16_t IFSC_Size) {
  phNxpEseProto7816_3_Var.phNxpEseNextTx_Cntx.IframeInfo.maxDataLen = IFSC_Size;
  return ESESTATUS_SUCCESS;
}
/** @} */
