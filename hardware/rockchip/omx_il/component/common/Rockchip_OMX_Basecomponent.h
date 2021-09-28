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
 * @file       Rockchip_OMX_Basecomponent.h
 * @brief
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */

#ifndef ROCKCHIP_OMX_BASECOMP
#define ROCKCHIP_OMX_BASECOMP

#include "Rockchip_OMX_Def.h"
#include "OMX_Component.h"
#include "Rockchip_OSAL_Queue.h"
#include "Rockchip_OMX_Baseport.h"


typedef struct _ROCKCHIP_OMX_MESSAGE {
    OMX_U32 messageType;
    OMX_U32 messageParam;
    OMX_PTR pCmdData;
} ROCKCHIP_OMX_MESSAGE;

/* for Check TimeStamp after Seek */
typedef struct _ROCKCHIP_OMX_TIMESTAMP {
    OMX_BOOL  needSetStartTimeStamp;
    OMX_BOOL  needCheckStartTimeStamp;
    OMX_TICKS startTimeStamp;
    OMX_U32   nStartFlags;
} ROCKCHIP_OMX_TIMESTAMP;

typedef enum _ROCKCHIP_ON2_FLAGS_MAP {
    RK_VPU_NEED_FLUSH_ON_SEEK      = 0x01,
} ROCKCHIP_ON2_FLAGS_MAP;

typedef struct _ROCKCHIP_OMX_BASECOMPONENT {
    OMX_STRING                      componentName;
    OMX_STRING                      rkversion;
    OMX_VERSIONTYPE                 componentVersion;
    OMX_VERSIONTYPE                 specVersion;

    OMX_STATETYPE                   currentState;
    ROCKCHIP_OMX_TRANS_STATETYPE    transientState;
    OMX_BOOL                        abendState;
    OMX_HANDLETYPE                  abendStateEvent;

    ROCKCHIP_CODEC_TYPE             codecType;

    ROCKCHIP_OMX_PRIORITYMGMTTYPE   compPriority;
    OMX_MARKTYPE                    propagateMarkType;
    OMX_HANDLETYPE                  compMutex;

    OMX_HANDLETYPE                  hComponentHandle;

    /* Message Handler */
    OMX_BOOL                        bExitMessageHandlerThread;
    OMX_HANDLETYPE                  hMessageHandler;
    OMX_HANDLETYPE                  msgSemaphoreHandle;
    ROCKCHIP_QUEUE                  messageQ;

    /* Port */
    OMX_PORT_PARAM_TYPE             portParam;
    ROCKCHIP_OMX_BASEPORT           *pRockchipPort;

    OMX_HANDLETYPE                  pauseEvent;

    /* Callback function */
    OMX_CALLBACKTYPE                *pCallbacks;
    OMX_PTR                         callbackData;

    /* Save Timestamp */
    OMX_TICKS                       timeStamp[MAX_TIMESTAMP];
    ROCKCHIP_OMX_TIMESTAMP          checkTimeStamp;

    /* Save Flags */
    OMX_U32                         nFlags[MAX_FLAGS];

    OMX_BOOL                        getAllDelayBuffer;
    OMX_BOOL                        reInputData;

    OMX_BOOL                        bUseFlagEOF;
    OMX_BOOL                        bSaveFlagEOS;    //bSaveFlagEOS is OMX_TRUE, if EOS flag is incoming.
    OMX_BOOL                        bBehaviorEOS;    //bBehaviorEOS is OMX_TRUE, if EOS flag with Data are incoming.
    OMX_U32                         nRkFlags;       //used to set extension flag for control vpu.
    /* Check for Old & New OMX Process type switch */
    OMX_BOOL bMultiThreadProcess;

    OMX_ERRORTYPE (*rockchip_codec_componentInit)(OMX_COMPONENTTYPE *pOMXComponent);
    OMX_ERRORTYPE (*rockchip_codec_componentTerminate)(OMX_COMPONENTTYPE *pOMXComponent);

    OMX_ERRORTYPE (*rockchip_AllocateTunnelBuffer)(ROCKCHIP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex);
    OMX_ERRORTYPE (*rockchip_FreeTunnelBuffer)(ROCKCHIP_OMX_BASEPORT *pOMXBasePort, OMX_U32 nPortIndex);
    OMX_ERRORTYPE (*rockchip_BufferProcessCreate)(OMX_COMPONENTTYPE *pOMXComponent);
    OMX_ERRORTYPE (*rockchip_BufferProcessTerminate)(OMX_COMPONENTTYPE *pOMXComponent);
    OMX_ERRORTYPE (*rockchip_BufferFlush)(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
} ROCKCHIP_OMX_BASECOMPONENT;

OMX_ERRORTYPE Rockchip_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure);

OMX_ERRORTYPE Rockchip_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure);

OMX_ERRORTYPE Rockchip_OMX_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_INOUT OMX_PTR     pComponentConfigStructure);

OMX_ERRORTYPE Rockchip_OMX_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentConfigStructure);

OMX_ERRORTYPE Rockchip_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType);

OMX_ERRORTYPE Rockchip_OMX_BaseComponent_Constructor(OMX_IN OMX_HANDLETYPE hComponent);
OMX_ERRORTYPE Rockchip_OMX_BaseComponent_Destructor(OMX_IN OMX_HANDLETYPE hComponent);

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Rockchip_OMX_Check_SizeVersion(OMX_PTR header, OMX_U32 size);


#ifdef __cplusplus
};
#endif

#endif
