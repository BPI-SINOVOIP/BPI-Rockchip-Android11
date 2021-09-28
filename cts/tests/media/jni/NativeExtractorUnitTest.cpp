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
#define LOG_TAG "NativeExtractorUnitTest"
#include <log/log.h>

#include <NdkMediaExtractor.h>
#include <jni.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cstring>

static media_status_t setExtractorDataSource(AMediaExtractor* extractor, FILE* fp) {
    media_status_t status = AMEDIA_ERROR_BASE;
    struct stat buf {};
    if (fp && !fstat(fileno(fp), &buf))
        status = AMediaExtractor_setDataSourceFd(extractor, fileno(fp), 0, buf.st_size);
    if (status != AMEDIA_OK) ALOGE("error: AMediaExtractor_setDataSourceFd failed %d", status);
    return status;
}

static jboolean nativeTestGetTrackCountBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = (0 == AMediaExtractor_getTrackCount(extractor));
    if (!isPass) ALOGE("error: received valid trackCount before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSelectTrackBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = (AMEDIA_OK != AMediaExtractor_selectTrack(extractor, 0));
    if (!isPass) ALOGE("error: selectTrack succeeds before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSelectTrackForInvalidIndex(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        if (AMEDIA_OK !=
            AMediaExtractor_selectTrack(extractor, AMediaExtractor_getTrackCount(extractor))) {
            isPass = true;
        } else {
            ALOGE("error: selectTrack succeeds for out of bounds track index: %zu",
                  AMediaExtractor_getTrackCount(extractor));
        }
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIdempotentSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_selectTrack(extractor, 0) == AMEDIA_OK;
        isPass &= AMediaExtractor_selectTrack(extractor, 0) == AMEDIA_OK;
        if (!isPass) ALOGE("error: multiple selection of same track has failed");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestUnselectTrackBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = (AMEDIA_OK != AMediaExtractor_unselectTrack(extractor, 0));
    if (!isPass) ALOGE("error: unselectTrack succeeds before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestUnselectTrackForInvalidIndex(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        if (AMEDIA_OK !=
            AMediaExtractor_unselectTrack(extractor, AMediaExtractor_getTrackCount(extractor))) {
            isPass = true;
        } else {
            ALOGE("error: unselectTrack succeeds for out of bounds track index: %zu",
                  AMediaExtractor_getTrackCount(extractor));
        }
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestUnselectTrackForUnSelectedTrackIndex(JNIEnv* env, jobject,
                                                               jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = (AMediaExtractor_unselectTrack(extractor, 0) == AMEDIA_OK);
        if (!isPass) ALOGE("error: un-selection of non-selected track has failed");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIdempotentUnselectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_selectTrack(extractor, 0) == AMEDIA_OK;
        if (isPass) {
            isPass = AMediaExtractor_unselectTrack(extractor, 0) == AMEDIA_OK;
            isPass &= AMediaExtractor_unselectTrack(extractor, 0) == AMEDIA_OK;
            if (!isPass) ALOGE("error: multiple unselection of selected track has failed");
        } else {
            ALOGE("error: selection of track 0 has failed for file %s", csrcPath);
        }
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSeekToBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC) != AMEDIA_OK;
    if (!isPass) ALOGE("error: seekTo() succeeds before setting data source");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSeekToBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC) != AMEDIA_OK;
        if (!isPass) ALOGE("error: seekTo() succeeds before selecting track");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetCachedDurationBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_getCachedDuration(extractor) == -1;
    if (!isPass) ALOGE("error: getCachedDuration returns unexpected value before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfGetFileFormatSucceedsBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    AMediaFormat* empty = AMediaFormat_new();
    AMediaFormat* format = AMediaExtractor_getFileFormat(extractor);
    bool isPass = true;
    if (format == nullptr ||
        strcmp(AMediaFormat_toString(empty), AMediaFormat_toString(format)) != 0) {
        isPass = false;
        ALOGE("error: getFileFormat before set data source yields unexpected result");
    }
    if (format) AMediaFormat_delete(format);
    AMediaFormat_delete(empty);
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestAdvanceBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = !AMediaExtractor_advance(extractor);
    if (!isPass) ALOGE("error: advance succeeds before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestAdvanceBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = !AMediaExtractor_advance(extractor);
        if (!isPass) ALOGE("error: advance succeeds without any active tracks");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleFlagsBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_getSampleFlags(extractor) == -1;
    if (!isPass) ALOGE("error: received valid sample flag before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleFlagsBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_getSampleFlags(extractor) == -1;
        if (!isPass) ALOGE("error: received valid sample flag without any active tracks");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleTimeBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_getSampleTime(extractor) == -1;
    if (!isPass) ALOGE("error: received valid pts before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleTimeBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    AMediaExtractor* extractor = AMediaExtractor_new();
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_getSampleTime(extractor) == -1;
        if (!isPass) ALOGE("error: received valid pts without any active tracks");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleSizeBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_getSampleSize(extractor) == -1;
    if (!isPass) ALOGE("error: received valid sample size before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleSizeBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_getSampleSize(extractor) == -1;
        if (!isPass) ALOGE("error: received valid sample size without any active tracks");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfGetSampleFormatBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    AMediaFormat* format = AMediaFormat_new();
    bool isPass = AMediaExtractor_getSampleFormat(extractor, format) != AMEDIA_OK;
    if (!isPass) ALOGE("error: getSampleFormat succeeds before setDataSource");
    AMediaFormat_delete(format);
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfGetSampleFormatBeforeSelectTrack(JNIEnv* env, jobject,
                                                             jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    AMediaFormat* format = AMediaFormat_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_getSampleFormat(extractor, format) != AMEDIA_OK;
        if (!isPass) ALOGE("error: getSampleFormat succeeds without any active tracks");
    }
    AMediaFormat_delete(format);
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleTrackIndexBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_getSampleTrackIndex(extractor) == -1;
    if (!isPass) ALOGE("error: received valid track index before setDataSource");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetSampleTrackIndexBeforeSelectTrack(JNIEnv* env, jobject,
                                                               jstring jsrcPath) {
    bool isPass = false;
    AMediaExtractor* extractor = AMediaExtractor_new();
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_getSampleTrackIndex(extractor) == -1;
        if (!isPass) ALOGE("error: received valid track index without any active tracks");
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetTrackFormatBeforeSetDataSource(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaExtractor* extractor = AMediaExtractor_new();
    AMediaFormat* empty = AMediaFormat_new();
    AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, 0);
    if (format == nullptr ||
        strcmp(AMediaFormat_toString(empty), AMediaFormat_toString(format)) != 0) {
        isPass = false;
        ALOGE("error: getTrackFormat before setDataSource yields unexpected result");
    }
    if (format) AMediaFormat_delete(format);
    AMediaFormat_delete(empty);
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetTrackFormatForInvalidIndex(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = true;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        AMediaFormat* format =
                AMediaExtractor_getTrackFormat(extractor, AMediaExtractor_getTrackCount(extractor));
        AMediaFormat* empty = AMediaFormat_new();
        if (format == nullptr ||
            strcmp(AMediaFormat_toString(empty), AMediaFormat_toString(format)) != 0) {
            isPass = false;
            ALOGE("error: getTrackFormat for out of bound track index %zu yields unexpected result",
                  AMediaExtractor_getTrackCount(extractor));
        }
        if (format) AMediaFormat_delete(format);
        AMediaFormat_delete(empty);
    } else {
        isPass = false;
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReadSampleDataBeforeSetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    const int maxSampleSize = 512 * 1024;
    auto buffer = new uint8_t[maxSampleSize];
    bool isPass = AMediaExtractor_readSampleData(extractor, buffer, maxSampleSize) < 0;
    if (!isPass) ALOGE("error: readSampleData return non-negative readBytes before setDataSource");
    delete[] buffer;
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReadSampleDataBeforeSelectTrack(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    AMediaExtractor* extractor = AMediaExtractor_new();
    const int maxSampleSize = (512 * 1024);
    auto buffer = new uint8_t[maxSampleSize];
    FILE* srcFp = fopen(csrcPath, "rbe");
    if (AMEDIA_OK == setExtractorDataSource(extractor, srcFp)) {
        isPass = AMediaExtractor_readSampleData(extractor, buffer, maxSampleSize) < 0;
        if (!isPass) {
            ALOGE("error: readSampleData returns non-negative readBytes with out active tracks");
        }
    }
    delete[] buffer;
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestIfNullLocationIsRejectedBySetDataSource(JNIEnv*, jobject) {
    AMediaExtractor* extractor = AMediaExtractor_new();
    bool isPass = AMediaExtractor_setDataSource(extractor, nullptr) != AMEDIA_OK;
    if (!isPass) ALOGE("error: setDataSource succeeds with null location");
    AMediaExtractor_delete(extractor);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsExtractorUnitTestApi(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestGetTrackCountBeforeSetDataSource", "()Z",
             (void*)nativeTestGetTrackCountBeforeSetDataSource},
            {"nativeTestSelectTrackBeforeSetDataSource", "()Z",
             (void*)nativeTestSelectTrackBeforeSetDataSource},
            {"nativeTestSelectTrackForInvalidIndex", "(Ljava/lang/String;)Z",
             (void*)nativeTestSelectTrackForInvalidIndex},
            {"nativeTestIdempotentSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestIdempotentSelectTrack},
            {"nativeTestUnselectTrackBeforeSetDataSource", "()Z",
             (void*)nativeTestUnselectTrackBeforeSetDataSource},
            {"nativeTestUnselectTrackForInvalidIndex", "(Ljava/lang/String;)Z",
             (void*)nativeTestUnselectTrackForInvalidIndex},
            {"nativeTestUnselectTrackForUnSelectedTrackIndex", "(Ljava/lang/String;)Z",
             (void*)nativeTestUnselectTrackForUnSelectedTrackIndex},
            {"nativeTestIdempotentUnselectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestIdempotentUnselectTrack},
            {"nativeTestSeekToBeforeSetDataSource", "()Z",
             (void*)nativeTestSeekToBeforeSetDataSource},
            {"nativeTestSeekToBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestSeekToBeforeSelectTrack},
            {"nativeTestGetCachedDurationBeforeSetDataSource", "()Z",
             (void*)nativeTestGetCachedDurationBeforeSetDataSource},
            {"nativeTestIfGetFileFormatSucceedsBeforeSetDataSource", "()Z",
             (void*)nativeTestIfGetFileFormatSucceedsBeforeSetDataSource},
            {"nativeTestAdvanceBeforeSetDataSource", "()Z",
             (void*)nativeTestAdvanceBeforeSetDataSource},
            {"nativeTestAdvanceBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestAdvanceBeforeSelectTrack},
            {"nativeTestGetSampleFlagsBeforeSetDataSource", "()Z",
             (void*)nativeTestGetSampleFlagsBeforeSetDataSource},
            {"nativeTestGetSampleFlagsBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestGetSampleFlagsBeforeSelectTrack},
            {"nativeTestGetSampleTimeBeforeSetDataSource", "()Z",
             (void*)nativeTestGetSampleTimeBeforeSetDataSource},
            {"nativeTestGetSampleTimeBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestGetSampleTimeBeforeSelectTrack},
            {"nativeTestGetSampleSizeBeforeSetDataSource", "()Z",
             (void*)nativeTestGetSampleSizeBeforeSetDataSource},
            {"nativeTestGetSampleSizeBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestGetSampleSizeBeforeSelectTrack},
            {"nativeTestIfGetSampleFormatBeforeSetDataSource", "()Z",
             (void*)nativeTestIfGetSampleFormatBeforeSetDataSource},
            {"nativeTestIfGetSampleFormatBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestIfGetSampleFormatBeforeSelectTrack},
            {"nativeTestGetSampleTrackIndexBeforeSetDataSource", "()Z",
             (void*)nativeTestGetSampleTrackIndexBeforeSetDataSource},
            {"nativeTestGetSampleTrackIndexBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestGetSampleTrackIndexBeforeSelectTrack},
            {"nativeTestGetTrackFormatBeforeSetDataSource", "()Z",
             (void*)nativeTestGetTrackFormatBeforeSetDataSource},
            {"nativeTestGetTrackFormatForInvalidIndex", "(Ljava/lang/String;)Z",
             (void*)nativeTestGetTrackFormatForInvalidIndex},
            {"nativeTestReadSampleDataBeforeSetDataSource", "()Z",
             (void*)nativeTestReadSampleDataBeforeSetDataSource},
            {"nativeTestReadSampleDataBeforeSelectTrack", "(Ljava/lang/String;)Z",
             (void*)nativeTestReadSampleDataBeforeSelectTrack},
            {"nativeTestIfNullLocationIsRejectedBySetDataSource", "()Z",
             (void*)nativeTestIfNullLocationIsRejectedBySetDataSource},
    };
    jclass c = env->FindClass("android/mediav2/cts/ExtractorUnitTest$TestApiNative");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}
