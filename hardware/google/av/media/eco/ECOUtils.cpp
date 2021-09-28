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

//#define LOG_NDEBUG 0
#define LOG_TAG "ECOUtils"

#include "eco/ECOUtils.h"

#include <utils/Timers.h>

namespace android {
namespace media {
namespace eco {

// Convert this SimpleEncoderConfig to ECOData with dataType.
ECOData SimpleEncoderConfig::toEcoData(ECOData::ECODatatype dataType) {
    ECOData data(dataType, systemTime(SYSTEM_TIME_BOOTTIME));
    data.setString(KEY_STATS_TYPE, VALUE_STATS_TYPE_SESSION);
    data.setInt32(ENCODER_TYPE, mCodecType);
    data.setInt32(ENCODER_PROFILE, mProfile);
    data.setInt32(ENCODER_LEVEL, mLevel);
    data.setInt32(ENCODER_TARGET_BITRATE_BPS, mTargetBitrate);
    data.setInt32(ENCODER_KFI_FRAMES, mKeyFrameIntervalFrames);
    data.setFloat(ENCODER_FRAMERATE_FPS, mFrameRateFps);
    return data;
}

// Convert this SimpleEncodedFrameData to ECOData with dataType.
ECOData SimpleEncodedFrameData::toEcoData(ECOData::ECODatatype dataType) {
    ECOData data(dataType, systemTime(SYSTEM_TIME_BOOTTIME));
    data.setString(KEY_STATS_TYPE, VALUE_STATS_TYPE_FRAME);
    data.setInt32(FRAME_NUM, mFrameNum);
    data.setInt8(FRAME_TYPE, mFrameType);
    data.setInt64(FRAME_PTS_US, mFramePtsUs);
    data.setInt32(FRAME_AVG_QP, mAvgQp);
    data.setInt32(FRAME_SIZE_BYTES, mFrameSizeBytes);
    return data;
}

bool copyKeyValue(const ECOData& src, ECOData* dst) {
    if (src.isEmpty() || dst == nullptr) return false;
    dst->mKeyValueStore = src.mKeyValueStore;
    return true;
}

}  // namespace eco
}  // namespace media
}  // namespace android