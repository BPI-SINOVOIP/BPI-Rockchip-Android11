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
#ifndef ANDROID_MEDIA_ECO_DATA_KEY_H_
#define ANDROID_MEDIA_ECO_DATA_KEY_H_

#include <android-base/unique_fd.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <stdint.h>
#include <sys/mman.h>

namespace android {
namespace media {
namespace eco {

// ================================================================================================
// Standard ECOData keys.
// ================================================================================================
constexpr char KEY_ECO_DATA_TYPE[] = "eco-data-type";
constexpr char KEY_ECO_DATA_TIME_US[] = "eco-data-time-us";

// ================================================================================================
// Standard ECOServiceStatsProvider config keys. These keys are used in the ECOData as an config
// when StatsProvider connects with ECOService.
// ================================================================================================
constexpr char KEY_PROVIDER_NAME[] = "provider-name";
constexpr char KEY_PROVIDER_TYPE[] = "provider-type";

// ================================================================================================
// Standard ECOServiceInfoListener config keys. These keys are used in the ECOData as config
// when ECOServiceInfoListener connects with ECOService to specify the informations that the
// listener wants to listen to.
// ================================================================================================
constexpr char KEY_LISTENER_NAME[] = "listener-name";
constexpr char KEY_LISTENER_TYPE[] = "listener-type";

// Following two keys are used together for the listener to specify the condition when it wants to
// receive notification. When a frame's avg-qp crosses KEY_LISTENER_QP_BLOCKINESS_THRESHOLD or
// the detla of qp between current frame and previous frame also goes beyond
// KEY_LISTENER_QP_CHANGE_THRESHOLD, ECOService will notify the listener.
constexpr char KEY_LISTENER_QP_BLOCKINESS_THRESHOLD[] = "listener-qp-blockness-threshold";
constexpr char KEY_LISTENER_QP_CHANGE_THRESHOLD[] = "listener-qp-change-threshold";

// ================================================================================================
// ECOService Stats keys. These key MUST BE specified when provider pushes the stats to ECOService
// to indicate the stats is session stats or frame stats.
// ================================================================================================
constexpr char KEY_STATS_TYPE[] = "stats-type";
constexpr char VALUE_STATS_TYPE_SESSION[] = "stats-type-session";  // value for KEY_STATS_TYPE.
constexpr char VALUE_STATS_TYPE_FRAME[] = "stats-type-frame";      // value for KEY_STATS_TYPE.

// ================================================================================================
// Standard ECOService Info keys. These key will be in the info provided by ECOService to indicate
// the info is session info or frame info.
// ================================================================================================
constexpr char KEY_INFO_TYPE[] = "info-type";
constexpr char VALUE_INFO_TYPE_SESSION[] = "info-type-session";  // value for KEY_INFO_TYPE.
constexpr char VALUE_INFO_TYPE_FRAME[] = "info-type-frame";      // value for KEY_INFO_TYPE.

// ================================================================================================
// General keys to be used by both stats and info in the ECOData.
// ================================================================================================
constexpr char ENCODER_NAME[] = "encoder-name";
constexpr char ENCODER_TYPE[] = "encoder-type";
constexpr char ENCODER_PROFILE[] = "encoder-profile";
constexpr char ENCODER_LEVEL[] = "encoder-level";
constexpr char ENCODER_INPUT_WIDTH[] = "encoder-input-width";
constexpr char ENCODER_INPUT_HEIGHT[] = "encoder-input-height";
constexpr char ENCODER_OUTPUT_WIDTH[] = "encoder-output-width";
constexpr char ENCODER_OUTPUT_HEIGHT[] = "encoder-output-height";
constexpr char ENCODER_TARGET_BITRATE_BPS[] = "encoder-target-bitrate-bps";  // Session info
constexpr char ENCODER_ACTUAL_BITRATE_BPS[] = "encoder-actual-bitrate-bps";  // Frame info
constexpr char ENCODER_KFI_FRAMES[] = "encoder-kfi-frames";
constexpr char ENCODER_FRAMERATE_FPS[] = "encoder-framerate-fps";

constexpr char FRAME_NUM[] = "frame-num";
constexpr char FRAME_PTS_US[] = "frame-pts-us";
constexpr char FRAME_AVG_QP[] = "frame-avg-qp";
constexpr char FRAME_TYPE[] = "frame-type";
constexpr char FRAME_SIZE_BYTES[] = "frame-size-bytes";

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_DATA_KEY_H_
