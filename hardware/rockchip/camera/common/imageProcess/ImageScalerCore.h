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

#ifndef _IMAGESCALER_CORE_H_
#define _IMAGESCALER_CORE_H_
#include <memory>
#include "CommonBuffer.h"

NAMESPACE_DECLARATION {
/**
 * \class ImageScalerCore
 *
 */
class ImageScalerCore {
public:
    static void downScaleImage(std::shared_ptr<CommonBuffer> srcBuf,
            std::shared_ptr<CommonBuffer> dstBuf);
    static void downScaleImage(void *src, void *dest,
            int dest_w, int dest_h, int dest_stride,
            int src_w, int src_h, int src_stride,
            int format, int src_skip_lines_top = 0,
            int src_skip_lines_bottom = 0);

protected:
    static void downScaleYUY2Image(unsigned char *dest, const unsigned char *src,
        const int dest_w, const int dest_h, const int dest_stride,
        const int src_w, const int src_h, const int src_stride);

    static void downScaleAndCropNv12Image(
        unsigned char *dest, const unsigned char *src,
        const int dest_w, const int dest_h, const int dest_stride,
        const int src_w, const int src_h, const int src_stride,
        const int src_skip_lines_top = 0,
        const int src_skip_lines_bottom = 0);

private:
    static const int MFP = 16;            // Fractional bits for fixed point calculations

private:
    static void cropComposeCopy(void *src, void *dst, unsigned int size);
public:
    static void cropComposeUpscaleNV12_bl(
        void *src, unsigned int srcH, unsigned int srcStride,
        unsigned int srcCropLeft, unsigned int srcCropTop,
        unsigned int srcCropW, unsigned int srcCropH,
        void *dst, unsigned int dstH, unsigned int dstStride,
        unsigned int dstCropLeft, unsigned int dstCropTop,
        unsigned int dstCropW, unsigned int dstCropH);

};

} NAMESPACE_DECLARATION_END
#endif // _IMAGESCALER_CORE_H_
