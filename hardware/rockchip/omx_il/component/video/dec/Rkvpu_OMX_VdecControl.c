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
 * @file        Rkvpu_OMX_VdecControl.c
 * @brief
 * @author      csy (csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.28 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_vdec_ctl"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include<sys/ioctl.h>
#include <unistd.h>

#include "Rockchip_OMX_Macros.h"
#include "Rockchip_OSAL_Event.h"
#include "Rkvpu_OMX_Vdec.h"
#include "Rkvpu_OMX_VdecControl.h"
#include "Rockchip_OSAL_RGA_Process.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "Rockchip_OSAL_Thread.h"
#include "Rockchip_OSAL_Semaphore.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OSAL_Queue.h"
#include "Rockchip_OSAL_SharedMemory.h"
#include "Rockchip_OSAL_ColorUtils.h"


#include <hardware/hardware.h>
#include "hardware/rga.h"
#include "vpu.h"
#include "vpu_mem_pool.h"
#include "vpu_mem.h"
#include "omx_video_global.h"
#if AVS100
#include <errno.h>
#include "drmrga.h"
#include "RgaApi.h"
#endif
#ifdef USE_ANB
#include "Rockchip_OSAL_Android.h"
#endif

#include "Rockchip_OSAL_Log.h"

typedef struct {
    OMX_U32 mProfile;
    OMX_U32 mLevel;
} CodecProfileLevel;

static const CodecProfileLevel kM2VProfileLevels[] = {
    { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelHL },
    { OMX_VIDEO_MPEG2ProfileMain,   OMX_VIDEO_MPEG2LevelHL},
};

static const CodecProfileLevel kM4VProfileLevels[] = {
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b},
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2 },
    { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3 },
};

static const CodecProfileLevel kH263ProfileLevels[] = {
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level20 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level30 },
    { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level45 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level10 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level20 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level30 },
    { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level45 },
};

