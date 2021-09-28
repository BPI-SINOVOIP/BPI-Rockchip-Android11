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
#define LOG_TAG "NativeMuxerUnitTest"
#include <log/log.h>

#include <NdkMediaExtractor.h>
#include <NdkMediaFormat.h>
#include <NdkMediaMuxer.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>

#include "NativeMediaCommon.h"

static media_status_t insertPerFrameSubtitles(AMediaMuxer* muxer, long pts, size_t trackID) {
    const char* greeting = "hello world";
    auto* info = new AMediaCodecBufferInfo;
    info->offset = 0;
    info->size = strlen(greeting);
    info->presentationTimeUs = pts;
    info->flags = 0;
    media_status_t status = AMediaMuxer_writeSampleData(muxer, trackID, (uint8_t*)greeting, info);
    delete info;
    return status;
}

static jboolean nativeTestIfInvalidFdIsRejected(JNIEnv*, jobject) {
    AMediaMuxer* muxer = AMediaMuxer_new(-1, (OutputFormat)OUTPUT_FORMAT_THREE_GPP);
    bool isPass = true;
    if (muxer != nullptr) {
        AMediaMuxer_delete(muxer);
        ALOGE("error: muxer constructor accepts invalid file descriptor");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfReadOnlyFdIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "rbe");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_THREE_GPP);
    bool isPass = true;
    if (muxer != nullptr) {
        AMediaMuxer_delete(muxer);
        ALOGE("error: muxer constructor accepts read-only file descriptor");
        isPass = false;
    }
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfWriteOnlyFdIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_WEBM);
    bool isPass = true;
    if (muxer != nullptr) {
        AMediaMuxer_delete(muxer);
        ALOGE("error: muxer constructor accepts write-only file descriptor");
        isPass = false;
    }
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfNonSeekableFdIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    mkfifo(cdstPath, 0666);
    int fd = open(cdstPath, O_WRONLY);
    AMediaMuxer* muxer = AMediaMuxer_new(fd, (OutputFormat)OUTPUT_FORMAT_THREE_GPP);
    bool isPass = true;
    if (muxer != nullptr) {
        AMediaMuxer_delete(muxer);
        ALOGE("error: muxer constructor accepts non-seekable file descriptor");
        isPass = false;
    }
    close(fd);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfInvalidOutputFormatIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)(OUTPUT_FORMAT_LIST_END + 1));
    bool isPass = true;
    if (muxer != nullptr) {
        AMediaMuxer_delete(muxer);
        ALOGE("error: muxer constructor accepts invalid output format");
        isPass = false;
    }
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfInvalidMediaFormatIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    bool isPass = true;
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds with format that has no mime key");
        isPass = false;
    }

    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "text/cea-608");
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds with format whose mime is non-compliant");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfCorruptMediaFormatIsRejected(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, AMEDIA_MIMETYPE_AUDIO_AAC);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, -1);
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds with erroneous key-value pairs in media format");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfAddTrackSucceedsAfterStart(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    isPass &= AMediaMuxer_addTrack(muxer, format) >= 0;
    isPass &= (AMediaMuxer_start(muxer) == AMEDIA_OK);
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds after muxer.start");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfAddTrackSucceedsAfterWriteSampleData(JNIEnv* env, jobject,
                                                                 jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds after muxer.writeSampleData");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfAddTrackSucceedsAfterStop(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    isPass &= AMediaMuxer_stop(muxer) == AMEDIA_OK;
    if (AMediaMuxer_addTrack(muxer, format) >= 0) {
        ALOGE("error: muxer.addTrack succeeds after muxer.stop");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfMuxerStartsBeforeAddTrack(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    if (AMediaMuxer_start(muxer) == AMEDIA_OK) {
        ALOGE("error: muxer.start succeeds before muxer.addTrack");
        isPass = false;
    }
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIdempotentStart(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    isPass &= AMediaMuxer_addTrack(muxer, format) >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    if (AMediaMuxer_start(muxer) == AMEDIA_OK) {
        ALOGE("error: double muxer.start succeeds");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfMuxerStartsAfterWriteSampleData(JNIEnv* env, jobject,
                                                            jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    if (AMediaMuxer_start(muxer) == AMEDIA_OK) {
        ALOGE("error: muxer.start succeeds after muxer.writeSampleData");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfMuxerStartsAfterStop(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    isPass &= AMediaMuxer_stop(muxer) == AMEDIA_OK;
    if (AMediaMuxer_start(muxer) == AMEDIA_OK) {
        ALOGE("error: muxer.start succeeds after muxer.stop");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStopOnANonStartedMuxer(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    isPass &= AMediaMuxer_addTrack(muxer, format) >= 0;
    if (AMEDIA_OK == AMediaMuxer_stop(muxer)) {
        ALOGE("error: muxer.stop succeeds before muxer.start");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIdempotentStop(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    isPass &= AMediaMuxer_stop(muxer) == AMEDIA_OK;
    if (AMEDIA_OK == AMediaMuxer_stop(muxer)) {
        ALOGE("error: double muxer.stop succeeds");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSimpleStartStop(JNIEnv* env, jobject, jstring jdstPath) {
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    bool isPass = true;
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= AMediaMuxer_stop(muxer) == AMEDIA_OK;
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfWriteSampleDataRejectsInvalidTrackIndex(JNIEnv* env, jobject,
                                                                    jstring jdstPath) {
    bool isPass = true;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    if (AMEDIA_OK == insertPerFrameSubtitles(muxer, 22000, trackID + 1)) {
        ALOGE("error: muxer.writeSampleData succeeds for invalid track ID");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfWriteSampleDataRejectsInvalidPts(JNIEnv* env, jobject,
                                                             jstring jdstPath) {
    bool isPass = true;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    if (AMEDIA_OK == insertPerFrameSubtitles(muxer, -33000, trackID)) {
        ALOGE("error: muxer.writeSampleData succeeds for invalid pts");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfWriteSampleDataSucceedsBeforeStart(JNIEnv* env, jobject,
                                                               jstring jdstPath) {
    bool isPass = true;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    if (AMEDIA_OK == insertPerFrameSubtitles(muxer, 0, trackID)) {
        ALOGE("error: muxer.writeSampleData succeeds before muxer.start");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfWriteSampleDataSucceedsAfterStop(JNIEnv* env, jobject,
                                                             jstring jdstPath) {
    bool isPass = true;
    const char* cdstPath = env->GetStringUTFChars(jdstPath, nullptr);
    FILE* ofp = fopen(cdstPath, "wbe+");
    AMediaMuxer* muxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)OUTPUT_FORMAT_MPEG_4);
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, "application/x-subrip");
    ssize_t trackID = AMediaMuxer_addTrack(muxer, format);
    isPass &= trackID >= 0;
    isPass &= AMediaMuxer_start(muxer) == AMEDIA_OK;
    isPass &= insertPerFrameSubtitles(muxer, 0, trackID) == AMEDIA_OK;
    isPass &= AMediaMuxer_stop(muxer) == AMEDIA_OK;
    if (AMEDIA_OK == insertPerFrameSubtitles(muxer, 33000, trackID)) {
        ALOGE("error: muxer.writeSampleData succeeds after muxer.stop");
        isPass = false;
    }
    AMediaFormat_delete(format);
    AMediaMuxer_delete(muxer);
    fclose(ofp);
    env->ReleaseStringUTFChars(jdstPath, cdstPath);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsMuxerUnitTestApi(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestIfInvalidFdIsRejected", "()Z", (void*)nativeTestIfInvalidFdIsRejected},
            {"nativeTestIfReadOnlyFdIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfReadOnlyFdIsRejected},
            {"nativeTestIfWriteOnlyFdIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfWriteOnlyFdIsRejected},
            {"nativeTestIfNonSeekableFdIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfNonSeekableFdIsRejected},
            {"nativeTestIfInvalidOutputFormatIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfInvalidOutputFormatIsRejected},

            {"nativeTestIfInvalidMediaFormatIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfInvalidMediaFormatIsRejected},
            {"nativeTestIfCorruptMediaFormatIsRejected", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfCorruptMediaFormatIsRejected},
            {"nativeTestIfAddTrackSucceedsAfterStart", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfAddTrackSucceedsAfterStart},
            {"nativeTestIfAddTrackSucceedsAfterWriteSampleData", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfAddTrackSucceedsAfterWriteSampleData},
            {"nativeTestIfAddTrackSucceedsAfterStop", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfAddTrackSucceedsAfterStop},

            {"nativeTestIfMuxerStartsBeforeAddTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfMuxerStartsBeforeAddTrack},
            {"nativeTestIdempotentStart", "(Ljava/lang/String;)Z",
             (void*)nativeTestIdempotentStart},
            {"nativeTestIfMuxerStartsAfterWriteSampleData", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfMuxerStartsAfterWriteSampleData},
            {"nativeTestIfMuxerStartsAfterStop", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfMuxerStartsAfterStop},

            {"nativeTestStopOnANonStartedMuxer", "(Ljava/lang/String;)Z",
             (void*)nativeTestStopOnANonStartedMuxer},
            {"nativeTestIdempotentStop", "(Ljava/lang/String;)Z", (void*)nativeTestIdempotentStop},
            {"nativeTestSimpleStartStop", "(Ljava/lang/String;)Z",
             (void*)nativeTestSimpleStartStop},

            {"nativeTestIfWriteSampleDataRejectsInvalidTrackIndex", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfWriteSampleDataRejectsInvalidTrackIndex},
            {"nativeTestIfWriteSampleDataRejectsInvalidPts", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfWriteSampleDataRejectsInvalidPts},
            {"nativeTestIfWriteSampleDataSucceedsBeforeStart", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfWriteSampleDataSucceedsBeforeStart},
            {"nativeTestIfWriteSampleDataSucceedsAfterStop", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfWriteSampleDataSucceedsAfterStop},
    };
    jclass c = env->FindClass("android/mediav2/cts/MuxerUnitTest$TestApiNative");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}
