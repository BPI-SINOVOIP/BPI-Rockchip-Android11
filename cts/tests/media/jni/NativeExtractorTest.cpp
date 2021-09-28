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
#define LOG_TAG "NativeExtractorTest"
#include <log/log.h>

#include <NdkMediaExtractor.h>
#include <jni.h>
#include <sys/stat.h>

#include <cstdlib>
#include <random>

#include "NativeMediaCommon.h"

static bool isExtractorOKonEOS(AMediaExtractor* extractor) {
    return AMediaExtractor_getSampleTrackIndex(extractor) < 0 &&
           AMediaExtractor_getSampleSize(extractor) < 0 &&
           (int)AMediaExtractor_getSampleFlags(extractor) < 0 &&
           AMediaExtractor_getSampleTime(extractor) < 0;
}

static bool isSampleInfoIdentical(AMediaCodecBufferInfo* refSample,
                                  AMediaCodecBufferInfo* testSample) {
    return refSample->flags == testSample->flags && refSample->size == testSample->size &&
           refSample->presentationTimeUs == testSample->presentationTimeUs;
}

static bool isSampleInfoValidAndIdentical(AMediaCodecBufferInfo* refSample,
                                          AMediaCodecBufferInfo* testSample) {
    return refSample->flags == testSample->flags && refSample->size == testSample->size &&
           abs(refSample->presentationTimeUs - testSample->presentationTimeUs) <= 1 &&
           (int)refSample->flags >= 0 && refSample->size >= 0 && refSample->presentationTimeUs >= 0;
}

static bool isFormatSimilar(AMediaFormat* refFormat, AMediaFormat* testFormat) {
    const char *refMime = nullptr, *testMime = nullptr;
    bool hasRefMime = AMediaFormat_getString(refFormat, AMEDIAFORMAT_KEY_MIME, &refMime);
    bool hasTestMime = AMediaFormat_getString(testFormat, AMEDIAFORMAT_KEY_MIME, &testMime);

    if (!hasRefMime || !hasTestMime || strcmp(refMime, testMime) != 0) return false;
    if (!isCSDIdentical(refFormat, testFormat)) return false;
    if (!strncmp(refMime, "audio/", strlen("audio/"))) {
        int32_t refSampleRate, testSampleRate, refNumChannels, testNumChannels;
        bool hasRefSampleRate =
                AMediaFormat_getInt32(refFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &refSampleRate);
        bool hasTestSampleRate =
                AMediaFormat_getInt32(testFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &testSampleRate);
        bool hasRefNumChannels =
                AMediaFormat_getInt32(refFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &refNumChannels);
        bool hasTestNumChannels =
                AMediaFormat_getInt32(testFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &testNumChannels);
        return hasRefSampleRate && hasTestSampleRate && hasRefNumChannels && hasTestNumChannels &&
               refNumChannels == testNumChannels && refSampleRate == testSampleRate;
    } else if (!strncmp(refMime, "video/", strlen("video/"))) {
        int32_t refWidth, testWidth, refHeight, testHeight;
        bool hasRefWidth = AMediaFormat_getInt32(refFormat, AMEDIAFORMAT_KEY_WIDTH, &refWidth);
        bool hasTestWidth = AMediaFormat_getInt32(testFormat, AMEDIAFORMAT_KEY_WIDTH, &testWidth);
        bool hasRefHeight = AMediaFormat_getInt32(refFormat, AMEDIAFORMAT_KEY_HEIGHT, &refHeight);
        bool hasTestHeight =
                AMediaFormat_getInt32(testFormat, AMEDIAFORMAT_KEY_HEIGHT, &testHeight);
        return hasRefWidth && hasTestWidth && hasRefHeight && hasTestHeight &&
               refWidth == testWidth && refHeight == testHeight;
    }
    return true;
}

static void inline setSampleInfo(AMediaExtractor* extractor, AMediaCodecBufferInfo* info) {
    info->flags = AMediaExtractor_getSampleFlags(extractor);
    info->offset = 0;
    info->size = AMediaExtractor_getSampleSize(extractor);
    info->presentationTimeUs = AMediaExtractor_getSampleTime(extractor);
}

