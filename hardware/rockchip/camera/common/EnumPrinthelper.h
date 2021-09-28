/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _ENUMPRINTHELP_H
#define _ENUMPRINTHELP_H


struct element_value_t {
    const char * name;
    int value;
};

NAMESPACE_DECLARATION {

#define ELEMENT(array) (sizeof(array)/sizeof((array)[0]))
#define ENUM2STR(array,value) enumToString((array),ELEMENT(array),(value))

const char *enumToString(const element_value_t array[], int size, int value);

} NAMESPACE_DECLARATION_END


#include <linux/videodev2.h>
const element_value_t v4l2_memory_enum[] = {
                 {"V4L2_MEMORY_MMAP", V4L2_MEMORY_MMAP },
                 {"V4L2_MEMORY_USERPTR", V4L2_MEMORY_USERPTR },
                 {"V4L2_MEMORY_OVERLAY", V4L2_MEMORY_OVERLAY },
                 {"V4L2_MEMORY_DMABUF", V4L2_MEMORY_DMABUF },
         };

const element_value_t v4l2_buf_type_enum[] = {
                 {"VIDEO_CAPTURE", V4L2_BUF_TYPE_VIDEO_CAPTURE },
                 {"VIDEO_OUTPUT", V4L2_BUF_TYPE_VIDEO_OUTPUT },
                 {"VIDEO_OVERLAY", V4L2_BUF_TYPE_VIDEO_OVERLAY },
                 {"VBI_CAPTURE", V4L2_BUF_TYPE_VBI_CAPTURE },
                 {"VBI_OUTPUT", V4L2_BUF_TYPE_VBI_OUTPUT },
                 {"SLICED_VBI_CAPTURE", V4L2_BUF_TYPE_SLICED_VBI_CAPTURE },
                 {"SLICED_VBI_OUTPUT", V4L2_BUF_TYPE_SLICED_VBI_OUTPUT },
                 {"VIDEO_OUTPUT_OVERLAY", V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY },
                 {"VIDEO_CAPTURE_MPLANE", V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE },
                 {"VIDEO_OUTPUT_MPLANE", V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE },
                 {"SDR_CAPTURE", V4L2_BUF_TYPE_SDR_CAPTURE },
                 {"SDR_OUTPUT", V4L2_BUF_TYPE_SDR_OUTPUT },
                 {"META_CAPTURE", V4L2_BUF_TYPE_META_CAPTURE },
                 {"META_OUTPUT", V4L2_BUF_TYPE_META_OUTPUT },
                 {"PRIVATE", V4L2_BUF_TYPE_PRIVATE },
         };

#endif // _ENUMPRINTHELP_H
