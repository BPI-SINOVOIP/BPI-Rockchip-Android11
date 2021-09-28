// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef C2_E2E_TEST_VIDEO_FRAME_H_
#define C2_E2E_TEST_VIDEO_FRAME_H_

#include <fstream>
#include <memory>
#include <string>

#include "common.h"
#include "md5.h"

namespace android {

// The helper class to convert video frame data to I420 format and make a copy
// of planes, cropped within the visible window.
class VideoFrame {
public:
    // Pre-check the validity of input parameters.
    static std::unique_ptr<VideoFrame> Create(const uint8_t* data, size_t data_size,
                                              const Size& coded_size, const Size& visible_size,
                                              int32_t color_format);
    VideoFrame() = delete;
    ~VideoFrame() = default;

    enum {
        // Android color format similar to I420.
        YUV_420_PLANAR = 0x13,
        // Android color format which is flexible. For Chrome OS devices, it may be
        // either YV12 or NV12 as HAL pixel format.
        // Note: This format is not able to parse, client is required to call
        //       MatchHalFormatByGoldenMD5() first to identify the corresponding HAL
        //       pixel format.
        YUV_420_FLEXIBLE = 0x7f420888,

        // NV12: semiplanar = true, crcb_swap = false.
        HAL_PIXEL_FORMAT_NV12 = 0x3231564e,
        // YV12: semiplanar = false, crcb_swap = true.
        HAL_PIXEL_FORMAT_YV12 = 0x32315659,
    };

    // Verify the calculated MD5 of video frame by comparing to |golden|.
    // It will call MatchHalFormatByGoldenMD5() to find corresponding HAL format
    // if current color format is YUV_420_FLEXIBLE
    bool VerifyMD5(const std::string& golden);

    // Write video frame planes to |output_file| as I420 format.
    bool WriteFrame(std::ofstream* output_file) const;

    int32_t color_format() const { return color_format_; }

private:
    VideoFrame(const uint8_t* data, const Size& coded_size, const Size& visible_size,
               int32_t color_format);

    // Convert frame data from source |data_| as format |curr_format| to
    // destination |frame_data_| as I420 and make a copy, with |visible_size_| as
    // crop window.
    void CopyAndConvertToI420Frame(int32_t curr_format);

    // Try to match corresponding HAL pixel format by comparing to |golden| MD5.
    // Return true on found and overwrite |color_format_| to HAL format.
    bool MatchHalFormatByGoldenMD5(const std::string& golden);

    // Compute and return MD5 for video frame planes as I420 format.
    std::string ComputeMD5FromFrame() const;

    // Return True if current color format is YUV_420_FLEXIBLE.
    bool IsFlexibleFormat() const;

    // The frame data returned by decoder.
    const uint8_t* data_;
    // The specified coded size from output format.
    Size coded_size_;
    // The specified visible size from output format.
    Size visible_size_;
    // The specified color format from output format. It may be overwritten by
    // MatchHalFormatByGoldenMD5().
    int32_t color_format_;

    // Converted frame data stored by planes. [0]:Y, [1]:U, [2]:V
    std::unique_ptr<uint8_t[]> frame_data_[3];
};

}  // namespace android

#endif  // C2_E2E_TEST_VIDEO_FRAME_H_
