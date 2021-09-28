// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #define LOG_NDEBUG 0
#define LOG_TAG "EncodedDataHelper"

#include "encoded_data_helper.h"

#include <assert.h>
#include <string.h>

#include <utility>

#include <utils/Log.h>

namespace android {

namespace {

bool IsAnnexb3ByteStartCode(const std::string& data, size_t pos) {
    // The Annex-B 3-byte start code "\0\0\1" will be prefixed by NALUs per AU
    // except for the first one.
    return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 1;
}

bool IsAnnexb4ByteStartCode(const std::string& data, size_t pos) {
    // The Annex-B 4-byte start code "\0\0\0\1" will be prefixed by first NALU per
    // AU.
    return data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 0 && data[pos + 3] == 1;
}

// Get the next position of NALU header byte in |data| from |next_header_pos|,
// and update to |next_header_pos|. Return true if there is one; false
// otherwise.
// Note: this function should be used within an AU.
bool GetPosForNextNALUHeader(const std::string& data, size_t* next_header_pos) {
    size_t pos = *next_header_pos;

    // Annex-B 4-byte could be also found by IsAnnexb3ByteStartCode().
    while (pos + 3 <= data.size() && !IsAnnexb3ByteStartCode(data, pos)) {
        ++pos;
    }
    if (pos + 3 >= data.size()) return false;  // No more NALUs

    // NALU header is the first byte after Annex-B start code.
    *next_header_pos = pos + 3;
    return true;
}

// For H264, return data bytes of next AU fragment in |data| from |next_pos|,
// and update the position to |next_pos|.
std::string GetBytesForNextAU(const std::string& data, size_t* next_pos) {
    // Helpful description:
    // https://en.wikipedia.org/wiki/Network_Abstraction_Layer
    size_t start_pos = *next_pos;
    size_t pos = start_pos;
    if (pos + 4 > data.size()) {
        ALOGE("Invalid AU: Start code is less than 4 bytes.\n");
        *next_pos = data.size();
        return std::string();
    }

    assert(IsAnnexb4ByteStartCode(data, pos));

    pos += 4;
    // The first 4 bytes must be Annex-B 4-byte start code for an AU.
    while (pos + 4 <= data.size() && !IsAnnexb4ByteStartCode(data, pos)) {
        ++pos;
    }
    if (pos + 3 >= data.size()) pos = data.size();

    // Update next_pos.
    *next_pos = pos;
    return data.substr(start_pos, pos - start_pos);
}

// For VP8/9, return data bytes of next frame in |data| from |next_pos|, and
// update the position to |next_pos|.
std::string GetBytesForNextFrame(const std::string& data, size_t* next_pos) {
    // Helpful description: http://wiki.multimedia.cx/index.php?title=IVF
    size_t pos = *next_pos;
    std::string bytes;
    if (pos == 0) pos = 32;  // Skip IVF header.

    const uint32_t frame_size = *reinterpret_cast<const uint32_t*>(&data[pos]);
    pos += 12;  // Skip frame header.

    // Update next_pos.
    *next_pos = pos + frame_size;
    return data.substr(pos, frame_size);
}

}  // namespace

EncodedDataHelper::EncodedDataHelper(const std::string& file_path, VideoCodecType type)
      : type_(type) {
    InputFileStream input(file_path);
    if (!input.IsValid()) {
        ALOGE("Failed to open file: %s", file_path.c_str());
        return;
    }

    int file_size = input.GetLength();
    if (file_size <= 0) {
        ALOGE("Stream byte size (=%d) is invalid", file_size);
        return;
    }
    input.Rewind();

    char* read_bytes = new char[file_size];
    if (input.Read(read_bytes, file_size) != file_size) {
        ALOGE("Failed to read input stream from file to buffer.");
        return;
    }

    // Note: must assign |file_size| here otherwise the constructor will terminate
    // copting at the first '\0' in |read_bytes|.
    std::string data(read_bytes, file_size);
    delete[] read_bytes;

    SliceToFragments(data);
}

EncodedDataHelper::~EncodedDataHelper() {}

const EncodedDataHelper::Fragment* const EncodedDataHelper::GetNextFragment() {
    if (ReachEndOfStream()) return nullptr;
    return next_fragment_iter_++->get();
}

bool EncodedDataHelper::AtHeadOfStream() const {
    return next_fragment_iter_ == fragments_.begin();
}

bool EncodedDataHelper::ReachEndOfStream() const {
    return next_fragment_iter_ == fragments_.end();
}

void EncodedDataHelper::SliceToFragments(const std::string& data) {
    size_t next_pos = 0;
    bool seen_csd = false;
    while (next_pos < data.size()) {
        std::unique_ptr<Fragment> fragment(new Fragment());
        switch (type_) {
        case VideoCodecType::H264:
            fragment->data = GetBytesForNextAU(data, &next_pos);
            if (!ParseAUFragmentType(fragment.get())) continue;
            if (!seen_csd && !fragment->csd_flag)
                // Skip all AUs beforehand until we get SPS NALU.
                continue;
            seen_csd = true;
            break;
        case VideoCodecType::VP8:
        case VideoCodecType::VP9:
            fragment->data = GetBytesForNextFrame(data, &next_pos);
            break;
        default:
            ALOGE("Unknown video codec type.");
            return;
        }
        fragments_.push_back(std::move(fragment));
    }

    ALOGD("Total %zu fragments in interest from input stream.", NumValidFragments());
    next_fragment_iter_ = fragments_.begin();
}

bool EncodedDataHelper::ParseAUFragmentType(Fragment* fragment) {
    size_t next_header_pos = 0;
    while (GetPosForNextNALUHeader(fragment->data, &next_header_pos)) {
        // Read the NALU header (first byte) which contains unit type.
        uint8_t nalu_header = static_cast<uint8_t>(fragment->data[next_header_pos]);

        // Check forbidden_zero_bit (MSB of NALU header) is 0;
        if (nalu_header & 0x80) {
            ALOGE("NALU header forbidden_zero_bit is 1.");
            return false;
        }

        // Check NALU type ([3:7], 5-bit).
        uint8_t nalu_type = nalu_header & 0x1f;
        switch (nalu_type) {
        case NON_IDR_SLICE:
        case IDR_SLICE:
            // If AU contains both CSD and VCL NALUs (e.g. PPS + IDR_SLICE), don't
            // raise csd_flag, treat this fragment as VCL one.
            fragment->csd_flag = false;
            return true;  // fragment in interest as VCL.
        case SPS:
        case PPS:
            fragment->csd_flag = true;
            // Continue on finding the subsequent NALUs, it may have VCL data.
            break;
        default:
            // Skip uninterested NALU type.
            break;
        }
    }
    return fragment->csd_flag;  // fragment in interest as CSD.
}

}  // namespace android
