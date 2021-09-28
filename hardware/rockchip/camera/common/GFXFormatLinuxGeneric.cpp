/*
 * Copyright (C) 2014-2017 Intel Corporation
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
#define LOG_TAG "GfxLinuxGeneric"

#include "Camera3GFXFormat.h"
#include "PlatformData.h"
#include "LogHelper.h"

NAMESPACE_DECLARATION {

/* Native handle int indices, as offsets on top of numFds.
 * Keep this in synch with gralloc_priv.h, which is in cameralibs!
 */
enum NativeHandleIntIndices {
    NATIVE_HANDLE_MAGIC = 0,
    NATIVE_HANDLE_FLAGS,
    NATIVE_HANDLE_SIZE,
    NATIVE_HANDLE_OFFSET,
    NATIVE_HANDLE_WIDTH,
    NATIVE_HANDLE_HEIGHT,
    NATIVE_HANDLE_STRIDE,
    NATIVE_HANDLE_HALFORMAT,
    NATIVE_HANDLE_USAGE,
    /* insert here when updating */
    NATIVE_HANDLE_INT_MAX
};

/* buffer value fetching function */
static int getIntFromHandle(buffer_handle_t *handle, int intIndex)
{
    if (handle == nullptr) {
        LOGE("null native handle");
        return -1;
    }
    if (intIndex >= NATIVE_HANDLE_INT_MAX ||
        intIndex >= (*handle)->numInts ||
        intIndex < 0) {
        LOGE("bad native handle int index %d", intIndex);
        return -1;
    }
    if ((*handle)->numInts < NATIVE_HANDLE_INT_MAX) {
        LOGE("bad native handle, numints %d enum max %d",
             (*handle)->numInts,
             NATIVE_HANDLE_INT_MAX);
        return -1;
    }
    return (*handle)->data[(*handle)->numFds + intIndex];
}

int v4L2Fmt2GFXFmt(int v4l2Fmt)
{
    int gfxFmt = -1;

    switch (v4l2Fmt) {
    case V4L2_PIX_FMT_JPEG:
        gfxFmt = HAL_PIXEL_FORMAT_BLOB;
        break;
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SRGGB8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB10:
    case V4L2_PIX_FMT_SGRBG10:
    case V4L2_PIX_FMT_SGRBG12:
    case V4L2_PIX_FMT_SBGGR10:
#ifdef V4L2_PIX_FMT_SBGGR10P
    case V4L2_PIX_FMT_SBGGR10P:
#endif
#ifdef V4L2_PIX_FMT_SGBRG10P
    case V4L2_PIX_FMT_SGBRG10P:
#endif
#ifdef V4L2_PIX_FMT_SGRBG10P
    case V4L2_PIX_FMT_SGRBG10P:
#endif
#ifdef V4L2_PIX_FMT_SRGGB10P
    case V4L2_PIX_FMT_SRGGB10P:
#endif
    case V4L2_PIX_FMT_SBGGR12:
    case V4L2_PIX_FMT_SGBRG12:
    case V4L2_PIX_FMT_SRGGB12:
#ifdef V4L2_PIX_FMT_SGRBG12V32
    case V4L2_PIX_FMT_SGRBG12V32:
#endif
#ifdef V4L2_PIX_FMT_CIO2_SRGGB10
    case V4L2_PIX_FMT_CIO2_SRGGB10:
#endif
        gfxFmt = HAL_PIXEL_FORMAT_RAW16;
        break;
    case V4L2_PIX_FMT_YVU420:
#ifdef V4L2_PIX_FMT_YUYV420_V32
    case V4L2_PIX_FMT_YUYV420_V32:
#endif
        gfxFmt = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_META_FMT_RK_ISP1_PARAMS:
    case V4L2_META_FMT_RK_ISP1_STAT_3A:
        gfxFmt = HAL_PIXEL_FORMAT_RAW_OPAQUE;
        break;
    case V4L2_PIX_FMT_NV21:
        gfxFmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case V4L2_PIX_FMT_NV12:
        gfxFmt = HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_RK;
        break;
    case V4L2_PIX_FMT_YUYV:
        gfxFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
        break;
    default:
        LOGE("%s: no gfx format for v4l2 0x%x, %s!",
             __FUNCTION__, v4l2Fmt,
             v4l2Fmt2Str(v4l2Fmt));
        break;
    }

    return gfxFmt;
}

int widthToStride(int fourcc, int width)
{
    /**
     * Raw format has special aligning requirements
     */
    int stride = 0;
    if (isBayerFormat(fourcc))
        return ALIGN128(width);
    switch (fourcc) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_YVU420:
        stride =  ALIGN64(width);
        break;
    case V4L2_PIX_FMT_YUYV:
        stride =  ALIGN32(width);
        break;
    default:
        stride = ALIGN64(width);
        break;
    }
    return stride;
}

} NAMESPACE_DECLARATION_END
