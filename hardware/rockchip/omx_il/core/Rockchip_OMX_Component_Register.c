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
 * @file       Rockchip_OMX_Component_Register.c
 * @brief      Rockchip OpenMAX IL Component Register
 * @author     csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_comp_regs"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>

#include "OMX_Component.h"
#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OSAL_Library.h"
#include "Rockchip_OMX_Component_Register.h"
#include "Rockchip_OMX_Macros.h"
#include "git_info.h"

#include "Rockchip_OSAL_Log.h"
static const ROCKCHIP_COMPONENT_INFO kCompInfo[] = {
    { "rk.omx_dec", "libomxvpu_dec.so" },
    { "rk.omx_enc", "libomxvpu_enc.so" },
};

OMX_ERRORTYPE Rockchip_OMX_Component_Register(ROCKCHIP_OMX_COMPONENT_REGLIST **compList, OMX_U32 *compNum)
{
    OMX_ERRORTYPE  ret = OMX_ErrorNone;
    int            componentNum = 0, totalCompNum = 0;
    OMX_U32        i = 0;
    const char    *errorMsg;
    omx_err_f("in");

    int (*Rockchip_OMX_COMPONENT_Library_Register)(RockchipRegisterComponentType **rockchipComponents);
    RockchipRegisterComponentType **rockchipComponentsTemp;
    ROCKCHIP_OMX_COMPONENT_REGLIST *componentList;

    FunctionIn();

    componentList = (ROCKCHIP_OMX_COMPONENT_REGLIST *)Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_COMPONENT_REGLIST) * MAX_OMX_COMPONENT_NUM);
    Rockchip_OSAL_Memset(componentList, 0, sizeof(ROCKCHIP_OMX_COMPONENT_REGLIST) * MAX_OMX_COMPONENT_NUM);

    for (i = 0; i < ARRAY_SIZE(kCompInfo); i++) {
        ROCKCHIP_COMPONENT_INFO com_inf = kCompInfo[i];
        OMX_PTR soHandle = NULL;
        omx_err_f("in");
        if ((soHandle = Rockchip_OSAL_dlopen(com_inf.lib_name, RTLD_NOW)) != NULL) {
            omx_err_f("in so name: %s", com_inf.lib_name);
            Rockchip_OSAL_dlerror();    /* clear error*/
            if ((Rockchip_OMX_COMPONENT_Library_Register = Rockchip_OSAL_dlsym(soHandle, "Rockchip_OMX_COMPONENT_Library_Register")) != NULL) {
                int i = 0;
                unsigned int j = 0;
                componentNum = (*Rockchip_OMX_COMPONENT_Library_Register)(NULL);
                omx_err_f("in num: %d", componentNum);
                rockchipComponentsTemp = (RockchipRegisterComponentType **)Rockchip_OSAL_Malloc(sizeof(RockchipRegisterComponentType*) * componentNum);
                for (i = 0; i < componentNum; i++) {
                    rockchipComponentsTemp[i] = Rockchip_OSAL_Malloc(sizeof(RockchipRegisterComponentType));
                    Rockchip_OSAL_Memset(rockchipComponentsTemp[i], 0, sizeof(RockchipRegisterComponentType));
                }
                (*Rockchip_OMX_COMPONENT_Library_Register)(rockchipComponentsTemp);

                for (i = 0; i < componentNum; i++) {
                    Rockchip_OSAL_Strcpy(componentList[totalCompNum].component.componentName, rockchipComponentsTemp[i]->componentName);
                    for (j = 0; j < rockchipComponentsTemp[i]->totalRoleNum; j++)
                        Rockchip_OSAL_Strcpy(componentList[totalCompNum].component.roles[j], rockchipComponentsTemp[i]->roles[j]);
                    componentList[totalCompNum].component.totalRoleNum = rockchipComponentsTemp[i]->totalRoleNum;

                    Rockchip_OSAL_Strcpy(componentList[totalCompNum].libName, com_inf.lib_name);

                    totalCompNum++;
                }
                for (i = 0; i < componentNum; i++) {
                    Rockchip_OSAL_Free(rockchipComponentsTemp[i]);
                }

                Rockchip_OSAL_Free(rockchipComponentsTemp);
            } else {
                if ((errorMsg = Rockchip_OSAL_dlerror()) != NULL)
                    omx_warn("dlsym failed: %s", errorMsg);
            }
            Rockchip_OSAL_dlclose(soHandle);
        }
        omx_err_f("Rockchip_OSAL_dlerror: %s", Rockchip_OSAL_dlerror());
    }
    *compList = componentList;
    *compNum = totalCompNum;

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_Component_Unregister(ROCKCHIP_OMX_COMPONENT_REGLIST *componentList)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    Rockchip_OSAL_Free(componentList);

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_ComponentAPICheck(OMX_COMPONENTTYPE *component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if ((NULL == component->GetComponentVersion)    ||
        (NULL == component->SendCommand)            ||
        (NULL == component->GetParameter)           ||
        (NULL == component->SetParameter)           ||
        (NULL == component->GetConfig)              ||
        (NULL == component->SetConfig)              ||
        (NULL == component->GetExtensionIndex)      ||
        (NULL == component->GetState)               ||
        (NULL == component->ComponentTunnelRequest) ||
        (NULL == component->UseBuffer)              ||
        (NULL == component->AllocateBuffer)         ||
        (NULL == component->FreeBuffer)             ||
        (NULL == component->EmptyThisBuffer)        ||
        (NULL == component->FillThisBuffer)         ||
        (NULL == component->SetCallbacks)           ||
        (NULL == component->ComponentDeInit)        ||
        (NULL == component->UseEGLImage)            ||
        (NULL == component->ComponentRoleEnum))
        ret = OMX_ErrorInvalidComponent;
    else
        ret = OMX_ErrorNone;

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_ComponentLoad(ROCKCHIP_OMX_COMPONENT *rockchip_component)
{
    OMX_ERRORTYPE      ret = OMX_ErrorNone;
    OMX_HANDLETYPE     libHandle;
    OMX_COMPONENTTYPE *pOMXComponent;

    FunctionIn();

    OMX_ERRORTYPE (*Rockchip_OMX_ComponentConstructor)(OMX_HANDLETYPE hComponent, OMX_STRING componentName);

    libHandle = Rockchip_OSAL_dlopen((OMX_STRING)rockchip_component->libName, RTLD_NOW);
    if (!libHandle) {
        ret = OMX_ErrorInvalidComponentName;
        omx_err("OMX_ErrorInvalidComponentName, Line:%d", __LINE__);
        goto EXIT;
    }

    Rockchip_OMX_ComponentConstructor = Rockchip_OSAL_dlsym(libHandle, "Rockchip_OMX_ComponentConstructor");
    if (!Rockchip_OMX_ComponentConstructor) {
        Rockchip_OSAL_dlclose(libHandle);
        ret = OMX_ErrorInvalidComponent;
        omx_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)Rockchip_OSAL_Malloc(sizeof(OMX_COMPONENTTYPE));
    INIT_SET_SIZE_VERSION(pOMXComponent, OMX_COMPONENTTYPE);
    ret = (*Rockchip_OMX_ComponentConstructor)((OMX_HANDLETYPE)pOMXComponent, (OMX_STRING)rockchip_component->componentName);
    if (ret != OMX_ErrorNone) {
        Rockchip_OSAL_Free(pOMXComponent);
        Rockchip_OSAL_dlclose(libHandle);
        ret = OMX_ErrorInvalidComponent;
        omx_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
        goto EXIT;
    } else {
        if (Rockchip_OMX_ComponentAPICheck(pOMXComponent) != OMX_ErrorNone) {
            if (NULL != pOMXComponent->ComponentDeInit)
                pOMXComponent->ComponentDeInit(pOMXComponent);
            Rockchip_OSAL_Free(pOMXComponent);
            Rockchip_OSAL_dlclose(libHandle);
            ret = OMX_ErrorInvalidComponent;
            omx_err("OMX_ErrorInvalidComponent, Line:%d", __LINE__);
            goto EXIT;
        }
        rockchip_component->libHandle = libHandle;
        rockchip_component->pOMXComponent = pOMXComponent;
        rockchip_component->rkversion = OMX_COMPILE_INFO;
        ret = OMX_ErrorNone;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OMX_ComponentUnload(ROCKCHIP_OMX_COMPONENT *rockchip_component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pOMXComponent = NULL;

    FunctionIn();

    if (!rockchip_component) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = rockchip_component->pOMXComponent;
    if (pOMXComponent != NULL) {
        pOMXComponent->ComponentDeInit(pOMXComponent);
        Rockchip_OSAL_Free(pOMXComponent);
        rockchip_component->pOMXComponent = NULL;
    }

    if (rockchip_component->libHandle != NULL) {
        Rockchip_OSAL_dlclose(rockchip_component->libHandle);
        rockchip_component->libHandle = NULL;
    }

EXIT:
    FunctionOut();

    return ret;
}

