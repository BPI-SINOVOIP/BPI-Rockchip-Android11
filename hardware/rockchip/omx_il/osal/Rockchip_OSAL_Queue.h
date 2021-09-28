/*
 *
 * Copyright 2013 Rockchip Electronics Co. LTD
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

/*
 * @file        Rockchip_OSAL_Queue.h
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */

#ifndef ROCKCHIP_OSAL_QUEUE
#define ROCKCHIP_OSAL_QUEUE

#include "OMX_Types.h"
#include "OMX_Core.h"

#define QUEUE_ELEMENTS        10
#define MAX_QUEUE_ELEMENTS    40

typedef struct _ROCKCHIP_QElem {
    void             *data;
    struct _ROCKCHIP_QElem *qNext;
} ROCKCHIP_QElem;

typedef struct _ROCKCHIP_QUEUE {
    ROCKCHIP_QElem     *first;
    ROCKCHIP_QElem     *last;
    int            numElem;
    int            maxNumElem;
    OMX_HANDLETYPE qMutex;
} ROCKCHIP_QUEUE;


#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Rockchip_OSAL_QueueCreate(ROCKCHIP_QUEUE *queueHandle, int maxNumElem);
OMX_ERRORTYPE Rockchip_OSAL_QueueTerminate(ROCKCHIP_QUEUE *queueHandle);
int           Rockchip_OSAL_Queue(ROCKCHIP_QUEUE *queueHandle, void *data);
void         *Rockchip_OSAL_Dequeue(ROCKCHIP_QUEUE *queueHandle);
int           Rockchip_OSAL_GetElemNum(ROCKCHIP_QUEUE *queueHandle);
int           Rockchip_OSAL_SetElemNum(ROCKCHIP_QUEUE *queueHandle, int ElemNum);
int           Rockchip_OSAL_ResetQueue(ROCKCHIP_QUEUE *queueHandle);

#ifdef __cplusplus
}
#endif

#endif
