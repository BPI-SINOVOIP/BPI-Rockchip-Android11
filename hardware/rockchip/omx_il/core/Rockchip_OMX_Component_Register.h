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
 * @file       Rockchip_OMX_Component_Register.h
 * @brief      Rockchip OpenMAX IL Component Register
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */

#ifndef ROCKCHIP_OMX_COMPONENT_REG
#define ROCKCHIP_OMX_COMPONENT_REG

#include "Rockchip_OMX_Def.h"
#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Component.h"


typedef struct _RockchipRegisterComponentType {
    OMX_U8  componentName[MAX_OMX_COMPONENT_NAME_SIZE];
    OMX_U8  roles[MAX_OMX_COMPONENT_ROLE_NUM][MAX_OMX_COMPONENT_ROLE_SIZE];
    OMX_U32 totalRoleNum;
} RockchipRegisterComponentType;

typedef struct _ROCKCHIP_OMX_COMPONENT_REGLIST {
    RockchipRegisterComponentType component;
    OMX_U8  libName[MAX_OMX_COMPONENT_LIBNAME_SIZE];
} ROCKCHIP_OMX_COMPONENT_REGLIST;

struct ROCKCHIP_OMX_COMPONENT;
typedef struct _ROCKCHIP_OMX_COMPONENT {
    OMX_U8                          componentName[MAX_OMX_COMPONENT_NAME_SIZE];
    OMX_U8                          libName[MAX_OMX_COMPONENT_LIBNAME_SIZE];
    OMX_HANDLETYPE                  libHandle;
    OMX_STRING                      rkversion;
    OMX_COMPONENTTYPE               *pOMXComponent;
    struct _ROCKCHIP_OMX_COMPONENT  *nextOMXComp;
} ROCKCHIP_OMX_COMPONENT;

typedef struct __ROCKCHIP_COMPONENT_INFO {
    char *comp_type;
    char *lib_name;
} ROCKCHIP_COMPONENT_INFO;



#ifdef __cplusplus
extern "C" {
#endif


OMX_ERRORTYPE Rockchip_OMX_Component_Register(ROCKCHIP_OMX_COMPONENT_REGLIST **compList, OMX_U32 *compNum);
OMX_ERRORTYPE Rockchip_OMX_Component_Unregister(ROCKCHIP_OMX_COMPONENT_REGLIST *componentList);
OMX_ERRORTYPE Rockchip_OMX_ComponentLoad(ROCKCHIP_OMX_COMPONENT *rockchip_component);
OMX_ERRORTYPE Rockchip_OMX_ComponentUnload(ROCKCHIP_OMX_COMPONENT *rockchip_component);


#ifdef __cplusplus
}
#endif

#endif
