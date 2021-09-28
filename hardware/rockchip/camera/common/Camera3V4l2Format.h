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

#ifndef _CAMERA3_V4L2_FORMAT_H_
#define _CAMERA3_V4L2_FORMAT_H_
#include <linux/videodev2.h>
#include "UtilityMacros.h"

NAMESPACE_DECLARATION {
/**
 *  \struct CameraFormatBridge
 *  Structure to hold static array of formats with their depths, description
 *  and information often needed in image buffer processing.
 *  This is derived concept from camera driver, with
 *  redefinition of info we are interested.
 */
struct CameraFormatBridge {
    unsigned int pixelformat;
    unsigned int depth;
    bool planar;
    bool bayer;
};
extern const struct CameraFormatBridge sV4l2PixelFormatBridge[];
const struct CameraFormatBridge* getCameraFormatBridge(unsigned int fourcc);

/**
 * return pixels based on bytes
 *
 * commonly used to calculate bytes-per-line as per pixel width
 */
int bytesToPixels(int fourcc, int bytes);
/**
 * return bytes-per-line based on given pixels
 */
int pixelsToBytes(int fourcc, int pixels);

/**
 * Return frame size (in bytes) based on image format description
 */
int frameSize(int fourcc, int width, int height);

const char* v4l2Fmt2Str(int format);
bool isBayerFormat(int fourcc);

} NAMESPACE_DECLARATION_END

#endif// _CAMERA3_V4L2_FORMAT_H_
