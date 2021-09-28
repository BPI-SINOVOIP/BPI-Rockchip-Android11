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
 * @file        Rockchip_OSAL_Thread.c
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */
#undef ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_osal_thread"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_Thread.h"
#include "Rockchip_OSAL_Log.h"


typedef struct _ROCKCHIP_THREAD_HANDLE_TYPE {
    pthread_t          pthread;
    pthread_attr_t     attr;
    struct sched_param schedparam;
    int                stack_size;
} ROCKCHIP_THREAD_HANDLE_TYPE;


OMX_ERRORTYPE Rockchip_OSAL_ThreadCreate(
    OMX_HANDLETYPE *threadHandle,
    OMX_PTR function_name,
    OMX_PTR argument,
    OMX_PTR thread_name)
{
    FunctionIn();

    int result = 0;
    int detach_ret = 0;
    ROCKCHIP_THREAD_HANDLE_TYPE *thread;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    thread = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_THREAD_HANDLE_TYPE));
    Rockchip_OSAL_Memset(thread, 0, sizeof(ROCKCHIP_THREAD_HANDLE_TYPE));

    pthread_attr_init(&thread->attr);
    if (thread->stack_size != 0)
        pthread_attr_setstacksize(&thread->attr, thread->stack_size);

    /* set priority */
    if (thread->schedparam.sched_priority != 0)
        pthread_attr_setschedparam(&thread->attr, &thread->schedparam);

    detach_ret = pthread_attr_setdetachstate(&thread->attr, PTHREAD_CREATE_JOINABLE);
    if (detach_ret != 0) {
        Rockchip_OSAL_Free(thread);
        *threadHandle = NULL;
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    result = pthread_create(&thread->pthread, &thread->attr, function_name, (void *)argument);
    /* pthread_setschedparam(thread->pthread, SCHED_RR, &thread->schedparam); */
    pthread_setname_np(thread->pthread, thread_name);

    switch (result) {
    case 0:
        *threadHandle = (OMX_HANDLETYPE)thread;
        ret = OMX_ErrorNone;
        break;
    case EAGAIN:
        Rockchip_OSAL_Free(thread);
        *threadHandle = NULL;
        ret = OMX_ErrorInsufficientResources;
        break;
    default:
        Rockchip_OSAL_Free(thread);
        *threadHandle = NULL;
        ret = OMX_ErrorUndefined;
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OSAL_ThreadTerminate(OMX_HANDLETYPE threadHandle)
{
    FunctionIn();

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    ROCKCHIP_THREAD_HANDLE_TYPE *thread = (ROCKCHIP_THREAD_HANDLE_TYPE *)threadHandle;

    if (!thread) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pthread_join(thread->pthread, NULL) != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    Rockchip_OSAL_Free(thread);
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Rockchip_OSAL_ThreadCancel(OMX_HANDLETYPE threadHandle)
{
    ROCKCHIP_THREAD_HANDLE_TYPE *thread = (ROCKCHIP_THREAD_HANDLE_TYPE *)threadHandle;

    if (!thread)
        return OMX_ErrorBadParameter;

    /* thread_cancel(thread->pthread); */
    pthread_exit(&thread->pthread);
    pthread_join(thread->pthread, NULL);

    Rockchip_OSAL_Free(thread);
    return OMX_ErrorNone;
}

void Rockchip_OSAL_ThreadExit(void *value_ptr)
{
    pthread_exit(value_ptr);
    return;
}

void Rockchip_OSAL_SleepMillisec(OMX_U32 ms)
{
    usleep(ms * 1000);
    return;
}
