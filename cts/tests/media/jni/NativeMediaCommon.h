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

#ifndef MEDIACTSNATIVE_NATIVE_MEDIA_COMMON_H
#define MEDIACTSNATIVE_NATIVE_MEDIA_COMMON_H

#include <NdkMediaFormat.h>

extern const char* AMEDIA_MIMETYPE_VIDEO_VP8;
extern const char* AMEDIA_MIMETYPE_VIDEO_VP9;
extern const char* AMEDIA_MIMETYPE_VIDEO_AV1;
extern const char* AMEDIA_MIMETYPE_VIDEO_AVC;
extern const char* AMEDIA_MIMETYPE_VIDEO_HEVC;
extern const char* AMEDIA_MIMETYPE_VIDEO_MPEG4;
extern const char* AMEDIA_MIMETYPE_VIDEO_H263;

extern const char* AMEDIA_MIMETYPE_AUDIO_AMR_NB;
extern const char* AMEDIA_MIMETYPE_AUDIO_AMR_WB;
extern const char* AMEDIA_MIMETYPE_AUDIO_AAC;
extern const char* AMEDIA_MIMETYPE_AUDIO_VORBIS;
extern const char* AMEDIA_MIMETYPE_AUDIO_OPUS;

// TODO(b/146420990)
typedef enum {
    OUTPUT_FORMAT_START = 0,
    OUTPUT_FORMAT_MPEG_4 = OUTPUT_FORMAT_START,
    OUTPUT_FORMAT_WEBM = OUTPUT_FORMAT_START + 1,
    OUTPUT_FORMAT_THREE_GPP = OUTPUT_FORMAT_START + 2,
    OUTPUT_FORMAT_HEIF = OUTPUT_FORMAT_START + 3,
    OUTPUT_FORMAT_OGG = OUTPUT_FORMAT_START + 4,
    OUTPUT_FORMAT_LIST_END = OUTPUT_FORMAT_START + 4,
} MuxerFormat;

// Color formats supported by encoder - should mirror supportedColorList
// from MediaCodecConstants.h (are these going to be deprecated)
constexpr int COLOR_FormatYUV420SemiPlanar = 21;
constexpr int COLOR_FormatYUV420Flexible = 0x7F420888;
constexpr int COLOR_FormatSurface = 0x7f000789;

// constants not defined in NDK
extern const char* TBD_AMEDIACODEC_PARAMETER_KEY_REQUEST_SYNC_FRAME;
extern const char* TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE;
extern const char* TBD_AMEDIACODEC_PARAMETER_KEY_MAX_B_FRAMES;
extern const char* TBD_AMEDIAFORMAT_KEY_BIT_RATE_MODE;
static const int TBD_AMEDIACODEC_BUFFER_FLAG_KEY_FRAME = 0x1;

static const int kBitrateModeConstant = 2;

// common utility functions
bool isCSDIdentical(AMediaFormat* refFormat, AMediaFormat* testFormat);

#endif  // MEDIACTSNATIVE_NATIVE_MEDIA_COMMON_H
