/*
 * Copyright (C) 2012-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "LogHelper"
#define LOG_TAG_CCA "CCA"

#include <stdint.h>
#include <limits.h> // INT_MAX, INT_MIN
#include <stdlib.h> // atoi.h
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "LogHelper.h"
#include "LogHelperAndroid.h"

NAMESPACE_DECLARATION {
int32_t gLogLevel = 0;
int32_t gPerfLevel = 0;
int32_t gDumpType = 0;
// Skip frame number before dump. Default: 0, not skip
int32_t gDumpSkipNum = 0;
// dump 1 frame every gDumpInterval frames. Default: 1, there is no skip frames between frames.
int32_t gDumpInterval = 1;
// Dump frame count. Default: -1, negative value means infinity
int32_t gDumpCount = -1;
// Path for dump data. Default: CAMERA_OPERATION_FOLDER
char    gDumpPath[PATH_MAX] = CAMERA_OPERATION_FOLDER;

namespace LogHelper {

void setDebugLevel(void)
{
    LOGI("%s:%d: enter", __func__, __LINE__);
    // The camera HAL adapter handled the logging initialization already.

    if (__getEnviromentValue(ENV_CAMERA_HAL_DEBUG, &gLogLevel)) {
        LOGD("Debug level is 0x%x", gLogLevel);
    }

    //Performance property
    if (__getEnviromentValue(ENV_CAMERA_HAL_PERF, &gPerfLevel)) {
    }
    // dump property, it's used to dump images or some parameters to a file.
    if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP, &gDumpType)) {
        LOGD("Dump type is 0x%x", gDumpType);

        if (gDumpType) {
            // Read options for dump.
            // skip number
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_SKIP_NUM, &gDumpSkipNum)) {
                LOGD("Skip %d frames before dump", gDumpSkipNum);
            }
            // set dump interval
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_INTERVAL, &gDumpInterval)) {
                LOGD("dump 1 frame every %d frames", gDumpInterval);
            }
            // total frame number for dump
            if (__getEnviromentValue(ENV_CAMERA_HAL_DUMP_COUNT, &gDumpCount)) {
                LOGD("Total %d frames will be dumped", gDumpCount);
            }
            // dump path
            __getEnviromentValue(ENV_CAMERA_HAL_DUMP_PATH, gDumpPath, sizeof(gDumpPath));
            if (access(gDumpPath, F_OK) < 0) {
                if (mkdir(gDumpPath, 0755) < 0) {
                    LOGE("mkdir failed, dir=%s, errmsg: %s\n", gDumpPath, strerror(errno));
                }
            }
            LOGI("Dump path: %s", gDumpPath);
        }
    }
}

bool isDumpTypeEnable(int dumpType)
{
    return gDumpType & dumpType;
}

bool isDebugTypeEnable(int debugType)
{
    return gLogLevel & debugType;
}

bool isPerfDumpTypeEnable(int dumpType)
{
    return gPerfLevel & dumpType;
}

bool __getEnviromentValue(const char* variable, int* value)
{
    char property_value[PROPERTY_VALUE_MAX] = {0};
    property_get(variable, property_value, "0");
    *value = (int)atoi(property_value);

    return true;
}

bool __getEnviromentValue(const char* variable, char *value, size_t buf_size)
{
    char property_value[PROPERTY_VALUE_MAX] = {0};
    property_get(variable, property_value, "0");
    if(strcmp(property_value, "0") == 0) {
        LOGI("%s:%d: Not find the property: %s", __func__, __LINE__, variable);
        return false;
    }
    MEMCPY_S(value, buf_size, property_value, strlen(property_value) + 1);
    return true;
}

} // namespace LogHelper
} NAMESPACE_DECLARATION_END
