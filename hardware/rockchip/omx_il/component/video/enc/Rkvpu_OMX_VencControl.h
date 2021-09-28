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
 * @file        Rkvpu_OMX_VencControl.h
 * @brief
 * @author      Csy (csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.28 : Create
 */

#ifndef RKVPU_OMX_VIDEO_ENCODERCONTROL
#define RKVPU_OMX_VIDEO_ENCODERCONTROL

#include "OMX_Component.h"
#include "Rockchip_OMX_Def.h"
#include "Rockchip_OSAL_Queue.h"
#include "Rockchip_OMX_Baseport.h"
#include "Rockchip_OMX_Basecomponent.h"
#include "library_register.h"
#include "vpu_global.h"
#include "OMX_IndexExt.h"

#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Rkvpu_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer);

OMX_ERRORTYPE Rkvpu_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBuffer,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes);

OMX_ERRORTYPE Rkvpu_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_U32        nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr);

OMX_ERRORTYPE Rkvpu_OMX_AllocateTunnelBuffer(
    ROCKCHIP_OMX_BASEPORT *pOMXBasePort,
    OMX_U32           nPortIndex);

OMX_ERRORTYPE Rkvpu_OMX_FreeTunnelBuffer(
    ROCKCHIP_OMX_BASEPORT *pOMXBasePort,
    OMX_U32           nPortIndex);

OMX_ERRORTYPE Rkvpu_OMX_ComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32         nPort,
    OMX_IN OMX_HANDLETYPE  hTunneledComp,
    OMX_IN OMX_U32         nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup);

OMX_ERRORTYPE Rkvpu_OMX_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     ComponentParameterStructure);

OMX_ERRORTYPE Rkvpu_OMX_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        ComponentParameterStructure);

OMX_ERRORTYPE Rkvpu_OMX_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE Rkvpu_OMX_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure);

OMX_ERRORTYPE Rkvpu_OMX_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex);


OMX_ERRORTYPE Rkvpu_OMX_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType);

OMX_ERRORTYPE Rkvpu_InputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATABUFFER *dataBuffer);
OMX_ERRORTYPE Rkvpu_OutputBufferReturn(OMX_COMPONENTTYPE *pOMXComponent, ROCKCHIP_OMX_DATABUFFER *dataBuffer);
OMX_ERRORTYPE Rkvpu_OMX_BufferFlush(OMX_COMPONENTTYPE *pOMXComponent, OMX_S32 nPortIndex, OMX_BOOL bEvent);
OMX_ERRORTYPE Rkvpu_OutputBufferGetQueue(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent);
OMX_ERRORTYPE Rkvpu_InputBufferGetQueue(ROCKCHIP_OMX_BASECOMPONENT *pRockchipComponent);
OMX_ERRORTYPE Rkvpu_ResolutionUpdate(OMX_COMPONENTTYPE *pOMXComponent);


#ifdef USE_ANB
OMX_ERRORTYPE Rkvpu_Shared_ANBBufferToData(ROCKCHIP_OMX_DATABUFFER *pUseBuffer, ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_BASEPORT *pRockchipPort, ROCKCHIP_OMX_PLANE nPlane);
OMX_ERRORTYPE Rkvpu_Shared_DataToANBBuffer(ROCKCHIP_OMX_DATA *pData, ROCKCHIP_OMX_DATABUFFER *pUseBuffer, ROCKCHIP_OMX_BASEPORT *pRockchipPort);
#endif

#ifdef __cplusplus
}
#endif

#endif

