// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "V4L2ComponentCommon"

#include <v4l2_codec2/common/V4L2ComponentCommon.h>

#include <log/log.h>

namespace android {

const std::string V4L2ComponentName::kH264Encoder = "c2.v4l2.avc.encoder";

const std::string V4L2ComponentName::kH264Decoder = "c2.v4l2.avc.decoder";
const std::string V4L2ComponentName::kVP8Decoder = "c2.v4l2.vp8.decoder";
const std::string V4L2ComponentName::kVP9Decoder = "c2.v4l2.vp9.decoder";
const std::string V4L2ComponentName::kH264SecureDecoder = "c2.v4l2.avc.decoder.secure";
const std::string V4L2ComponentName::kVP8SecureDecoder = "c2.v4l2.vp8.decoder.secure";
const std::string V4L2ComponentName::kVP9SecureDecoder = "c2.v4l2.vp9.decoder.secure";

// static
bool V4L2ComponentName::isValid(const char* name) {
    return name == kH264Encoder || name == kH264Decoder || name == kVP8Decoder ||
           name == kVP9Decoder || name == kH264SecureDecoder || name == kVP8SecureDecoder ||
           name == kVP9SecureDecoder;
}

// static
bool V4L2ComponentName::isEncoder(const char* name) {
    ALOG_ASSERT(isValid(name));

    return name == kH264Encoder;
}

}  // namespace android
