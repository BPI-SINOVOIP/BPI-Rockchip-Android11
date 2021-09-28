/*
 * Copyright (C) 2011-2017 Intel Corporation
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

#define LOG_TAG "ColorConverter"

#include <sys/types.h>
#include <stdlib.h>
#include <cstdlib>
#include <linux/videodev2.h>
#include "UtilityMacros.h"
#include "ColorConverter.h"
#include "LogHelper.h"

NAMESPACE_DECLARATION {
// covert YV12 (Y plane, V plane, U plane) to NV21 (Y plane, interlaced VU bytes)
void convertYV12ToNV21(int width, int height, int srcStride, int dstStride, void *src, void *dst)
{
    const int cStride = srcStride>>1;
    const int vuStride = dstStride;
    const int hhalf = height>>1;
    const int whalf = width>>1;

    // copy the entire Y plane
    unsigned char *srcPtr = (unsigned char *)src;
    unsigned char *dstPtr = (unsigned char *)dst;
    if (srcStride == dstStride) {
        STDCOPY((int8_t *) dst, (int8_t *) src, dstStride * height);
    } else {
        for (int i = 0; i < height; i++) {
            STDCOPY(dstPtr, srcPtr, width);
            srcPtr += srcStride;
            dstPtr += dstStride;
        }
    }

    // interlace the VU data
    unsigned char *srcPtrV = (unsigned char *)src + height*srcStride;
    unsigned char *srcPtrU = srcPtrV + cStride*hhalf;
    dstPtr = (unsigned char *)dst + dstStride*height;
    for (int i = 0; i < hhalf; ++i) {
        unsigned char *pDstVU = dstPtr;
        unsigned char *pSrcV = srcPtrV;
        unsigned char *pSrcU = srcPtrU;
        for (int j = 0; j < whalf; ++j) {
            *pDstVU ++ = *pSrcV ++;
            *pDstVU ++ = *pSrcU ++;
        }
        dstPtr += vuStride;
        srcPtrV += cStride;
        srcPtrU += cStride;
    }
}

// copy YV12 to YV12 (Y plane, V plan, U plan) in case of different stride length
void copyYV12ToYV12(int width, int height, int srcStride, int dstStride, void *src, void *dst)
{
    // copy the entire Y plane
    if (srcStride == dstStride) {
        STDCOPY((int8_t *) dst, (int8_t *) src, dstStride * height);
    } else {
        unsigned char *srcPtrY = (unsigned char *)src;
        unsigned char *dstPtrY = (unsigned char *)dst;
        for (int i = 0; i < height; i ++) {
            STDCOPY(dstPtrY, srcPtrY, width);
            srcPtrY += srcStride;
            dstPtrY += dstStride;
        }
    }

    // copy VU plane
    const int scStride = srcStride >> 1;
    const int dcStride = ALIGN16(dstStride >> 1); // Android CTS required: U/V plane needs 16 bytes aligned!
    if (dcStride == scStride) {
        unsigned char *srcPtrVU = (unsigned char *)src + height * srcStride;
        unsigned char *dstPtrVU = (unsigned char *)dst + height * dstStride;
        STDCOPY(dstPtrVU, srcPtrVU, height * dcStride);
    } else {
        const int wHalf = width >> 1;
        const int hHalf = height >> 1;
        unsigned char *srcPtrV = (unsigned char *)src + height * srcStride;
        unsigned char *srcPtrU = srcPtrV + scStride * hHalf;
        unsigned char *dstPtrV = (unsigned char *)dst + height * dstStride;
        unsigned char *dstPtrU = dstPtrV + dcStride * hHalf;
        for (int i = 0; i < hHalf; i ++) {
            STDCOPY(dstPtrU, srcPtrU, wHalf);
            STDCOPY(dstPtrV, srcPtrV, wHalf);
            dstPtrU += dcStride, srcPtrU += scStride;
            dstPtrV += dcStride, srcPtrV += scStride;
        }
    }
}

// convert NV12 (Y plane, interlaced UV bytes) to YV12 (Y plane, V plane, U plane)
// without Y and C 16 bytes aligned
void convertNV12ToYV12(int width, int height, int srcStride, void *src, void *dst)
{
    int yStride = width;
    size_t ySize = yStride * height;
    int cStride = yStride/2;
    size_t cSize = cStride * height/2;

    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrV = (unsigned char *) dst + ySize;
    unsigned char *dstPtrU = (unsigned char *) dst + ySize + cSize;

    // copy the entire Y plane
    if (srcStride == yStride) {
        STDCOPY(dstPtr, srcPtr, ySize);
        srcPtr += ySize;
    } else if (srcStride > width) {
        for (int i = 0; i < height; i++) {
            STDCOPY(dstPtr, srcPtr, width);
            srcPtr += srcStride;
            dstPtr += yStride;
        }
    } else {
        LOGE("bad src stride value");
        return;
    }

    // deinterlace the UV data
    int halfHeight = height / 2;
    int halfWidth = width / 2;
    for ( int i = 0; i < halfHeight; ++i) {
        for ( int j = 0; j < halfWidth; ++j) {
            dstPtrV[j] = srcPtr[j * 2 + 1];
            dstPtrU[j] = srcPtr[j * 2];
        }
        srcPtr += srcStride;
        dstPtrV += cStride;
        dstPtrU += cStride;
    }
}

// convert NV12 (Y plane, interlaced UV bytes) to YV12 (Y plane, V plane, U plane)
// with Y and C 16 bytes aligned
void align16ConvertNV12ToYV12(int width, int height, int srcStride, void *src, void *dst)
{
    int yStride = ALIGN16(width);
    size_t ySize = yStride * height;
    int cStride = ALIGN16(yStride/2);
    size_t cSize = cStride * height/2;

    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrV = (unsigned char *) dst + ySize;
    unsigned char *dstPtrU = (unsigned char *) dst + ySize + cSize;

    // copy the entire Y plane
    if (srcStride == yStride) {
        STDCOPY(dstPtr, srcPtr, ySize);
        srcPtr += ySize;
    } else if (srcStride > width) {
        for (int i = 0; i < height; i++) {
            STDCOPY(dstPtr, srcPtr, width);
            srcPtr += srcStride;
            dstPtr += yStride;
        }
    } else {
        LOGE("bad src stride value");
        return;
    }

    // deinterlace the UV data
    for ( int i = 0; i < height / 2; ++i) {
        for ( int j = 0; j < width / 2; ++j) {
            dstPtrV[j] = srcPtr[j * 2 + 1];
            dstPtrU[j] = srcPtr[j * 2];
        }
        srcPtr += srcStride;
        dstPtrV += cStride;
        dstPtrU += cStride;
    }
}

// P411's Y, U, V are seperated. But the YUY2's Y, U and V are interleaved.
void YUY2ToP411(int width, int height, int stride, void *src, void *dst)
{
    int ySize = width * height;
    int cSize = width * height / 4;
    int wHalf = width >> 1;

    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrU = (unsigned char *) dst + ySize;
    unsigned char *dstPtrV = (unsigned char *) dst + ySize + cSize;

    for (int i = 0; i < height; i++) {
        //The first line of the source
        //Copy first Y Plane first
        for (int j=0; j < width; j++) {
            dstPtr[j] = srcPtr[j*2];
        }

        if (i & 1) {
            //Copy the V plane
            for (int k = 0; k < wHalf; k++) {
                dstPtrV[k] = srcPtr[k * 4 + 3];
            }
            dstPtrV = dstPtrV + wHalf;
        } else {
            //Copy the U plane
            for (int k = 0; k< wHalf; k++) {
                dstPtrU[k] = srcPtr[k * 4 + 1];
            }
            dstPtrU = dstPtrU + wHalf;
        }

        srcPtr = srcPtr + stride * 2;
        dstPtr = dstPtr + width;
    }
}

// P411's Y, U, V are separated. But the NV12's U and V are interleaved.
void NV12ToP411Separate(int width, int height, int stride,
                                void *srcY, void *srcUV, void *dst)
{
    int i, j, p, q;
    unsigned char *psrcY = (unsigned char *) srcY;
    unsigned char *pdstY = (unsigned char *) dst;
    unsigned char *pdstU, *pdstV;
    unsigned char *psrcUV;

    // copy Y data
    for (i = 0; i < height; i++) {
        STDCOPY(pdstY, psrcY, width);
        pdstY += width;
        psrcY += stride;
    }

    // copy U data and V data
    psrcUV = (unsigned char *)srcUV;
    pdstU = (unsigned char *)dst + width * height;
    pdstV = pdstU + width * height / 4;
    p = q = 0;
    for (i = 0; i < height / 2; i++) {
        for (j = 0; j < width; j++) {
            if (j % 2 == 0) {
                pdstU[p] = (psrcUV[i * stride + j] & 0xFF);
                p++;
           } else {
                pdstV[q] = (psrcUV[i * stride + j] & 0xFF);
                q++;
            }
        }
    }
}

// P411's Y, U, V are seperated. But the NV12's U and V are interleaved.
void NV12ToP411(int width, int height, int stride, void *src, void *dst)
{
    NV12ToP411Separate(width, height, stride,
                    src, (void *)((unsigned char *)src + width * height), dst);
}

// P411's Y, U, V are separated. But the NV21's U and V are interleaved.
void NV21ToP411Separate(int width, int height, int stride,
                        void *srcY, void *srcUV, void *dst)
{
    int i, j, p, q;
    unsigned char *psrcY = (unsigned char *) srcY;
    unsigned char *pdstY = (unsigned char *) dst;
    unsigned char *pdstU, *pdstV;
    unsigned char *psrcUV;

    // copy Y data
    for (i = 0; i < height; i++) {
        STDCOPY(pdstY, psrcY, width);
        pdstY += width;
        psrcY += stride;
    }

    // copy U data and V data
    psrcUV = (unsigned char *)srcUV;
    pdstU = (unsigned char *)dst + width * height;
    pdstV = pdstU + width * height / 4;
    p = q = 0;
    for (i = 0; i < height / 2; i++) {
        for (j = 0; j < width; j++) {
            if ((j & 1) == 0) {
                pdstV[p] = (psrcUV[i * stride + j] & 0xFF);
                p++;
           } else {
                pdstU[q] = (psrcUV[i * stride + j] & 0xFF);
                q++;
            }
        }
    }
}

// P411's Y, U, V are seperated. But the NV21's U and V are interleaved.
void NV21ToP411(int width, int height, int stride, void *src, void *dst)
{
    NV21ToP411Separate(width, height, stride,
                       src, (void *)((unsigned char *)src + width * height), dst);
}

// Re-pad YUV420 format image, the format can be YV12, YU12 or YUV420 planar.
// If buffer size: (height*dstStride*1.5) > (height*srcStride*1.5), src and dst
// buffer start addresses are same, the re-padding can be done inplace.
void repadYUV420(int width, int height, int srcStride, int dstStride, void *src, void *dst)
{
    unsigned char *dptr;
    unsigned char *sptr;
    void * (*myCopy)(void *dst, const void *src, size_t n);

    const int whalf = width >> 1;
    const int hhalf = height >> 1;
    const int scStride = srcStride >> 1;
    const int dcStride = dstStride >> 1;
    const int sySize = height * srcStride;
    const int dySize = height * dstStride;
    const int scSize = hhalf * scStride;
    const int dcSize = hhalf * dcStride;

    // directly copy, if (srcStride == dstStride)
    if (srcStride == dstStride) {
        STDCOPY((int8_t *) dst, (int8_t *) src, dySize + 2*dcSize);
        return;
    }

    // copy V(YV12 case) or U(YU12 case) plane line by line
    sptr = (unsigned char *)src + sySize + 2*scSize - scStride;
    dptr = (unsigned char *)dst + dySize + 2*dcSize - dcStride;

    // try to avoid overlapped memcpy()
    myCopy = (std::abs(sptr -dptr) > dstStride) ? memcpy : memmove;

    for (int i = 0; i < hhalf; i ++) {
        myCopy(dptr, sptr, whalf);
        sptr -= scStride;
        dptr -= dcStride;
    }

    // copy  V(YV12 case) or U(YU12 case) U/V plane line by line
    sptr = (unsigned char *)src + sySize + scSize - scStride;
    dptr = (unsigned char *)dst + dySize + dcSize - dcStride;
    for (int i = 0; i < hhalf; i ++) {
        myCopy(dptr, sptr, whalf);
        sptr -= scStride;
        dptr -= dcStride;
    }

    // copy Y plane line by line
    sptr = (unsigned char *)src + sySize - srcStride;
    dptr = (unsigned char *)dst + dySize - dstStride;
    for (int i = 0; i < height; i ++) {
        myCopy(dptr, sptr, width);
        sptr -= srcStride;
        dptr -= dstStride;
    }
}

// covert YUYV(YUY2, YUV422 format) to YV12 (Y plane, V plane, U plane)
void convertYUYVToYV12(int width, int height, int srcStride, int dstStride, void *src, void *dst)
{
    int ySize = width * height;
    int cSize = ALIGN16(dstStride/2) * height / 2;
    int wHalf = width >> 1;

    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrV = (unsigned char *) dst + ySize;
    unsigned char *dstPtrU = (unsigned char *) dst + ySize + cSize;

    for (int i = 0; i < height; i++) {
        //The first line of the source
        //Copy first Y Plane first
        for (int j=0; j < width; j++) {
            dstPtr[j] = srcPtr[j*2];
        }

        if (i & 1) {
            //Copy the V plane
            for (int k = 0; k< wHalf; k++) {
                dstPtrV[k] = srcPtr[k * 4 + 3];
            }
            dstPtrV = dstPtrV + ALIGN16(dstStride>>1);
        } else {
            //Copy the U plane
            for (int k = 0; k< wHalf; k++) {
                dstPtrU[k] = srcPtr[k * 4 + 1];
            }
            dstPtrU = dstPtrU + ALIGN16(dstStride>>1);
        }

        srcPtr = srcPtr + srcStride * 2;
        dstPtr = dstPtr + width;
    }
}

// covert YUYV(YUY2, YUV422 format) to NV21 (Y plane, interlaced VU bytes)
void convertYUYVToNV21(int width, int height, int srcStride, void *src, void *dst)
{
    int ySize = width * height;
    int u_counter=1, v_counter=0;

    unsigned char *srcPtr = (unsigned char *) src;
    unsigned char *dstPtr = (unsigned char *) dst;
    unsigned char *dstPtrUV = (unsigned char *) dst + ySize;

    for (int i=0; i < height; i++) {
        //The first line of the source
        //Copy first Y Plane first
        for (int j=0; j < width * 2; j++) {
            if (j % 2 == 0)
                dstPtr[j/2] = srcPtr[j];
            if (i%2) {
                if (( j % 4 ) == 3) {
                    dstPtrUV[v_counter] = srcPtr[j]; //V plane
                    v_counter += 2;
                }
                if (( j % 4 ) == 1) {
                    dstPtrUV[u_counter] = srcPtr[j]; //U plane
                    u_counter += 2;
                }
            }
        }

        srcPtr = srcPtr + srcStride * 2;
        dstPtr = dstPtr + width;
    }
}

void convertNV12ToYUYV(int srcWidth, int srcHeight, int srcStride, int dstStride, const void *src, void *dst)
{
    int y_counter = 0, u_counter = 1, v_counter = 3, uv_counter = 0;
    unsigned char *srcYPtr = (unsigned char *) src;
    unsigned char *srcUVPtr = (unsigned char *)src + srcWidth * srcHeight;
    unsigned char *dstPtr = (unsigned char *) dst;

    for (int i = 0; i < srcHeight; i++) {
        for (int k = 0; k < srcWidth; k++) {
                dstPtr[y_counter] = srcYPtr[k];
                y_counter += 2;
                dstPtr[u_counter] = srcUVPtr[uv_counter];
                u_counter += 4;
                dstPtr[v_counter] = srcUVPtr[uv_counter + 1];
                v_counter += 4;
                uv_counter += 2;
        }
        if ((i % 2) == 0) {
            srcUVPtr = srcUVPtr + srcStride;
        }

        dstPtr = dstPtr + 2 * dstStride;
        srcYPtr = srcYPtr + srcStride;
        u_counter = 1;
        v_counter = 3;
        y_counter = 0;
        uv_counter = 0;
    }
}

void convertBuftoYV12(int format, int width, int height, int srcStride, int
                      dstStride, void *src, void *dst, bool align16)
{
    switch (format) {
    case V4L2_PIX_FMT_NV12:
        align16 ? align16ConvertNV12ToYV12(width, height, srcStride, src, dst)
            : convertNV12ToYV12(width, height, srcStride, src, dst);
        break;
    case V4L2_PIX_FMT_YVU420:
        copyYV12ToYV12(width, height, srcStride, dstStride, src, dst);
        break;
    case V4L2_PIX_FMT_YUYV:
        convertYUYVToYV12(width, height, srcStride, dstStride, src, dst);
        break;
    default:
        LOGE("%s: unsupported format %d", __func__, format);
        break;
    }
}

void convertBuftoNV21(int format, int width, int height, int srcStride, int
                      dstStride, void *src, void *dst)
{
    switch (format) {
    case V4L2_PIX_FMT_YVU420:
        convertYV12ToNV21(width, height, srcStride, dstStride, src, dst);
        break;
    case V4L2_PIX_FMT_YUYV:
        convertYUYVToNV21(width, height, srcStride, src, dst);
        break;
    default:
        LOGE("%s: unsupported format %d", __func__, format);
        break;
    }
}
} NAMESPACE_DECLARATION_END
