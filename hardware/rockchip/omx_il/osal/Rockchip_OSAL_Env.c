/*
 *
 * Copyright 2018 Rockchip Electronics Co. LTD
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/system_properties.h>

#include "Rockchip_OSAL_Env.h"
#include "Rockchip_OSAL_Log.h"

/*
 * NOTE: __system_property_set only available after android-21
 * So the library should compiled on latest ndk
 */
OMX_ERRORTYPE Rockchip_OSAL_GetEnvU32(const char *name, OMX_U32 *value, OMX_U32 default_value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    char prop[PROP_VALUE_MAX + 1];
    int len = __system_property_get(name, prop);
    if (len > 0) {
        char *endptr;
        int base = (prop[0] == '0' && prop[1] == 'x') ? (16) : (10);
        errno = 0;
        *value = strtoul(prop, &endptr, base);
        if (errno || (prop == endptr)) {
            errno = 0;
            *value = default_value;
        }
    } else {
        *value = default_value;
    }

    return ret;
}

OMX_ERRORTYPE Rockchip_OSAL_GetEnvStr(const char *name, char *value, char *default_value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if (value != NULL) {
        int len = __system_property_get(name, value);
        if (len <= 0 && default_value != NULL) {
            strcpy(value, default_value);
        }
    } else {
        omx_err("get env string failed, value is null");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_OSAL_SetEnvU32(const char *name, OMX_U32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    char buf[PROP_VALUE_MAX + 1];
    snprintf(buf, sizeof(buf), "%lu", value);
    int len = __system_property_set(name, buf);
    if (len <= 0) {
        omx_err("property set failed!");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
EXIT:
    return ret;
}

OMX_ERRORTYPE Rockchip_OSAL_SetEnvStr(const char *name, char *value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int len = __system_property_set(name, value);
    if (len <= 0) {
        omx_err("property set failed!");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    return ret;
}

