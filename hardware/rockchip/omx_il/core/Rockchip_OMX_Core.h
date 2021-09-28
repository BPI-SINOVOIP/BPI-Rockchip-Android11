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
 * @file       Rockchip_OMX_Core.h
 * @brief      Rockchip OpenMAX IL Core
 * @author     csy(csy@rock-chips.com)
 *
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */

#ifndef ROCKCHIP_OMX_CORE
#define ROCKCHIP_OMX_CORE

#include "Rockchip_OMX_Def.h"
#include "OMX_Types.h"
#include "OMX_Core.h"


#ifdef __cplusplus
extern "C" {
#endif


ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_Init(void);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_DeInit(void);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_ComponentNameEnum(
    OMX_OUT   OMX_STRING        cComponentName,
    OMX_IN    OMX_U32           nNameLength,
    OMX_IN    OMX_U32           nIndex);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_GetHandle(
    OMX_OUT   OMX_HANDLETYPE   *pHandle,
    OMX_IN    OMX_STRING        cComponentName,
    OMX_IN    OMX_PTR           pAppData,
    OMX_IN    OMX_CALLBACKTYPE *pCallBacks);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_FreeHandle(
    OMX_IN    OMX_HANDLETYPE    hComponent);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_SetupTunnel(
    OMX_IN    OMX_HANDLETYPE    hOutput,
    OMX_IN    OMX_U32           nPortOutput,
    OMX_IN    OMX_HANDLETYPE    hInput,
    OMX_IN    OMX_U32           nPortInput);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE              RKOMX_GetContentPipe(
    OMX_OUT   OMX_HANDLETYPE   *hPipe,
    OMX_IN    OMX_STRING        szURI);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE              RKOMX_GetComponentsOfRole(
    OMX_IN    OMX_STRING        role,
    OMX_INOUT OMX_U32          *pNumComps,
    OMX_INOUT OMX_U8          **compNames);
ROCKCHIP_EXPORT_REF OMX_API OMX_ERRORTYPE              RKOMX_GetRolesOfComponent(
    OMX_IN    OMX_STRING        compName,
    OMX_INOUT OMX_U32          *pNumRoles,
    OMX_OUT   OMX_U8          **roles);

#define MAX_COMPONENT_ROLE_NUM  1

typedef struct _omx_core_cb_type {
    char compName[64];  // Component name
    char roles[32]; // roles played
} omx_core_cb_type;


#ifdef __cplusplus
}
#endif

#endif

