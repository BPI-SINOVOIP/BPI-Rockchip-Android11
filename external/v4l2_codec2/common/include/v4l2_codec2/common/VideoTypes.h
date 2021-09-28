// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_TYPES_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_TYPES_H

#include <optional>
#include <string>

#include <android/hardware/graphics/common/1.0/types.h>

namespace android {

// Enumeration of supported video codecs.
enum class VideoCodec {
    H264,
    VP8,
    VP9,
};
const char* VideoCodecToString(VideoCodec codec);

// Enumeration of supported pixel format. The value should be the same as
// ::android::hardware::graphics::common::V1_0::PixelFormat.
using HPixelFormat = ::android::hardware::graphics::common::V1_0::PixelFormat;
enum class HalPixelFormat : int32_t {
    UNKNOWN = 0x0,
    YCBCR_420_888 = static_cast<int32_t>(HPixelFormat::YCBCR_420_888),
    YV12 = static_cast<int32_t>(HPixelFormat::YV12),
    // NV12 is not defined at PixelFormat, follow the convention to use fourcc value.
    NV12 = 0x3231564e,
};
const char* HalPixelFormatToString(HalPixelFormat format);

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_TYPES_H
