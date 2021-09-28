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
 * @file        Rockchip_OSAL_Log.h
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */


#ifndef ROCKCHIP_OSAL_LOG
#define ROCKCHIP_OSAL_LOG

#include "OMX_Core.h"
#include "OMX_Types.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef ROCKCHIP_LOG_TAG
#define ROCKCHIP_LOG_TAG    "omx_log"
#endif

typedef enum _LOG_LEVEL
{
    ROCKCHIP_LOG_TRACE,
    ROCKCHIP_LOG_INFO,
    ROCKCHIP_LOG_WARNING,
    ROCKCHIP_LOG_ERROR,
    ROCKCHIP_LOG_DEBUG
} ROCKCHIP_LOG_LEVEL;

/*
 * omx_debug bit definition
 */
#define OMX_DBG_UNKNOWN                 0x00000000
#define OMX_DBG_FUNCTION                0x80000000
#define OMX_DBG_MALLOC                  0x40000000
#define OMX_DBG_CAPACITYS               0x00000001

void _Rockchip_OSAL_Log(ROCKCHIP_LOG_LEVEL logLevel, OMX_U32 flag, const char *tag, const char *msg, ...);

#define omx_info(format, ...)        _Rockchip_OSAL_Log(ROCKCHIP_LOG_INFO, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, format "\n", ##__VA_ARGS__)
#define omx_trace(format, ...)       _Rockchip_OSAL_Log(ROCKCHIP_LOG_TRACE, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, format "\n", ##__VA_ARGS__)
#define omx_err(format, ...)         _Rockchip_OSAL_Log(ROCKCHIP_LOG_ERROR, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, format "\n", ##__VA_ARGS__)
#define omx_warn(format, ...)        _Rockchip_OSAL_Log(ROCKCHIP_LOG_WARNING, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, format "\n", ##__VA_ARGS__)

#define omx_info_f(format, ...)        _Rockchip_OSAL_Log(ROCKCHIP_LOG_INFO, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, "%s(%d): " format "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)
#define omx_trace_f(format, ...)       _Rockchip_OSAL_Log(ROCKCHIP_LOG_TRACE, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, "%s(%d): " format "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)
#define omx_err_f(format, ...)         _Rockchip_OSAL_Log(ROCKCHIP_LOG_ERROR, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, "%s(%d): " format "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)
#define omx_warn_f(format, ...)        _Rockchip_OSAL_Log(ROCKCHIP_LOG_WARNING, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, "%s(%d): " format "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)

#define _omx_dbg(fmt, ...)     _Rockchip_OSAL_Log(ROCKCHIP_LOG_INFO, OMX_DBG_UNKNOWN, ROCKCHIP_LOG_TAG, "%s(%d): " fmt "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)

#define omx_dbg_f(flags, fmt, ...) _Rockchip_OSAL_Log(ROCKCHIP_LOG_DEBUG, flags, ROCKCHIP_LOG_TAG, "%s(%d): " fmt "\n",__FUNCTION__, __LINE__, ##__VA_ARGS__)

#define omx_dbg(debug, flag, fmt, ...) \
            do { \
               if (debug & flag) \
                   _omx_dbg(fmt, ## __VA_ARGS__); \
            } while (0)

#define FunctionIn() omx_dbg_f(OMX_DBG_FUNCTION, "IN")

#define FunctionOut() omx_dbg_f(OMX_DBG_FUNCTION, "OUT")



#ifdef __cplusplus
}
#endif

#endif
