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
#include <phNxpEseDataMgr.h>
#include <phNxpEsePal.h>

extern bool ese_debug_enabled;

static phNxpEse_sCoreRecvBuff_List_t *head = NULL, *current = NULL;
static uint32_t total_len = 0;

static ESESTATUS phNxpEse_DeletList(phNxpEse_sCoreRecvBuff_List_t* head);
static ESESTATUS phNxpEse_GetDataFromList(uint32_t* data_len, uint8_t* pbuff);
/******************************************************************************
 * Function         phNxpEse_GetData
 *
 * Description      This function update the len and provided buffer
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEse_GetData(uint32_t* data_len, uint8_t** pbuffer) {
  uint32_t total_data_len = 0;
  uint8_t* pbuff = NULL;

  if (total_len == 0) {
    ALOGE("%s total_len = %d", __FUNCTION__, total_len);
    return ESESTATUS_FAILED;
  }
  pbuff = (uint8_t*)phNxpEse_memalloc(total_len);
  if (NULL == pbuff) {
    ALOGE("%s Error in malloc ", __FUNCTION__);
    return ESESTATUS_NOT_ENOUGH_MEMORY;
  }

  if (ESESTATUS_SUCCESS != phNxpEse_GetDataFromList(&total_data_len, pbuff)) {
    ALOGE("%s phNxpEse_GetDataFromList", __FUNCTION__);
    phNxpEse_free(pbuff);
    return ESESTATUS_FAILED;
  }
  if (total_data_len != total_len) {
    ALOGE("%s Mismatch of len total_data_len %d total_len %d", __FUNCTION__,
          total_data_len, total_len);
    phNxpEse_free(pbuff);
    return ESESTATUS_FAILED;
  }

  *pbuffer = pbuff;
  *data_len = total_data_len;
  phNxpEse_DeletList(head);
  total_len = 0;
  head = NULL;
  current = NULL;

  return ESESTATUS_SUCCESS;
}
/******************************************************************************
 * Function         phNxpEse_StoreDatainList
 *
 * Description      This function stores the received data in linked list
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
ESESTATUS phNxpEse_StoreDatainList(uint32_t data_len, uint8_t* pbuff) {
  phNxpEse_sCoreRecvBuff_List_t* newNode = NULL;

  newNode = (phNxpEse_sCoreRecvBuff_List_t*)phNxpEse_memalloc(
      sizeof(phNxpEse_sCoreRecvBuff_List_t));
  if (newNode == NULL) {
    return ESESTATUS_NOT_ENOUGH_MEMORY;
  }
  newNode->pNext = NULL;
  newNode->tData.wLen = data_len;
  phNxpEse_memcpy(newNode->tData.sbuffer, pbuff, data_len);
  total_len += data_len;
  if (head == NULL) {
    head = newNode;
    current = newNode;
  } else {
    current->pNext = newNode;
    current = newNode;
  }
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_GetDataFromList
 *
 * Description      This function copies all linked list data in provided buffer
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_GetDataFromList(uint32_t* data_len, uint8_t* pbuff) {
  phNxpEse_sCoreRecvBuff_List_t* new_node;
  uint32_t offset = 0;
  ALOGD_IF(ese_debug_enabled, "%s Enter ", __FUNCTION__);
  if (head == NULL || pbuff == NULL) {
    return ESESTATUS_FAILED;
  }

  new_node = head;
  while (new_node != NULL) {
    phNxpEse_memcpy((pbuff + offset), new_node->tData.sbuffer,
                    new_node->tData.wLen);
    offset += new_node->tData.wLen;
    new_node = new_node->pNext;
  }
  *data_len = offset;
  ALOGD_IF(ese_debug_enabled, "%s Exit ", __FUNCTION__);
  return ESESTATUS_SUCCESS;
}

/******************************************************************************
 * Function         phNxpEse_DeletList
 *
 * Description      This function deletes all nodes from linked list
 *
 * Returns          On Success ESESTATUS_SUCCESS else proper error code
 *
 ******************************************************************************/
static ESESTATUS phNxpEse_DeletList(phNxpEse_sCoreRecvBuff_List_t* head) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  phNxpEse_sCoreRecvBuff_List_t *current, *next;
  current = head;

  if (head == NULL) {
    return ESESTATUS_FAILED;
  }

  while (current != NULL) {
    next = current->pNext;
    phNxpEse_free(current);
    current = NULL;
    current = next;
  }
  head = NULL;
  return status;
}
