// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMMON_HELPERS_H
#define ANDROID_V4L2_CODEC2_COMMON_HELPERS_H

#include <C2Config.h>
#include <C2ParamDef.h>
#include <system/graphics.h>

#include <size.h>
#include <video_codecs.h>
#include <video_pixel_format.h>

namespace android {

// The encoder parameter set.
//  |mInputFormat| is the pixel format of the input frames.
//  |mInputVisibleSize| is the resolution of the input frames.
//  |mOutputProfile| is the codec profile of the encoded output stream.
//  |mInitialBitrate| is the initial bitrate of the encoded output stream, in bits per second.
//  |mInitialFramerate| is the initial requested framerate.
//  |mH264OutputLevel| is H264 level of encoded output stream.
//  |mStorageType| is the storage type of video frame provided on encode().
struct VideoEncoderAcceleratorConfig {
    enum VideoFrameStorageType {
        SHMEM = 0,
        DMABUF = 1,
    };

    media::VideoPixelFormat mInputFormat;
    media::Size mInputVisibleSize;
    media::VideoCodecProfile mOutputProfile;
    uint32_t mInitialBitrate;
    uint32_t mInitialFramerate;
    uint8_t mH264OutputLevel;
    VideoFrameStorageType mStorageType;
};

// Convert the specified C2Config profile to a media::VideoCodecProfile.
media::VideoCodecProfile c2ProfileToVideoCodecProfile(C2Config::profile_t profile);

// Convert the specified C2Config level to an integer value.
uint8_t c2LevelToLevelIDC(C2Config::level_t level);

// Get the specified graphics block in YCbCr format.
android_ycbcr getGraphicBlockInfo(const C2ConstGraphicBlock& block);

// When encoding a video the codec-specific data (CSD; e.g. SPS and PPS for H264 encoding) will be
// concatenated to the first encoded slice. This function extracts the CSD out of the bitstream and
// stores it into |csd|.
void extractCSDInfo(std::unique_ptr<C2StreamInitDataInfo::output>* const csd, const uint8_t* data,
                    size_t length);

// Helper class to parse H264 NAL units from data.
class NalParser {
public:
    NalParser(const uint8_t* data, size_t length);

    // Locates the next NAL after |mNextNalStartCodePos|. If there is one, updates |mCurrNalDataPos|
    // to the first byte of the NAL data (start code is not included), and |mNextNalStartCodePos| to
    // the position of the next start code, and returns true.
    // If there is no more NAL, returns false.
    //
    // Note: This method must be called prior to data() and length().
    bool locateNextNal();

    // Gets current NAL data (start code is not included).
    const uint8_t* data() const;

    // Gets the byte length of current NAL data (start code is not included).
    size_t length() const;

private:
    const uint8_t* findNextStartCodePos() const;

    // The byte pattern for the start of a H264 NAL unit.
    const uint8_t kNalStartCode[3] = {0x00, 0x00, 0x01};
    // The length in bytes of the NAL-unit start pattern.
    const size_t kNalStartCodeLength = 3;

    const uint8_t* mCurrNalDataPos;
    const uint8_t* mDataEnd;
    const uint8_t* mNextNalStartCodePos;
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMMON_HELPERS_H
