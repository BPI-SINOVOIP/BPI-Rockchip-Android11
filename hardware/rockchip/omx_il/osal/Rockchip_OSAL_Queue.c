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
 * @file        Rockchip_OSAL_Queue.c
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_Queue.h"


OMX_ERRORTYPE Rockchip_OSAL_QueueCreate(ROCKCHIP_QUEUE *queueHandle, int maxNumElem)
{
    int i = 0;
    ROCKCHIP_QElem *newqelem = NULL;
    ROCKCHIP_QElem *currentqelem = NULL;
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;

    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (!queue)
        return OMX_ErrorBadParameter;

    ret = Rockchip_OSAL_MutexCreate(&queue->qMutex);
    if (ret != OMX_ErrorNone)
        return ret;

    queue->first = (ROCKCHIP_QElem *)Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_QElem));
    if (queue->first == NULL)
        return OMX_ErrorInsufficientResources;

    Rockchip_OSAL_Memset(queue->first, 0, sizeof(ROCKCHIP_QElem));
    currentqelem = queue->last = queue->first;
    queue->numElem = 0;
    queue->maxNumElem = maxNumElem;
    for (i = 0; i < (queue->maxNumElem - 2); i++) {
        newqelem = (ROCKCHIP_QElem *)Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_QElem));
        if (newqelem == NULL) {
            while (queue->first != NULL) {
                currentqelem = queue->first->qNext;
                Rockchip_OSAL_Free((OMX_PTR)queue->first);
                queue->first = currentqelem;
            }
            return OMX_ErrorInsufficientResources;
        } else {
            Rockchip_OSAL_Memset(newqelem, 0, sizeof(ROCKCHIP_QElem));
            currentqelem->qNext = newqelem;
            currentqelem = newqelem;
        }
    }

    currentqelem->qNext = queue->first;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Rockchip_OSAL_QueueTerminate(ROCKCHIP_QUEUE *queueHandle)
{
    int i = 0;
    ROCKCHIP_QElem *currentqelem = NULL;
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (!queue)
        return OMX_ErrorBadParameter;

    for ( i = 0; i < (queue->maxNumElem - 2); i++) {
        currentqelem = queue->first->qNext;
        Rockchip_OSAL_Free(queue->first);
        queue->first = currentqelem;
    }

    if (queue->first) {
        Rockchip_OSAL_Free(queue->first);
        queue->first = NULL;
    }

    ret = Rockchip_OSAL_MutexTerminate(queue->qMutex);

    return ret;
}

int Rockchip_OSAL_Queue(ROCKCHIP_QUEUE *queueHandle, void *data)
{
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    Rockchip_OSAL_MutexLock(queue->qMutex);

    if ((queue->last->data != NULL) || (queue->numElem >= queue->maxNumElem)) {
        Rockchip_OSAL_MutexUnlock(queue->qMutex);
        return -1;
    }
    queue->last->data = data;
    queue->last = queue->last->qNext;
    queue->numElem++;

    Rockchip_OSAL_MutexUnlock(queue->qMutex);
    return 0;
}

void *Rockchip_OSAL_Dequeue(ROCKCHIP_QUEUE *queueHandle)
{
    void *data = NULL;
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    if (queue == NULL)
        return NULL;

    Rockchip_OSAL_MutexLock(queue->qMutex);

    if ((queue->first->data == NULL) || (queue->numElem <= 0)) {
        Rockchip_OSAL_MutexUnlock(queue->qMutex);
        return NULL;
    }
    data = queue->first->data;
    queue->first->data = NULL;
    queue->first = queue->first->qNext;
    queue->numElem--;

    Rockchip_OSAL_MutexUnlock(queue->qMutex);
    return data;
}

int Rockchip_OSAL_GetElemNum(ROCKCHIP_QUEUE *queueHandle)
{
    int ElemNum = 0;
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    Rockchip_OSAL_MutexLock(queue->qMutex);
    ElemNum = queue->numElem;
    Rockchip_OSAL_MutexUnlock(queue->qMutex);
    return ElemNum;
}

int Rockchip_OSAL_SetElemNum(ROCKCHIP_QUEUE *queueHandle, int ElemNum)
{
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    if (queue == NULL)
        return -1;

    Rockchip_OSAL_MutexLock(queue->qMutex);
    queue->numElem = ElemNum;
    Rockchip_OSAL_MutexUnlock(queue->qMutex);
    return ElemNum;
}

int Rockchip_OSAL_ResetQueue(ROCKCHIP_QUEUE *queueHandle)
{
    ROCKCHIP_QUEUE *queue = (ROCKCHIP_QUEUE *)queueHandle;
    ROCKCHIP_QElem *currentqelem = NULL;

    if (queue == NULL)
        return -1;

    Rockchip_OSAL_MutexLock(queue->qMutex);
    queue->first->data = NULL;
    currentqelem = queue->first->qNext;
    while (currentqelem != queue->first) {
        currentqelem->data = NULL;
        currentqelem = currentqelem->qNext;
    }
    queue->last = queue->first;
    queue->numElem = 0x00;
    Rockchip_OSAL_MutexUnlock(queue->qMutex);

    return 0;
}
