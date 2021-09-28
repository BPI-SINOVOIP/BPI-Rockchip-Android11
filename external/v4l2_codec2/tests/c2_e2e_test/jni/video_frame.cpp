// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "VideoFrame"

#include <string.h>

#include "video_frame.h"

#include <utils/Log.h>

namespace android {

namespace {

void CopyWindow(const uint8_t* src, uint8_t* dst, size_t stride, size_t width, size_t height,
                size_t inc) {
    if (inc == 1 && stride == width) {
        // Could copy by plane.
        memcpy(dst, src, width * height);
        return;
    }

    if (inc == 1) {
        // Could copy by row.
        for (size_t row = 0; row < height; ++row) {
            memcpy(dst, src, width);
            dst += width;
            src += stride;
        }
        return;
    }

    // inc != 1, copy by pixel.
    for (size_t row = 0; row < height; ++row) {
        for (size_t col = 0; col < width; ++col) {
            memcpy(dst, src, 1);
            dst++;
            src += inc;
        }
        src += (stride - width) * inc;
    }
}

}  // namespace

// static
std::unique_ptr<VideoFrame> VideoFrame::Create(const uint8_t* data, size_t data_size,
                                               const Size& coded_size, const Size& visible_size,
                                               int32_t color_format) {
    if (coded_size.IsEmpty() || visible_size.IsEmpty() || (visible_size.width > coded_size.width) ||
        (visible_size.height > coded_size.height) || (coded_size.width % 2 != 0) ||
        (coded_size.height % 2 != 0) || (visible_size.width % 2 != 0) ||
        (visible_size.height % 2 != 0)) {
        ALOGE("Size are not valid: coded: %dx%d, visible: %dx%d", coded_size.width,
              coded_size.height, visible_size.width, visible_size.height);
        return nullptr;
    }

    if (color_format != YUV_420_PLANAR && color_format != YUV_420_FLEXIBLE &&
        color_format != HAL_PIXEL_FORMAT_YV12 && color_format != HAL_PIXEL_FORMAT_NV12) {
        ALOGE("color_format is unknown: 0x%x", color_format);
        return nullptr;
    }

    if (data_size < coded_size.width * coded_size.height * 3 / 2) {
        ALOGE("data_size(=%zu) is not enough for coded_size(=%dx%d)", data_size, coded_size.width,
              coded_size.height);
        return nullptr;
    }
    // We've found in ARC++P H264 decoding, |data_size| of some output buffers are
    // bigger than the area which |coded_size| needs (not observed on other codec
    // and ARC++N).
    // TODO(johnylin): find the root cause (b/130398258)
    if (data_size > coded_size.width * coded_size.height * 3 / 2) {
        ALOGV("data_size(=%zu) is bigger than the area coded_size(=%dx%d) needs.", data_size,
              coded_size.width, coded_size.height);
    }

    return std::unique_ptr<VideoFrame>(
            new VideoFrame(data, coded_size, visible_size, color_format));
}

VideoFrame::VideoFrame(const uint8_t* data, const Size& coded_size, const Size& visible_size,
                       int32_t color_format)
      : data_(data),
        coded_size_(coded_size),
        visible_size_(visible_size),
        color_format_(color_format) {
    frame_data_[0] =
            std::unique_ptr<uint8_t[]>(new uint8_t[visible_size_.width * visible_size_.height]());
    frame_data_[1] = std::unique_ptr<uint8_t[]>(
            new uint8_t[visible_size_.width * visible_size_.height / 4]());
    frame_data_[2] = std::unique_ptr<uint8_t[]>(
            new uint8_t[visible_size_.width * visible_size_.height / 4]());
    if (IsFlexibleFormat()) {
        ALOGV("Cannot convert video frame now, should be done later by matching HAL "
              "format.");
        return;
    }
    CopyAndConvertToI420Frame(color_format_);
}

bool VideoFrame::IsFlexibleFormat() const {
    return color_format_ == YUV_420_FLEXIBLE;
}

void VideoFrame::CopyAndConvertToI420Frame(int32_t curr_format) {
    size_t stride = coded_size_.width;
    size_t slice_height = coded_size_.height;
    size_t width = visible_size_.width;
    size_t height = visible_size_.height;

    const uint8_t* src = data_;
    CopyWindow(src, frame_data_[0].get(), stride, width, height, 1);  // copy Y
    src += stride * slice_height;

    switch (curr_format) {
    case YUV_420_PLANAR:
        CopyWindow(src, frame_data_[1].get(), stride / 2, width / 2, height / 2,
                   1);  // copy U
        src += stride * slice_height / 4;
        CopyWindow(src, frame_data_[2].get(), stride / 2, width / 2, height / 2,
                   1);  // copy V
        return;
    case HAL_PIXEL_FORMAT_NV12:
        // NV12: semiplanar = true, crcb_swap = false.
        CopyWindow(src, frame_data_[1].get(), stride / 2, width / 2, height / 2,
                   2);  // copy U
        src++;
        CopyWindow(src, frame_data_[2].get(), stride / 2, width / 2, height / 2,
                   2);  // copy V
        return;
    case HAL_PIXEL_FORMAT_YV12:
        // YV12: semiplanar = false, crcb_swap = true.
        CopyWindow(src, frame_data_[2].get(), stride / 2, width / 2, height / 2,
                   1);  // copy V
        src += stride * slice_height / 4;
        CopyWindow(src, frame_data_[1].get(), stride / 2, width / 2, height / 2,
                   1);  // copy U
        return;
    default:
        ALOGE("Unknown format: 0x%x", curr_format);
        return;
    }
}

bool VideoFrame::MatchHalFormatByGoldenMD5(const std::string& golden) {
    if (!IsFlexibleFormat()) return true;

    // Try to match with HAL_PIXEL_FORMAT_NV12 first.
    int32_t format_candidates[2] = {HAL_PIXEL_FORMAT_NV12, HAL_PIXEL_FORMAT_YV12};
    for (int32_t format : format_candidates) {
        CopyAndConvertToI420Frame(format);
        color_format_ = format;
        std::string frame_md5 = ComputeMD5FromFrame();
        if (!strcmp(frame_md5.c_str(), golden.c_str())) {
            ALOGV("Matched YUV Flexible to HAL pixel format: 0x%x", format);
            return true;
        } else {
            ALOGV("Tried HAL pixel format: 0x%x un-matched (%s vs %s)", format, frame_md5.c_str(),
                  golden.c_str());
        }
    }

    // Change back to flexible format.
    color_format_ = YUV_420_FLEXIBLE;
    return false;
}

std::string VideoFrame::ComputeMD5FromFrame() const {
    if (IsFlexibleFormat()) {
        ALOGE("Cannot compute MD5 with format YUV_420_FLEXIBLE");
        return std::string();
    }

    MD5Context context;
    MD5Init(&context);
    MD5Update(&context, std::string(reinterpret_cast<const char*>(frame_data_[0].get()),
                                    visible_size_.width * visible_size_.height));
    MD5Update(&context, std::string(reinterpret_cast<const char*>(frame_data_[1].get()),
                                    visible_size_.width * visible_size_.height / 4));
    MD5Update(&context, std::string(reinterpret_cast<const char*>(frame_data_[2].get()),
                                    visible_size_.width * visible_size_.height / 4));
    MD5Digest digest;
    MD5Final(&digest, &context);
    return MD5DigestToBase16(digest);
}

bool VideoFrame::VerifyMD5(const std::string& golden) {
    if (IsFlexibleFormat()) {
        // Color format is YUV_420_FLEXIBLE and we haven't match its HAL pixel
        // format yet. Try to match now.
        if (!MatchHalFormatByGoldenMD5(golden)) {
            ALOGE("Failed to match any HAL format");
            return false;
        }
    } else {
        std::string md5 = ComputeMD5FromFrame();
        if (strcmp(md5.c_str(), golden.c_str())) {
            ALOGE("MD5 mismatched. expect: %s, got: %s", golden.c_str(), md5.c_str());
            return false;
        }
    }
    return true;
}

bool VideoFrame::WriteFrame(std::ofstream* output_file) const {
    if (IsFlexibleFormat()) {
        ALOGE("Cannot write frame with format YUV_420_FLEXIBLE");
        return false;
    }

    output_file->write(reinterpret_cast<const char*>(frame_data_[0].get()),
                       visible_size_.width * visible_size_.height);
    output_file->write(reinterpret_cast<const char*>(frame_data_[1].get()),
                       visible_size_.width * visible_size_.height / 4);
    output_file->write(reinterpret_cast<const char*>(frame_data_[2].get()),
                       visible_size_.width * visible_size_.height / 4);
    return output_file->good();
}

}  // namespace android
