/*
 *
 * Copyright 2013 rockchip Electronics Co. LTD
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
 * @file       Rockchip_OMX_Baseport.c
 * @brief
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */

#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_base_port"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Rockchip_OMX_Macros.h"
#include "Rockchip_OSAL_Event.h"
#include "Rockchip_OSAL_Semaphore.h"
#include "Rockchip_OSAL_Mutex.h"

#include "Rockchip_OMX_Baseport.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "Rockchip_OSAL_Android.h"

#include "Rockchip_OSAL_Log.h"
#include "omx_video_global.h"

OMX_U32 omx_vdec_debug = 0;
OMX_U32 omx_venc_debug = 0;


OMX_ERRORTYPE Rkvpu_OMX_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    OMX_U32                   i = 0;

    Rockchip_OSAL_MutexLock(pRockchipPort->hPortMutex);
    for (i = 0; i < pRockchipPort->portDefinition.nBufferCountActual; i++) {
        if (bufferHeader == pRockchipPort->extendBufferHeader[i].OMXBufferHeader) {
            pRockchipPort->extendBufferHeader[i].bBufferInOMX = OMX_FALSE;
            break;
        }
    }

    VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: empty buffer done(%p) timeus: %lld us, flags: 0x%x",
              pRockchipComponent->componentName,
              bufferHeader->pBuffer,
              bufferHeader->nTimeStamp,
              bufferHeader->nFlags);

    Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
    pRockchipComponent->pCallbacks->EmptyBufferDone(pOMXComponent, pRockchipComponent->callbackData, bufferHeader);

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, OMX_BUFFERHEADERTYPE* bufferHeader)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    OMX_U32                   i = 0;

    Rockchip_OSAL_MutexLock(pRockchipPort->hPortMutex);
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (bufferHeader == pRockchipPort->extendBufferHeader[i].OMXBufferHeader) {
            pRockchipPort->extendBufferHeader[i].bBufferInOMX = OMX_FALSE;
            break;
        }
    }

    if (bufferHeader->nFlags & OMX_BUFFERFLAG_EOS) {
        VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: fill EOS buffer done(%p) timeus: %lld us, flags: 0x%x",
                  pRockchipComponent->componentName,
                  bufferHeader->pBuffer,
                  bufferHeader->nTimeStamp,
                  bufferHeader->nFlags);

    } else {
        VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: fill buffer done(%p) timeus: %lld us, flags: 0x%x",
                  pRockchipComponent->componentName,
                  bufferHeader->pBuffer,
                  bufferHeader->nTimeStamp,
                  bufferHeader->nFlags);
    }

    Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
    pRockchipComponent->pCallbacks->FillBufferDone(pOMXComponent, pRockchipComponent->callbackData, bufferHeader);

    goto EXIT;
EXIT:
    omx_trace("bufferHeader:0x%x", bufferHeader);
    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_BufferFlushProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    OMX_S32                   portIndex = 0;
    OMX_U32                   i = 0, cnt = 0;

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

    cnt = (nPortIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        pRockchipComponent->rockchip_BufferFlush(pOMXComponent, portIndex, bEvent);
    }

    VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: buffer flush.", pRockchipComponent->componentName);

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

