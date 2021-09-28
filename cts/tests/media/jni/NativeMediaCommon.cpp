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
#define LOG_TAG "NativeMediaCommon"
#include <log/log.h>

#include <cstdio>
#include <cstring>
#include <utility>

#include "NativeMediaCommon.h"
/* TODO(b/153592281)
 * Note: constants used by the native media tests but not available in media ndk api
 */
const char* AMEDIA_MIMETYPE_VIDEO_VP8 = "video/x-vnd.on2.vp8";
const char* AMEDIA_MIMETYPE_VIDEO_VP9 = "video/x-vnd.on2.vp9";
const char* AMEDIA_MIMETYPE_VIDEO_AV1 = "video/av01";
const char* AMEDIA_MIMETYPE_VIDEO_AVC = "video/avc";
const char* AMEDIA_MIMETYPE_VIDEO_HEVC = "video/hevc";
const char* AMEDIA_MIMETYPE_VIDEO_MPEG4 = "video/mp4v-es";
const char* AMEDIA_MIMETYPE_VIDEO_H263 = "video/3gpp";

const char* AMEDIA_MIMETYPE_AUDIO_AMR_NB = "audio/3gpp";
const char* AMEDIA_MIMETYPE_AUDIO_AMR_WB = "audio/amr-wb";
const char* AMEDIA_MIMETYPE_AUDIO_AAC = "audio/mp4a-latm";
const char* AMEDIA_MIMETYPE_AUDIO_VORBIS = "audio/vorbis";
const char* AMEDIA_MIMETYPE_AUDIO_OPUS = "audio/opus";

/* TODO(b/153592281) */
const char* TBD_AMEDIACODEC_PARAMETER_KEY_REQUEST_SYNC_FRAME = "request-sync";
const char* TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE = "video-bitrate";
const char* TBD_AMEDIACODEC_PARAMETER_KEY_MAX_B_FRAMES = "max-bframes";
const char* TBD_AMEDIAFORMAT_KEY_BIT_RATE_MODE = "bitrate-mode";

bool isCSDIdentical(AMediaFormat* refFormat, AMediaFormat* testFormat) {
    const char* mime;
    AMediaFormat_getString(refFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    /* TODO(b/154177490) */
    if ((strcmp(mime, AMEDIA_MIMETYPE_VIDEO_VP9) == 0) ||
        (strcmp(mime, AMEDIA_MIMETYPE_VIDEO_AV1) == 0)) {
        return true;
    }
    for (int i = 0;; i++) {
        std::pair<void*, size_t> refCsd;
        std::pair<void*, size_t> testCsd;
        char name[16];
        snprintf(name, sizeof(name), "csd-%d", i);
        bool hasRefCSD = AMediaFormat_getBuffer(refFormat, name, &refCsd.first, &refCsd.second);
        bool hasTestCSD = AMediaFormat_getBuffer(testFormat, name, &testCsd.first, &testCsd.second);
        if (hasRefCSD != hasTestCSD) {
            ALOGW("mismatch, ref fmt has CSD %d, test fmt has CSD %d", hasRefCSD, hasTestCSD);
            return false;
        }
        if (hasRefCSD) {
            if (refCsd.second != testCsd.second) {
                ALOGW("ref/test %s buffer sizes are not identical %zu/%zu", name, refCsd.second,
                      testCsd.second);
                return false;
            }
            if (memcmp(refCsd.first, testCsd.first, refCsd.second)) {
                ALOGW("ref/test %s buffers are not identical", name);
                return false;
            }
        } else break;
    }
    return true;
}