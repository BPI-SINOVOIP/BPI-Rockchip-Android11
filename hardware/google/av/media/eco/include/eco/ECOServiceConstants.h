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
#ifndef ANDROID_MEDIA_ECO_SERVICE_CONSTANTS_H_
#define ANDROID_MEDIA_ECO_SERVICE_CONSTANTS_H_

#include <stdint.h>
#include <sys/mman.h>

namespace android {
namespace media {
namespace eco {

// Codec type.
constexpr int32_t CodecTypeUnknown = 0x00;
constexpr int32_t CodecTypeAVC = 0x01;
constexpr int32_t CodecTypeHEVC = 0x02;

// Encoded frame type.
constexpr int32_t FrameTypeUnknown = 0x0;
constexpr int32_t FrameTypeI = 0x01;
constexpr int32_t FrameTypeP = 0x02;
constexpr int32_t FrameTypeB = 0x04;

// Below constants are borrowed from
// frameworks/av/media/libstagefright/include/media/stagefright/MediaCodecConstants.h

// from MediaCodecInfo.java

// Profile types:
constexpr int32_t AVCProfileBaseline = 0x01;
constexpr int32_t AVCProfileMain = 0x02;
constexpr int32_t AVCProfileExtended = 0x04;
constexpr int32_t AVCProfileHigh = 0x08;
constexpr int32_t AVCProfileHigh10 = 0x10;
constexpr int32_t AVCProfileHigh422 = 0x20;
constexpr int32_t AVCProfileHigh444 = 0x40;
constexpr int32_t AVCProfileConstrainedBaseline = 0x10000;
constexpr int32_t AVCProfileConstrainedHigh = 0x80000;

constexpr int32_t HEVCProfileMain = 0x01;
constexpr int32_t HEVCProfileMain10 = 0x02;
constexpr int32_t HEVCProfileMainStill = 0x04;
constexpr int32_t HEVCProfileMain10HDR10 = 0x1000;
constexpr int32_t HEVCProfileMain10HDR10Plus = 0x2000;

// Level types:
constexpr int32_t AVCLevel1 = 0x01;
constexpr int32_t AVCLevel1b = 0x02;
constexpr int32_t AVCLevel11 = 0x04;
constexpr int32_t AVCLevel12 = 0x08;
constexpr int32_t AVCLevel13 = 0x10;
constexpr int32_t AVCLevel2 = 0x20;
constexpr int32_t AVCLevel21 = 0x40;
constexpr int32_t AVCLevel22 = 0x80;
constexpr int32_t AVCLevel3 = 0x100;
constexpr int32_t AVCLevel31 = 0x200;
constexpr int32_t AVCLevel32 = 0x400;
constexpr int32_t AVCLevel4 = 0x800;
constexpr int32_t AVCLevel41 = 0x1000;
constexpr int32_t AVCLevel42 = 0x2000;
constexpr int32_t AVCLevel5 = 0x4000;
constexpr int32_t AVCLevel51 = 0x8000;
constexpr int32_t AVCLevel52 = 0x10000;
constexpr int32_t AVCLevel6 = 0x20000;
constexpr int32_t AVCLevel61 = 0x40000;
constexpr int32_t AVCLevel62 = 0x80000;

constexpr int32_t HEVCMainTierLevel1 = 0x1;
constexpr int32_t HEVCHighTierLevel1 = 0x2;
constexpr int32_t HEVCMainTierLevel2 = 0x4;
constexpr int32_t HEVCHighTierLevel2 = 0x8;
constexpr int32_t HEVCMainTierLevel21 = 0x10;
constexpr int32_t HEVCHighTierLevel21 = 0x20;
constexpr int32_t HEVCMainTierLevel3 = 0x40;
constexpr int32_t HEVCHighTierLevel3 = 0x80;
constexpr int32_t HEVCMainTierLevel31 = 0x100;
constexpr int32_t HEVCHighTierLevel31 = 0x200;
constexpr int32_t HEVCMainTierLevel4 = 0x400;
constexpr int32_t HEVCHighTierLevel4 = 0x800;
constexpr int32_t HEVCMainTierLevel41 = 0x1000;
constexpr int32_t HEVCHighTierLevel41 = 0x2000;
constexpr int32_t HEVCMainTierLevel5 = 0x4000;
constexpr int32_t HEVCHighTierLevel5 = 0x8000;
constexpr int32_t HEVCMainTierLevel51 = 0x10000;
constexpr int32_t HEVCHighTierLevel51 = 0x20000;
constexpr int32_t HEVCMainTierLevel52 = 0x40000;
constexpr int32_t HEVCHighTierLevel52 = 0x80000;
constexpr int32_t HEVCMainTierLevel6 = 0x100000;
constexpr int32_t HEVCHighTierLevel6 = 0x200000;
constexpr int32_t HEVCMainTierLevel61 = 0x400000;
constexpr int32_t HEVCHighTierLevel61 = 0x800000;
constexpr int32_t HEVCMainTierLevel62 = 0x1000000;
constexpr int32_t HEVCHighTierLevel62 = 0x2000000;

inline static const char* asString_AVCProfile(int32_t i, const char* def = "??") {
    switch (i) {
    case AVCProfileBaseline:
        return "Baseline";
    case AVCProfileMain:
        return "Main";
    case AVCProfileExtended:
        return "Extended";
    case AVCProfileHigh:
        return "High";
    case AVCProfileHigh10:
        return "High10";
    case AVCProfileHigh422:
        return "High422";
    case AVCProfileHigh444:
        return "High444";
    case AVCProfileConstrainedBaseline:
        return "ConstrainedBaseline";
    case AVCProfileConstrainedHigh:
        return "ConstrainedHigh";
    default:
        return def;
    }
}

inline static const char* asString_AVCLevel(int32_t i, const char* def = "??") {
    switch (i) {
    case AVCLevel1:
        return "1";
    case AVCLevel1b:
        return "1b";
    case AVCLevel11:
        return "1.1";
    case AVCLevel12:
        return "1.2";
    case AVCLevel13:
        return "1.3";
    case AVCLevel2:
        return "2";
    case AVCLevel21:
        return "2.1";
    case AVCLevel22:
        return "2.2";
    case AVCLevel3:
        return "3";
    case AVCLevel31:
        return "3.1";
    case AVCLevel32:
        return "3.2";
    case AVCLevel4:
        return "4";
    case AVCLevel41:
        return "4.1";
    case AVCLevel42:
        return "4.2";
    case AVCLevel5:
        return "5";
    case AVCLevel51:
        return "5.1";
    case AVCLevel52:
        return "5.2";
    case AVCLevel6:
        return "6";
    case AVCLevel61:
        return "6.1";
    case AVCLevel62:
        return "6.2";
    default:
        return def;
    }
}

inline static const char* asString_HEVCProfile(int32_t i, const char* def = "??") {
    switch (i) {
    case HEVCProfileMain:
        return "Main";
    case HEVCProfileMain10:
        return "Main10";
    case HEVCProfileMainStill:
        return "MainStill";
    case HEVCProfileMain10HDR10:
        return "Main10HDR10";
    case HEVCProfileMain10HDR10Plus:
        return "Main10HDR10Plus";
    default:
        return def;
    }
}

inline static const char* asString_HEVCTierLevel(int32_t i, const char* def = "??") {
    switch (i) {
    case HEVCMainTierLevel1:
        return "Main 1";
    case HEVCHighTierLevel1:
        return "High 1";
    case HEVCMainTierLevel2:
        return "Main 2";
    case HEVCHighTierLevel2:
        return "High 2";
    case HEVCMainTierLevel21:
        return "Main 2.1";
    case HEVCHighTierLevel21:
        return "High 2.1";
    case HEVCMainTierLevel3:
        return "Main 3";
    case HEVCHighTierLevel3:
        return "High 3";
    case HEVCMainTierLevel31:
        return "Main 3.1";
    case HEVCHighTierLevel31:
        return "High 3.1";
    case HEVCMainTierLevel4:
        return "Main 4";
    case HEVCHighTierLevel4:
        return "High 4";
    case HEVCMainTierLevel41:
        return "Main 4.1";
    case HEVCHighTierLevel41:
        return "High 4.1";
    case HEVCMainTierLevel5:
        return "Main 5";
    case HEVCHighTierLevel5:
        return "High 5";
    case HEVCMainTierLevel51:
        return "Main 5.1";
    case HEVCHighTierLevel51:
        return "High 5.1";
    case HEVCMainTierLevel52:
        return "Main 5.2";
    case HEVCHighTierLevel52:
        return "High 5.2";
    case HEVCMainTierLevel6:
        return "Main 6";
    case HEVCHighTierLevel6:
        return "High 6";
    case HEVCMainTierLevel61:
        return "Main 6.1";
    case HEVCHighTierLevel61:
        return "High 6.1";
    case HEVCMainTierLevel62:
        return "Main 6.2";
    case HEVCHighTierLevel62:
        return "High 6.2";
    default:
        return def;
    }
}

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_SERVICE_CONSTANTS_H_
