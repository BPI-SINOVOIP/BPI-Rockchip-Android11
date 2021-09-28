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
 * @file        Rockchip_OSAL_Mutex.c
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_Mutex.h"

OMX_ERRORTYPE Rockchip_OSAL_MutexCreate(OMX_HANDLETYPE *mutexHandle)
{
    pthread_mutex_t *mutex;

    mutex = (pthread_mutex_t *)Rockchip_OSAL_Malloc(sizeof(pthread_mutex_t));
    if (!mutex)
        return OMX_ErrorInsufficientResources;

    if (pthread_mutex_init(mutex, NULL) != 0) {
        Rockchip_OSAL_Free(mutex);
        return OMX_ErrorUndefined;
    }

    *mutexHandle = (OMX_HANDLETYPE)mutex;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Rockchip_OSAL_MutexTerminate(OMX_HANDLETYPE mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;

    if (mutex == NULL)
        return OMX_ErrorBadParameter;

    if (pthread_mutex_destroy(mutex) != 0)
        return OMX_ErrorUndefined;

    Rockchip_OSAL_Free(mutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Rockchip_OSAL_MutexLock(OMX_HANDLETYPE mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;

    if (mutex == NULL)
        return OMX_ErrorBadParameter;

    if (pthread_mutex_lock(mutex) != 0)
        return OMX_ErrorUndefined;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Rockchip_OSAL_MutexUnlock(OMX_HANDLETYPE mutexHandle)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutexHandle;

    if (mutex == NULL)
        return OMX_ErrorBadParameter;

    if (pthread_mutex_unlock(mutex) != 0)
        return OMX_ErrorUndefined;

    return OMX_ErrorNone;
}
