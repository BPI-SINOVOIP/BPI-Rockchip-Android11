// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMMON_V4L2_COMPONENT_COMMON_H
#define ANDROID_V4L2_CODEC2_COMMON_V4L2_COMPONENT_COMMON_H

#include <string>

namespace android {

// Defines the names of all supported components.
struct V4L2ComponentName {
    static const std::string kH264Encoder;

    static const std::string kH264Decoder;
    static const std::string kVP8Decoder;
    static const std::string kVP9Decoder;
    static const std::string kH264SecureDecoder;
    static const std::string kVP8SecureDecoder;
    static const std::string kVP9SecureDecoder;

    // Return true if |name| is a valid component name.
    static bool isValid(const char* name);

    // Return true if |name| is a encoder name.
    // Note that |name| should be a valid component name.
    static bool isEncoder(const char* name);
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMMON_V4L2_COMPONENT_COMMON_H