static bool isMediaSimilar(AMediaExtractor* refExtractor, AMediaExtractor* testExtractor,
                           const char* mime, int sampleLimit = INT32_MAX) {
    const int maxSampleSize = (4 * 1024 * 1024);
    auto refBuffer = new uint8_t[maxSampleSize];
    auto testBuffer = new uint8_t[maxSampleSize];
    int noOfTracksMatched = 0;
    for (size_t refTrackID = 0; refTrackID < AMediaExtractor_getTrackCount(refExtractor);
         refTrackID++) {
        AMediaFormat* refFormat = AMediaExtractor_getTrackFormat(refExtractor, refTrackID);
        const char* refMime = nullptr;
        bool hasKey = AMediaFormat_getString(refFormat, AMEDIAFORMAT_KEY_MIME, &refMime);
        if (!hasKey || (mime != nullptr && strcmp(refMime, mime) != 0)) {
            AMediaFormat_delete(refFormat);
            continue;
        }
        for (size_t testTrackID = 0; testTrackID < AMediaExtractor_getTrackCount(testExtractor);
             testTrackID++) {
            AMediaFormat* testFormat = AMediaExtractor_getTrackFormat(testExtractor, testTrackID);
            if (!isFormatSimilar(refFormat, testFormat)) {
                AMediaFormat_delete(testFormat);
                continue;
            }
            AMediaExtractor_selectTrack(refExtractor, refTrackID);
            AMediaExtractor_selectTrack(testExtractor, testTrackID);

            AMediaCodecBufferInfo refSampleInfo, testSampleInfo;
            bool areTracksIdentical = true;
            for (int frameCount = 0;; frameCount++) {
                setSampleInfo(refExtractor, &refSampleInfo);
                setSampleInfo(testExtractor, &testSampleInfo);
                if (!isSampleInfoValidAndIdentical(&refSampleInfo, &testSampleInfo)) {
                    ALOGD(" Mime: %s mismatch for sample: %d", refMime, frameCount);
                    ALOGD(" flags exp/got: %d / %d", refSampleInfo.flags, testSampleInfo.flags);
                    ALOGD(" size exp/got: %d / %d ", refSampleInfo.size, testSampleInfo.size);
                    ALOGD(" ts exp/got: %d / %d ", (int)refSampleInfo.presentationTimeUs,
                          (int)testSampleInfo.presentationTimeUs);
                    areTracksIdentical = false;
                    break;
                }
                ssize_t refSz =
                        AMediaExtractor_readSampleData(refExtractor, refBuffer, maxSampleSize);
                if (refSz != refSampleInfo.size) {
                    ALOGD("Mime: %s Size exp/got:  %d / %zd ", refMime, refSampleInfo.size, refSz);
                    areTracksIdentical = false;
                    break;
                }
                ssize_t testSz =
                        AMediaExtractor_readSampleData(testExtractor, testBuffer, maxSampleSize);
                if (testSz != testSampleInfo.size) {
                    ALOGD("Mime: %s Size exp/got:  %d / %zd ", refMime, testSampleInfo.size,
                          testSz);
                    areTracksIdentical = false;
                    break;
                }
                int trackIndex = AMediaExtractor_getSampleTrackIndex(refExtractor);
                if (trackIndex != refTrackID) {
                    ALOGD("Mime: %s TrackID exp/got: %zu / %d", refMime, refTrackID, trackIndex);
                    areTracksIdentical = false;
                    break;
                }
                trackIndex = AMediaExtractor_getSampleTrackIndex(testExtractor);
                if (trackIndex != testTrackID) {
                    ALOGD("Mime: %s  TrackID exp/got %zd / %d : ", refMime, testTrackID,
                          trackIndex);
                    areTracksIdentical = false;
                    break;
                }
                if (memcmp(refBuffer, testBuffer, refSz)) {
                    ALOGD("Mime: %s Mismatch in sample data", refMime);
                    areTracksIdentical = false;
                    break;
                }
                bool haveRefSamples = AMediaExtractor_advance(refExtractor);
                bool haveTestSamples = AMediaExtractor_advance(testExtractor);
                if (haveRefSamples != haveTestSamples) {
                    ALOGD("Mime: %s Mismatch in sampleCount", refMime);
                    areTracksIdentical = false;
                    break;
                }

                if (!haveRefSamples && !isExtractorOKonEOS(refExtractor)) {
                    ALOGD("Mime: %s calls post advance() are not OK", refMime);
                    areTracksIdentical = false;
                    break;
                }
                if (!haveTestSamples && !isExtractorOKonEOS(testExtractor)) {
                    ALOGD("Mime: %s calls post advance() are not OK", refMime);
                    areTracksIdentical = false;
                    break;
                }
                ALOGV("Mime: %s Sample: %d flags: %d size: %d ts: %d", refMime, frameCount,
                      refSampleInfo.flags, refSampleInfo.size,
                      (int)refSampleInfo.presentationTimeUs);
                if (!haveRefSamples || frameCount >= sampleLimit) {
                    break;
                }
            }
            AMediaExtractor_unselectTrack(testExtractor, testTrackID);
            AMediaExtractor_unselectTrack(refExtractor, refTrackID);
            AMediaFormat_delete(testFormat);
            if (areTracksIdentical) {
                noOfTracksMatched++;
                break;
            }
        }
        AMediaFormat_delete(refFormat);
        if (mime != nullptr && noOfTracksMatched > 0) break;
    }
    delete[] refBuffer;
    delete[] testBuffer;
    if (mime == nullptr) {
        return noOfTracksMatched == AMediaExtractor_getTrackCount(refExtractor);
    } else {
        return noOfTracksMatched > 0;
    }
}

