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

#define LOG_TAG "ImageScalerCore"

#include "LogHelper.h"
#include "ImageScalerCore.h"
#include <libyuv.h>
#include <linux/videodev2.h>

#define RESOLUTION_VGA_WIDTH    640
#define RESOLUTION_VGA_HEIGHT   480
#define RESOLUTION_QVGA_WIDTH   320
#define RESOLUTION_QVGA_HEIGHT  240
#define RESOLUTION_QCIF_WIDTH   176
#define RESOLUTION_QCIF_HEIGHT  144
#define MIN(a,b) ((a)<(b)?(a):(b))

NAMESPACE_DECLARATION {
void ImageScalerCore::downScaleImage(std::shared_ptr<CommonBuffer> srcBuf,
        std::shared_ptr<CommonBuffer> dstBuf)
{
    downScaleImage(srcBuf->data(), dstBuf->data(),
            dstBuf->width(), dstBuf->height(), dstBuf->stride(),
            srcBuf->width(), srcBuf->height(), srcBuf->stride(),
            srcBuf->v4l2Fmt());
}

void ImageScalerCore::downScaleImage(void *src, void *dest,
    int dest_w, int dest_h, int dest_stride,
    int src_w, int src_h, int src_stride,
    int format, int src_skip_lines_top, // number of lines that are skipped from src image start pointer
    int src_skip_lines_bottom) // number of lines that are skipped after reading src_h (should be set always to reach full image height)
{
    unsigned char *m_dest = (unsigned char *)dest;
    const unsigned char * m_src = (const unsigned char *)src;
    switch (format) {
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12: {
            // downscale & crop
            ImageScalerCore::downScaleAndCropNv12Image(m_dest, m_src,
                dest_w, dest_h, dest_stride,
                src_w, src_h, src_stride,
                src_skip_lines_top, src_skip_lines_bottom);
            break;
        }
        case V4L2_PIX_FMT_YUYV: {
            ImageScalerCore::downScaleYUY2Image(m_dest, m_src,
                dest_w, dest_h, dest_stride,
                src_w, src_h, src_stride);
            break;
        }
        default: {
            LOGE("no downscale support for format = %d", format);
            break;
        }
    }
}

void ImageScalerCore::downScaleYUY2Image(unsigned char *dest, const unsigned char *src,
    const int dest_w, const int dest_h, const int dest_stride,
    const int src_w, const int src_h, const int src_stride)
{
    if (dest==nullptr || dest_w <=0 || dest_h <=0 || src==nullptr || src_w <=0 || src_h <= 0 )
        return;

    if (dest_w%2 != 0) // if the dest_w is not an even number, exit
        return;

    const int scale_w = (src_w<<8) / dest_w; // scale factors
    const int scale_h = (src_h<<8) / dest_h;
    int dx, dy;
    int macro_pixel_width = dest_w >> 1;
    int src_i, src_j; // the left up coordinates of src that correspond to (j,i) in dest
    unsigned int val_1, val_2; // for bi-linear-interpolation
    int i,j,k;

    for(i=0; i < dest_h; ++i) {
        src_i = i * scale_h;
        dy = src_i & 0xff;
        src_i >>= 8;
        for(j=0; j < macro_pixel_width; ++j) {
            src_j = j * scale_w;
            dx = src_j & 0xff;
            src_j = src_j >> 8;
            for(k = 0; k < 4; ++k) {
                // bi-linear-interpolation
                if(dx == 0 && dy == 0) {
                    dest[i * 2 * dest_stride + 4 * j + k] = src[src_i * 2 * src_stride + src_j * 4 + k];
                } else if(dx == 0 && dy != 0){
                    val_1 = (unsigned int)src[src_i * 2 * src_stride + src_j * 4 + k];
                    val_2 = (unsigned int)src[(src_i + 1) * 2 * src_stride + src_j * 4 + k];
                    val_1 = (val_1 * (256 - dy) + val_2 * dy) >> 8;
                    dest[i * 2 * dest_stride + 4 * j + k] = ((val_1 <= 255) ? val_1: 255);
                } else if(dx != 0 && dy == 0) {
                    val_1 = ((unsigned int)src[src_i * 2 * src_stride + src_j * 4 + k] * (256 - dx)
                        + (unsigned int)src[src_i * 2 * src_stride + (src_j +1) * 4 + k] * dx) >> 8;
                    dest[i * 2 * dest_stride + 4 * j + k] = ((val_1 <= 255) ? val_1: 255);
                } else {
                    val_1 = ((unsigned int)src[src_i * 2 * src_stride + src_j * 4 + k] * (256 - dx)
                        + (unsigned int)src[src_i * 2 * src_stride + (src_j +1) * 4 + k] * dx) >> 8;
                    val_2 = ((unsigned int)src[(src_i + 1) * 2 * src_stride + src_j * 4 + k] * (256 - dx)
                        + (unsigned int)src[(src_i + 1) * 2 * src_stride + (src_j+1) * 4 + k] * dx) >> 8;
                    val_1 = (val_1 * (256 - dy) + val_2 * dy) >> 8;
                    dest[i * 2 * dest_stride + 4 * j + k] = ((val_1 <= 255) ? val_1: 255);
                }
            }
        }
    }
}

void ImageScalerCore::downScaleAndCropNv12Image(unsigned char *dest, const unsigned char *src,
    const int dest_w, const int dest_h, const int dest_stride,
    const int src_w, const int src_h, const int src_stride,
    const int src_skip_lines_top, // number of lines that are skipped from src image start pointer
    const int src_skip_lines_bottom) // number of lines that are skipped after reading src_h (should be set always to reach full image height)
{
    LOGI("@%s: dest_w: %d, dest_h: %d, dest_stride: %d, src_w: %d, src_h: %d, src_stride: %d, skip_top: %d, skip_bottom: %d, dest: %p, src: %p",
         __FUNCTION__, dest_w, dest_h, dest_stride, src_w, src_h, src_stride, src_skip_lines_top, src_skip_lines_bottom, dest, src);

    int total_height = src_skip_lines_top + src_h + src_skip_lines_bottom;
    int width = src_w, height = src_h;
    int left = 0, top = src_skip_lines_top;

    int proper_source_width = (int)((float)dest_w / dest_h * src_h);
    // Aligned to 4
    proper_source_width = (proper_source_width + 3) & ~3;

    int proper_source_height = (int)((float)dest_h / dest_w * src_w);

    if (src_w != dest_w || src_h != dest_h) {
        // Crop width/height if needed
        if (proper_source_width < src_w) {
            width = proper_source_width;
            left = (src_w - width) / 2;
            left = (left + 1) & ~1; // Aligned for uv
        } else if (proper_source_height < src_h) {
            height = proper_source_height;
            top += (src_h - height) / 2;
        }
    }

    int offset = top * src_stride + left;
    uint8_t *src_y = (uint8_t *)src + offset;
    uint8_t *src_uv = (uint8_t *)src + total_height * src_stride + offset/2;

    libyuv::ScalePlane(src_y, src_stride, width, height,
                       dest, dest_stride, dest_w, dest_h,
                       libyuv::kFilterNone);

    dest += dest_stride * dest_h;
    libyuv::ScalePlane_16((uint16_t *)src_uv, src_stride/2,
                          width/2, height/2,
                          (uint16_t *)dest, dest_stride/2,
                          dest_w/2, dest_h/2,
                          libyuv::kFilterNone);
}

void ImageScalerCore::cropComposeCopy(void *src, void *dst, unsigned int size)
{
    STDCOPY((int8_t *) dst, (int8_t *) src, size);
}

// Bilinear scaling, chrominance with nearest neighbor
void ImageScalerCore::cropComposeUpscaleNV12_bl(
    void *src, unsigned int srcH, unsigned int srcStride,
    unsigned int srcCropLeft, unsigned int srcCropTop,
    unsigned int srcCropW, unsigned int srcCropH,
    void *dst, unsigned int dstH, unsigned int dstStride,
    unsigned int dstCropLeft, unsigned int dstCropTop,
    unsigned int dstCropW, unsigned int dstCropH)
{
    static const int BILINEAR = 1;
    static const unsigned int FP_1  = 1 << MFP;       // Fixed point 1.0
    static const unsigned int FRACT = (1 << MFP) - 1; // Fractional part mask
    unsigned int dx, dy, sx, sy;
    unsigned char *s = (unsigned char *)src;
    unsigned char *d = (unsigned char *)dst;
    unsigned int sx0, sy0, dx0, dy0, dx1, dy1;

    unsigned int sxd = ((srcCropW<<MFP) + (dstCropW>>1)) / dstCropW;
    unsigned int syd = ((srcCropH<<MFP) + (dstCropH>>1)) / dstCropH;

    if (!src || !dst) {
        LOGE("buffer pointer is nullptr");
        return;
    }

    // Upscale luminance
    sx0 = srcCropLeft << MFP;
    sy0 = srcCropTop << MFP;
    dx0 = dstCropLeft;
    dy0 = dstCropTop;
    dx1 = dstCropLeft + dstCropW;
    dy1 = dstCropTop + dstCropH;
    for (dy = dy0, sy = sy0; dy < dy1; dy++, sy += syd) {
        for (dx = dx0, sx = sx0; dx < dx1; dx++, sx += sxd) {
            unsigned int sxi = sx >> MFP;
            unsigned int syi = sy >> MFP;
            unsigned int s0 = s[srcStride*syi+sxi];
            if (BILINEAR) {
                unsigned int fx = sx & FRACT;             // Fractional part
                unsigned int fy = sy & FRACT;
                unsigned int fx1 = FP_1 - fx;               // 1 - fractional part
                unsigned int fy1 = FP_1 - fy;
                unsigned int s1 = s[srcStride*syi+sxi+1];
                unsigned int s2 = s[srcStride*(syi+1)+sxi];
                unsigned int s3 = s[srcStride*(syi+1)+sxi+1];
                unsigned int s4 = (s0 * fx1 + s1 * fx) >> MFP;
                unsigned int s5 = (s2 * fx1 + s3 * fx) >> MFP;
                s0 = (s4 * fy1 + s5 * fy) >> MFP;
            }
            d[dstStride*dy+dx] = s0;
        }
    }

    // Upscale chrominance
    s = (unsigned char *)src + srcStride*srcH;
    d = (unsigned char *)dst + dstStride*dstH;
    sx0 = srcCropLeft << (MFP - 1);
    sy0 = srcCropTop << (MFP - 1);
    dx0 = dstCropLeft >> 1;
    dy0 = dstCropTop >> 1;
    dx1 = (dstCropLeft + dstCropW) >> 1;
    dy1 = (dstCropTop + dstCropH) >> 1;
    for (dy = dy0, sy = sy0; dy < dy1; dy++, sy += syd) {
        for (dx = dx0, sx = sx0; dx < dx1; dx++, sx += sxd) {
            unsigned int sxi = sx >> MFP;
            unsigned int syi = sy >> MFP;
            d[dstStride*dy+dx*2+0] = s[srcStride*syi+sxi*2+0];
            d[dstStride*dy+dx*2+1] = s[srcStride*syi+sxi*2+1];
        }
    }
}
} NAMESPACE_DECLARATION_END

