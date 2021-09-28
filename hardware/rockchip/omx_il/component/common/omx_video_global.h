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

#ifndef _OMX_VDEC_GLOBAL_H_
#define _OMX_VDEC_GLOBAL_H_

#include "OMX_Video.h"
#include "OMX_Types.h"

/*
 * this definition for debug
 */
#define VDEC_DBG_RECORD_MASK                0xff000000
#define VDEC_DBG_RECORD_IN                  0x01000000
#define VDEC_DBG_RECORD_OUT                 0x02000000

#define VIDEO_DBG_LOG_MASK                  0x0000ffff
#define VIDEO_DBG_LOG_PTS                   0x00000001
#define VIDEO_DBG_LOG_FPS                   0x00000002
#define VIDEO_DBG_LOG_BUFFER                0x00000004
#define VIDEO_DBG_LOG_DBG                   0x00000008
#define VIDEO_DBG_LOG_PORT                  0x00000010
#define VIDEO_DBG_LOG_BUFFER_POSITION       0x00000020

#define VDEC_DBG_VPU_MPP_FIRST              0x00000001
#define VDEC_DBG_VPU_VPUAPI_FIRST           0x00000002

extern OMX_U32 omx_vdec_debug;
extern OMX_U32 omx_venc_debug;

#define VDEC_DBG_LOG(fmt, ...) \
    do { \
        if (omx_vdec_debug & VIDEO_DBG_LOG_DBG) \
            { omx_info(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VDEC_DBG_LOG_F(fmt, ...) \
            do { \
                if (omx_vdec_debug & VIDEO_DBG_LOG_DBG) \
                    { omx_info_f(fmt, ## __VA_ARGS__); } \
            } while (0)

#define VDEC_DBG(level, fmt, ...) \
    do { \
        if (omx_vdec_debug & level) \
            { omx_info(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VDEC_DBG_F(level, fmt, ...) \
    do { \
        if (omx_vdec_debug & level) \
            { omx_info_f(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VENC_DBG_LOG(fmt, ...) \
    do { \
        if (omx_venc_debug & VIDEO_DBG_LOG_DBG) \
            { omx_info(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VENC_DBG_LOG_F(fmt, ...) \
    do { \
        if (omx_venc_debug & VIDEO_DBG_LOG_DBG) \
            { omx_info_f(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VENC_DBG(level, fmt, ...) \
    do { \
        if (omx_venc_debug & level) \
            { omx_info(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VENC_DBG_F(level, fmt, ...) \
    do { \
        if (omx_venc_debug & level) \
            { omx_info_f(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VIDEO_DBG(level, fmt, ...) \
    do { \
        if (omx_venc_debug & level || omx_vdec_debug & level) \
            { omx_info(fmt, ## __VA_ARGS__); } \
    } while (0)

#define VIDEO_DBG_F(level, fmt, ...) \
    do { \
        if (omx_venc_debug & level || omx_vdec_debug & level) \
            { omx_info_f(fmt, ## __VA_ARGS__); } \
    } while (0)


#define MAX_VIDEO_INPUTBUFFER_NUM           4
#define MAX_VIDEO_OUTPUTBUFFER_NUM          2

#define DEFAULT_FRAME_WIDTH                 1920
#define DEFAULT_FRAME_HEIGHT                1088

#define DEFAULT_VIDEO_INPUT_BUFFER_SIZE    (DEFAULT_FRAME_WIDTH * DEFAULT_FRAME_HEIGHT) * 2
#define DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE   (DEFAULT_FRAME_WIDTH * DEFAULT_FRAME_HEIGHT * 3) / 2

#define INPUT_PORT_SUPPORTFORMAT_NUM_MAX    1

#define DEFAULT_IEP_OUTPUT_BUFFER_COUNT     2

typedef struct _DECODE_CODEC_EXTRA_BUFFERINFO {
    /* For Decode Output */
    OMX_U32 imageWidth;
    OMX_U32 imageHeight;
    OMX_COLOR_FORMATTYPE ColorFormat;
    //PrivateDataShareBuffer PDSB;
} DECODE_CODEC_EXTRA_BUFFERINFO;

typedef enum _RKVPU_OMX_VDEC_FLAG_MAP {
    RKVPU_OMX_VDEC_NONE         = 0,
    RKVPU_OMX_VDEC_IS_DIV3      = 0x01,
    RKVPU_OMX_VDEC_USE_DTS      = 0x02,
    RKVPU_OMX_VDEC_THUMBNAIL    = 0x04,
    RKVPU_OMX_VDEC_BUTT,
} RKVPU_OMX_VDEC_FLAG_MAP;


#endif
