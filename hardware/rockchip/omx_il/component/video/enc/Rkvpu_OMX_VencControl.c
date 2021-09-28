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
 * @file        Rkon2_OMX_VdecControl.c
 * @brief
 * @author      csy (csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.28 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_venc_ctl"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Rockchip_OMX_Macros.h"
#include "Rockchip_OSAL_Event.h"
#include "Rkvpu_OMX_Venc.h"
#include "Rkvpu_OMX_VencControl.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "Rockchip_OSAL_Thread.h"
#include "Rockchip_OSAL_Semaphore.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OSAL_SharedMemory.h"
#include "Rockchip_OSAL_Queue.h"
#include <hardware/hardware.h>
#include "hardware/rga.h"
#include <fcntl.h>
#include <poll.h>

//#include "csc.h"

#ifdef USE_ANB
#include "Rockchip_OSAL_Android.h"
#endif

#include "Rockchip_OSAL_Log.h"

typedef struct {
    OMX_U32 mProfile;
    OMX_U32 mLevel;
} CodecProfileLevel;

static const CodecProfileLevel kProfileLevels[] = {
    { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
    { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51},
    { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51},
};

static const CodecProfileLevel kH265ProfileLevels[] = {
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel1  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel2  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel21 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel3  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel31 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel4  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel41 },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel5  },
    { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel51 },
};

