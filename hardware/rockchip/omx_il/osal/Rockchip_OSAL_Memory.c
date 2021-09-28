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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Rockchip_OSAL_Memory.h"

#define ROCKCHIP_LOG_OFF
#include "Rockchip_OSAL_Log.h"


static int mem_cnt = 0;

OMX_PTR Rockchip_OSAL_Malloc_With_Caller(
    OMX_U32 size,
    const char *tag,
    const char *caller,
    const OMX_U32 line)
{
    mem_cnt++;
    omx_dbg_f(OMX_DBG_MALLOC, "tag: %s, caller: %s(%d), malloc count: %d",
              tag, caller, line, mem_cnt);

    return (OMX_PTR)malloc(size);
}

void Rockchip_OSAL_Free_With_Caller(
    OMX_PTR addr,
    const char *tag,
    const char *caller,
    const OMX_U32 line)
{
    if (addr) {
        mem_cnt--;
        omx_dbg_f(OMX_DBG_MALLOC, "tag: %s, caller: %s(%d), free count: %d",
                  tag, caller, line, mem_cnt);
        free(addr);
    }

    return;
}

OMX_PTR Rockchip_OSAL_Memset(OMX_PTR dest, OMX_S32 c, OMX_S32 n)
{
    return memset(dest, c, n);
}

OMX_PTR Rockchip_OSAL_Memcpy(OMX_PTR dest, OMX_PTR src, OMX_S32 n)
{
    return memcpy(dest, src, n);
}

OMX_PTR Rockchip_OSAL_Memmove(OMX_PTR dest, OMX_PTR src, OMX_S32 n)
{
    return memmove(dest, src, n);
}