static bool validateCachedDuration(AMediaExtractor* extractor, bool isNetworkSource) {
    if (isNetworkSource) {
        AMediaExtractor_selectTrack(extractor, 0);
        for (unsigned cnt = 0;; cnt++) {
            if ((cnt & (cnt - 1)) == 0) {
                if (AMediaExtractor_getCachedDuration(extractor) < 0) {
                    ALOGE("getCachedDuration is less than zero for network source");
                    return false;
                }
            }
            if (!AMediaExtractor_advance(extractor)) break;
        }
        AMediaExtractor_unselectTrack(extractor, 0);
    } else {
        if (AMediaExtractor_getCachedDuration(extractor) != -1) {
            ALOGE("getCachedDuration != -1 for non-network source");
            return false;
        }
    }
    return true;
}

static AMediaExtractor* createExtractorFromFD(FILE* fp) {
    AMediaExtractor* extractor = nullptr;
    struct stat buf {};
    if (fp && !fstat(fileno(fp), &buf)) {
        extractor = AMediaExtractor_new();
        media_status_t res = AMediaExtractor_setDataSourceFd(extractor, fileno(fp), 0, buf.st_size);
        if (res != AMEDIA_OK) {
            AMediaExtractor_delete(extractor);
            extractor = nullptr;
        }
    }
    return extractor;
}

// content necessary for testing seek are grouped in this class
class SeekTestParams {
  public:
    SeekTestParams(AMediaCodecBufferInfo expected, int64_t timeStamp, SeekMode mode)
        : mExpected{expected}, mTimeStamp{timeStamp}, mMode{mode} {}

    AMediaCodecBufferInfo mExpected;
    int64_t mTimeStamp;
    SeekMode mMode;
};