OMX_ERRORTYPE Rkvpu_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U32                i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];
    if (nPortIndex >= pRockchipComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    if (pRockchipPort->portState != OMX_StateIdle) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    temp_bufferHeader = (OMX_BUFFERHEADERTYPE *)Rockchip_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (temp_bufferHeader == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Rockchip_OSAL_Memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pRockchipPort->portDefinition.nBufferCountActual; i++) {
        if (pRockchipPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pRockchipPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pRockchipPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            temp_bufferHeader->pBuffer        = pBuffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;

            pRockchipPort->assignedBufferNum++;
            if (pRockchipPort->assignedBufferNum == pRockchipPort->portDefinition.nBufferCountActual) {
                pRockchipPort->portDefinition.bPopulated = OMX_TRUE;
                /* Rockchip_OSAL_MutexLock(pRockchipPort->compMutex); */
                Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
                /* Rockchip_OSAL_MutexUnlock(pRockchipPort->compMutex); */
            }
            *ppBufferHdr = temp_bufferHeader;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    Rockchip_OSAL_Free(temp_bufferHeader);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U8                *temp_buffer = NULL;
    int                    temp_buffer_fd = -1;
    OMX_U32                i = 0;
    MEMORY_TYPE            mem_type = NORMAL_MEMORY;

    FunctionIn();

    omx_err("Rkvpu_OMX_AllocateBuffer in");
    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];
    if (nPortIndex >= pRockchipComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    /*
        if (pRockchipPort->portState != OMX_StateIdle ) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    */
    if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    temp_buffer = (OMX_U8 *)Rockchip_OSAL_Malloc(nSizeBytes);
    if (temp_buffer == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    temp_bufferHeader = (OMX_BUFFERHEADERTYPE *)Rockchip_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (temp_bufferHeader == NULL) {
        Rockchip_OSAL_Free(temp_buffer);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Rockchip_OSAL_Memset(temp_bufferHeader, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pRockchipPort->portDefinition.nBufferCountActual; i++) {
        if (pRockchipPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pRockchipPort->extendBufferHeader[i].OMXBufferHeader = temp_bufferHeader;
            pRockchipPort->extendBufferHeader[i].buf_fd[0] = temp_buffer_fd;
            pRockchipPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            if (mem_type == SECURE_MEMORY)
                ;//temp_bufferHeader->pBuffer = temp_buffer_fd;
            else
                temp_bufferHeader->pBuffer = temp_buffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;
            pRockchipPort->assignedBufferNum++;
            if (pRockchipPort->assignedBufferNum == pRockchipPort->portDefinition.nBufferCountActual) {
                pRockchipPort->portDefinition.bPopulated = OMX_TRUE;
                /* Rockchip_OSAL_MutexLock(pRockchipComponent->compMutex); */
                Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
                /* Rockchip_OSAL_MutexUnlock(pRockchipComponent->compMutex); */
            }
            *ppBuffer = temp_bufferHeader;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    Rockchip_OSAL_Free(temp_bufferHeader);
    Rockchip_OSAL_Free(temp_buffer);

    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    omx_err("Rkvpu_OMX_AllocateBuffer in ret = 0x%x", ret);
    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_U32        nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_U32                i = 0;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pRockchipPort->portState != OMX_StateLoaded) && (pRockchipPort->portState != OMX_StateInvalid)) {
        (*(pRockchipComponent->pCallbacks->EventHandler)) (pOMXComponent,
                                                           pRockchipComponent->callbackData,
                                                           (OMX_U32)OMX_EventError,
                                                           (OMX_U32)OMX_ErrorPortUnpopulated,
                                                           nPortIndex, NULL);
    }

    for (i = 0; i < /*pRockchipPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if (((pRockchipPort->bufferStateAllocate[i] | BUFFER_STATE_FREE) != 0) && (pRockchipPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            if (pRockchipPort->extendBufferHeader[i].OMXBufferHeader->pBuffer == pBufferHdr->pBuffer) {
                if (pRockchipPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    Rockchip_OSAL_Free(pRockchipPort->extendBufferHeader[i].OMXBufferHeader->pBuffer);
                    pRockchipPort->extendBufferHeader[i].OMXBufferHeader->pBuffer = NULL;
                    pBufferHdr->pBuffer = NULL;
                } else if (pRockchipPort->bufferStateAllocate[i] & BUFFER_STATE_ASSIGNED) {
                    ; /* None*/
                }
                pRockchipPort->assignedBufferNum--;
                if (pRockchipPort->bufferStateAllocate[i] & HEADER_STATE_ALLOCATED) {
                    Rockchip_OSAL_Free(pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
                    pRockchipPort->extendBufferHeader[i].OMXBufferHeader = NULL;
                    pBufferHdr = NULL;
                }
                pRockchipPort->bufferStateAllocate[i] = BUFFER_STATE_FREE;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pRockchipPort->assignedBufferNum == 0) {
            omx_trace("pRockchipPort->unloadedResource signal set");
            /* Rockchip_OSAL_MutexLock(pRockchipPort->compMutex); */
            Rockchip_OSAL_SemaphorePost(pRockchipPort->unloadedResource);
            /* Rockchip_OSAL_MutexUnlock(pRockchipPort->compMutex); */
            pRockchipPort->portDefinition.bPopulated = OMX_FALSE;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_AllocateTunnelBuffer(ROCKCHIP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                 ret = OMX_ErrorNone;
    (void)pOMXBasePort;
    (void)nPortIndex;
    ret = OMX_ErrorTunnelingUnsupported;
    goto EXIT;
EXIT:
    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_FreeTunnelBuffer(ROCKCHIP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    (void)pOMXBasePort;
    (void)nPortIndex;
    ret = OMX_ErrorTunnelingUnsupported;
    goto EXIT;
EXIT:
    return ret;
}
OMX_ERRORTYPE Rkvpu_OMX_ComponentTunnelRequest(
    OMX_IN OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32        nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32        nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    (void)hComp;
    (void)nPort;
    (void)hTunneledComp;
    (void)nTunneledPort;
    (void)pTunnelSetup;
    ret = OMX_ErrorTunnelingUnsupported;
    goto EXIT;
EXIT:
    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_GetFlushBuffer(ROCKCHIP_OMX_BASEPORT *pRockchipPort, ROCKCHIP_OMX_DATABUFFER *pDataBuffer[])
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    *pDataBuffer = NULL;

    if (pRockchipPort->portWayType == WAY1_PORT) {
        *pDataBuffer = &pRockchipPort->way.port1WayDataBuffer.dataBuffer;
    } else if (pRockchipPort->portWayType == WAY2_PORT) {
        pDataBuffer[0] = &(pRockchipPort->way.port2WayDataBuffer.inputDataBuffer);
        pDataBuffer[1] = &(pRockchipPort->way.port2WayDataBuffer.outputDataBuffer);
    }

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_FlushPort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;
    ROCKCHIP_OMX_DATABUFFER    *pDataPortBuffer[2] = {NULL, NULL};
    ROCKCHIP_OMX_MESSAGE       *message = NULL;
    OMX_S32                semValue = 0;
    int i = 0, maxBufferNum = 0;
    FunctionIn();

    pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

    while (Rockchip_OSAL_GetElemNum(&pRockchipPort->bufferQ) > 0) {
        Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->pRockchipPort[portIndex].bufferSemID, &semValue);
        if (semValue == 0)
            Rockchip_OSAL_SemaphorePost(pRockchipComponent->pRockchipPort[portIndex].bufferSemID);

        Rockchip_OSAL_SemaphoreWait(pRockchipComponent->pRockchipPort[portIndex].bufferSemID);
        message = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
        if ((message != NULL) && (message->messageType != ROCKCHIP_OMX_CommandFakeBuffer)) {
            bufferHeader = (OMX_BUFFERHEADERTYPE *)message->pCmdData;
            bufferHeader->nFilledLen = 0;

            if (portIndex == OUTPUT_PORT_INDEX) {
                Rockchip_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
            } else if (portIndex == INPUT_PORT_INDEX) {
                Rkvpu_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
            }
        }
        Rockchip_OSAL_Free(message);
        message = NULL;
    }

    Rkvpu_OMX_GetFlushBuffer(pRockchipPort, pDataPortBuffer);
    if (portIndex == INPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            Rkvpu_InputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            Rkvpu_InputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    } else if (portIndex == OUTPUT_PORT_INDEX) {
        if (pDataPortBuffer[0]->dataValid == OMX_TRUE)
            Rkvpu_OutputBufferReturn(pOMXComponent, pDataPortBuffer[0]);
        if (pDataPortBuffer[1]->dataValid == OMX_TRUE)
            Rkvpu_OutputBufferReturn(pOMXComponent, pDataPortBuffer[1]);
    }

    if (pRockchipComponent->bMultiThreadProcess == OMX_TRUE) {
        if (pRockchipPort->bufferProcessType == BUFFER_SHARE) {
            if (pRockchipPort->processData.bufferHeader != NULL) {
                if (portIndex == INPUT_PORT_INDEX) {
                    Rkvpu_OMX_InputBufferReturn(pOMXComponent, pRockchipPort->processData.bufferHeader);
                } else if (portIndex == OUTPUT_PORT_INDEX) {
                    Rockchip_OMX_OutputBufferReturn(pOMXComponent, pRockchipPort->processData.bufferHeader);
                }
            }
            Rockchip_ResetCodecData(&pRockchipPort->processData);

            maxBufferNum = pRockchipPort->portDefinition.nBufferCountActual;
            for (i = 0; i < maxBufferNum; i++) {
                if (pRockchipPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                    if (portIndex == OUTPUT_PORT_INDEX) {
                        Rockchip_OMX_OutputBufferReturn(pOMXComponent, pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
                    } else if (portIndex == INPUT_PORT_INDEX) {
                        Rkvpu_OMX_InputBufferReturn(pOMXComponent, pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
                    }
                }
            }
        }
    } else {
        Rockchip_ResetCodecData(&pRockchipPort->processData);
    }

    if ((pRockchipPort->bufferProcessType == BUFFER_SHARE) &&
        (portIndex == OUTPUT_PORT_INDEX)) {
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;

        if (pOMXComponent->pComponentPrivate == NULL) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
        pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
    }

    while (1) {
        OMX_S32 cnt = 0;
        Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->pRockchipPort[portIndex].bufferSemID, &cnt);
        if (cnt <= 0)
            break;
        Rockchip_OSAL_SemaphoreWait(pRockchipComponent->pRockchipPort[portIndex].bufferSemID);
    }
    Rockchip_OSAL_ResetQueue(&pRockchipPort->bufferQ);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_BufferFlush(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    ROCKCHIP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    omx_trace("OMX_CommandFlush start, port:%d", nPortIndex);

    pRockchipComponent->pRockchipPort[nPortIndex].bIsPortFlushed = OMX_TRUE;

    if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
        Rockchip_OSAL_SignalSet(pRockchipComponent->pauseEvent);
    } else {
        Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[nPortIndex].pauseEvent);
    }

    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];
    Rkvpu_OMX_GetFlushBuffer(pRockchipPort, flushPortBuffer);


    Rockchip_OSAL_SemaphorePost(pRockchipPort->bufferSemID);

    Rockchip_OSAL_MutexLock(flushPortBuffer[0]->bufferMutex);
    Rockchip_OSAL_MutexLock(flushPortBuffer[1]->bufferMutex);

    ret = Rkvpu_OMX_FlushPort(pOMXComponent, nPortIndex);

    Rockchip_ResetCodecData(&pRockchipPort->processData);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pRockchipComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pRockchipComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            Rockchip_OSAL_Memset(pRockchipComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
            Rockchip_OSAL_Memset(pRockchipComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pRockchipComponent->getAllDelayBuffer = OMX_FALSE;
            pRockchipComponent->bSaveFlagEOS = OMX_FALSE;
            pRockchipComponent->bBehaviorEOS = OMX_FALSE;
            pRockchipComponent->reInputData = OMX_FALSE;
        }

        pRockchipComponent->pRockchipPort[nPortIndex].bIsPortFlushed = OMX_FALSE;
        omx_trace("OMX_CommandFlush EventCmdComplete, port:%d", nPortIndex);
        if (bEvent == OMX_TRUE)
            pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandFlush, nPortIndex, NULL);
    }
    Rockchip_OSAL_MutexUnlock(flushPortBuffer[1]->bufferMutex);
    Rockchip_OSAL_MutexUnlock(flushPortBuffer[0]->bufferMutex);

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pRockchipComponent != NULL)) {
        omx_err("ERROR");
        pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pRockchipComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_ResolutionUpdate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent   = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT           *pInputPort         = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_BASEPORT           *pOutputPort        = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];

    pOutputPort->cropRectangle.nTop     = pOutputPort->newCropRectangle.nTop;
    pOutputPort->cropRectangle.nLeft    = pOutputPort->newCropRectangle.nLeft;
    pOutputPort->cropRectangle.nWidth   = pOutputPort->newCropRectangle.nWidth;
    pOutputPort->cropRectangle.nHeight  = pOutputPort->newCropRectangle.nHeight;

    pInputPort->portDefinition.format.video.nFrameWidth     = pInputPort->newPortDefinition.format.video.nFrameWidth;
    pInputPort->portDefinition.format.video.nFrameHeight    = pInputPort->newPortDefinition.format.video.nFrameHeight;
    pInputPort->portDefinition.format.video.nStride         = pInputPort->newPortDefinition.format.video.nStride;
    pInputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;

    pOutputPort->portDefinition.nBufferCountActual  = pOutputPort->newPortDefinition.nBufferCountActual;
    pOutputPort->portDefinition.nBufferCountMin     = pOutputPort->newPortDefinition.nBufferCountMin;

    UpdateFrameSize(pOMXComponent);

    return ret;
}

OMX_ERRORTYPE Rkvpu_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATABUFFER *dataBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *rockchipOMXInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        if (rockchipOMXInputPort->markType.hMarkTargetComponent != NULL ) {
            bufferHeader->hMarkTargetComponent      = rockchipOMXInputPort->markType.hMarkTargetComponent;
            bufferHeader->pMarkData                 = rockchipOMXInputPort->markType.pMarkData;
            rockchipOMXInputPort->markType.hMarkTargetComponent = NULL;
            rockchipOMXInputPort->markType.pMarkData = NULL;
        }

        if (bufferHeader->hMarkTargetComponent != NULL) {
            if (bufferHeader->hMarkTargetComponent == pOMXComponent) {
                pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                             pRockchipComponent->callbackData,
                                                             OMX_EventMark,
                                                             0, 0, bufferHeader->pMarkData);
            } else {
                pRockchipComponent->propagateMarkType.hMarkTargetComponent = bufferHeader->hMarkTargetComponent;
                pRockchipComponent->propagateMarkType.pMarkData = bufferHeader->pMarkData;
            }
        }

        bufferHeader->nFilledLen = 0;
        bufferHeader->nOffset = 0;
        Rkvpu_OMX_InputBufferReturn(pOMXComponent, bufferHeader);
    }

    /* reset dataBuffer */
    Rockchip_ResetDataBuffer(dataBuffer);

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_InputBufferGetQueue(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent)
{
    OMX_U32 ret = OMX_ErrorUndefined;
    ROCKCHIP_OMX_BASEPORT   *pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    ROCKCHIP_OMX_MESSAGE    *message = NULL;
    ROCKCHIP_OMX_DATABUFFER *inputUseBuffer = NULL;

    FunctionIn();

    inputUseBuffer = &(pRockchipPort->way.port2WayDataBuffer.inputDataBuffer);

    if (pRockchipComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pRockchipPort))) {
        Rockchip_OSAL_SemaphoreWait(pRockchipPort->bufferSemID);
        if (inputUseBuffer->dataValid != OMX_TRUE) {
            message = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
            if (message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->messageType == ROCKCHIP_OMX_CommandFakeBuffer) {
                Rockchip_OSAL_Free(message);
                ret = OMX_ErrorCodecFlush;
                goto EXIT;
            }

            inputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            inputUseBuffer->allocSize     = inputUseBuffer->bufferHeader->nAllocLen;
            inputUseBuffer->dataLen       = inputUseBuffer->bufferHeader->nFilledLen;
            inputUseBuffer->remainDataLen = inputUseBuffer->dataLen;
            inputUseBuffer->usedDataLen   = 0;
            inputUseBuffer->dataValid     = OMX_TRUE;
            inputUseBuffer->nFlags        = inputUseBuffer->bufferHeader->nFlags;
            inputUseBuffer->timeStamp     = inputUseBuffer->bufferHeader->nTimeStamp;

            Rockchip_OSAL_Free(message);

            if (inputUseBuffer->allocSize <= inputUseBuffer->dataLen)
                omx_trace("Input Buffer Full, Check input buffer size! allocSize:%d, dataLen:%d", inputUseBuffer->allocSize, inputUseBuffer->dataLen);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATABUFFER *dataBuffer)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *rockchipOMXOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader != NULL) {
        bufferHeader->nFilledLen = dataBuffer->remainDataLen;
        bufferHeader->nOffset    = 0;
        bufferHeader->nFlags     = dataBuffer->nFlags;
        bufferHeader->nTimeStamp = dataBuffer->timeStamp;

        if ((rockchipOMXOutputPort->bStoreMetaData == OMX_TRUE) && (bufferHeader->nFilledLen > 0))
            bufferHeader->nFilledLen = bufferHeader->nAllocLen;

        if (pRockchipComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            bufferHeader->hMarkTargetComponent = pRockchipComponent->propagateMarkType.hMarkTargetComponent;
            bufferHeader->pMarkData = pRockchipComponent->propagateMarkType.pMarkData;
            pRockchipComponent->propagateMarkType.hMarkTargetComponent = NULL;
            pRockchipComponent->propagateMarkType.pMarkData = NULL;
        }

        if ((bufferHeader->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            omx_trace("event OMX_BUFFERFLAG_EOS!!!");
            pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventBufferFlag,
                                                         OUTPUT_PORT_INDEX,
                                                         bufferHeader->nFlags, NULL);
        }

        Rockchip_OMX_OutputBufferReturn(pOMXComponent, bufferHeader);
    }

    /* reset dataBuffer */
    Rockchip_ResetDataBuffer(dataBuffer);

    goto EXIT;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OutputBufferGetQueue(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent)
{
    OMX_U32       ret = OMX_ErrorUndefined;
    ROCKCHIP_OMX_BASEPORT   *pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    ROCKCHIP_OMX_MESSAGE    *message = NULL;
    ROCKCHIP_OMX_DATABUFFER *outputUseBuffer = NULL;

    FunctionIn();

    outputUseBuffer = &(pRockchipPort->way.port2WayDataBuffer.outputDataBuffer);

    if (pRockchipComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pRockchipPort))) {
        Rockchip_OSAL_SemaphoreWait(pRockchipPort->bufferSemID);
        if (outputUseBuffer->dataValid != OMX_TRUE) {
            message = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
            if (message == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (message->messageType == ROCKCHIP_OMX_CommandFakeBuffer) {
                Rockchip_OSAL_Free(message);
                ret = OMX_ErrorCodecFlush;
                goto EXIT;
            }

            outputUseBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
            outputUseBuffer->allocSize     = outputUseBuffer->bufferHeader->nAllocLen;
            outputUseBuffer->dataLen       = 0; //dataBuffer->bufferHeader->nFilledLen;
            outputUseBuffer->remainDataLen = outputUseBuffer->dataLen;
            outputUseBuffer->usedDataLen   = 0; //dataBuffer->bufferHeader->nOffset;
            outputUseBuffer->dataValid     = OMX_TRUE;
            Rockchip_OSAL_Free(message);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;

}

OMX_BUFFERHEADERTYPE *Rkvpu_OutputBufferGetQueue_Direct(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent)
{
    OMX_BUFFERHEADERTYPE  *retBuffer = NULL;
    ROCKCHIP_OMX_BASEPORT   *pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    ROCKCHIP_OMX_MESSAGE    *message = NULL;

    FunctionIn();

    if (pRockchipComponent->currentState != OMX_StateExecuting) {
        retBuffer = NULL;
        goto EXIT;
    } else if ((pRockchipComponent->transientState != ROCKCHIP_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pRockchipPort))) {
        Rockchip_OSAL_SemaphoreWait(pRockchipPort->bufferSemID);

        message = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
        if (message == NULL) {
            retBuffer = NULL;
            goto EXIT;
        }
        if (message->messageType == ROCKCHIP_OMX_CommandFakeBuffer) {
            Rockchip_OSAL_Free(message);
            retBuffer = NULL;
            goto EXIT;
        }

        retBuffer  = (OMX_BUFFERHEADERTYPE *)(message->pCmdData);
        Rockchip_OSAL_Free(message);
    }

EXIT:
    FunctionOut();

    return retBuffer;
}

OMX_ERRORTYPE Rkvpu_CodecBufferReset(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 PortIndex)
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASEPORT   *pRockchipPort = NULL;

    FunctionIn();

    pRockchipPort = &pRockchipComponent->pRockchipPort[PortIndex];

    ret = Rockchip_OSAL_ResetQueue(&pRockchipPort->codecBufferQ);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    while (1) {
        OMX_S32 cnt = 0;
        Rockchip_OSAL_Get_SemaphoreCount(pRockchipPort->codecSemID, &cnt);
        if (cnt > 0)
            Rockchip_OSAL_SemaphoreWait(pRockchipPort->codecSemID);
        else
            break;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pRockchipComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch ((OMX_U32)nParamIndex) {
    case OMX_IndexParamVideoInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = Rockchip_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        portParam->nPorts           = pRockchipComponent->portParam.nPorts;
        portParam->nStartPortNumber = pRockchipComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *portFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         portIndex = portFormat->nPortIndex;
        OMX_U32                         index    = portFormat->nIndex;
        ROCKCHIP_OMX_BASEPORT            *pRockchipPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;
        OMX_U32                         supportFormatNum = 0;

        ret = Rockchip_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pRockchipComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }


        if (portIndex == INPUT_PORT_INDEX) {
            pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
            portDefinition = &pRockchipPort->portDefinition;

            switch (index) {
            case supportFormat_0:
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                break;
            case supportFormat_1:
                portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                portFormat->eColorFormat       = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque;
                portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                break;
            default:
                if (index > supportFormat_0) {
                    ret = OMX_ErrorNoMore;
                    goto EXIT;
                }
                break;
            }
        } else if (portIndex == OUTPUT_PORT_INDEX) {
            supportFormatNum = OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (index > supportFormatNum) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
            portDefinition = &pRockchipPort->portDefinition;

            portFormat->eCompressionFormat = portDefinition->format.video.eCompressionFormat;
            portFormat->eColorFormat       = portDefinition->format.video.eColorFormat;
            portFormat->xFramerate         = portDefinition->format.video.xFramerate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE     *videoRateControl = (OMX_VIDEO_PARAM_BITRATETYPE *)ComponentParameterStructure;
        OMX_U32                          portIndex = videoRateControl->nPortIndex;
        ROCKCHIP_OMX_BASEPORT             *pRockchipPort = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT   *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE    *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            portDefinition = &pRockchipPort->portDefinition;

            videoRateControl->eControlRate = pVideoEnc->eControlRate[portIndex];
            videoRateControl->nTargetBitrate = portDefinition->format.video.nBitrate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoQuantization: {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE  *videoQuantizationControl = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)ComponentParameterStructure;
        OMX_U32                            portIndex = videoQuantizationControl->nPortIndex;
        ROCKCHIP_OMX_BASEPORT               *pRockchipPort = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT     *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE      *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            portDefinition = &pRockchipPort->portDefinition;

            videoQuantizationControl->nQpI = pVideoEnc->quantization.nQpI;
            videoQuantizationControl->nQpP = pVideoEnc->quantization.nQpP;
            videoQuantizationControl->nQpB = pVideoEnc->quantization.nQpB;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = portDefinition->nPortIndex;
        ROCKCHIP_OMX_BASEPORT          *pRockchipPort;

        if (portIndex >= pRockchipComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Rockchip_OMX_Check_SizeVersion(portDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
        Rockchip_OSAL_Memcpy(portDefinition, &pRockchipPort->portDefinition, portDefinition->nSize);
    }
    break;
    case OMX_IndexParamVideoIntraRefresh: {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)ComponentParameterStructure;
        OMX_U32                           portIndex = pIntraRefresh->nPortIndex;
        RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;

        if (portIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            pIntraRefresh->eRefreshMode = pVideoEnc->intraRefresh.eRefreshMode;
            pIntraRefresh->nAirMBs = pVideoEnc->intraRefresh.nAirMBs;
            pIntraRefresh->nAirRef = pVideoEnc->intraRefresh.nAirRef;
            pIntraRefresh->nCirMBs = pVideoEnc->intraRefresh.nCirMBs;
        }
        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParameterStructure;
        ret = Rockchip_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_H264_ENC_ROLE);
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingVP8) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE);
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingHEVC) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_ENC_ROLE);
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(pDstAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pVideoEnc->AVCComponent[pDstAVCComponent->nPortIndex];
        Rockchip_OSAL_Memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }
    break;
    case OMX_IndexParamVideoHevc: {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHEVCComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHEVCComponent = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(pDstHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstHEVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcHEVCComponent = &pVideoEnc->HEVCComponent[pDstHEVCComponent->nPortIndex];
        Rockchip_OSAL_Memcpy(pDstHEVCComponent, pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
    }
    break;
    case OMX_IndexParamVideoProfileLevelQuerySupported: {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) ComponentParameterStructure;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

        OMX_U32 index = profileLevel->nProfileIndex;
        OMX_U32 nProfileLevels = 0;
        if (profileLevel->nPortIndex  >= ALL_PORT_NUM) {
            omx_err("Invalid port index: %ld", profileLevel->nPortIndex);
            ret = OMX_ErrorUnsupportedIndex;
            goto EXIT;
        }
        if (pVideoEnc->codecId == OMX_VIDEO_CodingAVC) {
            nProfileLevels = ARRAY_SIZE(kProfileLevels);
            if (index >= nProfileLevels) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            profileLevel->eProfile = kProfileLevels[index].mProfile;
            profileLevel->eLevel = kProfileLevels[index].mLevel;
        } else if (pVideoEnc->codecId == OMX_VIDEO_CodingHEVC) {
            nProfileLevels = ARRAY_SIZE(kH265ProfileLevels);
            if (index >= nProfileLevels) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }
            profileLevel->eProfile = kH265ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH265ProfileLevels[index].mLevel;
        } else {
            ret = OMX_ErrorNoMore;
            goto EXIT;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamRkEncExtendedVideo: {   // extern for huawei param setting
        OMX_VIDEO_PARAMS_EXTENDED  *params_extend = (OMX_VIDEO_PARAMS_EXTENDED *)ComponentParameterStructure;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        omx_trace("get OMX_IndexParamRkEncExtendedVideo in ");
        Rockchip_OSAL_MutexLock(pVideoEnc->bScale_Mutex);
        Rockchip_OSAL_Memcpy(params_extend, &pVideoEnc->params_extend, sizeof(OMX_VIDEO_PARAMS_EXTENDED));
        Rockchip_OSAL_MutexUnlock(pVideoEnc->bScale_Mutex);
    }
    break;


    default: {
        ret = Rockchip_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    break;
    }

EXIT:
    FunctionOut();

    return ret;
}
OMX_ERRORTYPE Rkvpu_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pRockchipComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch ((OMX_U32)nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *portFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)ComponentParameterStructure;
        OMX_U32                         portIndex = portFormat->nPortIndex;
        ROCKCHIP_OMX_BASEPORT            *pRockchipPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;

        ret = Rockchip_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pRockchipComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            portDefinition = &pRockchipPort->portDefinition;
            portDefinition->format.video.eColorFormat       = portFormat->eColorFormat;
            portDefinition->format.video.eCompressionFormat = portFormat->eCompressionFormat;
            portDefinition->format.video.xFramerate         = portFormat->xFramerate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE     *videoRateControl = (OMX_VIDEO_PARAM_BITRATETYPE *)ComponentParameterStructure;
        OMX_U32                          portIndex = videoRateControl->nPortIndex;
        ROCKCHIP_OMX_BASEPORT             *pRockchipPort = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT   *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE    *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            portDefinition = &pRockchipPort->portDefinition;

            pVideoEnc->eControlRate[portIndex] = videoRateControl->eControlRate;
            portDefinition->format.video.nBitrate = videoRateControl->nTargetBitrate;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoQuantization: {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE *videoQuantizationControl = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)ComponentParameterStructure;
        OMX_U32                           portIndex = videoQuantizationControl->nPortIndex;
        ROCKCHIP_OMX_BASEPORT              *pRockchipPort = NULL;
        RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE     *portDefinition = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            portDefinition = &pRockchipPort->portDefinition;

            pVideoEnc->quantization.nQpI = videoQuantizationControl->nQpI;
            pVideoEnc->quantization.nQpP = videoQuantizationControl->nQpP;
            pVideoEnc->quantization.nQpB = videoQuantizationControl->nQpB;
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = pPortDefinition->nPortIndex;
        ROCKCHIP_OMX_BASEPORT          *pRockchipPort;

        if (portIndex >= pRockchipComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Rockchip_OMX_Check_SizeVersion(pPortDefinition, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

        if ((pRockchipComponent->currentState != OMX_StateLoaded) && (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
            if (pRockchipPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }
        if (pPortDefinition->nBufferCountActual < pRockchipPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Rockchip_OSAL_Memcpy(&pRockchipPort->portDefinition, pPortDefinition, pPortDefinition->nSize);
        if (portIndex == INPUT_PORT_INDEX) {
            ROCKCHIP_OMX_BASEPORT *pRockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
            UpdateFrameSize(pOMXComponent);
            omx_trace("pRockchipOutputPort->portDefinition.nBufferSize: %d",
                      pRockchipOutputPort->portDefinition.nBufferSize);
        }
        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoIntraRefresh: {
        OMX_VIDEO_PARAM_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)ComponentParameterStructure;
        OMX_U32                           portIndex = pIntraRefresh->nPortIndex;
        RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;

        if (portIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
            if (pIntraRefresh->eRefreshMode == OMX_VIDEO_IntraRefreshCyclic) {
                pVideoEnc->intraRefresh.eRefreshMode = pIntraRefresh->eRefreshMode;
                pVideoEnc->intraRefresh.nCirMBs = pIntraRefresh->nCirMBs;
                omx_trace("OMX_VIDEO_IntraRefreshCyclic Enable, nCirMBs: %d",
                          pVideoEnc->intraRefresh.nCirMBs);
            } else {
                ret = OMX_ErrorUnsupportedSetting;
                goto EXIT;
            }
        }
        ret = OMX_ErrorNone;
    }
    break;

#ifdef USE_STOREMETADATA
    case OMX_IndexParamStoreANWBuffer:
    case OMX_IndexParamStoreMetaDataBuffer: {
        ret = Rockchip_OSAL_SetANBParameter(hComponent, nIndex, ComponentParameterStructure);

    }
    break;
#endif
    case OMX_IndexParamPrependSPSPPSToIDR: {
#if 0
        RKON2_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (RKON2_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        PrependSPSPPSToIDRFramesParams *PrependParams =
            (PrependSPSPPSToIDRFramesParams*)ComponentParameterStructure;
        omx_trace("OMX_IndexParamPrependSPSPPSToIDR set true");

        pVideoEnc->bPrependSpsPpsToIdr = PrependParams->bEnable;

        return OMX_ErrorNone;
#endif
        RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        ret = Rockchip_OSAL_SetPrependSPSPPSToIDR(ComponentParameterStructure, &pVideoEnc->bPrependSpsPpsToIdr);
    }
    break;

    case OMX_IndexRkEncExtendedWfdState: {
        RKVPU_OMX_VIDEOENC_COMPONENT    *pVideoEnc = NULL;
        pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        ROCKCHIP_OMX_WFD *pRkWFD = (ROCKCHIP_OMX_WFD*)ComponentParameterStructure;
        pVideoEnc->bRkWFD = pRkWFD->bEnable;
        omx_trace("OMX_IndexRkEncExtendedWfdState set as:%d", pRkWFD->bEnable);
        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)ComponentParameterStructure;

        ret = Rockchip_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pRockchipComponent->currentState != OMX_StateLoaded) && (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_H264_ENC_ROLE)) {
            pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE)) {
            pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_ENC_ROLE)) {
            pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else {
            ret = OMX_ErrorInvalidComponentName;
            goto EXIT;
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = NULL;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        ret = Rockchip_OMX_Check_SizeVersion(pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pVideoEnc->AVCComponent[pSrcAVCComponent->nPortIndex];

        Rockchip_OSAL_Memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }

    break;
    case OMX_IndexParamVideoHevc: {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHEVCComponent = NULL;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHEVCComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;
        ret = Rockchip_OMX_Check_SizeVersion(pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcHEVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstHEVCComponent = &pVideoEnc->HEVCComponent[pSrcHEVCComponent->nPortIndex];
        Rockchip_OSAL_Memcpy(pDstHEVCComponent, pSrcHEVCComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
    }
    break;
    case OMX_IndexParamRkEncExtendedVideo: {   // extern for huawei param setting
        OMX_VIDEO_PARAMS_EXTENDED  *params_extend = (OMX_VIDEO_PARAMS_EXTENDED *)ComponentParameterStructure;
        RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

        omx_trace("OMX_IndexParamRkEncExtendedVideo in ");
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        Rockchip_OSAL_MutexLock(pVideoEnc->bScale_Mutex);
        Rockchip_OSAL_Memcpy(&pVideoEnc->params_extend, params_extend, sizeof(OMX_VIDEO_PARAMS_EXTENDED));
        omx_trace("OMX_IndexParamRkEncExtendedVideo in flags %d bEableCrop %d,cl %d cr %d ct %d cb %d, bScaling %d ScaleW %d ScaleH %d",
                  pVideoEnc->params_extend.ui32Flags, pVideoEnc->params_extend.bEnableCropping, pVideoEnc->params_extend.ui16CropLeft, pVideoEnc->params_extend.ui16CropRight,
                  pVideoEnc->params_extend.ui16CropTop, pVideoEnc->params_extend.ui16CropBottom, pVideoEnc->params_extend.bEnableScaling,
                  pVideoEnc->params_extend.ui16ScaledWidth, pVideoEnc->params_extend.ui16ScaledHeight);
        Rockchip_OSAL_MutexUnlock(pVideoEnc->bScale_Mutex);
    }
    break;
    default: {
        ret = Rockchip_OMX_SetParameter(hComponent, nIndex, ComponentParameterStructure);
    }
    break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }
    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    switch (nIndex) {
    case OMX_IndexConfigVideoAVCIntraPeriod: {

        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32           portIndex = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pAVCIntraPeriod->nIDRPeriod = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames + 1;
            pAVCIntraPeriod->nPFrames = pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames;
        }
    }
    break;
    case OMX_IndexConfigVideoBitrate: {
        OMX_VIDEO_CONFIG_BITRATETYPE *pEncodeBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       portIndex = pEncodeBitrate->nPortIndex;
        ROCKCHIP_OMX_BASEPORT          *pRockchipPort = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            pEncodeBitrate->nEncodeBitrate = pRockchipPort->portDefinition.format.video.nBitrate;
        }
    }
    break;
    case OMX_IndexConfigVideoFramerate: {
        OMX_CONFIG_FRAMERATETYPE *pFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   portIndex = pFramerate->nPortIndex;
        ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            pFramerate->xEncodeFramerate = pRockchipPort->portDefinition.format.video.xFramerate;
        }
    }
    break;
#ifdef AVS80
    case OMX_IndexParamRkDescribeColorAspects: {
        OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *pParam = (OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *)pComponentConfigStructure;
        if (pParam->bRequestingDataSpace) {
            pParam->sAspects.mPrimaries = PrimariesUnspecified;
            pParam->sAspects.mRange = RangeUnspecified;
            pParam->sAspects.mTransfer = TransferUnspecified;
            pParam->sAspects.mMatrixCoeffs = MatrixUnspecified;
            return ret;//OMX_ErrorUnsupportedSetting;
        }
        if (pParam->bDataSpaceChanged == OMX_TRUE) {
            // If the dataspace says RGB, recommend 601-limited;
            // since that is the destination colorspace that C2D or Venus will convert to.
            if (pParam->nPixelFormat == HAL_PIXEL_FORMAT_RGBA_8888) {
                memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
                pParam->sAspects.mPrimaries = PrimariesUnspecified;
                pParam->sAspects.mRange = RangeUnspecified;
                pParam->sAspects.mTransfer = TransferUnspecified;
                pParam->sAspects.mMatrixCoeffs = MatrixUnspecified;
            } else {
                memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
            }
        } else {
            memcpy(pParam, &pVideoEnc->ConfigColorAspects, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
        }
    }
    break;
#endif
    default:
        ret = Rockchip_OMX_GetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    RKVPU_OMX_VIDEOENC_COMPONENT *pVideoEnc = (RKVPU_OMX_VIDEOENC_COMPONENT *)pRockchipComponent->hComponentHandle;

    switch ((OMX_U32)nIndex) {
    case OMX_IndexConfigVideoIntraPeriod: {
        OMX_U32 nPFrames = (*((OMX_U32 *)pComponentConfigStructure)) - 1;

        pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = nPFrames;

        ret = OMX_ErrorNone;
    }
    break;
    case OMX_IndexConfigVideoAVCIntraPeriod: {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pAVCIntraPeriod = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentConfigStructure;
        OMX_U32           portIndex = pAVCIntraPeriod->nPortIndex;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            if (pAVCIntraPeriod->nIDRPeriod == (pAVCIntraPeriod->nPFrames + 1))
                pVideoEnc->AVCComponent[OUTPUT_PORT_INDEX].nPFrames = pAVCIntraPeriod->nPFrames;
            else {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
    }
    break;
    case OMX_IndexConfigVideoBitrate: {
        OMX_VIDEO_CONFIG_BITRATETYPE *pEncodeBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       portIndex = pEncodeBitrate->nPortIndex;
        ROCKCHIP_OMX_BASEPORT          *pRockchipPort = NULL;
        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            pRockchipPort->portDefinition.format.video.nBitrate = pEncodeBitrate->nEncodeBitrate;
            if (p_vpu_ctx !=  NULL) {
                EncParameter_t vpug;
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
                vpug.bitRate = pEncodeBitrate->nEncodeBitrate;
                omx_err("set bitRate %d", pEncodeBitrate->nEncodeBitrate);
                p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
            }
        }
    }
    break;
    case OMX_IndexConfigVideoFramerate: {
        OMX_CONFIG_FRAMERATETYPE *pFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   portIndex = pFramerate->nPortIndex;
        ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
            pRockchipPort->portDefinition.format.video.xFramerate = pFramerate->xEncodeFramerate;
        }

        if (p_vpu_ctx !=  NULL) {
            EncParameter_t vpug;
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_GETCFG, (void*)&vpug);
            vpug.framerate = (pFramerate->xEncodeFramerate >> 16);
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETCFG, (void*)&vpug);
        }
    }
    break;
    case OMX_IndexConfigVideoIntraVOPRefresh: {
        OMX_CONFIG_INTRAREFRESHVOPTYPE *pIntraRefreshVOP = (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentConfigStructure;
        OMX_U32 portIndex = pIntraRefreshVOP->nPortIndex;

        VpuCodecContext_t *p_vpu_ctx = pVideoEnc->vpu_ctx;

        if ((portIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc->IntraRefreshVOP = pIntraRefreshVOP->IntraRefreshVOP;
        }

        if (p_vpu_ctx !=  NULL && pVideoEnc->IntraRefreshVOP) {
            p_vpu_ctx->control(p_vpu_ctx, VPU_API_ENC_SETIDRFRAME, NULL);
        }
    }
    break;
#ifdef AVS80
    case OMX_IndexParamRkDescribeColorAspects: {
        memcpy(&pVideoEnc->ConfigColorAspects, pComponentConfigStructure, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
        pVideoEnc->bIsCfgColorAsp = OMX_TRUE;
    }
    break;
#endif
    default:
        ret = Rockchip_OMX_SetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nIndex == 0) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_H264_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 1) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_VP8_ENC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }
EXIT:
    FunctionOut();

    return ret;
}


OMX_ERRORTYPE Rkvpu_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();
    omx_trace("cParameterName:%s", cParameterName);

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if ((cParameterName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_CONFIG_VIDEO_INTRAPERIOD) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexConfigVideoIntraPeriod;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_PREPEND_SPSPPS_TO_IDR) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamPrependSPSPPSToIDR;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (!Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_RKWFD)) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexRkEncExtendedWfdState;
        ret = OMX_ErrorNone;
        goto EXIT;
    } else if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_EXTENDED_VIDEO) == 0) {

        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkEncExtendedVideo;
        ret = OMX_ErrorNone;
    }
#ifdef AVS80
    else if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_DSECRIBECOLORASPECTS) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDescribeColorAspects;
        goto EXIT;
    }
#endif
#ifdef USE_STOREMETADATA
    else if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_STORE_ANW_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamStoreANWBuffer;
        ret = OMX_ErrorNone;
    } else if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamStoreMetaDataBuffer;
        goto EXIT;
    } else {
        ret = Rockchip_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);
    }
#else
    else {
        ret = Rockchip_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);
    }
#endif

EXIT:
    FunctionOut();
    return ret;
}
