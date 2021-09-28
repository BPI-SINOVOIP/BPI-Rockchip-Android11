// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 9e40822e3a3d
// Note: only necessary functions are ported.

#include "video_encode_accelerator.h"

namespace media {

Vp8Metadata::Vp8Metadata()
    : non_reference(false), temporal_idx(0), layer_sync(false) {}
Vp8Metadata::Vp8Metadata(const Vp8Metadata& other) = default;
Vp8Metadata::Vp8Metadata(Vp8Metadata&& other) = default;
Vp8Metadata::~Vp8Metadata() = default;

BitstreamBufferMetadata::BitstreamBufferMetadata()
    : payload_size_bytes(0), key_frame(false) {}
BitstreamBufferMetadata::BitstreamBufferMetadata(
    BitstreamBufferMetadata&& other) = default;
BitstreamBufferMetadata::BitstreamBufferMetadata(size_t payload_size_bytes,
                                                 bool key_frame,
                                                 base::TimeDelta timestamp)
    : payload_size_bytes(payload_size_bytes),
      key_frame(key_frame),
      timestamp(timestamp) {}
BitstreamBufferMetadata::~BitstreamBufferMetadata() = default;

VideoEncodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN),
      max_framerate_numerator(0),
      max_framerate_denominator(0) {}

VideoEncodeAccelerator::SupportedProfile::SupportedProfile(
    VideoCodecProfile profile,
    const Size& max_resolution,
    uint32_t max_framerate_numerator,
    uint32_t max_framerate_denominator)
    : profile(profile),
      max_resolution(max_resolution),
      max_framerate_numerator(max_framerate_numerator),
      max_framerate_denominator(max_framerate_denominator) {}

VideoEncodeAccelerator::SupportedProfile::~SupportedProfile() = default;

}  // namespace media
