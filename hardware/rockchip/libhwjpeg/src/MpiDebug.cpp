/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

#include "MpiDebug.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/system_properties.h>

uint32_t get_env_u32(const char* name, uint32_t *value, uint32_t default_value)
{
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

    return 0;
}

