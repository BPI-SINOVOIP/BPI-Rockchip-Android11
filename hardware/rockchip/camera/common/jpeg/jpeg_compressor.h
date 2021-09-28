/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_JPEG_COMPRESSOR_H_
#define INCLUDE_ARC_JPEG_COMPRESSOR_H_

// We must include cstdio before jpeglib.h. It is a requirement of libjpeg.
#include <cstdio>
#include <string>
#include <vector>

extern "C" {
#include <jerror.h>
#include <jpeglib.h>
}

namespace arc {

// Encapsulates a converter from YU12 to JPEG format. This class is not
// thread-safe.
class JpegCompressor {
public:
    JpegCompressor();
    ~JpegCompressor();

    // Compresses YU12 image to JPEG format. |quality| is the resulted jpeg
    // image quality. It ranges from 1 (poorest quality) to 100 (highest quality).
    // |app1_buffer| is the buffer of APP1 segment (exif) which will be added to
    // the compressed image. Caller should pass the size of output buffer to
    // |out_buffer_size|. Encoded result will be written into |output_buffer|.
    // The actually encoded size will be written into |out_data_size| if image
    // encoded successfully. Returns false if errors occur during compression.
    bool CompressImage(const void* image,
                       int width,
                       int height,
                       int quality,
                       const void* app1_buffer,
                       uint32_t app1_size,
                       uint32_t out_buffer_size,
                       void* out_buffer,
                       uint32_t* out_data_size);

    // Compresses YU12 image to JPEG format. |quality| is the resulted jpeg
    // image quality. It ranges from 1 (poorest quality) to 100 (highest quality).
    // Caller should pass the size of output buffer to |out_buffer_size|. Encoded
    // result will be written into |output_buffer|. The actually encoded size will
    // be written into |out_data_size| if image encoded successfully. Returns
    // false if errors occur during compression.
    bool GenerateThumbnail(const void* image,
                           int image_width,
                           int image_height,
                           int thumbnail_width,
                           int thumbnail_height,
                           int quality,
                           uint32_t out_buffer_size,
                           void* out_buffer,
                           uint32_t* out_data_size);

private:
    // InitDestination(), EmptyOutputBuffer() and TerminateDestination() are
    // callback functions to be passed into jpeg library.
    static void InitDestination(j_compress_ptr cinfo);
    static boolean EmptyOutputBuffer(j_compress_ptr cinfo);
    static void TerminateDestination(j_compress_ptr cinfo);
    static void OutputErrorMessage(j_common_ptr cinfo);

    // Returns false if errors occur.
    bool Encode(const void* inYuv,
                int width,
                int height,
                int jpegQuality,
                const void* app1_buffer,
                unsigned int app1_size,
                uint32_t out_buffer_size,
                void* out_buffer,
                uint32_t* out_data_size);
    void SetJpegDestination(jpeg_compress_struct* cinfo);
    void SetJpegCompressStruct(int width,
                               int height,
                               int quality,
                               jpeg_compress_struct* cinfo);
    // Returns false if errors occur.
    bool Compress(jpeg_compress_struct* cinfo, const uint8_t* yuv);

    // Process 16 lines of Y and 16 lines of U/V each time.
    // We must pass at least 16 scanlines according to libjpeg documentation.
    static const int kCompressBatchSize = 16;

    // Point to output buffer. JpegCompressor doesn't own this buffer.
    JOCTET* out_buffer_ptr_;

    // Output buffer size.
    uint32_t out_buffer_size_;

    // Final JPEG encoded size.
    uint32_t out_data_size_;

    // Since output buffer is passed from caller, use a variable to indicate
    // buffer is enough to encode or not.
    bool is_encode_success_;
};

}  // namespace arc

#endif  // INCLUDE_ARC_JPEG_COMPRESSOR_H_
