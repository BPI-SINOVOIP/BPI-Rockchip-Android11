// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef C2_E2E_TEST_ENCODED_DATA_HELPER_H_
#define C2_E2E_TEST_ENCODED_DATA_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "common.h"

namespace android {

// Helper class for MediaCodecDecoder to read encoded stream from input file,
// and slice it into fragments. MediaCodecDecoder could call GetNextFragment()
// to obtain fragment data sequentially.
class EncodedDataHelper {
public:
    EncodedDataHelper(const std::string& file_path, VideoCodecType type);
    ~EncodedDataHelper();

    // A fragment will contain the bytes of one AU (H264) or frame (VP8/9) in
    // |data|, and |csd_flag| indicator for input buffer flag CODEC_CONFIG.
    struct Fragment {
        std::string data;
        bool csd_flag = false;
    };

    // Return the next fragment to be sent to the decoder, and advance the
    // iterator to after the returned fragment.
    const Fragment* const GetNextFragment();

    void Rewind() { next_fragment_iter_ = fragments_.begin(); }
    bool IsValid() const { return !fragments_.empty(); }
    size_t NumValidFragments() const { return fragments_.size(); }
    bool AtHeadOfStream() const;
    bool ReachEndOfStream() const;

private:
    // NALU type enumeration as defined in H264 Annex-B. Only interested ones are
    // listed here.
    enum NALUType : uint8_t {
        NON_IDR_SLICE = 0x1,
        IDR_SLICE = 0x5,
        SPS = 0x7,
        PPS = 0x8,
    };

    // Slice input stream into fragments. This should be done in constructor.
    void SliceToFragments(const std::string& data);

    // For H264, parse csd_flag from |fragment| data and store inside. Return true
    // if this fragment is in interest; false otherwise (fragment will be
    // discarded.)
    bool ParseAUFragmentType(Fragment* fragment);

    VideoCodecType type_;
    std::vector<std::unique_ptr<Fragment>> fragments_;
    std::vector<std::unique_ptr<Fragment>>::iterator next_fragment_iter_;
};

}  // namespace android

#endif  // C2_E2E_TEST_ENCODED_DATA_HELPER_H_
