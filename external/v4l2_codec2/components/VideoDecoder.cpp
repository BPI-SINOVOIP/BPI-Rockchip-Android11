// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <v4l2_codec2/components/VideoDecoder.h>

namespace android {

// static
const char* VideoDecoder::DecodeStatusToString(VideoDecoder::DecodeStatus status) {
    switch (status) {
    case VideoDecoder::DecodeStatus::kOk:
        return "OK";
    case VideoDecoder::DecodeStatus::kAborted:
        return "ABORTED";
    case VideoDecoder::DecodeStatus::kError:
        return "ERROR";
    }
}

VideoDecoder::~VideoDecoder() = default;

}  // namespace android
