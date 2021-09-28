/*
* Copyright (C) 2016 The Android Open Source Project
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

#include "Thumbnail.h"

#define LOG_NDEBUG 0
#define LOG_TAG "EmulatedCamera_Thumbnail"
#include <log/log.h>
#include <libexif/exif-data.h>
#include <libyuv.h>

#include "JpegCompressor.h"

#include <vector>

/*
 * The YU12 format is a YUV format with an 8-bit Y-component and the U and V
 * components are stored as 8 bits each but they are shared between a block of
 * 2x2 pixels. So when calculating bits per pixel the 16 bits of U and V are
 * shared between 4 pixels leading to 4 bits of U and V per pixel. Together
 * with the 8 bits of Y this gives us 12 bits per pixel..
 *
 * The components are not grouped by pixels but separated into one Y-plane, one
 * U-plane and one V-plane.
 */

namespace android {

static bool createRawThumbnail(const unsigned char* sourceImage,
                               int sourceWidth, int sourceHeight,
                               int thumbnailWidth, int thumbnailHeight,
                               std::vector<unsigned char>* thumbnail) {
    const unsigned char* ySourcePlane = sourceImage;
    const unsigned char* uSourcePlane = sourceImage + sourceWidth * sourceHeight;
    const unsigned char* vSourcePlane = uSourcePlane + sourceWidth * sourceHeight / 4;

    // Create enough space in the output vector for the result
    thumbnail->resize((thumbnailWidth * thumbnailHeight * 12) / 8);

    // The downscaled U and V planes will also be linear instead of interleaved,
    // allocate space for them here
    const size_t destUVPlaneSize = (thumbnailWidth * thumbnailHeight) / 4;
    std::vector<unsigned char> destPlanes(destUVPlaneSize * 2);
    unsigned char* yDestPlane = &(*thumbnail)[0];
    unsigned char* uDestPlane = yDestPlane + thumbnailWidth * thumbnailHeight;
    unsigned char* vDestPlane = uDestPlane + thumbnailWidth * thumbnailHeight / 4;

    // The strides for the U and V planes are half the width because the U and V
    // components are common to 2x2 pixel blocks
    int result = libyuv::I420Scale(ySourcePlane, sourceWidth,
                                   uSourcePlane, sourceWidth / 2,
                                   vSourcePlane, sourceWidth / 2,
                                   sourceWidth, sourceHeight,
                                   yDestPlane, thumbnailWidth,
                                   uDestPlane, thumbnailWidth / 2,
                                   vDestPlane, thumbnailWidth / 2,
                                   thumbnailWidth, thumbnailHeight,
                                   libyuv::kFilterBilinear);
    if (result != 0) {
        ALOGE("Unable to create thumbnail, downscaling failed with error: %d",
              result);
        return false;
    }

    return true;
}

bool createThumbnail(const unsigned char* sourceImage,
                     int sourceWidth, int sourceHeight,
                     int thumbWidth, int thumbHeight, int quality,
                     ExifData* exifData) {
    if (thumbWidth <= 0 || thumbHeight <= 0) {
        ALOGE("%s: Invalid thumbnail width=%d or height=%d, must be > 0",
              __FUNCTION__, thumbWidth, thumbHeight);
        return false;
    }

    // First downscale the source image into a thumbnail-sized raw image
    std::vector<unsigned char> rawThumbnail;
    if (!createRawThumbnail(sourceImage, sourceWidth, sourceHeight,
                            thumbWidth, thumbHeight, &rawThumbnail)) {
        // The thumbnail function will log an appropriate error if needed
        return false;
    }

    // And then compress it into JPEG format without any EXIF data
    NV21JpegCompressor compressor;
    status_t result = compressor.compressRawImage(&rawThumbnail[0],
                                                  thumbWidth, thumbHeight,
                                                  quality, nullptr /* EXIF */);
    if (result != NO_ERROR) {
        ALOGE("%s: Unable to compress thumbnail", __FUNCTION__);
        return false;
    }

    // And finally put it in the EXIF data. This transfers ownership of the
    // malloc'd memory to the EXIF data structure. As long as the EXIF data
    // structure is free'd using the EXIF library this memory will be free'd.
    exifData->size = compressor.getCompressedSize();
    exifData->data = reinterpret_cast<unsigned char*>(malloc(exifData->size));
    if (exifData->data == nullptr) {
        ALOGE("%s: Unable to allocate %u bytes of memory for thumbnail",
              __FUNCTION__, exifData->size);
        exifData->size = 0;
        return false;
    }
    compressor.getCompressedImage(exifData->data);
    return true;
}

}  // namespace android