//only report echo profile highest level, Reference soft avc dec
static const CodecProfileLevel kH264ProfileLevelsMax[] = {
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
    { OMX_VIDEO_HEVCProfileMain10, OMX_VIDEO_HEVCMainTierLevel51 },
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
            pRockchipPort->extendBufferHeader[i].pRegisterFlag = 0;
            pRockchipPort->extendBufferHeader[i].pPrivate = NULL;
            pRockchipPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(temp_bufferHeader, OMX_BUFFERHEADERTYPE);
            temp_bufferHeader->pBuffer        = pBuffer;
            temp_bufferHeader->nAllocLen      = nSizeBytes;
            temp_bufferHeader->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                temp_bufferHeader->nInputPortIndex = INPUT_PORT_INDEX;
            else {
                omx_trace("bufferHeader[%d] = 0x%x ", i, temp_bufferHeader);
                temp_bufferHeader->nOutputPortIndex = OUTPUT_PORT_INDEX;
            }
            VIDEO_DBG(VIDEO_DBG_LOG_BUFFER, "[%s]: Using %s buffer from OMX AL, count: %d, index: %d, buffer: %p, size: %d",
                      pRockchipComponent->componentName,
                      nPortIndex == INPUT_PORT_INDEX ? "input" : "output",
                      pRockchipPort->portDefinition.nBufferCountActual,
                      i,
                      pBuffer,
                      nSizeBytes);

            pRockchipPort->assignedBufferNum++;
            if (pRockchipPort->assignedBufferNum == pRockchipPort->portDefinition.nBufferCountActual) {
                pRockchipPort->portDefinition.bPopulated = OMX_TRUE;
                Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BUFFERHEADERTYPE  *temp_bufferHeader = NULL;
    OMX_U8                *temp_buffer = NULL;
    int                    temp_buffer_fd = -1;
    OMX_U32                i = 0;
    MEMORY_TYPE            mem_type = NORMAL_MEMORY;

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
    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];
    if (nPortIndex >= pRockchipComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

#if 1
    if ((pVideoDec->bDRMPlayerMode == OMX_TRUE) && (nPortIndex == INPUT_PORT_INDEX)) {
        mem_type = SECURE_MEMORY;
    } else if (pRockchipPort->bufferProcessType == BUFFER_SHARE) {
        mem_type = NORMAL_MEMORY;
    } else {
        mem_type = SYSTEM_MEMORY;
    }
#endif

    if (pVideoDec->bDRMPlayerMode == OMX_TRUE) {
        omx_trace("Rkvpu_OMX_AllocateBuffer bDRMPlayerMode");
        temp_buffer = (OMX_U8 *)Rockchip_OSAL_SharedMemory_Alloc(pVideoDec->hSharedMemory, nSizeBytes, mem_type);
        if (temp_buffer == NULL) {
            omx_err("Rkvpu_OMX_AllocateBuffer bDRMPlayerMode error");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    } else {
        temp_buffer = (OMX_U8 *)Rockchip_OSAL_Malloc(nSizeBytes);
        if (temp_buffer == NULL) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
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
            omx_err("buf_fd: 0x%x, OMXBufferHeader:%p", temp_buffer_fd, temp_bufferHeader);
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
                Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
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
    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
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
                    if (pVideoDec->bDRMPlayerMode != 1)
                        Rockchip_OSAL_Free(pRockchipPort->extendBufferHeader[i].OMXBufferHeader->pBuffer);
                    pRockchipPort->extendBufferHeader[i].OMXBufferHeader->pBuffer = NULL;
                    pBufferHdr->pBuffer = NULL;
                } else if (pRockchipPort->bufferStateAllocate[i] & BUFFER_STATE_ASSIGNED) {
                    ; /* None*/
                }
                VIDEO_DBG(VIDEO_DBG_LOG_BUFFER, "[%s]: free %s buffer, count: %d, index: %d, buffer: %p, size: %d",
                          pRockchipComponent->componentName,
                          nPortIndex == INPUT_PORT_INDEX ? "input" : "output",
                          pRockchipPort->portDefinition.nBufferCountActual,
                          i,
                          pBufferHdr->pBuffer,
                          pBufferHdr->nAllocLen);

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
    (void) pOMXBasePort;
    (void) nPortIndex;

    ret = OMX_ErrorTunnelingUnsupported;
    goto EXIT;
EXIT:
    return ret;
}

OMX_ERRORTYPE Rkvpu_OMX_FreeTunnelBuffer(ROCKCHIP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                 ret = OMX_ErrorNone;
    (void) pOMXBasePort;
    (void) nPortIndex;
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
    (void) hComp;
    (void) nPort;
    (void) hTunneledComp;
    (void) nTunneledPort;
    (void) pTunnelSetup;
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
                pRockchipPort->extendBufferHeader[i].pRegisterFlag = 0;
                pRockchipPort->extendBufferHeader[i].buf_fd[0] = 0;
                if (pRockchipPort->extendBufferHeader[i].pPrivate != NULL) {
                    Rockchip_OSAL_FreeVpumem(pRockchipPort->extendBufferHeader[i].pPrivate);
                    pRockchipPort->extendBufferHeader[i].pPrivate = NULL;
                }

                if (pRockchipPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                    if (portIndex == OUTPUT_PORT_INDEX) {
                        Rockchip_OMX_OutputBufferReturn(pOMXComponent, pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
                    } else if (portIndex == INPUT_PORT_INDEX) {
                        Rkvpu_OMX_InputBufferReturn(pOMXComponent, pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
                    }
                }
            }
            Rockchip_OSAL_resetVpumemPool(pRockchipComponent);
        }
    } else {
        Rockchip_ResetCodecData(&pRockchipPort->processData);
    }

    if ((pRockchipPort->bufferProcessType == BUFFER_SHARE) &&
        (portIndex == OUTPUT_PORT_INDEX)) {
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
        if (pOMXComponent->pComponentPrivate == NULL) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
        pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    ROCKCHIP_OMX_DATABUFFER    *flushPortBuffer[2] = {NULL, NULL};
    ROCKCHIP_OMX_BASEPORT      *pInputPort  = NULL;

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
    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    pInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];

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

    VpuCodecContext_t *p_vpu_ctx = pVideoDec->vpu_ctx;
    if (pRockchipComponent->nRkFlags & RK_VPU_NEED_FLUSH_ON_SEEK) {
        p_vpu_ctx->flush(p_vpu_ctx);
        pRockchipComponent->nRkFlags &= ~RK_VPU_NEED_FLUSH_ON_SEEK;
        Rockchip_OSAL_MutexLock(pInputPort->secureBufferMutex);
        pVideoDec->invalidCount = 0;
        Rockchip_OSAL_MutexUnlock(pInputPort->secureBufferMutex);
    }

    omx_trace("OMX_CommandFlush start, port:%d", nPortIndex);
    Rockchip_ResetCodecData(&pRockchipPort->processData);

    Rockchip_OSAL_MutexLock(pInputPort->secureBufferMutex);
    if (pVideoDec->bDRMPlayerMode == OMX_TRUE && pVideoDec->bInfoChange == OMX_FALSE) {
        int securebufferNum = Rockchip_OSAL_GetElemNum(&pInputPort->securebufferQ);
        omx_trace("Rkvpu_OMX_BufferFlush in securebufferNum = %d", securebufferNum);
        while (securebufferNum != 0) {
            ROCKCHIP_OMX_DATABUFFER *securebuffer = (ROCKCHIP_OMX_DATABUFFER *) Rockchip_OSAL_Dequeue(&pInputPort->securebufferQ);
            Rkvpu_InputBufferReturn(pOMXComponent, securebuffer);
            Rockchip_OSAL_Free(securebuffer);
            securebufferNum = Rockchip_OSAL_GetElemNum(&pInputPort->securebufferQ);
        }
        omx_trace("Rkvpu_OMX_BufferFlush out securebufferNum = %d", securebufferNum);
    }
    Rockchip_OSAL_MutexUnlock(pInputPort->secureBufferMutex);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pRockchipComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pRockchipComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            Rockchip_OSAL_Memset(pRockchipComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
            Rockchip_OSAL_Memset(pRockchipComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pRockchipComponent->getAllDelayBuffer = OMX_FALSE;
            pRockchipComponent->bSaveFlagEOS = OMX_FALSE;
            pRockchipComponent->bBehaviorEOS = OMX_FALSE;
            pVideoDec->bDecSendEOS = OMX_FALSE;
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
    if (pVideoDec->bInfoChange == OMX_TRUE)
        pVideoDec->bInfoChange = OMX_FALSE;
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
    RKVPU_OMX_VIDEODEC_COMPONENT    *pVideoDec          = NULL;

    pOutputPort->cropRectangle.nTop     = pOutputPort->newCropRectangle.nTop;
    pOutputPort->cropRectangle.nLeft    = pOutputPort->newCropRectangle.nLeft;
    pOutputPort->cropRectangle.nWidth   = pOutputPort->newCropRectangle.nWidth;
    pOutputPort->cropRectangle.nHeight  = pOutputPort->newCropRectangle.nHeight;

    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

    pInputPort->portDefinition.format.video.nFrameWidth     = pInputPort->newPortDefinition.format.video.nFrameWidth;
    pInputPort->portDefinition.format.video.nFrameHeight    = pInputPort->newPortDefinition.format.video.nFrameHeight;
    pInputPort->portDefinition.format.video.nStride         = pInputPort->newPortDefinition.format.video.nStride;
    pOutputPort->portDefinition.format.video.nStride        = pInputPort->newPortDefinition.format.video.nStride;
    pInputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;
    pOutputPort->portDefinition.format.video.nSliceHeight    = pInputPort->newPortDefinition.format.video.nSliceHeight;
    pOutputPort->portDefinition.format.video.eColorFormat    = pOutputPort->newPortDefinition.format.video.eColorFormat;

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

OMX_ERRORTYPE Rkvpu_Frame2Outbuf(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE *pOutputBuffer, VPU_FRAME *pframe)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT      *pRockchipComponent   = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec          = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT           *pOutputPort        = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];

#ifdef USE_STOREMETADATA
    if (pVideoDec->bStoreMetaData == OMX_TRUE) {
        OMX_U32 mWidth = pOutputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 mHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
        RockchipVideoPlane vplanes;
        OMX_PTR pGrallocHandle;
        OMX_COLOR_FORMATTYPE omx_format = 0;
        OMX_U32 pixel_format = 0;

        if (Rockchip_OSAL_GetInfoFromMetaData(pOutputBuffer->pBuffer, &pGrallocHandle)) {
            return OMX_ErrorBadParameter;
        }

        if (pVideoDec->bPvr_Flag == OMX_TRUE) {
            pixel_format = RK_FORMAT_BGRA_8888;
        } else {
            omx_format = Rockchip_OSAL_GetANBColorFormat(pGrallocHandle);
            pixel_format = Rockchip_OSAL_OMX2HalPixelFormat(omx_format);
            if (pixel_format == HAL_PIXEL_FORMAT_RGBA_8888) {
                pixel_format = RK_FORMAT_RGBA_8888;
            } else {
                pixel_format = RK_FORMAT_BGRA_8888;
            }
        }
        if (pVideoDec->rga_ctx != NULL) {
            Rockchip_OSAL_LockANB(pGrallocHandle, mWidth, mHeight, omx_format, &vplanes);
            VPUMemLink(&pframe->vpumem);
            rga_nv122rgb(&vplanes, &pframe->vpumem, mWidth, mHeight, pixel_format, pVideoDec->rga_ctx);
            VPUFreeLinear(&pframe->vpumem);
            Rockchip_OSAL_UnlockANB(pGrallocHandle);
        }
        return ret;
    }
#endif

#ifdef USE_ANB
    if (pVideoDec->bIsANBEnabled == OMX_TRUE) {
        omx_trace("enableNativeBuffer");
        OMX_U32 mWidth = pOutputPort->portDefinition.format.video.nFrameWidth;
        OMX_U32 mHeight = pOutputPort->portDefinition.format.video.nFrameHeight;
        RockchipVideoPlane vplanes;
        OMX_U32 mStride = 0;
        OMX_U32 mSliceHeight =  0;
        OMX_COLOR_FORMATTYPE omx_format = 0;
        OMX_U32 pixel_format = 0;
        mStride = Get_Video_HorAlign(pVideoDec->codecId, mWidth, mHeight, pVideoDec->codecProfile);
        mSliceHeight = Get_Video_VerAlign(pVideoDec->codecId, mHeight, pVideoDec->codecProfile);
        omx_format = Rockchip_OSAL_GetANBColorFormat(pOutputBuffer->pBuffer);
        pixel_format = Rockchip_OSAL_OMX2HalPixelFormat(omx_format);
        Rockchip_OSAL_LockANB(pOutputBuffer->pBuffer, mWidth, mHeight, omx_format, &vplanes);
        {
            VPUMemLink(&pframe->vpumem);
            VPUMemInvalidate(&pframe->vpumem);
            {
                OMX_U8 * buff_vir = (OMX_U8 *)pframe->vpumem.vir_addr;
                pOutputBuffer->nFilledLen = mWidth * mHeight * 3 / 2;
                OMX_U32 uv_offset = mStride * mSliceHeight;
                OMX_U32 y_size = mWidth * mHeight;
                OMX_U8 *dst_uv = (OMX_U8 *)((OMX_U8 *)vplanes.addr + y_size);
                OMX_U8 *src_uv =  (OMX_U8 *)(buff_vir + uv_offset);
                OMX_U32 i = 0;

                omx_trace("mWidth = %d mHeight = %d mStride = %d,mSlicHeight %d", mWidth, mHeight, mStride, mSliceHeight);
                for (i = 0; i < mHeight; i++) {
                    Rockchip_OSAL_Memcpy((char*)vplanes.addr + i * mWidth, buff_vir + i * mStride, mWidth);
                }

                for (i = 0; i < mHeight / 2; i++) {
                    Rockchip_OSAL_Memcpy((OMX_U8*)dst_uv, (OMX_U8*)src_uv, mWidth);
                    dst_uv += mWidth;
                    src_uv += mStride;
                }
            }
            VPUFreeLinear(&pframe->vpumem);
        }
        Rockchip_OSAL_UnlockANB(pOutputBuffer->pBuffer);
        return ret;
    }
#endif
    OMX_U32 mStride = 0;
    OMX_U32 mSliceHeight =  0;
    OMX_U32 mWidth = (pOutputPort->portDefinition.format.video.nFrameWidth );
    OMX_U32 mHeight =  (pOutputPort->portDefinition.format.video.nFrameHeight );
    VPUMemLink(&pframe->vpumem);
    VPUMemInvalidate(&pframe->vpumem);

    omx_trace("width:%d,height:%d ", mWidth, mHeight);
    mStride = pframe->FrameWidth;
    mSliceHeight = pframe->FrameHeight;
    {
        //csy@rock-chips.com
        OMX_U8 *buff_vir = (OMX_U8 *)pframe->vpumem.vir_addr;
        OMX_U32 uv_offset = mStride * mSliceHeight;
        OMX_U32 y_size = mWidth * mHeight;
        OMX_U8 *dst_uv = (OMX_U8 *)(pOutputBuffer->pBuffer + y_size);
        OMX_U8 *src_uv =  (OMX_U8 *)(buff_vir + uv_offset);
        OMX_U32 i = 0, j = 0;
#if AVS100
        OMX_U32 srcFormat, dstFormat;
        srcFormat = dstFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;
#endif
        omx_trace("mWidth = %d mHeight = %d mStride = %d,mSlicHeight %d", mWidth, mHeight, mStride, mSliceHeight);
        pOutputBuffer->nFilledLen = mWidth * mHeight * 3 / 2;
#if AVS100
        if ((pVideoDec->codecProfile == OMX_VIDEO_AVCProfileHigh10 && pVideoDec->codecId == OMX_VIDEO_CodingAVC)
            || ((pVideoDec->codecProfile == OMX_VIDEO_HEVCProfileMain10 || pVideoDec->codecProfile == OMX_VIDEO_HEVCProfileMain10HDR10)
                && pVideoDec->codecId == OMX_VIDEO_CodingHEVC)) {
            OMX_U32 horStride = Get_Video_HorAlign(pVideoDec->codecId, pframe->DisplayWidth, pframe->DisplayHeight, pVideoDec->codecProfile);
            OMX_U32 verStride = Get_Video_VerAlign(pVideoDec->codecId, pframe->DisplayHeight, pVideoDec->codecProfile);
            pOutputBuffer->nFilledLen = horStride * verStride * 3 / 2;
            Rockchip_OSAL_Memcpy((char *)pOutputBuffer->pBuffer, buff_vir, pOutputBuffer->nFilledLen);
            omx_trace("debug 10bit mWidth = %d mHeight = %d horStride = %d,verStride %d", mWidth, mHeight, horStride, verStride);
        } else {
            OMX_BOOL useRga = (mWidth * mHeight >= 1280 * 720) ? OMX_TRUE : OMX_FALSE;
            if (useRga) {
                rga_info_t rgasrc;
                rga_info_t rgadst;
                memset(&rgasrc, 0, sizeof(rga_info_t));
                rgasrc.fd = -1;
                rgasrc.mmuFlag = 1;
                rgasrc.virAddr = buff_vir;

                memset(&rgadst, 0, sizeof(rga_info_t));
                rgadst.fd = -1;
                rgadst.mmuFlag = 1;
                rgadst.virAddr = pOutputBuffer->pBuffer;

                rga_set_rect(&rgasrc.rect, 0, 0, mWidth, mHeight, mStride, mSliceHeight, srcFormat);
                rga_set_rect(&rgadst.rect, 0, 0, mWidth, mHeight, mWidth, mHeight, dstFormat);
                RgaBlit(&rgasrc, &rgadst, NULL);
            } else {
                for (i = 0; i < mHeight; i++) {
                    Rockchip_OSAL_Memcpy((char*)pOutputBuffer->pBuffer + i * mWidth, buff_vir + i * mStride, mWidth);
                }

                for (i = 0; i < mHeight / 2; i++) {
                    Rockchip_OSAL_Memcpy((OMX_U8*)dst_uv, (OMX_U8*)src_uv, mWidth);
                    dst_uv += mWidth;
                    src_uv += mStride;
                }
            }
        }
#else
        for (i = 0; i < mHeight; i++) {
            Rockchip_OSAL_Memcpy((char*)pOutputBuffer->pBuffer + i * mWidth, buff_vir + i * mStride, mWidth);
        }

        for (i = 0; i < mHeight / 2; i++) {
            Rockchip_OSAL_Memcpy((OMX_U8*)dst_uv, (OMX_U8*)src_uv, mWidth);
            dst_uv += mWidth;
            src_uv += mStride;
        }
#endif
    }
    VPUFreeLinear(&pframe->vpumem);

    return ret;

}

OMX_ERRORTYPE Rkvpu_InputBufferGetQueue(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent)
{
    OMX_U32          ret = OMX_ErrorUndefined;
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
            omx_trace("input buffer count = %d", pRockchipPort->bufferQ.numElem);
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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    ROCKCHIP_OMX_BASEPORT      *rockchipOMXOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    OMX_BUFFERHEADERTYPE     *bufferHeader = NULL;
    OMX_U32 i = 0;

    FunctionIn();

    bufferHeader = dataBuffer->bufferHeader;

    if (bufferHeader == NULL) {
        if (dataBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
            omx_info("eos reach, but don't have buffer.");
            VPUMemLinear_t *handle = NULL;
            OMX_U32 nUnusedCount = 0;
            OMX_U32 retry = 10;
            struct vpu_display_mem_pool *pMem_pool = (struct vpu_display_mem_pool *)pVideoDec->vpumem_handle;
            while (retry--) {
                nUnusedCount = pMem_pool->get_unused_num(pMem_pool);
                if (nUnusedCount > 0) {
                    handle = pMem_pool->get_free(pMem_pool);
                    if (handle) {
                        omx_trace("handle: 0x%x fd: 0x%x", handle, VPUMemGetFD(handle));
                        for (i = 0; i < rockchipOMXOutputPort->portDefinition.nBufferCountActual; i++) {
                            if (rockchipOMXOutputPort->extendBufferHeader[i].buf_fd[0] == VPUMemGetFD(handle)) {
                                bufferHeader = rockchipOMXOutputPort->extendBufferHeader[i].OMXBufferHeader;
                                break;
                            }
                        }
                        VPUMemLink(handle);
                        VPUFreeLinear(handle);
                        if (bufferHeader != NULL) {
                            break;
                        }
                    }
                } /* if (nUnusedCount > 0) */
                Rockchip_OSAL_SleepMillisec(20);
            } /* while (retry--) */

            if (bufferHeader != NULL) {
                omx_info("found matching buffer header");
                dataBuffer->bufferHeader = bufferHeader;
            } else {
                omx_err("not matching buffer header, callback error!");
                pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                             pRockchipComponent->callbackData, OMX_EventError,
                                                             OUTPUT_PORT_INDEX,
                                                             OMX_IndexParamPortDefinition, NULL);
            }
        }
    }

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
            omx_err("event OMX_BUFFERFLAG_EOS!!!");
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
    OMX_U32 ret = OMX_ErrorUndefined;
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
        Rockchip_OSAL_Get_SemaphoreCount(pRockchipPort->codecSemID, (OMX_S32*)&cnt);
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
        ROCKCHIP_OMX_BASEPORT               *pRockchipPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *portDefinition = NULL;
        OMX_U32                         supportFormatNum = 0; /* supportFormatNum = N-1 */
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(portFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((portIndex >= pRockchipComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }


        if (portIndex == INPUT_PORT_INDEX) {
            supportFormatNum = INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (index > supportFormatNum) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
            portDefinition = &pRockchipPort->portDefinition;

            portFormat->eCompressionFormat = portDefinition->format.video.eCompressionFormat;
            portFormat->eColorFormat       = portDefinition->format.video.eColorFormat;
            portFormat->xFramerate           = portDefinition->format.video.xFramerate;
        } else if (portIndex == OUTPUT_PORT_INDEX) {
            pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
            portDefinition = &pRockchipPort->portDefinition;

            if (pRockchipPort->bStoreMetaData == OMX_FALSE) {
                switch (index) {
                case supportFormat_0:
                    portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                    break;
                    /*case supportFormat_1:
                        if (pVideoDec->codecId != OMX_VIDEO_CodingVP8) {
                            portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                            portFormat->eColorFormat       = OMX_COLOR_FormatYUV420Planar;
                            portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                        }
                        break;*/
                default:
                    if (index > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            } else {
                switch (index) {
                case supportFormat_0:
                    portFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    portFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    portFormat->xFramerate         = portDefinition->format.video.xFramerate;
                    break;
                default:
                    if (index > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            }
        }
        ret = OMX_ErrorNone;
    }
    break;
#ifdef USE_ANB
    case OMX_IndexParamGetAndroidNativeBufferUsage:
    case OMX_IndexParamdescribeColorFormat: {

        omx_trace("Rockchip_OSAL_GetANBParameter!!");
        ret = Rockchip_OSAL_GetANBParameter(hComponent, nParamIndex, ComponentParameterStructure);
    }
    break;
    case OMX_IndexParamPortDefinition: {
        ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE *portDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = portDefinition->nPortIndex;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

        ret = Rockchip_OMX_GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        /* at this point, GetParameter has done all the verification, we
         * just dereference things directly here
         */
        if ((pVideoDec->bIsANBEnabled == OMX_TRUE) ||
            (pVideoDec->bStoreMetaData == OMX_TRUE)) {
            portDefinition->format.video.eColorFormat = pRockchipPort->portDefinition.format.video.eColorFormat;
            // (OMX_COLOR_FORMATTYPE)Rockchip_OSAL_OMX2HalPixelFormat(portDefinition->format.video.eColorFormat);
            omx_trace("portDefinition->format.video.eColorFormat:0x%x", portDefinition->format.video.eColorFormat);
        }
        if (portIndex == OUTPUT_PORT_INDEX &&
            pRockchipPort->bufferProcessType != BUFFER_SHARE) {
            portDefinition->format.video.nStride = portDefinition->format.video.nFrameWidth;
            portDefinition->format.video.nSliceHeight = portDefinition->format.video.nFrameHeight;
        }
#ifdef AVS80
        if (portIndex == OUTPUT_PORT_INDEX &&
            pRockchipPort->bufferProcessType == BUFFER_SHARE) {
            ROCKCHIP_OMX_BASEPORT *pInputPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
            int32_t depth = (pVideoDec->bIs10bit) ? OMX_DEPTH_BIT_10 : OMX_DEPTH_BIT_8;
            OMX_BOOL fbcMode = Rockchip_OSAL_Check_Use_FBCMode(pVideoDec->codecId, depth, pRockchipPort);

            /*
             * We use pixel_stride instead of byte_stride to setup nativeWindow surface
             * for 10bit source at fbcMode
             */
            if (!pVideoDec->bIs10bit || !fbcMode) {
                portDefinition->format.video.nFrameWidth = portDefinition->format.video.nStride;
            }

            if (fbcMode && (pVideoDec->codecId == OMX_VIDEO_CodingHEVC
                            || pVideoDec->codecId == OMX_VIDEO_CodingAVC)) {
                OMX_U32 height = pInputPort->portDefinition.format.video.nFrameHeight;
                // On FBC case H.264/H.265 decoder will add 4 lines blank on top.
                portDefinition->format.video.nFrameHeight
                    = Get_Video_VerAlign(pVideoDec->codecId, height + 4, pVideoDec->codecProfile);
            } else {
                portDefinition->format.video.nFrameHeight = portDefinition->format.video.nSliceHeight;
            }
        }
#endif
    }
    break;
#endif

    case OMX_IndexParamStandardComponentRole: {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParameterStructure;
        ret = Rockchip_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_H264_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMPEG4) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_MPEG4_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingH263) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_H263_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMPEG2) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_MPEG2_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingVP8) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_FLV_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VP6_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMJPEG) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_MJPEG_DEC_ROLE);
        } else if (pVideoDec->codecId == (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VC1_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingWMV) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_WMV3_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingRV) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_RMVB_DEC_ROLE);
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingVP9) {
            Rockchip_OSAL_Strcpy((char *)pComponentRole->cRole, RK_OMX_COMPONENT_VP9_DEC_ROLE);
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = NULL;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(pDstAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pSrcAVCComponent = &pVideoDec->AVCComponent[pDstAVCComponent->nPortIndex];
        Rockchip_OSAL_Memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }
    break;
    case OMX_IndexParamVideoProfileLevelQuerySupported: {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *profileLevel =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *) ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        OMX_U32 index = profileLevel->nProfileIndex;
        OMX_U32 nProfileLevels = 0;
        if (profileLevel->nPortIndex != 0) {
            omx_err("Invalid port index: %ld", profileLevel->nPortIndex);
            return OMX_ErrorUnsupportedIndex;
        }
        if (pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
            nProfileLevels =
                sizeof(kH264ProfileLevelsMax) / sizeof(kH264ProfileLevelsMax[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH264ProfileLevelsMax[index].mProfile;
            profileLevel->eLevel = kH264ProfileLevelsMax[index].mLevel;
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
            nProfileLevels =
                sizeof(kH265ProfileLevels) / sizeof(kH265ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH265ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH265ProfileLevels[index].mLevel;
        } else if (pVideoDec->codecId  == OMX_VIDEO_CodingMPEG4) {
            nProfileLevels =
                sizeof(kM4VProfileLevels) / sizeof(kM4VProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kM4VProfileLevels[index].mProfile;
            profileLevel->eLevel = kM4VProfileLevels[index].mLevel;
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingH263) {
            nProfileLevels =
                sizeof(kH263ProfileLevels) / sizeof(kH263ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kH263ProfileLevels[index].mProfile;
            profileLevel->eLevel = kH263ProfileLevels[index].mLevel;
        } else if (pVideoDec->codecId == OMX_VIDEO_CodingMPEG2) {
            nProfileLevels =
                sizeof(kM2VProfileLevels) / sizeof(kM2VProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }
            profileLevel->eProfile = kM2VProfileLevels[index].mProfile;
            profileLevel->eLevel = kM2VProfileLevels[index].mLevel;
        } else {
            return OMX_ErrorNoMore;
        }
        return OMX_ErrorNone;
    }
    break;
    case OMX_IndexParamVideoHDRRockchipExtensions: {
        OMX_EXTENSION_VIDEO_PARAM_HDR *hdrParams =
            (OMX_EXTENSION_VIDEO_PARAM_HDR *) ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(hdrParams, sizeof(OMX_EXTENSION_VIDEO_PARAM_HDR));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        hdrParams->eColorSpace = pVideoDec->extColorSpace;
        hdrParams->eDyncRange = pVideoDec->extDyncRange;
        ret = OMX_ErrorNone;
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
        ROCKCHIP_OMX_BASEPORT               *pRockchipPort = NULL;
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

            omx_trace("portIndex:%d, portFormat->eColorFormat:0x%x", portIndex, portFormat->eColorFormat);
        }
    }
    break;
    case OMX_IndexParamPortDefinition: {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = pPortDefinition->nPortIndex;
        ROCKCHIP_OMX_BASEPORT             *pRockchipPort;

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

        ret = Rkvpu_UpdatePortDefinition(hComponent, pPortDefinition, portIndex);
        if (OMX_ErrorNone != ret) {
            goto EXIT;
        }
    }
    break;
#ifdef USE_ANB
    case OMX_IndexParamEnableAndroidBuffers:
    case OMX_IndexParamUseAndroidNativeBuffer:
    case OMX_IndexParamStoreMetaDataBuffer:
    case OMX_IndexParamprepareForAdaptivePlayback:
    case OMX_IndexParamAllocateNativeHandle: {
        omx_trace("Rockchip_OSAL_SetANBParameter!!");
        ret = Rockchip_OSAL_SetANBParameter(hComponent, nIndex, ComponentParameterStructure);
    }
    break;
#endif

    case OMX_IndexParamEnableThumbnailMode: {
        ROCKCHIP_OMX_VIDEO_THUMBNAILMODE *pThumbnailMode = (ROCKCHIP_OMX_VIDEO_THUMBNAILMODE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        ret = Rockchip_OMX_Check_SizeVersion(pThumbnailMode, sizeof(ROCKCHIP_OMX_VIDEO_THUMBNAILMODE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pVideoDec->bThumbnailMode = pThumbnailMode->bEnable;
        if (pVideoDec->bThumbnailMode == OMX_TRUE) {
            ROCKCHIP_OMX_BASEPORT *pRockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
            pRockchipOutputPort->portDefinition.nBufferCountMin = 1;
            pRockchipOutputPort->portDefinition.nBufferCountActual = 1;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionDiv3: {
        OMX_BOOL *pIsDiv3 = (OMX_BOOL *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        if ((*pIsDiv3) == OMX_TRUE) {
            pVideoDec->flags |= RKVPU_OMX_VDEC_IS_DIV3;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionUseDts: {
        OMX_BOOL *pUseDtsFlag = (OMX_BOOL *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;

        if ((*pUseDtsFlag) == OMX_TRUE) {
            pVideoDec->flags |= RKVPU_OMX_VDEC_USE_DTS;
        }

        ret = OMX_ErrorNone;
    }
    break;

    case OMX_IndexParamRkDecoderExtensionThumbNailCodecProfile: {
        OMX_PARAM_U32TYPE *tmp = (OMX_PARAM_U32TYPE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
        pVideoDec->codecProfile = tmp->nU32;

        omx_trace("debug omx codecProfile %d", pVideoDec->codecProfile);

        ret = OMX_ErrorNone;
    } break;
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

        if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_H264_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_MPEG4_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_H263_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_MPEG2_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VP8_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VP9_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_HEVC_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_FLV_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingFLV1;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VP6_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP6;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_MJPEG_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_VC1_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVC1;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_WMV3_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
        } else if (!Rockchip_OSAL_Strcmp((char*)pComponentRole->cRole, RK_OMX_COMPONENT_RMVB_DEC_ROLE)) {
            pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingRV;
        } else {
            ret = OMX_ErrorInvalidComponentName;
            goto EXIT;
        }
    }
    break;
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *pDstAVCComponent = NULL;
        OMX_VIDEO_PARAM_AVCTYPE *pSrcAVCComponent = (OMX_VIDEO_PARAM_AVCTYPE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
        ret = Rockchip_OMX_Check_SizeVersion(pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (pSrcAVCComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pDstAVCComponent = &pVideoDec->AVCComponent[pSrcAVCComponent->nPortIndex];

        Rockchip_OSAL_Memcpy(pDstAVCComponent, pSrcAVCComponent, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
    }
    break;
    case OMX_IndexParamVideoProfileLevelCurrent: {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *params = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)ComponentParameterStructure;
        RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
        if (pVideoDec != NULL) {
            if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC) {
                if (params->eProfile >= OMX_VIDEO_HEVCProfileMain10) {
                    pVideoDec->bIs10bit = OMX_TRUE;
                }
            } else if (pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
                if (params->eProfile == OMX_VIDEO_AVCProfileHigh10) {
                    pVideoDec->bIs10bit = OMX_TRUE;
                }
            } else if (pVideoDec->codecId == OMX_VIDEO_CodingVP9) {
            }
        }
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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;

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

    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch (nIndex) {
#ifdef AVS80
    case OMX_IndexConfigCommonOutputCrop: {
        OMX_CONFIG_RECTTYPE *rectParams = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;
        OMX_U32 portIndex = rectParams->nPortIndex;
        ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;
        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

        if (rectParams->nPortIndex != OUTPUT_PORT_INDEX) {
            return OMX_ErrorUndefined;
        }
        /*Avoid rectParams->nWidth and rectParams->nHeight to be set as 0*/
        if (pRockchipPort->cropRectangle.nHeight > 0 && pRockchipPort->cropRectangle.nWidth > 0)
            Rockchip_OSAL_Memcpy(rectParams, &(pRockchipPort->cropRectangle), sizeof(OMX_CONFIG_RECTTYPE));
        else
            rectParams->nWidth = rectParams->nHeight = 1;

        // fbc output buffer offset X/Y
        int32_t depth = (pVideoDec->bIs10bit) ? OMX_DEPTH_BIT_10 : OMX_DEPTH_BIT_8;
        if (Rockchip_OSAL_Check_Use_FBCMode(pVideoDec->codecId, depth, pRockchipPort)) {
            if (pVideoDec->codecId == OMX_VIDEO_CodingHEVC
                || pVideoDec->codecId == OMX_VIDEO_CodingAVC) {
                rectParams->nTop = 4;
            }
        }
        omx_info("rectParams:%d %d %d %d", rectParams->nLeft, rectParams->nTop, rectParams->nWidth, rectParams->nHeight);
    }
    break;
#endif
    case OMX_IndexParamRkDescribeColorAspects: {
        OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *colorAspectsParams = (OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *)pComponentConfigStructure;

        ret = Rockchip_OMX_Check_SizeVersion((void *)colorAspectsParams, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (colorAspectsParams->nPortIndex != OUTPUT_PORT_INDEX) {
            return OMX_ErrorBadParameter;
        }

        colorAspectsParams->sAspects.mRange = pVideoDec->mFinalColorAspects.mRange;
        colorAspectsParams->sAspects.mPrimaries = pVideoDec->mFinalColorAspects.mPrimaries;
        colorAspectsParams->sAspects.mTransfer = pVideoDec->mFinalColorAspects.mTransfer;
        colorAspectsParams->sAspects.mMatrixCoeffs = pVideoDec->mFinalColorAspects.mMatrixCoeffs;

        if (colorAspectsParams->bRequestingDataSpace || colorAspectsParams->bDataSpaceChanged) {
            return OMX_ErrorUnsupportedSetting;
        }
    }
    break;

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
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;

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
    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch (nIndex) {
    case OMX_IndexParamRkDescribeColorAspects: {
        const OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS* colorAspectsParams = (const OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS *)pComponentConfigStructure;

        ret = Rockchip_OMX_Check_SizeVersion((void *)colorAspectsParams, sizeof(OMX_CONFIG_DESCRIBECOLORASPECTSPARAMS));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        if (colorAspectsParams->nPortIndex != OUTPUT_PORT_INDEX) {
            return OMX_ErrorBadParameter;
        }
        // Update color aspects if necessary.
        if (colorAspectsDiffer(&colorAspectsParams->sAspects, &pVideoDec->mDefaultColorAspects)) {
            pVideoDec->mDefaultColorAspects.mRange = colorAspectsParams->sAspects.mRange;
            pVideoDec->mDefaultColorAspects.mPrimaries = colorAspectsParams->sAspects.mPrimaries;
            pVideoDec->mDefaultColorAspects.mTransfer = colorAspectsParams->sAspects.mTransfer;
            pVideoDec->mDefaultColorAspects.mMatrixCoeffs = colorAspectsParams->sAspects.mMatrixCoeffs;

            if (pVideoDec->codecId != OMX_VIDEO_CodingVP8) {
                handleColorAspectsChange(&pVideoDec->mDefaultColorAspects/*mDefaultColorAspects*/,
                                         &pVideoDec->mBitstreamColorAspects/*mBitstreamColorAspects*/,
                                         &pVideoDec->mFinalColorAspects/*mFinalColorAspects*/,
                                         kPreferBitstream);
            } else {
                handleColorAspectsChange(&pVideoDec->mDefaultColorAspects/*mDefaultColorAspects*/,
                                         &pVideoDec->mBitstreamColorAspects/*mBitstreamColorAspects*/,
                                         &pVideoDec->mFinalColorAspects/*mFinalColorAspects*/,
                                         kPreferContainer);
            }
        }
    }
    break;
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
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_H264_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 1) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_MPEG4_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 2) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_H263_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 3) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_FLV_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 4) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_MPEG2_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 5) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_RMVB_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 6) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_VP8_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 7) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_VC1_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 8) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_WMV3_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 9) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_VP6_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 10) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_HEVC_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else if (nIndex == 12) {
        Rockchip_OSAL_Strcpy((char *)cRole, RK_OMX_COMPONENT_H264_DEC_ROLE);
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
#ifdef USE_ANB
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ENABLE_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamEnableAndroidBuffers;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_GET_ANB_Usage) == 0) {
        *pIndexType = (OMX_INDEXTYPE) OMX_IndexParamGetAndroidNativeBufferUsage;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_USE_ANB) == 0) {
        *pIndexType = (OMX_INDEXTYPE) NULL;//OMX_IndexParamUseAndroidNativeBuffer;
        goto EXIT;
    }

    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PREPARE_ADAPTIVE_PLAYBACK) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamprepareForAdaptivePlayback;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_DESCRIBE_COLORFORMAT) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamdescribeColorFormat;
        goto EXIT;
    }
#endif

    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ENABLE_THUMBNAIL) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamEnableThumbnailMode;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_DIV3) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionDiv3;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_THUMBNAILCODECPROFILE) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionThumbNailCodecProfile;
        goto EXIT;
    }
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ROCKCHIP_DEC_EXTENSION_USE_DTS) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDecoderExtensionUseDts;
        goto EXIT;
    }
