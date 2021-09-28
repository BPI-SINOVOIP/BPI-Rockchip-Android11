
/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RTChips"

#ifdef DEBUG_FLAG
#undef DEBUG_FLAG
#endif
#define DEBUG_FLAG 0x0

#include "RTChips.h"      // NOLINT

#include <stdio.h>        // NOLINT
#include <unistd.h>       // NOLINT
#include <utils/Log.h>    // NOLINT

#include "rt_common.h"    // NOLINT
#include "rt_type.h"      // NOLINT
#include "string.h"       // NOLINT
#include "fcntl.h"        // NOLINT

#define MAX_SOC_NAME_LENGTH 1024

RKChipInfo* match(char *buf) {
    for (INT32 i = 0; i < RT_ARRAY_ELEMS(ChipList); i++) {
        if (strstr(buf, (char*)ChipList[i].name)) {   // NOLINT
            return (RKChipInfo*)&ChipList[i];         // NOLINT
        }
    }

    return RT_NULL;                                   // NOLINT
}

RKChipInfo* readDeviceTree() {
    INT32 fd = open("/proc/device-tree/compatible", O_RDONLY);
    if (fd < 0) {
        ALOGE("open /proc/device-tree/compatible error");
        return RT_NULL;
    }

    char name[MAX_SOC_NAME_LENGTH] = {0};
    char* ptr = RT_NULL;
    RKChipInfo* infor = RT_NULL;

    INT32 length = read(fd, name, MAX_SOC_NAME_LENGTH - 1);
    if (length > 0) {
        /* replacing the termination character to space */
        for (ptr = name;; ptr = name) {
            ptr += strnlen(name, MAX_SOC_NAME_LENGTH);
            *ptr = ' ';
            if (ptr >= name + length - 1)
                break;
        }

        infor = match(name);
        if (infor == RT_NULL) {
            ALOGD("devices tree can not found match chip name: %s", name);
        }
    }

    close(fd);

    return infor;
}

RKChipInfo* readCpuInforNode() {
    INT32 fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd < 0) {
        ALOGE("open /proc/cpuinfo error");
        return RT_NULL;
    }

    char buffer[MAX_SOC_NAME_LENGTH] = {0};
    char name[128];
    char* ptr = RT_NULL;
    RKChipInfo* infor = RT_NULL;

    INT32 length = read(fd, buffer, MAX_SOC_NAME_LENGTH - 1);
    if (length > 0) {
        ptr = strstr(buffer, "Hardware");
        if (ptr != NULL) {
            sscanf(ptr, "Hardware\t: Rockchip %s", name);

            infor = match(name);
            if (infor == RT_NULL) {
                ALOGD("cpu node can not found match chip name: %s", name);
            }
        }
    }

    close(fd);

    return infor;
}

RKChipInfo* readEfuse() {
    const char* NODE = "/sys/bus/nvmem/devices/rockchip-efuse0/nvmem";
    INT32 fd = open(NODE, O_RDONLY, 0);
    if (fd < 0) {
        ALOGE("open %s error", NODE);
        return RT_NULL;
    }

    const INT32 length = 128;
    char buffer[length] = {0};
    INT32 size = read(fd, buffer, length);
    if (size > 0) {
        ALOGD("%s: %s", __FUNCTION__, buffer);
    }

    close(fd);

    // FIXME: efuse is error in my test
    return RT_NULL;
}

RKChipInfo* getChipName() {
    RKChipInfo* infor = readEfuse();
    if (infor != RT_NULL) {
        return infor;
    }

    infor = readDeviceTree();
    if (infor != RT_NULL) {
        return infor;
    }

    infor = readCpuInforNode();
    if (infor != RT_NULL) {
        return infor;
    }

    return RT_NULL;
}