static std::vector<AMediaCodecBufferInfo*> getSeekablePoints(const char* srcFile,
                                                             const char* mime) {
    std::vector<AMediaCodecBufferInfo*> bookmarks;
    if (mime == nullptr) return bookmarks;
    FILE* srcFp = fopen(srcFile, "rbe");
    if (!srcFp) {
        ALOGE("fopen failed for srcFile %s", srcFile);
        return bookmarks;
    }
    AMediaExtractor* extractor = createExtractorFromFD(srcFp);
    if (!extractor) {
        if (srcFp) fclose(srcFp);
        ALOGE("createExtractorFromFD failed");
        return bookmarks;
    }

    for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(extractor); trackID++) {
        AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, trackID);
        const char* currMime = nullptr;
        bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &currMime);
        if (!hasKey || strcmp(currMime, mime) != 0) {
            AMediaFormat_delete(format);
            continue;
        }
        AMediaExtractor_selectTrack(extractor, trackID);
        do {
            uint32_t sampleFlags = AMediaExtractor_getSampleFlags(extractor);
            if ((sampleFlags & AMEDIAEXTRACTOR_SAMPLE_FLAG_SYNC) != 0) {
                auto sampleInfo = new AMediaCodecBufferInfo;
                setSampleInfo(extractor, sampleInfo);
                bookmarks.push_back(sampleInfo);
            }
        } while (AMediaExtractor_advance(extractor));
        AMediaExtractor_unselectTrack(extractor, trackID);
        AMediaFormat_delete(format);
        break;
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    return bookmarks;
}

static constexpr unsigned kSeed = 0x7ab7;

static std::vector<SeekTestParams*> generateSeekTestArgs(const char* srcFile, const char* mime,
                                                         bool isRandom) {
    std::vector<SeekTestParams*> testArgs;
    if (mime == nullptr) return testArgs;
    const int MAX_SEEK_POINTS = 7;
    std::srand(kSeed);
    if (isRandom) {
        FILE* srcFp = fopen(srcFile, "rbe");
        if (!srcFp) {
            ALOGE("fopen failed for srcFile %s", srcFile);
            return testArgs;
        }
        AMediaExtractor* extractor = createExtractorFromFD(srcFp);
        if (!extractor) {
            if (srcFp) fclose(srcFp);
            ALOGE("createExtractorFromFD failed");
            return testArgs;
        }

        const int64_t maxEstDuration = 4000000;
        for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(extractor); trackID++) {
            AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, trackID);
            const char* currMime = nullptr;
            bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &currMime);
            if (!hasKey || strcmp(currMime, mime) != 0) {
                AMediaFormat_delete(format);
                continue;
            }
            AMediaExtractor_selectTrack(extractor, trackID);
            for (int i = 0; i < MAX_SEEK_POINTS; i++) {
                double r = ((double)rand() / (RAND_MAX));
                long pts = (long)(r * maxEstDuration);

                for (int mode = AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC;
                     mode <= AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC; mode++) {
                    AMediaExtractor_seekTo(extractor, pts, (SeekMode)mode);
                    AMediaCodecBufferInfo currInfo;
                    setSampleInfo(extractor, &currInfo);
                    testArgs.push_back((new SeekTestParams(currInfo, pts, (SeekMode)mode)));
                }
            }
            AMediaExtractor_unselectTrack(extractor, trackID);
            AMediaFormat_delete(format);
            break;
        }
        AMediaExtractor_delete(extractor);
        if (srcFp) fclose(srcFp);
    } else {
        std::vector<AMediaCodecBufferInfo*> bookmarks = getSeekablePoints(srcFile, mime);
        if (bookmarks.empty()) return testArgs;
        int size = bookmarks.size();
        int* indices;
        int indexSize = 0;
        if (size > MAX_SEEK_POINTS) {
            indices = new int[MAX_SEEK_POINTS];
            indexSize = MAX_SEEK_POINTS;
            indices[0] = 0;
            indices[MAX_SEEK_POINTS - 1] = size - 1;
            for (int i = 1; i < MAX_SEEK_POINTS - 1; i++) {
                double r = ((double)rand() / (RAND_MAX));
                indices[i] = (int)(r * (MAX_SEEK_POINTS - 1) + 1);
            }
        } else {
            indices = new int[size];
            indexSize = size;
            for (int i = 0; i < size; i++) indices[i] = i;
        }
        for (int i = 0; i < indexSize; i++) {
            AMediaCodecBufferInfo currInfo = *bookmarks[i];
            int64_t pts = currInfo.presentationTimeUs;
            testArgs.push_back(
                    (new SeekTestParams(currInfo, pts, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)));
            testArgs.push_back((new SeekTestParams(currInfo, pts, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC)));
            testArgs.push_back(
                    (new SeekTestParams(currInfo, pts, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC)));
            if (i > 0) {
                AMediaCodecBufferInfo prevInfo = *bookmarks[i - 1];
                int64_t ptsMinus = prevInfo.presentationTimeUs;
                ptsMinus = pts - ((pts - ptsMinus) >> 3);
                testArgs.push_back((
                        new SeekTestParams(currInfo, ptsMinus, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)));
                testArgs.push_back(
                        (new SeekTestParams(currInfo, ptsMinus, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC)));
                testArgs.push_back((new SeekTestParams(prevInfo, ptsMinus,
                                                       AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC)));
            }
            if (i < size - 1) {
                AMediaCodecBufferInfo nextInfo = *bookmarks[i + 1];
                int64_t ptsPlus = nextInfo.presentationTimeUs;
                ptsPlus = pts + ((ptsPlus - pts) >> 3);
                testArgs.push_back(
                        (new SeekTestParams(currInfo, ptsPlus, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC)));
                testArgs.push_back(
                        (new SeekTestParams(nextInfo, ptsPlus, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC)));
                testArgs.push_back((
                        new SeekTestParams(currInfo, ptsPlus, AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC)));
            }
        }
        for (auto bookmark : bookmarks) {
            delete bookmark;
        }
        bookmarks.clear();
        delete[] indices;
    }
    return testArgs;
}

