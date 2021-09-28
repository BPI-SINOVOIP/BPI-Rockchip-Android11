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
 * @file       Rockchip_OMX_Basecomponent.c
 * @brief
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_base_comp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "Rockchip_OSAL_Event.h"
#include "Rockchip_OSAL_Thread.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OSAL_Semaphore.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_Log.h"
#include "Rockchip_OMX_Baseport.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "Rockchip_OMX_Resourcemanager.h"
#include "Rockchip_OMX_Macros.h"
#include "git_info.h"



/* Change CHECK_SIZE_VERSION Macro */
OMX_ERRORTYPE Rockchip_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_VERSIONTYPE* version = NULL;
    if (header == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    version = (OMX_VERSIONTYPE*)((char*)header + sizeof(OMX_U32));
    if (*((OMX_U32*)header) != size) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (version->s.nVersionMajor != VERSIONMAJOR_NUMBER ||
        version->s.nVersionMinor != VERSIONMINOR_NUMBER) {
        ret = OMX_ErrorVersionMismatch;
        goto EXIT;
    }
    ret = OMX_ErrorNone;
EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_GetComponentVersion(
    OMX_IN  OMX_HANDLETYPE   hComponent,
    OMX_OUT OMX_STRING       pComponentName,
    OMX_OUT OMX_VERSIONTYPE *pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE *pSpecVersion,
    OMX_OUT OMX_UUIDTYPE    *pComponentUUID)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    OMX_U32                   compUUID[3];

    FunctionIn();

    /* check parameters */
    if (hComponent     == NULL ||
        pComponentName == NULL || pComponentVersion == NULL ||
        pSpecVersion   == NULL || pComponentUUID    == NULL) {
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

    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Rockchip_OSAL_Strcpy(pComponentName, pRockchipComponent->componentName);
    Rockchip_OSAL_Memcpy(pComponentVersion, &(pRockchipComponent->componentVersion), sizeof(OMX_VERSIONTYPE));
    Rockchip_OSAL_Memcpy(pSpecVersion, &(pRockchipComponent->specVersion), sizeof(OMX_VERSIONTYPE));

    /* Fill UUID with handle address, PID and UID.
     * This should guarantee uiniqness */
    compUUID[0] = (OMX_U32)pOMXComponent;
    compUUID[1] = getpid();
    compUUID[2] = getuid();
    Rockchip_OSAL_Memcpy(*pComponentUUID, compUUID, 3 * sizeof(*compUUID));

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_GetState (
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_OUT OMX_STATETYPE *pState)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pState == NULL) {
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

    *pState = pRockchipComponent->currentState;
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_ComponentStateSet(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 messageParam)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_MESSAGE       *message;
    OMX_STATETYPE             destState = messageParam;
    OMX_STATETYPE             currentState = pRockchipComponent->currentState;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    unsigned int              i = 0, j = 0;
    int                       k = 0;

    FunctionIn();

    /* check parameters */
    if (currentState == destState) {
        ret = OMX_ErrorSameState;
        goto EXIT;
    }
    if (currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if ((currentState == OMX_StateLoaded) && (destState == OMX_StateIdle)) {
        ret = Rockchip_OMX_Get_Resource(pOMXComponent);
        if (ret != OMX_ErrorNone) {
            Rockchip_OSAL_SignalSet(pRockchipComponent->abendStateEvent);
            goto EXIT;
        }
    }
    if (((currentState == OMX_StateIdle) && (destState == OMX_StateLoaded))       ||
        ((currentState == OMX_StateIdle) && (destState == OMX_StateInvalid))      ||
        ((currentState == OMX_StateExecuting) && (destState == OMX_StateInvalid)) ||
        ((currentState == OMX_StatePause) && (destState == OMX_StateInvalid))) {
        Rockchip_OMX_Release_Resource(pOMXComponent);
    }

    omx_trace("destState: %d currentState: %d", destState, currentState);
    switch (destState) {
    case OMX_StateInvalid:
        switch (currentState) {
        case OMX_StateWaitForResources:
            Rockchip_OMX_Out_WaitForResource(pOMXComponent);
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
        case OMX_StateLoaded:
            pRockchipComponent->currentState = OMX_StateInvalid;
            ret = pRockchipComponent->rockchip_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                if (pRockchipComponent->pRockchipPort[i].portWayType == WAY1_PORT) {
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pRockchipComponent->pRockchipPort[i].portWayType == WAY2_PORT) {
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }
                Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].hPortMutex);
                pRockchipComponent->pRockchipPort[i].hPortMutex = NULL;
                Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].secureBufferMutex);
                pRockchipComponent->pRockchipPort[i].secureBufferMutex = NULL;
            }

            if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                Rockchip_OSAL_SignalTerminate(pRockchipComponent->pauseEvent);
                pRockchipComponent->pauseEvent = NULL;
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].pauseEvent);
                    pRockchipComponent->pRockchipPort[i].pauseEvent = NULL;
                    if (pRockchipComponent->pRockchipPort[i].bufferProcessType == BUFFER_SHARE) {
                        Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent);
                        pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent = NULL;
                    }
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Rockchip_OSAL_SemaphoreTerminate(pRockchipComponent->pRockchipPort[i].bufferSemID);
                pRockchipComponent->pRockchipPort[i].bufferSemID = NULL;
            }
            if (pRockchipComponent->rockchip_codec_componentTerminate != NULL)
                pRockchipComponent->rockchip_codec_componentTerminate(pOMXComponent);

            ret = OMX_ErrorInvalidState;
            break;
        default:
            ret = OMX_ErrorInvalidState;
            break;
        }
        break;
    case OMX_StateLoaded:
        switch (currentState) {
        case OMX_StateIdle:
            ret = pRockchipComponent->rockchip_BufferProcessTerminate(pOMXComponent);

            for (i = 0; i < ALL_PORT_NUM; i++) {
                if (pRockchipComponent->pRockchipPort[i].portWayType == WAY1_PORT) {
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                } else if (pRockchipComponent->pRockchipPort[i].portWayType == WAY2_PORT) {
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                }
                Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].hPortMutex);
                pRockchipComponent->pRockchipPort[i].hPortMutex = NULL;
                Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].secureBufferMutex);
                pRockchipComponent->pRockchipPort[i].secureBufferMutex = NULL;
            }
            if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                Rockchip_OSAL_SignalTerminate(pRockchipComponent->pauseEvent);
                pRockchipComponent->pauseEvent = NULL;
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].pauseEvent);
                    pRockchipComponent->pRockchipPort[i].pauseEvent = NULL;
                    if (pRockchipComponent->pRockchipPort[i].bufferProcessType == BUFFER_SHARE) {
                        Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent);
                        pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent = NULL;
                    }
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                Rockchip_OSAL_SemaphoreTerminate(pRockchipComponent->pRockchipPort[i].bufferSemID);
                pRockchipComponent->pRockchipPort[i].bufferSemID = NULL;
            }

            pRockchipComponent->rockchip_codec_componentTerminate(pOMXComponent);

            for (i = 0; i < (pRockchipComponent->portParam.nPorts); i++) {
                pRockchipPort = (pRockchipComponent->pRockchipPort + i);
                if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    while (Rockchip_OSAL_GetElemNum(&pRockchipPort->bufferQ) > 0) {
                        message = (ROCKCHIP_OMX_MESSAGE*)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
                        if (message != NULL)
                            Rockchip_OSAL_Free(message);
                    }
                    ret = pRockchipComponent->rockchip_FreeTunnelBuffer(pRockchipPort, i);
                    if (OMX_ErrorNone != ret) {
                        goto EXIT;
                    }
                } else {
                    if (CHECK_PORT_ENABLED(pRockchipPort)) {
                        Rockchip_OSAL_SemaphoreWait(pRockchipPort->unloadedResource);
                        pRockchipPort->portDefinition.bPopulated = OMX_FALSE;
                    }
                }
            }
            pRockchipComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateWaitForResources:
            ret = Rockchip_OMX_Out_WaitForResource(pOMXComponent);
            pRockchipComponent->currentState = OMX_StateLoaded;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateIdle:
        switch (currentState) {
        case OMX_StateLoaded:
            omx_trace("OMX_StateLoaded in loadedResource");
            for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
                pRockchipPort = (pRockchipComponent->pRockchipPort + i);
                if (pRockchipPort == NULL) {
                    ret = OMX_ErrorBadParameter;
                    goto EXIT;
                }
                if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    if (CHECK_PORT_ENABLED(pRockchipPort)) {
                        ret = pRockchipComponent->rockchip_AllocateTunnelBuffer(pRockchipPort, i);
                        if (ret != OMX_ErrorNone)
                            goto EXIT;
                    }
                } else {
                    if (CHECK_PORT_ENABLED(pRockchipPort)) {
                        omx_trace("Rockchip_OSAL_SemaphoreWait loadedResource ");
                        Rockchip_OSAL_SemaphoreWait(pRockchipComponent->pRockchipPort[i].loadedResource);
                        omx_trace("Rockchip_OSAL_SemaphoreWait loadedResource out");
                        if (pRockchipComponent->abendState == OMX_TRUE) {
                            omx_err("Rockchip_OSAL_SemaphoreWait abendState == OMX_TRUE");
                            Rockchip_OSAL_SignalSet(pRockchipComponent->abendStateEvent);
                            ret = Rockchip_OMX_Release_Resource(pOMXComponent);
                            goto EXIT;
                        }
                        pRockchipPort->portDefinition.bPopulated = OMX_TRUE;
                    }
                }
            }

            if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                Rockchip_OSAL_SignalCreate(&pRockchipComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SignalCreate(&pRockchipComponent->pRockchipPort[i].pauseEvent);
                    if (pRockchipComponent->pRockchipPort[i].bufferProcessType == BUFFER_SHARE)
                        Rockchip_OSAL_SignalCreate(&pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent);
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                ret = Rockchip_OSAL_SemaphoreCreate(&pRockchipComponent->pRockchipPort[i].bufferSemID);
                if (ret != OMX_ErrorNone) {
                    ret = OMX_ErrorInsufficientResources;
                    omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                    goto EXIT;
                }
            }
            for (i = 0; i < ALL_PORT_NUM; i++) {
                if (pRockchipComponent->pRockchipPort[i].portWayType == WAY1_PORT) {
                    ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        ret = OMX_ErrorInsufficientResources;
                        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                } else if (pRockchipComponent->pRockchipPort[i].portWayType == WAY2_PORT) {
                    ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        ret = OMX_ErrorInsufficientResources;
                        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                    ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                    if (ret != OMX_ErrorNone) {
                        ret = OMX_ErrorInsufficientResources;
                        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
                        goto EXIT;
                    }
                }
                ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->pRockchipPort[i].hPortMutex);
                if (ret != OMX_ErrorNone) {
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->pRockchipPort[i].secureBufferMutex);
                if (ret != OMX_ErrorNone) {
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
            }
            omx_trace("rockchip_BufferProcessCreate");

            ret = pRockchipComponent->rockchip_BufferProcessCreate(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
                if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                    Rockchip_OSAL_SignalTerminate(pRockchipComponent->pauseEvent);
                    pRockchipComponent->pauseEvent = NULL;
                } else {
                    for (i = 0; i < ALL_PORT_NUM; i++) {
                        Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].pauseEvent);
                        pRockchipComponent->pRockchipPort[i].pauseEvent = NULL;
                        if (pRockchipComponent->pRockchipPort[i].bufferProcessType == BUFFER_SHARE) {
                            Rockchip_OSAL_SignalTerminate(pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent);
                            pRockchipComponent->pRockchipPort[i].hAllCodecBufferReturnEvent = NULL;
                        }
                    }
                }
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    if (pRockchipComponent->pRockchipPort[i].portWayType == WAY1_PORT) {
                        Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex);
                        pRockchipComponent->pRockchipPort[i].way.port1WayDataBuffer.dataBuffer.bufferMutex = NULL;
                    } else if (pRockchipComponent->pRockchipPort[i].portWayType == WAY2_PORT) {
                        Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex);
                        pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.inputDataBuffer.bufferMutex = NULL;
                        Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex);
                        pRockchipComponent->pRockchipPort[i].way.port2WayDataBuffer.outputDataBuffer.bufferMutex = NULL;
                    }
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].hPortMutex);
                    pRockchipComponent->pRockchipPort[i].hPortMutex = NULL;
                    Rockchip_OSAL_MutexTerminate(pRockchipComponent->pRockchipPort[i].secureBufferMutex);
                    pRockchipComponent->pRockchipPort[i].secureBufferMutex = NULL;
                }
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SemaphoreTerminate(pRockchipComponent->pRockchipPort[i].bufferSemID);
                    pRockchipComponent->pRockchipPort[i].bufferSemID = NULL;
                }

                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            omx_trace(" OMX_StateIdle");
            pRockchipComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateExecuting:
        case OMX_StatePause:
            if (currentState == OMX_StateExecuting) {
                pRockchipComponent->nRkFlags |= RK_VPU_NEED_FLUSH_ON_SEEK;
            }
            Rockchip_OMX_BufferFlushProcess(pOMXComponent, ALL_PORT_INDEX, OMX_FALSE);
            pRockchipComponent->currentState = OMX_StateIdle;
            break;
        case OMX_StateWaitForResources:
            pRockchipComponent->currentState = OMX_StateIdle;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateExecuting:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            omx_trace("rockchip_codec_componentInit");
            ret = pRockchipComponent->rockchip_codec_componentInit(pOMXComponent);
            if (ret != OMX_ErrorNone) {
                /*
                 * if (CHECK_PORT_TUNNELED == OMX_TRUE) thenTunnel Buffer Free
                 */
                omx_err_f("rockchip_codec_componentInit failed!");
                Rockchip_OSAL_SignalSet(pRockchipComponent->abendStateEvent);
                Rockchip_OMX_Release_Resource(pOMXComponent);
                goto EXIT;
            }

            for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
                pRockchipPort = &pRockchipComponent->pRockchipPort[i];
                if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort) && CHECK_PORT_ENABLED(pRockchipPort)) {
                    for (j = 0; j < pRockchipPort->tunnelBufferNum; j++) {
                        Rockchip_OSAL_SemaphorePost(pRockchipComponent->pRockchipPort[i].bufferSemID);
                    }
                }
            }

            pRockchipComponent->transientState = ROCKCHIP_OMX_TransStateMax;
            pRockchipComponent->currentState = OMX_StateExecuting;
            if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                Rockchip_OSAL_SignalSet(pRockchipComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[i].pauseEvent);
                }
            }
            break;
        case OMX_StatePause:
            for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
                pRockchipPort = &pRockchipComponent->pRockchipPort[i];
                if (CHECK_PORT_TUNNELED(pRockchipPort) && CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort) && CHECK_PORT_ENABLED(pRockchipPort)) {
                    OMX_S32 semaValue = 0, cnt = 0;
                    Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->pRockchipPort[i].bufferSemID, &semaValue);
                    if (Rockchip_OSAL_GetElemNum(&pRockchipPort->bufferQ) > semaValue) {
                        cnt = Rockchip_OSAL_GetElemNum(&pRockchipPort->bufferQ) - semaValue;
                        for (k = 0; k < cnt; k++) {
                            Rockchip_OSAL_SemaphorePost(pRockchipComponent->pRockchipPort[i].bufferSemID);
                        }
                    }
                }
            }

            pRockchipComponent->currentState = OMX_StateExecuting;
            if (pRockchipComponent->bMultiThreadProcess == OMX_FALSE) {
                Rockchip_OSAL_SignalSet(pRockchipComponent->pauseEvent);
            } else {
                for (i = 0; i < ALL_PORT_NUM; i++) {
                    Rockchip_OSAL_SignalSet(pRockchipComponent->pRockchipPort[i].pauseEvent);
                }
            }
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StatePause:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        case OMX_StateIdle:
            pRockchipComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateExecuting:
            pRockchipComponent->currentState = OMX_StatePause;
            break;
        case OMX_StateWaitForResources:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    case OMX_StateWaitForResources:
        switch (currentState) {
        case OMX_StateLoaded:
            ret = Rockchip_OMX_In_WaitForResource(pOMXComponent);
            pRockchipComponent->currentState = OMX_StateWaitForResources;
            break;
        case OMX_StateIdle:
        case OMX_StateExecuting:
        case OMX_StatePause:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        default:
            ret = OMX_ErrorIncorrectStateTransition;
            break;
        }
        break;
    default:
        ret = OMX_ErrorIncorrectStateTransition;
        break;
    }

