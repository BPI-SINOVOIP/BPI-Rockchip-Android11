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

#ifndef _LOGHELPER_H_
#define _LOGHELPER_H_

#include <inttypes.h> // For PRId64 used to print int64_t for both 64bits and 32bits
#include <cutils/compiler.h>
#include <limits.h>
#include <stdarg.h>

#include <utils/Errors.h>
#include "CommonUtilMacros.h"
#include "LogHelperAndroid.h"
#include "EnumPrinthelper.h"
#include <cutils/properties.h>

NAMESPACE_DECLARATION {
/**
 * global log level
 * This global variable is set from system properties
 * It is used to control the level of verbosity of the traces in logcat
 * It is also used to store the status of certain RD features
 */
/* extern int32_t gLogLevel;    //< camera.hal.debug */
/* extern int32_t gLogCcaLevel; //< camera.hal.cca */
/* extern int32_t gPerfLevel;   //< camera.hal.perf */
/* extern int32_t gEnforceDvs;  //< camera.hal.dvs */

extern int32_t gDumpSkipNum;         //< camera.hal.dump.skip_num
extern int32_t gDumpInterval;         //< camera.hal.dump.interval
extern int32_t gDumpCount;           //< camera.hal.dump.count
extern char    gDumpPath[PATH_MAX];  //< camera.hal.dump.path

// RGBS is one of the analog component video standards.
// composite sync, where the horizontal and vertical signals are mixed together on a separate wire (the S in RGBS)
extern int32_t gRgbsGridDump;
extern int32_t gAfGridDump;


// All dump related definition
// camera.hal.dump
enum {
// Dump image related flags
    CAMERA_DUMP_PREVIEW =         1 << 0, // 1
    CAMERA_DUMP_VIDEO =           1 << 1, // 2
    CAMERA_DUMP_ZSL =             1 << 2, // 4
    CAMERA_DUMP_JPEG =            1 << 3, // 8
    CAMERA_DUMP_RAW =             1 << 4, // 16

// Dump metadata related flags
    CAMERA_DUMP_META =            1 << 5, // 32

// Dump parameter related flags
    CAMERA_DUMP_MEDIA_CTL =       1 << 6, // 64
// Dump data that pulled from videoNode and not processed by hal
    CAMERA_DUMP_ISP_PURE =        1 << 7, // 128
};

// camera.hal.perf
enum  {
    /* Emit well-formed performance traces */
    CAMERA_DEBUG_LOG_PERF_TRACES = 1,

    /* Print out detailed timing analysis */
    CAMERA_DEBUG_LOG_PERF_TRACES_BREAKDOWN = 2,

    /* Print out detailed timing analysis for IOCTL */
    CAMERA_DEBUG_LOG_PERF_IOCTL_BREAKDOWN = 1<<2,

    /* Print out detailed memory information analysis for IOCTL */
    CAMERA_DEBUG_LOG_PERF_MEMORY = 1<<3,

    /*set camera atrace level for pytimechart*/
    CAMERA_DEBUG_LOG_ATRACE_LEVEL = 1<<4,

    /*enable media topology dump*/
    CAMERA_DEBUG_LOG_MEDIA_TOPO_LEVEL = 1<<5,

    /*enable media controller info dump*/
    CAMERA_DEBUG_LOG_MEDIA_CONTROLLER_LEVEL = 1<<6
};

namespace LogHelper {

/**
 * Runtime selection of debugging level.
 */
void setDebugLevel(void);
bool isDebugLevelEnable(int level);
bool isDumpTypeEnable(int dumpType);
bool isDebugTypeEnable(int debugType);
bool isPerfDumpTypeEnable(int dumpType);

/**
 * Helper for enviroment variable access
 * (Impementation depends on OS)
 */
bool __getEnviromentValue(const char* variable, int* value);
bool __getEnviromentValue(const char* variable, char *value, size_t buf_size);

} // namespace LogHelper
} NAMESPACE_DECLARATION_END

#endif // _LOGHELPER_H_
