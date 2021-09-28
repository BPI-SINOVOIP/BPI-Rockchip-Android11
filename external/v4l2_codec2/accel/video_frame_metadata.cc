// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 19cd1babcaff
// Note: only functions related to END_OF_STREAM are ported.

#include "video_frame_metadata.h"

#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace media {

namespace {

// Map enum key to internal std::string key used by base::DictionaryValue.
inline std::string ToInternalKey(VideoFrameMetadata::Key key) {
  DCHECK_LT(key, VideoFrameMetadata::NUM_KEYS);
  return base::NumberToString(static_cast<int>(key));
}

}  // namespace

VideoFrameMetadata::VideoFrameMetadata() = default;

VideoFrameMetadata::~VideoFrameMetadata() = default;

bool VideoFrameMetadata::HasKey(Key key) const {
  return dictionary_.HasKey(ToInternalKey(key));
}

void VideoFrameMetadata::SetBoolean(Key key, bool value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

bool VideoFrameMetadata::GetBoolean(Key key, bool* value) const {
  DCHECK(value);
  return dictionary_.GetBooleanWithoutPathExpansion(ToInternalKey(key), value);
}

bool VideoFrameMetadata::IsTrue(Key key) const {
  bool value = false;
  return GetBoolean(key, &value) && value;
}

}  // namespace media
