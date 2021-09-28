/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Helper classes and functions to be used by provider and listener.
#ifndef ANDROID_MEDIA_ECO_UTILS_H_
#define ANDROID_MEDIA_ECO_UTILS_H_

#include <cutils/atomic.h>
#include <utils/Errors.h>

#include "ECOData.h"
#include "ECODataKey.h"
#include "ECOServiceConstants.h"

namespace android {
namespace media {
namespace eco {

#define RETURN_STATUS_IF_ERROR(expr)  \
    {                                 \
        status_t _errorCode = (expr); \
        if (_errorCode != NO_ERROR) { \
            return _errorCode;        \
        }                             \
    }

// Helper structure to hold encoder config. This is used by EncoderProvider to provide stats to
// ECOService or by ECOService to provide info to the listener. Providers could implement their
// own more complex encoder config as long as ECOService supports parsing them.
struct SimpleEncoderConfig {
    // Encoder name as defined in device/google/[device]/media_codecs.xml.
    std::string mEncoderName;

    // Codec Type as defined in ECOServiceConstants.h. -1 means unavailable.
    int32_t mCodecType;

    // Codec profile as defined in ECOServiceConstants.h. -1 means unavailable.
    int32_t mProfile;

    // Codec level as defined in ECOServiceConstants.h. -1 means unavailable.
    int32_t mLevel;

    // Target bitrate in bits per second. -1 means unavailable.
    int32_t mTargetBitrate;

    // Key frame interval in frames. -1 means unavailable.
    int32_t mKeyFrameIntervalFrames;

    // Frame rate in frames per second. -1 means unavailable.
    float mFrameRateFps;

    SimpleEncoderConfig()
          : mEncoderName(""),
            mCodecType(-1),
            mProfile(-1),
            mLevel(-1),
            mTargetBitrate(-1),
            mKeyFrameIntervalFrames(-1),
            mFrameRateFps(-1) {}

    SimpleEncoderConfig(const std::string& name, int32_t codecType, int32_t profile, int32_t level,
                        int32_t bitrate, int32_t kfi, float framerateFps)
          : mEncoderName(name),
            mCodecType(codecType),
            mProfile(profile),
            mLevel(level),
            mTargetBitrate(bitrate),
            mKeyFrameIntervalFrames(kfi),
            mFrameRateFps(framerateFps) {}

    // Convert this SimpleEncoderConfig to ECOData with dataType.
    ECOData toEcoData(ECOData::ECODatatype dataType);
};

// Helper structure for
struct SimpleEncodedFrameData {
    // Frame sequence number starts from 0.
    int32_t mFrameNum;

    // Frame type as defined in ECOServiceConstants.h. -1 means unavailable.
    int8_t mFrameType;

    // Frame presentation timestamp in us. -1 means unavailable.
    int64_t mFramePtsUs;

    // Avg quantization parameter of the frame. -1 means unavailable.
    int32_t mAvgQp;

    // Frame size in bytes. -1 means unavailable.
    int32_t mFrameSizeBytes;

    SimpleEncodedFrameData()
          : mFrameNum(-1),
            mFrameType(FrameTypeUnknown),
            mFramePtsUs(-1),
            mAvgQp(-1),
            mFrameSizeBytes(-1) {}

    SimpleEncodedFrameData(int32_t frameNum, int8_t frameType, int64_t ptsUs, int32_t qp,
                           int32_t sizeBytes)
          : mFrameNum(frameNum),
            mFrameType(frameType),
            mFramePtsUs(ptsUs),
            mAvgQp(qp),
            mFrameSizeBytes(sizeBytes) {}

    // Convert this SimpleEncoderConfig to ECOData with dataType.
    ECOData toEcoData(ECOData::ECODatatype dataType);
};

bool copyKeyValue(const ECOData& src, ECOData* dst);

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_UTILS_H_
