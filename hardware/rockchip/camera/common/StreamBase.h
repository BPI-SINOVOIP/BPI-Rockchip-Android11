/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#ifndef _CAMERA_COMMON_STREAM_BASE_
#define _CAMERA_COMMON_STREAM_BASE_

#include <stdint.h>

NAMESPACE_DECLARATION {
// Define the stream intent
enum StreamUsage {
    STREAM_USAGE_COMMON = 0,
    STREAM_USAGE_PREVIEW,        // For HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
    STREAM_USAGE_VIDEO,          // For HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
    STREAM_USAGE_STILL_CAPTURE,  // For HAL_PIXEL_FORMAT_BLOB/HAL_PIXEL_FORMAT_YCbCr_420_888
    STREAM_USAGE_RAW,            // For HAL_PIXEL_FORMAT_RAW16
    STREAM_USAGE_ZSL,            // For CAMERA3_STREAM_BIDIRECTIONAL/GRALLOC_USAGE_HW_CAMERA_ZSL
    STREAM_USAGE_INPUT           // For input stream
};

struct StreamProps {
    uint32_t width;
    uint32_t height;
    int32_t  fourcc;// v4l2 format
    StreamUsage usage;
};

class StreamBase
{
public:
    StreamBase(StreamProps &props, void *priv):
        mWidth(props.width),
        mHeight(props.height),
        mFourcc(props.fourcc),
        mUsage(props.usage),
        mPrivate(priv) { }
    ~StreamBase() { }

    uint32_t width() { return mWidth; }
    uint32_t height() { return mHeight; }
    int32_t  v4l2Fmt() { return mFourcc; }
    StreamUsage usage() { return mUsage; }
    void *priv() { return mPrivate; }

private:
    uint32_t mWidth;
    uint32_t mHeight;
    int32_t  mFourcc;
    StreamUsage mUsage;
    void    *mPrivate;
};
} NAMESPACE_DECLARATION_END

#endif /* _CAMERA_COMMON_STREAM_BASE_ */