OMX_ERRORTYPE Rockchip_OMX_EnablePort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;

    FunctionIn();

    pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

    if ((pRockchipComponent->currentState != OMX_StateLoaded) && (pRockchipComponent->currentState != OMX_StateWaitForResources)) {
        Rockchip_OSAL_SemaphoreWait(pRockchipPort->loadedResource);

        if (pRockchipPort->exceptionFlag == INVALID_STATE) {
            pRockchipPort->exceptionFlag = NEED_PORT_DISABLE;
            goto EXIT;
        }
        pRockchipPort->portDefinition.bPopulated = OMX_TRUE;
    }
    pRockchipPort->exceptionFlag = GENERAL_STATE;
    pRockchipPort->portDefinition.bEnabled = OMX_TRUE;

    VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: now enable %s port.",
              pRockchipComponent->componentName,
              portIndex == INPUT_PORT_INDEX ? "input" : "output");

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_PortEnableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    OMX_S32                portIndex = 0;
    OMX_U32                i = 0, cnt = 0;

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

    cnt = (nPortIndex == ALL_PORT_INDEX) ? ALL_PORT_NUM : 1;

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        ret = Rockchip_OMX_EnablePort(pOMXComponent, portIndex);
        if (ret == OMX_ErrorNone) {
            pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandPortEnable, portIndex, NULL);
        }
    }

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pRockchipComponent != NULL)) {
        pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pRockchipComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_DisablePort(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 portIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    ROCKCHIP_OMX_MESSAGE       *message;

    FunctionIn();

    pRockchipPort = &pRockchipComponent->pRockchipPort[portIndex];

    if (!CHECK_PORT_ENABLED(pRockchipPort)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pRockchipComponent->currentState != OMX_StateLoaded) {
        if (CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)) {
            while (Rockchip_OSAL_GetElemNum(&pRockchipPort->bufferQ) > 0) {
                message = (ROCKCHIP_OMX_MESSAGE*)Rockchip_OSAL_Dequeue(&pRockchipPort->bufferQ);
                Rockchip_OSAL_Free(message);
            }
        }
        pRockchipPort->portDefinition.bPopulated = OMX_FALSE;
        Rockchip_OSAL_SemaphoreWait(pRockchipPort->unloadedResource);
    }


    if (pRockchipComponent->codecType == HW_VIDEO_DEC_CODEC
        && portIndex == OUTPUT_PORT_INDEX) {
        ret = Rkvpu_ComputeDecBufferCount((OMX_HANDLETYPE)pOMXComponent);
        if (ret != OMX_ErrorNone) {
            omx_err("compute decoder buffer count failed!");
            goto EXIT;
        }
    }
    pRockchipPort->portDefinition.bEnabled = OMX_FALSE;

    VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: now disable %s port.",
              pRockchipComponent->componentName,
              portIndex == INPUT_PORT_INDEX ? "input" : "output");

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_PortDisableProcess(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    OMX_S32                portIndex = 0;
    OMX_U32                i = 0, cnt = 0;

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

    cnt = (nPortIndex == ALL_PORT_INDEX ) ? ALL_PORT_NUM : 1;

    /* port flush*/
    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        Rockchip_OMX_BufferFlushProcess(pOMXComponent, portIndex, OMX_FALSE);
    }

    for (i = 0; i < cnt; i++) {
        if (nPortIndex == ALL_PORT_INDEX)
            portIndex = i;
        else
            portIndex = nPortIndex;

        ret = Rockchip_OMX_DisablePort(pOMXComponent, portIndex);
        pRockchipComponent->pRockchipPort[portIndex].bIsPortDisabled = OMX_FALSE;
        if (ret == OMX_ErrorNone) {
            pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                         pRockchipComponent->callbackData,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandPortDisable, portIndex, NULL);
        }
    }

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pRockchipComponent != NULL)) {
        pRockchipComponent->pCallbacks->EventHandler(pOMXComponent,
                                                     pRockchipComponent->callbackData,
                                                     OMX_EventError,
                                                     ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_EmptyThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BOOL               findBuffer = OMX_FALSE;
    ROCKCHIP_OMX_MESSAGE       *message;
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
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pBuffer->nInputPortIndex != INPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = Rockchip_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pRockchipComponent->currentState != OMX_StateIdle) &&
        (pRockchipComponent->currentState != OMX_StateExecuting) &&
        (pRockchipComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pRockchipPort = &pRockchipComponent->pRockchipPort[INPUT_PORT_INDEX];
    if ((!CHECK_PORT_ENABLED(pRockchipPort)) ||
        (CHECK_PORT_BEING_FLUSHED(pRockchipPort) &&
         (!CHECK_PORT_TUNNELED(pRockchipPort) || !CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort))) ||
        ((pRockchipComponent->transientState == ROCKCHIP_OMX_TransStateExecutingToIdle) &&
         (CHECK_PORT_TUNNELED(pRockchipPort) && !CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)))) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    Rockchip_OSAL_MutexLock(pRockchipPort->hPortMutex);
    for (i = 0; i < pRockchipPort->portDefinition.nBufferCountActual; i++) {
        if (pBuffer == pRockchipPort->extendBufferHeader[i].OMXBufferHeader) {
            pRockchipPort->extendBufferHeader[i].bBufferInOMX = OMX_TRUE;
            findBuffer = OMX_TRUE;
            break;
        }
    }

    if (findBuffer == OMX_FALSE) {
        ret = OMX_ErrorBadParameter;
        Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
        goto EXIT;
    }

    if (pBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA) {
        VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: empty this extradata buffer(%p) timeus: %lld us, size: %d, flags: 0x%x",
                  pRockchipComponent->componentName,
                  pBuffer->pBuffer,
                  pBuffer->nTimeStamp,
                  pBuffer->nFilledLen,
                  pBuffer->nFlags);

    } else if (pBuffer->nFlags & OMX_BUFFERFLAG_EOS) {
        VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: empty this EOS buffer(%p) timeus: %lld us, size: %d, flags: 0x%x",
                  pRockchipComponent->componentName,
                  pBuffer->pBuffer,
                  pBuffer->nTimeStamp,
                  pBuffer->nFilledLen,
                  pBuffer->nFlags);
    } else {
        VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: empty this buffer(%p) timeus: %lld us, size: %d, flags: 0x%x",
                  pRockchipComponent->componentName,
                  pBuffer->pBuffer,
                  pBuffer->nTimeStamp,
                  pBuffer->nFilledLen,
                  pBuffer->nFlags);
    }

    message = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_MESSAGE));
    if (message == NULL) {
        ret = OMX_ErrorInsufficientResources;
        Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
        goto EXIT;
    }
    message->messageType = ROCKCHIP_OMX_CommandEmptyBuffer;
    message->messageParam = (OMX_U32) i;
    message->pCmdData = (OMX_PTR)pBuffer;

    ret = Rockchip_OSAL_Queue(&pRockchipPort->bufferQ, (void *)message);

    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
        goto EXIT;
    }
    ret = Rockchip_OSAL_SemaphorePost(pRockchipPort->bufferSemID);
    Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_FillThisBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_BUFFERHEADERTYPE *pBuffer)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    OMX_BOOL               findBuffer = OMX_FALSE;
    ROCKCHIP_OMX_MESSAGE       *message;
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
    if (pRockchipComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (pBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pBuffer->nOutputPortIndex != OUTPUT_PORT_INDEX) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    ret = Rockchip_OMX_Check_SizeVersion(pBuffer, sizeof(OMX_BUFFERHEADERTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if ((pRockchipComponent->currentState != OMX_StateIdle) &&
        (pRockchipComponent->currentState != OMX_StateExecuting) &&
        (pRockchipComponent->currentState != OMX_StatePause)) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    pRockchipPort = &pRockchipComponent->pRockchipPort[OUTPUT_PORT_INDEX];
    if ((!CHECK_PORT_ENABLED(pRockchipPort)) ||
        (CHECK_PORT_BEING_FLUSHED(pRockchipPort) &&
         (!CHECK_PORT_TUNNELED(pRockchipPort) || !CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort))) ||
        ((pRockchipComponent->transientState == ROCKCHIP_OMX_TransStateExecutingToIdle) &&
         (CHECK_PORT_TUNNELED(pRockchipPort) && !CHECK_PORT_BUFFER_SUPPLIER(pRockchipPort)))) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    Rockchip_OSAL_MutexLock(pRockchipPort->hPortMutex);
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (pBuffer == pRockchipPort->extendBufferHeader[i].OMXBufferHeader) {
            pRockchipPort->extendBufferHeader[i].bBufferInOMX = OMX_TRUE;
            findBuffer = OMX_TRUE;
            break;
        }
    }

    if (findBuffer == OMX_FALSE) {
        ret = OMX_ErrorBadParameter;
        Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
        goto EXIT;
    }

    VIDEO_DBG(VIDEO_DBG_LOG_PORT, "[%s]: fill this buffer(%p) flags: 0x%x",
              pRockchipComponent->componentName,
              pBuffer->pBuffer,
              pBuffer->nFlags);

    if (pRockchipPort->bufferProcessType == BUFFER_SHARE) {
        Rockchip_OSAL_Fd2VpumemPool(pRockchipComponent, pRockchipPort->extendBufferHeader[i].OMXBufferHeader);
    } else {
        message = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_MESSAGE));
        if (message == NULL) {
            ret = OMX_ErrorInsufficientResources;
            Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
            goto EXIT;
        }
        message->messageType = ROCKCHIP_OMX_CommandFillBuffer;
        message->messageParam = (OMX_U32) i;
        message->pCmdData = (OMX_PTR)pBuffer;

        ret = Rockchip_OSAL_Queue(&pRockchipPort->bufferQ, (void *)message);
        if (ret != 0) {
            ret = OMX_ErrorUndefined;
            Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);
            goto EXIT;
        }
    }

    ret = Rockchip_OSAL_SemaphorePost(pRockchipPort->bufferSemID);
    Rockchip_OSAL_MutexUnlock(pRockchipPort->hPortMutex);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_Port_Constructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipInputPort = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipOutputPort = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        omx_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Rockchip_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        omx_err("OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    pRockchipComponent = (ROCKCHIP_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    INIT_SET_SIZE_VERSION(&pRockchipComponent->portParam, OMX_PORT_PARAM_TYPE);
    pRockchipComponent->portParam.nPorts = ALL_PORT_NUM;
    pRockchipComponent->portParam.nStartPortNumber = INPUT_PORT_INDEX;

    pRockchipPort = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_BASEPORT) * ALL_PORT_NUM);
    if (pRockchipPort == NULL) {
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipPort, 0, sizeof(ROCKCHIP_OMX_BASEPORT) * ALL_PORT_NUM);
    pRockchipComponent->pRockchipPort = pRockchipPort;

    /* Input Port */
    pRockchipInputPort = &pRockchipPort[INPUT_PORT_INDEX];

    Rockchip_OSAL_QueueCreate(&pRockchipInputPort->bufferQ, MAX_QUEUE_ELEMENTS);
    Rockchip_OSAL_QueueCreate(&pRockchipInputPort->securebufferQ, MAX_QUEUE_ELEMENTS);

    pRockchipInputPort->extendBufferHeader = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);
    if (pRockchipInputPort->extendBufferHeader == NULL) {
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipInputPort->extendBufferHeader, 0, sizeof(ROCKCHIP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pRockchipInputPort->bufferStateAllocate = Rockchip_OSAL_Malloc(sizeof(OMX_U32) * MAX_BUFFER_NUM);
    if (pRockchipInputPort->bufferStateAllocate == NULL) {
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        omx_err("OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipInputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pRockchipInputPort->bufferSemID = NULL;
    pRockchipInputPort->assignedBufferNum = 0;
    pRockchipInputPort->portState = OMX_StateMax;
    pRockchipInputPort->bIsPortFlushed = OMX_FALSE;
    pRockchipInputPort->bIsPortDisabled = OMX_FALSE;
    pRockchipInputPort->tunneledComponent = NULL;
    pRockchipInputPort->tunneledPort = 0;
    pRockchipInputPort->tunnelBufferNum = 0;
    pRockchipInputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pRockchipInputPort->tunnelFlags = 0;
    ret = Rockchip_OSAL_SemaphoreCreate(&pRockchipInputPort->loadedResource);
    if (ret != OMX_ErrorNone) {
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        goto EXIT;
    }
    ret = Rockchip_OSAL_SemaphoreCreate(&pRockchipInputPort->unloadedResource);
    if (ret != OMX_ErrorNone) {
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->loadedResource);
        pRockchipInputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        goto EXIT;
    }

    INIT_SET_SIZE_VERSION(&pRockchipInputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pRockchipInputPort->portDefinition.nPortIndex = INPUT_PORT_INDEX;
    pRockchipInputPort->portDefinition.eDir = OMX_DirInput;
    pRockchipInputPort->portDefinition.nBufferCountActual = 0;
    pRockchipInputPort->portDefinition.nBufferCountMin = 0;
    pRockchipInputPort->portDefinition.nBufferSize = 0;
    pRockchipInputPort->portDefinition.bEnabled = OMX_FALSE;
    pRockchipInputPort->portDefinition.bPopulated = OMX_FALSE;
    pRockchipInputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pRockchipInputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pRockchipInputPort->portDefinition.nBufferAlignment = 0;
    pRockchipInputPort->markType.hMarkTargetComponent = NULL;
    pRockchipInputPort->markType.pMarkData = NULL;
    pRockchipInputPort->exceptionFlag = GENERAL_STATE;

    /* Output Port */
    pRockchipOutputPort = &pRockchipPort[OUTPUT_PORT_INDEX];

    Rockchip_OSAL_QueueCreate(&pRockchipOutputPort->bufferQ, MAX_QUEUE_ELEMENTS); /* For in case of "Output Buffer Share", MAX ELEMENTS(DPB + EDPB) */

    pRockchipOutputPort->extendBufferHeader = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);
    if (pRockchipOutputPort->extendBufferHeader == NULL) {
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->unloadedResource);
        pRockchipInputPort->unloadedResource = NULL;
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->loadedResource);
        pRockchipInputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipOutputPort->extendBufferHeader, 0, sizeof(ROCKCHIP_OMX_BUFFERHEADERTYPE) * MAX_BUFFER_NUM);

    pRockchipOutputPort->bufferStateAllocate = Rockchip_OSAL_Malloc(sizeof(OMX_U32) * MAX_BUFFER_NUM);
    if (pRockchipOutputPort->bufferStateAllocate == NULL) {
        Rockchip_OSAL_Free(pRockchipOutputPort->extendBufferHeader);
        pRockchipOutputPort->extendBufferHeader = NULL;

        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->unloadedResource);
        pRockchipInputPort->unloadedResource = NULL;
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->loadedResource);
        pRockchipInputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Rockchip_OSAL_Memset(pRockchipOutputPort->bufferStateAllocate, 0, sizeof(OMX_U32) * MAX_BUFFER_NUM);

    pRockchipOutputPort->bufferSemID = NULL;
    pRockchipOutputPort->assignedBufferNum = 0;
    pRockchipOutputPort->portState = OMX_StateMax;
    pRockchipOutputPort->bIsPortFlushed = OMX_FALSE;
    pRockchipOutputPort->bIsPortDisabled = OMX_FALSE;
    pRockchipOutputPort->tunneledComponent = NULL;
    pRockchipOutputPort->tunneledPort = 0;
    pRockchipOutputPort->tunnelBufferNum = 0;
    pRockchipOutputPort->bufferSupplier = OMX_BufferSupplyUnspecified;
    pRockchipOutputPort->tunnelFlags = 0;
    ret = Rockchip_OSAL_SemaphoreCreate(&pRockchipOutputPort->loadedResource);
    if (ret != OMX_ErrorNone) {
        Rockchip_OSAL_Free(pRockchipOutputPort->bufferStateAllocate);
        pRockchipOutputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipOutputPort->extendBufferHeader);
        pRockchipOutputPort->extendBufferHeader = NULL;

        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->unloadedResource);
        pRockchipInputPort->unloadedResource = NULL;
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->loadedResource);
        pRockchipInputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        goto EXIT;
    }
    ret = Rockchip_OSAL_SignalCreate(&pRockchipOutputPort->unloadedResource);
    if (ret != OMX_ErrorNone) {
        Rockchip_OSAL_SemaphoreTerminate(pRockchipOutputPort->loadedResource);
        pRockchipOutputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipOutputPort->bufferStateAllocate);
        pRockchipOutputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipOutputPort->extendBufferHeader);
        pRockchipOutputPort->extendBufferHeader = NULL;

        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->unloadedResource);
        pRockchipInputPort->unloadedResource = NULL;
        Rockchip_OSAL_SemaphoreTerminate(pRockchipInputPort->loadedResource);
        pRockchipInputPort->loadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->bufferStateAllocate);
        pRockchipInputPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipInputPort->extendBufferHeader);
        pRockchipInputPort->extendBufferHeader = NULL;
        Rockchip_OSAL_Free(pRockchipPort);
        pRockchipPort = NULL;
        goto EXIT;
    }

    INIT_SET_SIZE_VERSION(&pRockchipOutputPort->portDefinition, OMX_PARAM_PORTDEFINITIONTYPE);
    pRockchipOutputPort->portDefinition.nPortIndex = OUTPUT_PORT_INDEX;
    pRockchipOutputPort->portDefinition.eDir = OMX_DirOutput;
    pRockchipOutputPort->portDefinition.nBufferCountActual = 0;
    pRockchipOutputPort->portDefinition.nBufferCountMin = 0;
    pRockchipOutputPort->portDefinition.nBufferSize = 0;
    pRockchipOutputPort->portDefinition.bEnabled = OMX_FALSE;
    pRockchipOutputPort->portDefinition.bPopulated = OMX_FALSE;
    pRockchipOutputPort->portDefinition.eDomain = OMX_PortDomainMax;
    pRockchipOutputPort->portDefinition.bBuffersContiguous = OMX_FALSE;
    pRockchipOutputPort->portDefinition.nBufferAlignment = 0;
    pRockchipOutputPort->markType.hMarkTargetComponent = NULL;
    pRockchipOutputPort->markType.pMarkData = NULL;
    pRockchipOutputPort->exceptionFlag = GENERAL_STATE;

    pRockchipComponent->checkTimeStamp.needSetStartTimeStamp = OMX_FALSE;
    pRockchipComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
    pRockchipComponent->checkTimeStamp.startTimeStamp = 0;
    pRockchipComponent->checkTimeStamp.nStartFlags = 0x0;

    pOMXComponent->EmptyThisBuffer = &Rockchip_OMX_EmptyThisBuffer;
    pOMXComponent->FillThisBuffer  = &Rockchip_OMX_FillThisBuffer;

    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_Port_Destructor(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent = NULL;
    ROCKCHIP_OMX_BASEPORT      *pRockchipPort = NULL;
    int i = 0;

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

    if (pRockchipComponent->transientState == ROCKCHIP_OMX_TransStateLoadedToIdle) {
        pRockchipComponent->abendState = OMX_TRUE;
        for (i = 0; i < ALL_PORT_NUM; i++) {
            pRockchipPort = &pRockchipComponent->pRockchipPort[i];
            Rockchip_OSAL_SemaphorePost(pRockchipPort->loadedResource);
        }
        Rockchip_OSAL_SignalWait(pRockchipComponent->abendStateEvent, DEF_MAX_WAIT_TIME);
        Rockchip_OSAL_SignalReset(pRockchipComponent->abendStateEvent);
    }

    for (i = 0; i < ALL_PORT_NUM; i++) {
        pRockchipPort = &pRockchipComponent->pRockchipPort[i];

        Rockchip_OSAL_SemaphoreTerminate(pRockchipPort->loadedResource);
        pRockchipPort->loadedResource = NULL;
        Rockchip_OSAL_SemaphoreTerminate(pRockchipPort->unloadedResource);
        pRockchipPort->unloadedResource = NULL;
        Rockchip_OSAL_Free(pRockchipPort->bufferStateAllocate);
        pRockchipPort->bufferStateAllocate = NULL;
        Rockchip_OSAL_Free(pRockchipPort->extendBufferHeader);
        pRockchipPort->extendBufferHeader = NULL;

        Rockchip_OSAL_QueueTerminate(&pRockchipPort->bufferQ);
        Rockchip_OSAL_QueueTerminate(&pRockchipPort->securebufferQ);
    }
    Rockchip_OSAL_Free(pRockchipComponent->pRockchipPort);
    pRockchipComponent->pRockchipPort = NULL;
    ret = OMX_ErrorNone;
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_ResetDataBuffer(ROCKCHIP_OMX_DATABUFFER *pDataBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pDataBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pDataBuffer->dataValid     = OMX_FALSE;
    pDataBuffer->dataLen       = 0;
    pDataBuffer->remainDataLen = 0;
    pDataBuffer->usedDataLen   = 0;
    pDataBuffer->bufferHeader  = NULL;
    pDataBuffer->nFlags        = 0;
    pDataBuffer->timeStamp     = 0;
    pDataBuffer->pPrivate      = NULL;

EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_ResetCodecData(ROCKCHIP_OMX_DATA *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pData == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pData->dataLen       = 0;
    pData->usedDataLen   = 0;
    pData->remainDataLen = 0;
    pData->nFlags        = 0;
    pData->timeStamp     = 0;
    pData->pPrivate      = NULL;
    pData->bufferHeader  = NULL;
    pData->allocSize     = 0;

EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_Shared_BufferToData(ROCKCHIP_OMX_DATABUFFER *pUseBuffer, ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_PLANE nPlane)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (nPlane == ONE_PLANE) {
        /* Case of Shared Buffer, Only support singlePlaneBuffer */
        pData->buffer.singlePlaneBuffer.dataBuffer = pUseBuffer->bufferHeader->pBuffer;
    } else {
        omx_err("Can not support plane");
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    }

    pData->allocSize     = pUseBuffer->allocSize;
    pData->dataLen       = pUseBuffer->dataLen;
    pData->usedDataLen   = pUseBuffer->usedDataLen;
    pData->remainDataLen = pUseBuffer->remainDataLen;
    pData->timeStamp     = pUseBuffer->timeStamp;
    pData->nFlags        = pUseBuffer->nFlags;
    pData->pPrivate      = pUseBuffer->pPrivate;
    pData->bufferHeader  = pUseBuffer->bufferHeader;

EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_Shared_DataToBuffer(ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_DATABUFFER *pUseBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    pUseBuffer->bufferHeader          = pData->bufferHeader;
    pUseBuffer->allocSize             = pData->allocSize;
    pUseBuffer->dataLen               = pData->dataLen;
    pUseBuffer->usedDataLen           = pData->usedDataLen;
    pUseBuffer->remainDataLen         = pData->remainDataLen;
    pUseBuffer->timeStamp             = pData->timeStamp;
    pUseBuffer->nFlags                = pData->nFlags;
    pUseBuffer->pPrivate              = pData->pPrivate;

    return ret;
}
