// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Note: ported from Chromium commit head: 9e40822e3a3d
// Note: only necessary functions are ported.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"

#include "size.h"
#include "video_codecs.h"

namespace media {

// class BitstreamBuffer;
// class VideoFrame;

//  Metadata for a VP8 bitstream buffer.
//  |non_reference| is true iff this frame does not update any reference buffer,
//                  meaning dropping this frame still results in a decodable
//                  stream.
//  |temporal_idx|  indicates the temporal index for this frame.
//  |layer_sync|    if true iff this frame has |temporal_idx| > 0 and does NOT
//                  reference any reference buffer containing a frame with
//                  temporal_idx > 0.
struct Vp8Metadata final {
  Vp8Metadata();
  Vp8Metadata(const Vp8Metadata& other);
  Vp8Metadata(Vp8Metadata&& other);
  ~Vp8Metadata();
  bool non_reference;
  uint8_t temporal_idx;
  bool layer_sync;
};

//  Metadata associated with a bitstream buffer.
//  |payload_size| is the byte size of the used portion of the buffer.
//  |key_frame| is true if this delivered frame is a keyframe.
//  |timestamp| is the same timestamp as in VideoFrame passed to Encode().
//  |vp8|, if set, contains metadata specific to VP8. See above.
struct BitstreamBufferMetadata final {
  BitstreamBufferMetadata();
  BitstreamBufferMetadata(BitstreamBufferMetadata&& other);
  BitstreamBufferMetadata(size_t payload_size_bytes,
                          bool key_frame,
                          base::TimeDelta timestamp);
  ~BitstreamBufferMetadata();
  size_t payload_size_bytes;
  bool key_frame;
  base::TimeDelta timestamp;
  base::Optional<Vp8Metadata> vp8;
};

// Video encoder interface.
class VideoEncodeAccelerator {
 public:
  // Specification of an encoding profile supported by an encoder.
  struct SupportedProfile {
    SupportedProfile();
    SupportedProfile(VideoCodecProfile profile,
                     const Size& max_resolution,
                     uint32_t max_framerate_numerator = 0u,
                     uint32_t max_framerate_denominator = 1u);
    ~SupportedProfile();

    VideoCodecProfile profile;
    Size min_resolution;
    Size max_resolution;
    uint32_t max_framerate_numerator;
    uint32_t max_framerate_denominator;
  };
  using SupportedProfiles = std::vector<SupportedProfile>;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
