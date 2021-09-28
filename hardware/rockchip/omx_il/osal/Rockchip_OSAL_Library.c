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
 * @file        Rockchip_OSAL_Library.c
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "Rockchip_OSAL_Library.h"


void *Rockchip_OSAL_dlopen(const char *filename, int flag)
{
    return dlopen(filename, flag);
}

void *Rockchip_OSAL_dlsym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

int Rockchip_OSAL_dlclose(void *handle)
{
    return dlclose(handle);
}

const char *Rockchip_OSAL_dlerror(void)
{
    return dlerror();
}
