/*
 *
 * Copyright 2013 rockchip Electronics Co. LTD
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
 * @file    library_register.c
 * @brief
 * @author    Csy (csy@rock-chips.com)
 * @version    1.0.0
 * @history
 *   2013.11.27 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "Rockchip_OSAL_Memory.h"
#include "Rockchip_OSAL_ETC.h"
#include "library_register.h"
#include "Rockchip_OSAL_Log.h"
#include "RkOMX_Core.h"


OSCL_EXPORT_REF int Rockchip_OMX_COMPONENT_Library_Register(RockchipRegisterComponentType **rockchipComponents)
{
    FunctionIn();

    if (rockchipComponents == NULL)
        goto EXIT;

    unsigned int i = 0;
    for (i = 0; i < SIZE_OF_ENC_CORE; i++) {
        Rockchip_OSAL_Strcpy(rockchipComponents[i]->componentName, (OMX_PTR)enc_core[i].compName);
        Rockchip_OSAL_Strcpy(rockchipComponents[i]->roles[0], (OMX_PTR)enc_core[i].roles);
        rockchipComponents[i]->totalRoleNum = MAX_COMPONENT_ROLE_NUM;
    }

EXIT:
    FunctionOut();

    return SIZE_OF_ENC_CORE;
}