static int checkSeekPoints(const char* srcFile, const char* mime,
                           const std::vector<SeekTestParams*>& seekTestArgs) {
    int errCnt = 0;
    FILE* srcFp = fopen(srcFile, "rbe");
    AMediaExtractor* extractor = createExtractorFromFD(srcFp);
    if (!extractor) {
        if (srcFp) fclose(srcFp);
        ALOGE("createExtractorFromFD failed");
        return -1;
    }
    for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(extractor); trackID++) {
        AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, trackID);
        const char* currMime = nullptr;
        bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &currMime);
        if (!hasKey || strcmp(currMime, mime) != 0) {
            continue;
        }
        AMediaExtractor_selectTrack(extractor, trackID);
        AMediaCodecBufferInfo received;
        for (auto arg : seekTestArgs) {
            AMediaExtractor_seekTo(extractor, arg->mTimeStamp, arg->mMode);
            setSampleInfo(extractor, &received);
            if (!isSampleInfoIdentical(&arg->mExpected, &received)) {
                ALOGE(" flags exp/got: %d / %d", arg->mExpected.flags, received.flags);
                ALOGE(" size exp/got: %d / %d ", arg->mExpected.size, received.size);
                ALOGE(" ts exp/got: %d / %d ", (int)arg->mExpected.presentationTimeUs,
                      (int)received.presentationTimeUs);
                errCnt++;
            }
        }
        AMediaExtractor_unselectTrack(extractor, trackID);
        AMediaFormat_delete(format);
        break;
    }
    AMediaExtractor_delete(extractor);
    if (srcFp) fclose(srcFp);
    return errCnt;
}

static bool isFileFormatIdentical(AMediaExtractor* refExtractor, AMediaExtractor* testExtractor) {
    bool result = false;
    if (refExtractor && testExtractor) {
        AMediaFormat* refFormat = AMediaExtractor_getFileFormat(refExtractor);
        AMediaFormat* testFormat = AMediaExtractor_getFileFormat(testExtractor);
        if (refFormat && testFormat) {
            const char *refMime = nullptr, *testMime = nullptr;
            bool hasRefKey = AMediaFormat_getString(refFormat, AMEDIAFORMAT_KEY_MIME, &refMime);
            bool hasTestKey = AMediaFormat_getString(testFormat, AMEDIAFORMAT_KEY_MIME, &testMime);
            /* TODO: Not Sure if we need to verify any other parameter of file format */
            if (hasRefKey && hasTestKey && strcmp(refMime, testMime) == 0) {
                result = true;
            } else {
                ALOGE("file format exp/got : %s/%s", refMime, testMime);
            }
        }
        if (refFormat) AMediaFormat_delete(refFormat);
        if (testFormat) AMediaFormat_delete(testFormat);
    }
    return result;
}

