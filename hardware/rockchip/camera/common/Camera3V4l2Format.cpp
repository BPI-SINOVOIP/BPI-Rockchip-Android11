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

#define LOG_TAG "V4l2Format"
#include <string>
#include <sstream>
#include "LogHelper.h"
#include "Camera3V4l2Format.h"

NAMESPACE_DECLARATION {
const struct CameraFormatBridge sV4L2PixelFormatBridge[] = {
    {
        .pixelformat = V4L2_PIX_FMT_NV12,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUV420,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YVU420,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUV422P,
        .depth = 16,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUV444,
        .depth = 24,
        .planar = false,
        .bayer = false
#ifdef V4L2_PIX_FMT_YUYV420_V32
    }, {
        .pixelformat = V4L2_PIX_FMT_YUYV420_V32,
        .depth = 24,
        .planar = true,
        .bayer = false
#endif
    }, {
        .pixelformat = V4L2_PIX_FMT_NV21,
        .depth = 12,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_NV16,
        .depth = 16,
        .planar = true,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_YUYV,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_UYVY,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, { /* This one is for parallel sensors! DO NOT USE! */
        .pixelformat = V4L2_PIX_FMT_UYVY,
        .depth = 16,
        .planar = false,
        .bayer = false
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR16,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB8,
        .depth = 8,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG10,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB10,
        .depth = 16,
        .planar = false,
        .bayer = true
#ifdef V4L2_PIX_FMT_SBGGR10P
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR10P,
        .depth = 12,
        .planar = false,
        .bayer = true
#endif
#ifdef V4L2_PIX_FMT_SGBRG10P
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG10P,
        .depth = 12,
        .planar = false,
        .bayer = true
#endif
#ifdef V4L2_PIX_FMT_SGRBG10P
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG10P,
        .depth = 12,
        .planar = false,
        .bayer = true
#endif
#ifdef V4L2_PIX_FMT_SRGGB10P
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB10P,
        .depth = 12,
        .planar = false,
        .bayer = true
#endif
#ifdef V4L2_PIX_FMT_CIO2_SRGGB10
    }, {
        .pixelformat = V4L2_PIX_FMT_CIO2_SRGGB10,
        .depth = 12,
        .planar = false,
        .bayer = true
#endif
    }, {
        .pixelformat = V4L2_PIX_FMT_SBGGR12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGBRG12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG12,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_SRGGB12,
        .depth = 16,
        .planar = false,
        .bayer = true
#ifdef V4L2_PIX_FMT_SGRBG12V32
    }, {
        .pixelformat = V4L2_PIX_FMT_SGRBG12V32,
        .depth = 16,
        .planar = false,
        .bayer = true
#endif
    }, {
        .pixelformat = V4L2_PIX_FMT_RGB32,
        .depth = 32,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_RGB565,
        .depth = 16,
        .planar = false,
        .bayer = true
    }, {
        .pixelformat = V4L2_PIX_FMT_JPEG,
        .depth = 8,
        .planar = false,
        .bayer = false
    },{
        .pixelformat = V4L2_PIX_FMT_MJPEG,
        .depth = 8,
        .planar = false,
        .bayer = false
    },
};

/**
 * returns sCameraFormatBrige* for given V4L2 pixelformat (fourcc)
 */
const struct CameraFormatBridge* getCameraFormatBridge(unsigned int fourcc)
{
    unsigned int i;
    for (i = 0;i < sizeof(sV4L2PixelFormatBridge)/sizeof(CameraFormatBridge);i++) {
        if (fourcc == sV4L2PixelFormatBridge[i].pixelformat)
            return &sV4L2PixelFormatBridge[i];
    }

    LOGI("Unkown pixel format %d being used, use NV12 as default", fourcc);
    return &sV4L2PixelFormatBridge[0];
}

bool isBayerFormat(int fourcc)
{
    const CameraFormatBridge* afb = getCameraFormatBridge(fourcc);
    return afb->bayer;
}

int pixelsToBytes(int fourcc, int pixels)
{
    const CameraFormatBridge* afb = getCameraFormatBridge(fourcc);

    if (afb->planar) {
        // All our planar YUV formats are with depth 8 luma
        // and here byte means one pixel. Chroma planes are
        // to be handled according to fourcc respectively
        return pixels;
    }

    return ALIGN8(afb->depth * pixels) / 8;
}

int bytesToPixels(int fourcc, int bytes)
{
    const CameraFormatBridge* afb = getCameraFormatBridge(fourcc);

    if (afb->planar) {
        // All our planar YUV formats are with depth 8 luma
        // and here byte means one pixel. Chroma planes are
        // to be handled according to fourcc respectively
        return bytes;
    }

    return (bytes * 8) / afb->depth;
}

int frameSize(int fourcc, int width, int height)
{
    // JPEG buffers are generated from HAL_PIXEL_FORMAT_BLOB, where
    // the "stride" (width here)is the full size of the buffer in bytes, so use it
    // as the buffer size.
    if (fourcc == V4L2_PIX_FMT_JPEG)
        return width;
    const CameraFormatBridge* afb = getCameraFormatBridge(fourcc);
    return height * ALIGN8(afb->depth * width) / 8;
}

const char* v4l2Fmt2Str(int format)
{
    static char fourccBuf[5];
    CLEAR(fourccBuf);
    char *fourccPtr = (char*) &format;
    std::ostringstream stringStream;
    stringStream << *fourccPtr << *(fourccPtr+1) << *(fourccPtr+2) << *(fourccPtr+3);
    std::string str = stringStream.str();
    const char *tmp = str.c_str();
    MEMCPY_S(fourccBuf, sizeof(fourccBuf), tmp, str.length());
    fourccBuf[4] = '\0';
    return fourccBuf;
}

} NAMESPACE_DECLARATION_END

