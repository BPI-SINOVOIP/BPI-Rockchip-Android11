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
#ifndef _PHNXPESE_RECVMGR_H_
#define _PHNXPESE_RECVMGR_H_
#include <phNxpEse_Internal.h>

typedef struct phNxpEse_DataPacket {
  uint8_t sbuffer[MAX_DATA_LEN]; /* buffer to be used to store the received
                                    packet */
  uint16_t wLen;                 /* hold the length of the buffer */
} phNxpEse_DataPacket_t;

typedef struct phNxpEse_sCoreRecvBuff_List {
  phNxpEse_DataPacket_t
      tData; /* buffer to be used to store the received payload */
  struct phNxpEse_sCoreRecvBuff_List*
      pNext; /* pointer to the next node present in lined list*/
} phNxpEse_sCoreRecvBuff_List_t;

ESESTATUS phNxpEse_GetData(uint32_t* data_len, uint8_t** pbuff);
ESESTATUS phNxpEse_StoreDatainList(uint32_t data_len, uint8_t* pbuff);

#endif /* PHNXPESE_RECVMGR_H */
