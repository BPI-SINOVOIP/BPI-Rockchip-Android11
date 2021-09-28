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
 * @file        Rockchip_OSAL_Memory.h
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */


#ifndef Rockchip_OSAL_MEMORY
#define Rockchip_OSAL_MEMORY

#include "OMX_Types.h"
#include "Rockchip_OSAL_Log.h"

typedef enum _MEMORY_TYPE {
    NORMAL_MEMORY = 0x00,
    SECURE_MEMORY = 0x01,
    SYSTEM_MEMORY = 0x02
} MEMORY_TYPE;

#define Rockchip_OSAL_Malloc(size) \
        Rockchip_OSAL_Malloc_With_Caller(size, ROCKCHIP_LOG_TAG, __FUNCTION__, __LINE__)

#define Rockchip_OSAL_Free(addr) \
            Rockchip_OSAL_Free_With_Caller(addr, ROCKCHIP_LOG_TAG, __FUNCTION__, __LINE__)

#ifdef __cplusplus
extern "C" {
#endif

OMX_PTR Rockchip_OSAL_Malloc_With_Caller(
    OMX_U32 size,
    const char *tag,
    const char *caller,
    const OMX_U32 line);
void    Rockchip_OSAL_Free_With_Caller(
    OMX_PTR addr,
    const char *tag,
    const char *caller,
    const OMX_U32 line);
OMX_PTR Rockchip_OSAL_Memset(OMX_PTR dest, OMX_S32 c, OMX_S32 n);
OMX_PTR Rockchip_OSAL_Memcpy(OMX_PTR dest, OMX_PTR src, OMX_S32 n);
OMX_PTR Rockchip_OSAL_Memmove(OMX_PTR dest, OMX_PTR src, OMX_S32 n);

#ifdef __cplusplus
}
#endif

#endif

