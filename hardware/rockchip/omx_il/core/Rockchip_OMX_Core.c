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
 * @file       Rockchip_OMX_Core.c
 * @brief      Rockchip OpenMAX IL Core
 * @author     csy (csy@rock-chips.com)
 *
 * @version    1.0.0
 * @history
 *    2013.11.26 : Create
 */
#undef  ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_core"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Rockchip_OMX_Core.h"
#include "Rockchip_OMX_Component_Register.h"
#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_Mutex.h"
#include "Rockchip_OSAL_ETC.h"
#include "Rockchip_OMX_Resourcemanager.h"
#include "git_info.h"
#include <pthread.h>

#include "Rockchip_OSAL_Log.h"


static int gInitialized = 0;
static OMX_U32 gComponentNum = 0;
static OMX_U32 gCount = 0;
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
static ROCKCHIP_OMX_COMPONENT_REGLIST *gComponentList = NULL;
static ROCKCHIP_OMX_COMPONENT *gLoadComponentList = NULL;
static OMX_HANDLETYPE ghLoadComponentListMutex = NULL;


OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_Init(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();
    Rockchip_OSAL_MutexLock(&gMutex);
    gCount++;
    if (gInitialized == 0) {
        if (Rockchip_OMX_Component_Register(&gComponentList, &gComponentNum)) {
            ret = OMX_ErrorInsufficientResources;
            omx_err("Rockchip_OMX_Init : %s", "OMX_ErrorInsufficientResources");
            goto EXIT;
        }

        ret = Rockchip_OMX_ResourceManager_Init();
        if (OMX_ErrorNone != ret) {
            omx_err("Rockchip_OMX_Init : Rockchip_OMX_ResourceManager_Init failed");
            goto EXIT;
        }

        ret = Rockchip_OSAL_MutexCreate(&ghLoadComponentListMutex);
        if (OMX_ErrorNone != ret) {
            omx_err("Rockchip_OMX_Init : Rockchip_OSAL_MutexCreate(&ghLoadComponentListMutex) failed");
            goto EXIT;
        }

        gInitialized = 1;
        omx_trace("1Rockchip_OMX_Init : %s", "OMX_ErrorNone");
    }

EXIT:

    Rockchip_OSAL_MutexUnlock(&gMutex);
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_DeInit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();
    Rockchip_OSAL_MutexLock(&gMutex);
    gCount--;
    if (gCount == 0) {
        Rockchip_OSAL_MutexTerminate(ghLoadComponentListMutex);
        ghLoadComponentListMutex = NULL;

        Rockchip_OMX_ResourceManager_Deinit();

        if (OMX_ErrorNone != Rockchip_OMX_Component_Unregister(gComponentList)) {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
        gComponentList = NULL;
        gComponentNum = 0;
        gInitialized = 0;
    }
EXIT:
    Rockchip_OSAL_MutexUnlock(&gMutex);
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    if (nIndex >= gComponentNum) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }

    snprintf(cComponentName, nNameLength, "%s", gComponentList[nIndex].component.componentName);
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_GetHandle(
    OMX_OUT OMX_HANDLETYPE *pHandle,
    OMX_IN  OMX_STRING cComponentName,
    OMX_IN  OMX_PTR pAppData,
    OMX_IN  OMX_CALLBACKTYPE *pCallBacks)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    ROCKCHIP_OMX_COMPONENT *loadComponent;
    ROCKCHIP_OMX_COMPONENT *currentComponent;
    unsigned int i = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    if ((pHandle == NULL) || (cComponentName == NULL) || (pCallBacks == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    omx_trace("ComponentName : %s", cComponentName);

    for (i = 0; i < gComponentNum; i++) {
        if (Rockchip_OSAL_Strcmp(cComponentName, gComponentList[i].component.componentName) == 0) {
            loadComponent = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_OMX_COMPONENT));
            Rockchip_OSAL_Memset(loadComponent, 0, sizeof(ROCKCHIP_OMX_COMPONENT));

            Rockchip_OSAL_Strcpy(loadComponent->libName, gComponentList[i].libName);
            Rockchip_OSAL_Strcpy(loadComponent->componentName, gComponentList[i].component.componentName);
            ret = Rockchip_OMX_ComponentLoad(loadComponent);
            if (ret != OMX_ErrorNone) {
                Rockchip_OSAL_Free(loadComponent);
                omx_err("OMX_Error, Line:%d", __LINE__);
                goto EXIT;
            }

            ret = loadComponent->pOMXComponent->SetCallbacks(loadComponent->pOMXComponent, pCallBacks, pAppData);
            if (ret != OMX_ErrorNone) {
                Rockchip_OMX_ComponentUnload(loadComponent);
                Rockchip_OSAL_Free(loadComponent);
                omx_err("OMX_Error 0x%x, Line:%d", ret, __LINE__);
                goto EXIT;
            }

            ret = Rockchip_OMX_Check_Resource(loadComponent->pOMXComponent);
            if (ret != OMX_ErrorNone) {
                Rockchip_OMX_ComponentUnload(loadComponent);
                Rockchip_OSAL_Free(loadComponent);
                omx_err("OMX_Error 0x%x, Line:%d", ret, __LINE__);

                goto EXIT;
            }
            Rockchip_OSAL_MutexLock(ghLoadComponentListMutex);
            if (gLoadComponentList == NULL) {
                gLoadComponentList = loadComponent;
            } else {
                currentComponent = gLoadComponentList;
                while (currentComponent->nextOMXComp != NULL) {
                    currentComponent = currentComponent->nextOMXComp;
                }
                currentComponent->nextOMXComp = loadComponent;
            }
            Rockchip_OSAL_MutexUnlock(ghLoadComponentListMutex);

            *pHandle = loadComponent->pOMXComponent;
            ret = OMX_ErrorNone;
            omx_trace("Rockchip_OMX_GetHandle : %s", "OMX_ErrorNone");
            goto EXIT;
        }
    }

    ret = OMX_ErrorComponentNotFound;

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_FreeHandle(OMX_IN OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE         ret = OMX_ErrorNone;
    ROCKCHIP_OMX_COMPONENT *currentComponent = NULL;
    ROCKCHIP_OMX_COMPONENT *deleteComponent = NULL;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    if (!hComponent) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Rockchip_OSAL_MutexLock(ghLoadComponentListMutex);
    currentComponent = gLoadComponentList;
    if (gLoadComponentList->pOMXComponent == hComponent) {
        deleteComponent = gLoadComponentList;
        gLoadComponentList = gLoadComponentList->nextOMXComp;
    } else {
        while ((currentComponent != NULL) && (((ROCKCHIP_OMX_COMPONENT *)(currentComponent->nextOMXComp))->pOMXComponent != hComponent))
            currentComponent = currentComponent->nextOMXComp;

        if (((ROCKCHIP_OMX_COMPONENT *)(currentComponent->nextOMXComp))->pOMXComponent == hComponent) {
            deleteComponent = currentComponent->nextOMXComp;
            currentComponent->nextOMXComp = deleteComponent->nextOMXComp;
        } else if (currentComponent == NULL) {
            ret = OMX_ErrorComponentNotFound;
            Rockchip_OSAL_MutexUnlock(ghLoadComponentListMutex);
            goto EXIT;
        }
    }
    Rockchip_OSAL_MutexUnlock(ghLoadComponentListMutex);

    Rockchip_OMX_ComponentUnload(deleteComponent);
    Rockchip_OSAL_Free(deleteComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE OMX_APIENTRY RKOMX_SetupTunnel(
    OMX_IN OMX_HANDLETYPE hOutput,
    OMX_IN OMX_U32 nPortOutput,
    OMX_IN OMX_HANDLETYPE hInput,
    OMX_IN OMX_U32 nPortInput)
{
    OMX_ERRORTYPE ret = OMX_ErrorNotImplemented;
    (void)hOutput;
    (void)nPortOutput;
    (void)hInput;
    (void)nPortInput;
    goto EXIT;
EXIT:
    return ret;
}

OMX_API OMX_ERRORTYPE RKOMX_GetContentPipe(
    OMX_OUT OMX_HANDLETYPE *hPipe,
    OMX_IN  OMX_STRING szURI)
{
    OMX_ERRORTYPE ret = OMX_ErrorNotImplemented;
    (void)hPipe;
    (void)szURI;
    goto EXIT;
EXIT:
    return ret;
}

OMX_API OMX_ERRORTYPE RKOMX_GetComponentsOfRole (
    OMX_IN    OMX_STRING role,
    OMX_INOUT OMX_U32 *pNumComps,
    OMX_INOUT OMX_U8  **compNames)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int           max_role_num = 0;
    int i = 0, j = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    *pNumComps = 0;

    for (i = 0; i < MAX_OMX_COMPONENT_NUM; i++) {
        max_role_num = gComponentList[i].component.totalRoleNum;

        for (j = 0; j < max_role_num; j++) {
            if (Rockchip_OSAL_Strcmp(gComponentList[i].component.roles[j], role) == 0) {
                if (compNames != NULL) {
                    Rockchip_OSAL_Strcpy((OMX_STRING)compNames[*pNumComps], gComponentList[i].component.componentName);
                }
                *pNumComps = (*pNumComps + 1);
            }
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_API OMX_ERRORTYPE RKOMX_GetRolesOfComponent (
    OMX_IN    OMX_STRING compName,
    OMX_INOUT OMX_U32 *pNumRoles,
    OMX_OUT   OMX_U8 **roles)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BOOL      detectComp = OMX_FALSE;
    int           compNum = 0, totalRoleNum = 0;
    int i = 0;

    FunctionIn();

    if (gInitialized != 1) {
        ret = OMX_ErrorNotReady;
        goto EXIT;
    }

    for (i = 0; i < MAX_OMX_COMPONENT_NUM; i++) {
        if (gComponentList != NULL) {
            if (Rockchip_OSAL_Strcmp(gComponentList[i].component.componentName, compName) == 0) {
                *pNumRoles = totalRoleNum = gComponentList[i].component.totalRoleNum;
                compNum = i;
                detectComp = OMX_TRUE;
                break;
            }
        } else {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }
    }

    if (detectComp == OMX_FALSE) {
        *pNumRoles = 0;
        ret = OMX_ErrorComponentNotFound;
        goto EXIT;
    }

    if (roles != NULL) {
        for (i = 0; i < totalRoleNum; i++) {
            Rockchip_OSAL_Strcpy(roles[i], gComponentList[compNum].component.roles[i]);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}