static bool isSeekOk(AMediaExtractor* refExtractor, AMediaExtractor* testExtractor) {
    const long maxEstDuration = 14000000;
    const int MAX_SEEK_POINTS = 7;
    std::srand(kSeed);
    AMediaCodecBufferInfo refSampleInfo, testSampleInfo;
    bool result = true;
    for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(refExtractor); trackID++) {
        AMediaExtractor_selectTrack(refExtractor, trackID);
        AMediaExtractor_selectTrack(testExtractor, trackID);
        for (int i = 0; i < MAX_SEEK_POINTS && result; i++) {
            double r = ((double)rand() / (RAND_MAX));
            long pts = (long)(r * maxEstDuration);
            for (int mode = AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC;
                 mode <= AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC; mode++) {
                AMediaExtractor_seekTo(refExtractor, pts, (SeekMode)mode);
                AMediaExtractor_seekTo(testExtractor, pts, (SeekMode)mode);
                setSampleInfo(refExtractor, &refSampleInfo);
                setSampleInfo(testExtractor, &testSampleInfo);
                result = isSampleInfoIdentical(&refSampleInfo, &testSampleInfo);
                if (!result) {
                    ALOGE(" flags exp/got: %d / %d", refSampleInfo.flags, testSampleInfo.flags);
                    ALOGE(" size exp/got: %d / %d ", refSampleInfo.size, testSampleInfo.size);
                    ALOGE(" ts exp/got: %d / %d ", (int)refSampleInfo.presentationTimeUs,
                          (int)testSampleInfo.presentationTimeUs);
                }
                int refTrackIdx = AMediaExtractor_getSampleTrackIndex(refExtractor);
                int testTrackIdx = AMediaExtractor_getSampleTrackIndex(testExtractor);
                if (refTrackIdx != testTrackIdx) {
                    ALOGE("trackIdx exp/got: %d/%d ", refTrackIdx, testTrackIdx);
                    result = false;
                }
            }
        }
        AMediaExtractor_unselectTrack(refExtractor, trackID);
        AMediaExtractor_unselectTrack(testExtractor, trackID);
    }
    return result;
}