EXIT:
    if (ret == OMX_ErrorNone) {
        if (pRockchipComponent->pCallbacks != NULL) {
            pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventCmdComplete, OMX_CommandStateSet,
                                                         destState, NULL);
        }
    } else {
        omx_err("ERROR");
        if (pRockchipComponent->pCallbacks != NULL) {
            pRockchipComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventError, ret, 0, NULL);
        }
    }
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Rockchip_OMX_MessageHandlerThread(OMX_PTR threadData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_MESSAGE       *message = NULL;
    OMX_U32                   messageType = 0, portIndex = 0;

    FunctionIn();

    if (threadData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)threadData;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    while (pRockchipComponent->bExitMessageHandlerThread == OMX_FALSE) {
        Rockchip_OSAL_SemaphoreWait(pRockchipComponent->msgSemaphoreHandle);
        message = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Dequeue(&pRockchipComponent->messageQ);
        if (message != NULL) {
            messageType = message->messageType;
            switch (messageType) {
            case OMX_CommandStateSet:
                ret = Rockchip_OMX_ComponentStateSet(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandFlush:
                ret = Rockchip_OMX_BufferFlushProcess(pOMXComponent, message->messageParam, OMX_TRUE);
                break;
            case OMX_CommandPortDisable:
                ret = Rockchip_OMX_PortDisableProcess(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandPortEnable:
                ret = Rockchip_OMX_PortEnableProcess(pOMXComponent, message->messageParam);
                break;
            case OMX_CommandMarkBuffer:
                portIndex = message->messageParam;
                pRockchipComponent->pRockchipPort[portIndex].markType.hMarkTargetComponent = ((OMX_MARKTYPE *)message->pCmdData)->hMarkTargetComponent;
                pRockchipComponent->pRockchipPort[portIndex].markType.pMarkData            = ((OMX_MARKTYPE *)message->pCmdData)->pMarkData;
                break;
            case (OMX_COMMANDTYPE)ROCKCHIP_OMX_CommandComponentDeInit:
                pRockchipComponent->bExitMessageHandlerThread = OMX_TRUE;
                break;
            default:
                break;
            }
            Rockchip_OSAL_Free(message);
            message = NULL;
        }
    }

    Rockchip_OSAL_ThreadExit(NULL);

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Rockchip_StateSet(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nParam)
{
    OMX_U32 destState = nParam;
    OMX_U32 i = 0;

    if ((destState == OMX_StateIdle) && (pRockchipComponent->currentState == OMX_StateLoaded)) {
        pRockchipComponent->transientState = ROCKCHIP_OMX_TransStateLoadedToIdle;
        for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
            pRockchipComponent->pRockchipPort[i].portState = OMX_StateIdle;
        }
        omx_trace("to OMX_StateIdle");
    } else if ((destState == OMX_StateLoaded) && (pRockchipComponent->currentState == OMX_StateIdle)) {
        pRockchipComponent->transientState = ROCKCHIP_OMX_TransStateIdleToLoaded;
        for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
            pRockchipComponent->pRockchipPort[i].portState = OMX_StateLoaded;
        }
        omx_trace("to OMX_StateLoaded");
    } else if ((destState == OMX_StateIdle) && (pRockchipComponent->currentState == OMX_StateExecuting)) {
        ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;

        pRockchipPort = &(pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX]);
        if ((pRockchipPort->portDefinition.bEnabled == OMX_FALSE) &&
            (pRockchipPort->portState == OMX_StateIdle)) {
            pRockchipPort->exceptionFlag = INVALID_STATE;
            Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
        }

        pRockchipPort = &(pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX]);
        if ((pRockchipPort->portDefinition.bEnabled == OMX_FALSE) &&
            (pRockchipPort->portState == OMX_StateIdle)) {
            pRockchipPort->exceptionFlag = INVALID_STATE;
            Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
        }

        pRockchipComponent->transientState = ROCKCHIP_OMX_TransStateExecutingToIdle;
        omx_trace("to OMX_StateIdle");
    } else if ((destState == OMX_StateExecuting) && (pRockchipComponent->currentState == OMX_StateIdle)) {
        pRockchipComponent->transientState = ROCKCHIP_OMX_TransStateIdleToExecuting;
        omx_trace("to OMX_StateExecuting");
    } else if (destState == OMX_StateInvalid) {
        for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
            pRockchipComponent->pRockchipPort[i].portState = OMX_StateInvalid;
        }
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE Rockchip_SetPortFlush(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0, cnt = 0, index = 0;


    if ((pRockchipComponent->currentState == OMX_StateExecuting) ||
        (pRockchipComponent->currentState == OMX_StatePause)) {
        if ((portIndex != ALL_PORT_INDEX) &&
            ((OMX_S32)portIndex >= (OMX_S32)pRockchipComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        /*********************
        *    need flush event set ?????
        **********************/
        cnt = (portIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;
        for (i = 0; i < cnt; i++) {
            if (portIndex == ALL_PORT_INDEX)
                index = i;
            else
                index = portIndex;
            pRockchipComponent->pRockchipPort[index].bIsPortFlushed = OMX_TRUE;
        }
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }
    ret = OMX_ErrorNone;

EXIT:
    return ret;
}

static OMX_ERRORTYPE Rockchip_SetPortEnable(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0;

    FunctionIn();

    if ((portIndex != ALL_PORT_INDEX) &&
        ((OMX_S32)portIndex >= (OMX_S32)pRockchipComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (portIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
            pRockchipPort = &pRockchipComponent->pRockchipPort[i];
            if (CHECK_PORT_ENABLED(pRockchipPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            } else {
                pRockchipPort->portState = OMX_StateIdle;
            }
        }
    } else {
        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
        if (CHECK_PORT_ENABLED(pRockchipPort)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        } else {
            pRockchipPort->portState = OMX_StateIdle;
        }
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;

}

static OMX_ERRORTYPE Rockchip_SetPortDisable(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;
    OMX_S32              portIndex = nParam;
    OMX_U16              i = 0;

    FunctionIn();

    if ((portIndex != ALL_PORT_INDEX) &&
        ((OMX_S32)portIndex >= (OMX_S32)pRockchipComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (portIndex == ALL_PORT_INDEX) {
        for (i = 0; i < pRockchipComponent->portParam.nPorts; i++) {
            pRockchipPort = &pRockchipComponent->pRockchipPort[i];
            if (!CHECK_PORT_ENABLED(pRockchipPort)) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
            pRockchipPort->portState = OMX_StateLoaded;
            pRockchipPort->bIsPortDisabled = OMX_TRUE;
        }
    } else {
        pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];
        pRockchipPort->portState = OMX_StateLoaded;
        pRockchipPort->bIsPortDisabled = OMX_TRUE;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

static OMX_ERRORTYPE Rockchip_SetMarkBuffer(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent, OMX_U32 nParam)
{
    OMX_ERRORTYPE        ret = OMX_ErrorNone;

    if (nParam >= pRockchipComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pRockchipComponent->currentState == OMX_StateExecuting) ||
        (pRockchipComponent->currentState == OMX_StatePause)) {
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorIncorrectStateOperation;
    }

EXIT:
    return ret;
}

static OMX_ERRORTYPE Rockchip_OMX_CommandQueue(
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent,
    OMX_COMMANDTYPE        Cmd,
    OMX_U32                nParam,
    OMX_PTR                pCmdData)
{
    OMX_ERRORTYPE    ret = OMX_ErrorNone;
    ROCKCHIP_OMX_MESSAGE *command = (ROCKCHIP_OMX_MESSAGE *)Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_MESSAGE));

    if (command == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    command->messageType  = (OMX_U32)Cmd;
    command->messageParam = nParam;
    command->pCmdData     = pCmdData;

    ret = Rockchip_OSAL_Queue(&pRockchipComponent->messageQ, (void *)command);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    ret = Rockchip_OSAL_SemaphorePost(pRockchipComponent->msgSemaphoreHandle);

EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_SendCommand(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_COMMANDTYPE Cmd,
    OMX_IN OMX_U32         nParam,
    OMX_IN OMX_PTR         pCmdData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (Cmd) {
    case OMX_CommandStateSet :
        omx_trace("Command: OMX_CommandStateSet");
        Rockchip_StateSet(pRockchipComponent, nParam);
        break;
    case OMX_CommandFlush :
        omx_trace("Command: OMX_CommandFlush");
        pRockchipComponent->nRkFlags |= RK_VPU_NEED_FLUSH_ON_SEEK;
        ret = Rockchip_SetPortFlush(pRockchipComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandPortDisable :
        omx_trace("Command: OMX_CommandPortDisable");
        ret = Rockchip_SetPortDisable(pRockchipComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandPortEnable :
        omx_trace("Command: OMX_CommandPortEnable");
        ret = Rockchip_SetPortEnable(pRockchipComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    case OMX_CommandMarkBuffer :
        omx_trace("Command: OMX_CommandMarkBuffer");
        ret = Rockchip_SetMarkBuffer(pRockchipComponent, nParam);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        break;
    default:
        break;
    }

    ret = Rockchip_OMX_CommandQueue(pRockchipComponent, Cmd, nParam, pCmdData);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = Rockchip_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }
        portParam->nPorts         = 0;
        portParam->nStartPortNumber     = 0;
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
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)ComponentParameterStructure;

        ret = Rockchip_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        compPriority->nGroupID       = pRockchipComponent->compPriority.nGroupID;
        compPriority->nGroupPriority = pRockchipComponent->compPriority.nGroupPriority;
    }
    break;

    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)ComponentParameterStructure;
        OMX_U32                       portIndex = bufferSupplier->nPortIndex;
        ROCKCHIP_OMX_BASEPORT          *pRockchipPort;

        if ((pRockchipComponent->currentState == OMX_StateLoaded) ||
            (pRockchipComponent->currentState == OMX_StateWaitForResources)) {
            if (portIndex >= pRockchipComponent->portParam.nPorts) {
                ret = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            ret = Rockchip_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
            if (ret != OMX_ErrorNone) {
                goto EXIT;
            }

            pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];


            if (pRockchipPort->portDefinition.eDir == OMX_DirInput) {
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else if (CHECK_PORT_TUNNELED(pRockchipPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            } else {
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyOutput;
                } else if (CHECK_PORT_TUNNELED(pRockchipPort)) {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyInput;
                } else {
                    bufferSupplier->eBufferSupplier = OMX_BufferSupplyUnspecified;
                }
            }
        } else {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
    }
    break;
    default: {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
    break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    if (ComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    case OMX_IndexParamAudioInit:
    case OMX_IndexParamVideoInit:
    case OMX_IndexParamImageInit:
    case OMX_IndexParamOtherInit: {
        OMX_PORT_PARAM_TYPE *portParam = (OMX_PORT_PARAM_TYPE *)ComponentParameterStructure;
        ret = Rockchip_OMX_Check_SizeVersion(portParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pRockchipComponent->currentState != OMX_StateLoaded) &&
            (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }
        ret = OMX_ErrorUndefined;
        /* Rockchip_OSAL_Memcpy(&pRockchipComponent->portParam, portParam, sizeof(OMX_PORT_PARAM_TYPE)); */
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

        if ((pRockchipComponent->currentState != OMX_StateLoaded) && (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
            if (pRockchipPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }
        if (portDefinition->nBufferCountActual < pRockchipPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Rockchip_OSAL_Memcpy(&pRockchipPort->portDefinition, portDefinition, portDefinition->nSize);
    }
    break;
    case OMX_IndexParamPriorityMgmt: {
        OMX_PRIORITYMGMTTYPE *compPriority = (OMX_PRIORITYMGMTTYPE *)ComponentParameterStructure;

        if ((pRockchipComponent->currentState != OMX_StateLoaded) &&
            (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        ret = Rockchip_OMX_Check_SizeVersion(compPriority, sizeof(OMX_PRIORITYMGMTTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pRockchipComponent->compPriority.nGroupID = compPriority->nGroupID;
        pRockchipComponent->compPriority.nGroupPriority = compPriority->nGroupPriority;
    }
    break;
    case OMX_IndexParamCompBufferSupplier: {
        OMX_PARAM_BUFFERSUPPLIERTYPE *bufferSupplier = (OMX_PARAM_BUFFERSUPPLIERTYPE *)ComponentParameterStructure;
        OMX_U32           portIndex = bufferSupplier->nPortIndex;
        ROCKCHIP_OMX_BASEPORT *pRockchipPort = NULL;


        if (portIndex >= pRockchipComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        ret = Rockchip_OMX_Check_SizeVersion(bufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
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

        if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyUnspecified) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
        if (CHECK_PORT_TUNNELED(pRockchipPort) == 0) {
            ret = OMX_ErrorNone; /*OMX_ErrorNone ?????*/
            goto EXIT;
        }

        if (pRockchipPort->portDefinition.eDir == OMX_DirInput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pRockchipPort->tunnelFlags |= ROCKCHIP_TUNNEL_IS_SUPPLIER;
                bufferSupplier->nPortIndex = pRockchipPort->tunneledPort;
                ret = OMX_SetParameter(pRockchipPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    pRockchipPort->tunnelFlags &= ~ROCKCHIP_TUNNEL_IS_SUPPLIER;
                    bufferSupplier->nPortIndex = pRockchipPort->tunneledPort;
                    ret = OMX_SetParameter(pRockchipPort->tunneledComponent, OMX_IndexParamCompBufferSupplier, bufferSupplier);
                }
                goto EXIT;
            }
        } else if (pRockchipPort->portDefinition.eDir == OMX_DirOutput) {
            if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyInput) {
                ret = OMX_ErrorNone;
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    pRockchipPort->tunnelFlags &= ~ROCKCHIP_TUNNEL_IS_SUPPLIER;
                    ret = OMX_ErrorNone;
                }
                goto EXIT;
            } else if (bufferSupplier->eBufferSupplier == OMX_BufferSupplyOutput) {
                /*
                if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
                    ret = OMX_ErrorNone;
                }
                */
                pRockchipPort->tunnelFlags |= ROCKCHIP_TUNNEL_IS_SUPPLIER;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }
    break;
    default: {
        ret = OMX_ErrorUnsupportedIndex;
        goto EXIT;
    }
    break;
    }

    ret = OMX_ErrorNone;

EXIT:

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_INOUT OMX_PTR     pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    switch (nIndex) {
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    switch (nIndex) {
    default:
        ret = OMX_ErrorUnsupportedIndex;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
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

    ret = OMX_ErrorBadParameter;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_SetCallbacks (
    OMX_IN OMX_HANDLETYPE    hComponent,
    OMX_IN OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN OMX_PTR           pAppData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;

        omx_err("OMX_ErrorBadParameter :%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {

        omx_err("OMX_ErrorNone :%d", __LINE__);
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {

        omx_err("OMX_ErrorBadParameter :%d", __LINE__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pCallbacks == NULL) {
        omx_err("OMX_ErrorBadParameter :%d", __LINE__);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pRockchipComponent->currentState == OMX_StateInvalid) {

        omx_err("OMX_ErrorInvalidState :%d", __LINE__);
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }
    if (pRockchipComponent->currentState != OMX_StateLoaded) {
        omx_err("OMX_StateLoaded :%d", __LINE__);
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pRockchipComponent->pCallbacks = pCallbacks;
    pRockchipComponent->callbackData = pAppData;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_UseEGLImage(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN void                     *eglImage)
{
    (void)hComponent;
    (void)ppBufferHdr;
    (void)nPortIndex;
    (void)pAppPrivate;
    (void)eglImage;
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Rockchip_OMX_BaseComponent_Constructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        omx_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pRockchipComponent = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_BASECOMPONENT));
    if (pRockchipComponent == NULL) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipComponent, 0, sizeof(ROCKCHIP_OMX_BASECOMPONENT));
    pRockchipComponent->rkversion = OMX_COMPILE_INFO;
    pOMXComponent->pComponentPrivate = (OMX_PTR)pRockchipComponent;

    ret = Rockchip_OSAL_SemaphoreCreate(&pRockchipComponent->msgSemaphoreHandle);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    ret = Rockchip_OSAL_MutexCreate(&pRockchipComponent->compMutex);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    ret = Rockchip_OSAL_SignalCreate(&pRockchipComponent->abendStateEvent);
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    pRockchipComponent->bExitMessageHandlerThread = OMX_FALSE;
    Rockchip_OSAL_QueueCreate(&pRockchipComponent->messageQ, MAX_QUEUE_ELEMENTS);
    ret = Rockchip_OSAL_ThreadCreate(&pRockchipComponent->hMessageHandler,
                                     Rockchip_OMX_MessageHandlerThread,
                                     pOMXComponent,
                                     "omx_msg_hdl");
    if (ret != OMX_ErrorNone) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }

    pRockchipComponent->bMultiThreadProcess = OMX_FALSE;

    pOMXComponent->GetComponentVersion = &Rockchip_OMX_GetComponentVersion;
    pOMXComponent->SendCommand         = &Rockchip_OMX_SendCommand;
    pOMXComponent->GetState            = &Rockchip_OMX_GetState;
    pOMXComponent->SetCallbacks        = &Rockchip_OMX_SetCallbacks;
    pOMXComponent->UseEGLImage         = &Rockchip_OMX_UseEGLImage;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_BaseComponent_Destructor(
    OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    OMX_S32                   semaValue = 0;

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

    Rockchip_OMX_CommandQueue(pRockchipComponent, (OMX_COMMANDTYPE)ROCKCHIP_OMX_CommandComponentDeInit, 0, NULL);
    Rockchip_OSAL_SleepMillisec(0);
    Rockchip_OSAL_Get_SemaphoreCount(pRockchipComponent->msgSemaphoreHandle, &semaValue);
    if (semaValue == 0)
        Rockchip_OSAL_SemaphorePost(pRockchipComponent->msgSemaphoreHandle);
    Rockchip_OSAL_SemaphorePost(pRockchipComponent->msgSemaphoreHandle);

    Rockchip_OSAL_ThreadTerminate(pRockchipComponent->hMessageHandler);
    pRockchipComponent->hMessageHandler = NULL;

    Rockchip_OSAL_SignalTerminate(pRockchipComponent->abendStateEvent);
    pRockchipComponent->abendStateEvent = NULL;
    Rockchip_OSAL_MutexTerminate(pRockchipComponent->compMutex);
    pRockchipComponent->compMutex = NULL;
    Rockchip_OSAL_SemaphoreTerminate(pRockchipComponent->msgSemaphoreHandle);
    pRockchipComponent->msgSemaphoreHandle = NULL;
    Rockchip_OSAL_QueueTerminate(&pRockchipComponent->messageQ);

    Rockchip_OSAL_Free(pRockchipComponent);
    pRockchipComponent = NULL;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}