#ifdef USE_STOREMETADATA
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE) NULL;
        goto EXIT;
    }
#endif

#ifdef AVS80
#ifdef HAVE_L1_SVP_MODE
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_ALLOCATENATIVEHANDLE) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamAllocateNativeHandle;
        goto EXIT;
    }
#endif
#endif
    if (Rockchip_OSAL_Strcmp(cParameterName, ROCKCHIP_INDEX_PARAM_DSECRIBECOLORASPECTS) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamRkDescribeColorAspects;
        goto EXIT;
    }

    ret = Rockchip_OMX_GetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rkvpu_UpdatePortDefinition(
    OMX_HANDLETYPE hComponent,
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition,
    const OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;
    RKVPU_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    OMX_U32 nFrameWidth = 0;
    OMX_U32 nFrameHeight = 0;
    OMX_S32 nStride = 0;
    OMX_U32 nSliceHeight = 0;

    FunctionIn();

    if (hComponent == NULL) {
        omx_err("error in");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        omx_err("error in");
        goto EXIT;
    }

    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (NULL == pRockchipComponent) {
        omx_err("error in");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pRockchipPort = &pRockchipComponent->pRockchipPort[nPortIndex];
    if (NULL == pRockchipPort) {
        omx_err("error in");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (RKVPU_OMX_VIDEODEC_COMPONENT *)pRockchipComponent->hComponentHandle;
    if (NULL == pVideoDec) {
        omx_err("error in");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /*
     * check set param legal
     */
    ret = Rkvpu_CheckPortDefinition(pPortDefinition, &pRockchipPort->portDefinition, nPortIndex);
    if (OMX_ErrorNone != ret) {
        pRockchipPort->portDefinition.nBufferCountActual = pPortDefinition->nBufferCountActual;
        //omx_err("check portdefinition param failed , ret: 0x%x", ret);
        //goto EXIT;
    }

    Rockchip_OSAL_Memcpy((OMX_PTR)&pRockchipPort->portDefinition, (OMX_PTR)pPortDefinition, pPortDefinition->nSize);

    nFrameWidth = pRockchipPort->portDefinition.format.video.nFrameWidth;
    nFrameHeight = pRockchipPort->portDefinition.format.video.nFrameHeight;

    nStride = Get_Video_HorAlign(pVideoDec->codecId, nFrameWidth, nFrameHeight, pVideoDec->codecProfile);
    nSliceHeight = Get_Video_VerAlign(pVideoDec->codecId, nFrameHeight, pVideoDec->codecProfile);

    omx_trace("[%s:%d] nStride = %d,nSliceHeight = %d", __func__, __LINE__, nStride, nSliceHeight);

    pRockchipPort->portDefinition.format.video.nStride = nStride;
    pRockchipPort->portDefinition.format.video.nSliceHeight = nSliceHeight;

    if (INPUT_PORT_INDEX == nPortIndex) {
        /*
         * Determining the compression ratio by coding type.
         */
        pRockchipPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
        {
            /*
             * update output port info from input port.
             */
            ROCKCHIP_OMX_BASEPORT *pRockchipOutputPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
            pRockchipOutputPort->portDefinition.format.video.nFrameWidth = pRockchipPort->portDefinition.format.video.nFrameWidth;
            pRockchipOutputPort->portDefinition.format.video.nFrameHeight = pRockchipPort->portDefinition.format.video.nFrameHeight;
            pRockchipOutputPort->portDefinition.format.video.nStride = nStride;
            pRockchipOutputPort->portDefinition.format.video.nSliceHeight = nSliceHeight;
#ifdef AVS80
            Rockchip_OSAL_Memset(&(pRockchipOutputPort->cropRectangle), 0, sizeof(OMX_CONFIG_RECTTYPE));
            pRockchipOutputPort->cropRectangle.nWidth = pRockchipOutputPort->portDefinition.format.video.nFrameWidth;
            pRockchipOutputPort->cropRectangle.nHeight = pRockchipOutputPort->portDefinition.format.video.nFrameHeight;
            omx_info("cropRectangle.nWidth: %d, height: %d", pRockchipOutputPort->cropRectangle.nWidth, pRockchipOutputPort->cropRectangle.nHeight);
            pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventPortSettingsChanged,
                                                         OUTPUT_PORT_INDEX,
                                                         OMX_IndexConfigCommonOutputCrop,
                                                         NULL);
#endif
            switch ((OMX_U32)pRockchipOutputPort->portDefinition.format.video.eColorFormat) {
            case OMX_COLOR_FormatYUV420Planar:
            case OMX_COLOR_FormatYUV420SemiPlanar:
                pRockchipOutputPort->portDefinition.nBufferSize = nStride * nSliceHeight * 3 / 2;
                omx_trace("%s nStride = %d,nSliceHeight = %d", __func__, nStride, nSliceHeight);
                break;
#ifdef USE_STOREMETADATA
                /*
                 * this case used RGBA buffer size
                 */
            case OMX_COLOR_FormatAndroidOpaque:
                pRockchipOutputPort->portDefinition.nBufferSize = nStride * nSliceHeight * 4;
                if (pVideoDec->bPvr_Flag == OMX_TRUE) {
                    pRockchipOutputPort->portDefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_BGRA_8888;
                } else {
                    pRockchipOutputPort->portDefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_RGBA_8888;
                }
                break;
#endif
            default:
                omx_err("Color format is not support!! use default YUV size!");
                ret = OMX_ErrorUnsupportedSetting;
                break;
            }
        }
    }

    if (OUTPUT_PORT_INDEX == nPortIndex) {
        int32_t depth = (pVideoDec->bIs10bit) ? OMX_DEPTH_BIT_10 : OMX_DEPTH_BIT_8;
        OMX_BOOL fbcMode = Rockchip_OSAL_Check_Use_FBCMode(pVideoDec->codecId, depth, pRockchipPort);
        OMX_COLOR_FORMATTYPE format = pRockchipPort->portDefinition.format.video.eColorFormat;

        if (fbcMode) {
            // fbc stride default 64 align
            nStride = (nFrameWidth + 63) & (~63);
            pRockchipPort->portDefinition.format.video.nStride = nStride;

            if (format == OMX_COLOR_FormatYUV420Planar || format == OMX_COLOR_FormatYUV420SemiPlanar) {
                pRockchipPort->portDefinition.format.video.eColorFormat
                    = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YUV420_8BIT_I;
            } else if (format == OMX_COLOR_FormatYUV422Planar || format == OMX_COLOR_FormatYUV422SemiPlanar) {
                pRockchipPort->portDefinition.format.video.eColorFormat
                    = (OMX_COLOR_FORMATTYPE)HAL_PIXEL_FORMAT_YCbCr_422_I;
            }
        }
        omx_info("update output PortDefinition [%d,%d,%d,%d], eColorFormat 0x%x->0x%x",
                 nFrameWidth, nFrameHeight, nStride, nSliceHeight, format,
                 pRockchipPort->portDefinition.format.video.eColorFormat);
    }

    /*
     * compute buffer count if need
     */
    ret = Rkvpu_ComputeDecBufferCount(hComponent);
    if (OMX_ErrorNone != ret) {
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_U32 Rkvpu_GetCompressRatioByCodingtype(
    OMX_VIDEO_CODINGTYPE codingType)
{
    OMX_U32 nCompressRatio = 1;
    switch (codingType) {
    case OMX_VIDEO_CodingAVC:
        nCompressRatio = 2;
        break;
    case OMX_VIDEO_CodingHEVC:
    case OMX_VIDEO_CodingVP9:
        nCompressRatio = 4;
        break;
    default:
        nCompressRatio = 2;
        break;
    }

    return nCompressRatio;
}

OMX_ERRORTYPE Rkvpu_CheckPortDefinition(
    const OMX_PARAM_PORTDEFINITIONTYPE *pNewPortDefinition,
    const OMX_PARAM_PORTDEFINITIONTYPE *pPortDefinition,
    const OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 nSupportWidthMax = 0;

    FunctionIn();

    if (pNewPortDefinition->nBufferCountActual > pPortDefinition->nBufferCountActual) {
        omx_err("error: SET buffer count: %d, count min: %d "
                "NOW buffer count: %d, count min: %d",
                pNewPortDefinition->nBufferCountActual,
                pNewPortDefinition->nBufferCountMin,
                pPortDefinition->nBufferCountActual,
                pPortDefinition->nBufferCountMin);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (INPUT_PORT_INDEX == nPortIndex) {
        if (pNewPortDefinition->format.video.eCompressionFormat == OMX_VIDEO_CodingUnused) {
            omx_err("error: input conding type is OMX_VIDEO_CodingUnused!");
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        nSupportWidthMax = VPUCheckSupportWidth();
        if (nSupportWidthMax == 0) {
            omx_warn("VPUCheckSupportWidth is failed , force max width to 4096.");
            nSupportWidthMax = 4096;
        }

        if (pNewPortDefinition->format.video.nFrameWidth > nSupportWidthMax) {
            if (access("/dev/rkvdec", 06) == 0) {
                if (pNewPortDefinition->format.video.eCompressionFormat == OMX_VIDEO_CodingHEVC ||
                    pNewPortDefinition->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC ||
                    pNewPortDefinition->format.video.eCompressionFormat == OMX_VIDEO_CodingVP9) {
                    nSupportWidthMax = 4096;
                } else {
                    omx_err("decoder width %d big than support width %d return error",
                            pNewPortDefinition->format.video.nFrameWidth,
                            nSupportWidthMax);
                    ret = OMX_ErrorBadParameter;
                    goto EXIT;
                }
            } else {
                omx_err("decoder width %d big than support width %d return error",
                        pNewPortDefinition->format.video.nFrameWidth,
                        nSupportWidthMax);
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
        }
        omx_info("decoder width %d support %d",
                 pNewPortDefinition->format.video.nFrameWidth,
                 nSupportWidthMax);
    }

EXIT:
    FunctionOut();

    return ret;
}