static jboolean nativeTestExtract(JNIEnv* env, jobject, jstring jsrcPath, jstring jrefPath,
                                  jstring jmime) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* ctestPath = env->GetStringUTFChars(jrefPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jmime, nullptr);
    FILE* srcFp = fopen(csrcPath, "rbe");
    AMediaExtractor* srcExtractor = createExtractorFromFD(srcFp);
    FILE* testFp = fopen(ctestPath, "rbe");
    AMediaExtractor* testExtractor = createExtractorFromFD(testFp);
    if (srcExtractor && testExtractor) {
        isPass = isMediaSimilar(srcExtractor, testExtractor, cmime);
        if (!isPass) {
            ALOGE(" Src and test are different from extractor perspective");
        }
        AMediaExtractor_delete(srcExtractor);
        AMediaExtractor_delete(testExtractor);
    }
    if (srcFp) fclose(srcFp);
    if (testFp) fclose(testFp);
    env->ReleaseStringUTFChars(jmime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    env->ReleaseStringUTFChars(jrefPath, ctestPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSeek(JNIEnv* env, jobject, jstring jsrcPath, jstring jmime) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jmime, nullptr);
    std::vector<SeekTestParams*> seekTestArgs = generateSeekTestArgs(csrcPath, cmime, false);
    if (!seekTestArgs.empty()) {
        std::shuffle(seekTestArgs.begin(), seekTestArgs.end(), std::default_random_engine(kSeed));
        int seekAccErrCnt = checkSeekPoints(csrcPath, cmime, seekTestArgs);
        if (seekAccErrCnt != 0) {
            ALOGE("For %s seek chose inaccurate Sync point in: %d / %d", csrcPath, seekAccErrCnt,
                  (int)seekTestArgs.size());
            isPass = false;
        } else {
            isPass = true;
        }
        for (auto seekTestArg : seekTestArgs) {
            delete seekTestArg;
        }
        seekTestArgs.clear();
    } else {
        ALOGE("No sync samples found.");
    }
    env->ReleaseStringUTFChars(jmime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSeekFlakiness(JNIEnv* env, jobject, jstring jsrcPath, jstring jmime) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jmime, nullptr);
    std::vector<SeekTestParams*> seekTestArgs = generateSeekTestArgs(csrcPath, cmime, true);
    if (!seekTestArgs.empty()) {
        std::shuffle(seekTestArgs.begin(), seekTestArgs.end(), std::default_random_engine(kSeed));
        int flakyErrCnt = checkSeekPoints(csrcPath, cmime, seekTestArgs);
        if (flakyErrCnt != 0) {
            ALOGE("No. of Samples where seek showed flakiness is: %d", flakyErrCnt);
            isPass = false;
        } else {
            isPass = true;
        }
        for (auto seekTestArg : seekTestArgs) {
            delete seekTestArg;
        }
        seekTestArgs.clear();
    } else {
        ALOGE("No sync samples found.");
    }
    env->ReleaseStringUTFChars(jmime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSeekToZero(JNIEnv* env, jobject, jstring jsrcPath, jstring jmime) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jmime, nullptr);
    FILE* srcFp = fopen(csrcPath, "rbe");
    AMediaExtractor* extractor = createExtractorFromFD(srcFp);
    if (extractor) {
        AMediaCodecBufferInfo sampleInfoAtZero;
        AMediaCodecBufferInfo currInfo;
        static long randomPts = 1 << 20;
        for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(extractor); trackID++) {
            AMediaFormat* format = AMediaExtractor_getTrackFormat(extractor, trackID);
            if (format) {
                const char* currMime = nullptr;
                bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &currMime);
                if (!hasKey || strcmp(currMime, cmime) != 0) {
                    AMediaFormat_delete(format);
                    continue;
                }
                AMediaExtractor_selectTrack(extractor, trackID);
                setSampleInfo(extractor, &sampleInfoAtZero);
                AMediaExtractor_seekTo(extractor, randomPts, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC);
                AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
                setSampleInfo(extractor, &currInfo);
                isPass = isSampleInfoIdentical(&sampleInfoAtZero, &currInfo);
                if (!isPass) {
                    ALOGE("seen mismatch seekTo(0, SEEK_TO_CLOSEST_SYNC)");
                    ALOGE(" flags exp/got: %d / %d", sampleInfoAtZero.flags, currInfo.flags);
                    ALOGE(" size exp/got: %d / %d ", sampleInfoAtZero.size, currInfo.size);
                    ALOGE(" ts exp/got: %d / %d ", (int)sampleInfoAtZero.presentationTimeUs,
                          (int)currInfo.presentationTimeUs);
                    AMediaFormat_delete(format);
                    break;
                }
                AMediaExtractor_seekTo(extractor, -1L, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
                setSampleInfo(extractor, &currInfo);
                isPass = isSampleInfoIdentical(&sampleInfoAtZero, &currInfo);
                if (!isPass) {
                    ALOGE("seen mismatch seekTo(-1, SEEK_TO_CLOSEST_SYNC)");
                    ALOGE(" flags exp/got: %d / %d", sampleInfoAtZero.flags, currInfo.flags);
                    ALOGE(" size exp/got: %d / %d ", sampleInfoAtZero.size, currInfo.size);
                    ALOGE(" ts exp/got: %d / %d ", (int)sampleInfoAtZero.presentationTimeUs,
                          (int)currInfo.presentationTimeUs);
                    AMediaFormat_delete(format);
                    break;
                }
                AMediaExtractor_unselectTrack(extractor, trackID);
                AMediaFormat_delete(format);
            }
        }
        AMediaExtractor_delete(extractor);
    }
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jmime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFileFormat(JNIEnv* env, jobject, jstring jsrcPath) {
    bool isPass = false;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    FILE* srcFp = fopen(csrcPath, "rbe");
    AMediaExtractor* extractor = createExtractorFromFD(srcFp);
    if (extractor) {
        AMediaFormat* format = AMediaExtractor_getFileFormat(extractor);
        const char* mime = nullptr;
        bool hasKey = AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
        /* TODO: Not Sure if we need to verify any other parameter of file format */
        if (hasKey && mime && strlen(mime) > 0) {
            isPass = true;
        }
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
    }
    if (srcFp) fclose(srcFp);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDataSource(JNIEnv* env, jobject, jstring jsrcPath, jstring jsrcUrl) {
    bool isPass = true;
    const char* csrcUrl = env->GetStringUTFChars(jsrcUrl, nullptr);
    AMediaExtractor* refExtractor = AMediaExtractor_new();
    media_status_t status = AMediaExtractor_setDataSource(refExtractor, csrcUrl);
    if (status == AMEDIA_OK) {
        isPass &= validateCachedDuration(refExtractor, true);
        const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
        AMediaDataSource* dataSource = AMediaDataSource_newUri(csrcUrl, 0, nullptr);
        AMediaExtractor* testExtractor = AMediaExtractor_new();
        status = AMediaExtractor_setDataSourceCustom(testExtractor, dataSource);
        if (status != AMEDIA_OK) {
            ALOGE("setDataSourceCustom failed");
            isPass = false;
        } else {
            isPass &= validateCachedDuration(testExtractor, true);
            if (!(isMediaSimilar(refExtractor, testExtractor, nullptr) &&
                  isFileFormatIdentical(refExtractor, testExtractor) &&
                  isSeekOk(refExtractor, testExtractor))) {
                isPass = false;
            }
        }
        if (testExtractor) AMediaExtractor_delete(testExtractor);
        if (dataSource) AMediaDataSource_delete(dataSource);

        FILE* testFp = fopen(csrcPath, "rbe");
        testExtractor = createExtractorFromFD(testFp);
        if (testExtractor == nullptr) {
            ALOGE("createExtractorFromFD failed for test extractor");
            isPass = false;
        } else {
            isPass &= validateCachedDuration(testExtractor, false);
            if (!(isMediaSimilar(refExtractor, testExtractor, nullptr) &&
                  isFileFormatIdentical(refExtractor, testExtractor) &&
                  isSeekOk(refExtractor, testExtractor))) {
                isPass = false;
            }
        }
        if (testExtractor) AMediaExtractor_delete(testExtractor);
        if (testFp) fclose(testFp);
        env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    } else {
        ALOGE("setDataSource failed");
        isPass = false;
    }
    if (refExtractor) AMediaExtractor_delete(refExtractor);
    env->ReleaseStringUTFChars(jsrcUrl, csrcUrl);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsExtractorTestSetDS(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestDataSource", "(Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestDataSource},
    };
    jclass c = env->FindClass("android/mediav2/cts/ExtractorTest$SetDataSourceTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

int registerAndroidMediaV2CtsExtractorTestFunc(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestExtract", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestExtract},
            {"nativeTestSeek", "(Ljava/lang/String;Ljava/lang/String;)Z", (void*)nativeTestSeek},
            {"nativeTestSeekFlakiness", "(Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSeekFlakiness},
            {"nativeTestSeekToZero", "(Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSeekToZero},
            {"nativeTestFileFormat", "(Ljava/lang/String;)Z", (void*)nativeTestFileFormat},
    };
    jclass c = env->FindClass("android/mediav2/cts/ExtractorTest$FunctionalityTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

extern int registerAndroidMediaV2CtsExtractorUnitTestApi(JNIEnv* env);

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsExtractorTestSetDS(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsExtractorTestFunc(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsExtractorUnitTestApi(env) != JNI_OK) return JNI_ERR;
    return JNI_VERSION_1_6;
}