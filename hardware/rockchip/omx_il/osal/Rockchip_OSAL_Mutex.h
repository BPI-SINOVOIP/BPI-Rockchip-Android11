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
 * @file        Rockchip_OSAL_Mutex.h
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */


#ifndef Rockchip_OSAL_MUTEX
#define Rockchip_OSAL_MUTEX

#include "OMX_Types.h"
#include "OMX_Core.h"


#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE Rockchip_OSAL_MutexCreate(OMX_HANDLETYPE *mutexHandle);
OMX_ERRORTYPE Rockchip_OSAL_MutexTerminate(OMX_HANDLETYPE mutexHandle);
OMX_ERRORTYPE Rockchip_OSAL_MutexLock(OMX_HANDLETYPE mutexHandle);
OMX_ERRORTYPE Rockchip_OSAL_MutexUnlock(OMX_HANDLETYPE mutexHandle);

#ifdef __cplusplus
}
#endif

#endif
