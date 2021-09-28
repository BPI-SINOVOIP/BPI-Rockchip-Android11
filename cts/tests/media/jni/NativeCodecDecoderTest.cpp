/*
 * Copyright (C) 2020 The Android Open Source Project
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
#define LOG_TAG "NativeCodecDecoderTest"
#include <log/log.h>
#include <android/native_window_jni.h>
#include <NdkMediaExtractor.h>
#include <jni.h>
#include <sys/stat.h>

#include <array>
#include <fstream>

#include "NativeCodecTestBase.h"
#include "NativeMediaCommon.h"

class CodecDecoderTest final : public CodecTestBase {
  private:
    uint8_t* mRefData;
    size_t mRefLength;
    AMediaExtractor* mExtractor;
    AMediaFormat* mInpDecFormat;
    AMediaFormat* mInpDecDupFormat;
    std::vector<std::pair<void*, size_t>> mCsdBuffers;
    int mCurrCsdIdx;
    ANativeWindow* mWindow;

    void setUpAudioReference(const char* refFile);
    void deleteReference();
    bool setUpExtractor(const char* srcFile);
    void deleteExtractor();
    bool configureCodec(AMediaFormat* format, bool isAsync, bool signalEOSWithLastFrame,
                        bool isEncoder) override;
    bool enqueueInput(size_t bufferIndex) override;
    bool dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* bufferInfo) override;
    bool queueCodecConfig();
    bool enqueueCodecConfig(int32_t bufferIndex);
    bool decodeToMemory(const char* decoder, AMediaFormat* format, int frameLimit,
                        OutputManager* ref, int64_t pts, SeekMode mode);

  public:
    explicit CodecDecoderTest(const char* mime, ANativeWindow* window);
    ~CodecDecoderTest();

    bool testSimpleDecode(const char* decoder, const char* testFile, const char* refFile,
                          float rmsError);
    bool testFlush(const char* decoder, const char* testFile);
    bool testOnlyEos(const char* decoder, const char* testFile);
    bool testSimpleDecodeQueueCSD(const char* decoder, const char* testFile);
};

CodecDecoderTest::CodecDecoderTest(const char* mime, ANativeWindow* window)
    : CodecTestBase(mime),
      mRefData(nullptr),
      mRefLength(0),
      mExtractor(nullptr),
      mInpDecFormat(nullptr),
      mInpDecDupFormat(nullptr),
      mCurrCsdIdx(0),
      mWindow{window} {}

CodecDecoderTest::~CodecDecoderTest() {
    deleteReference();
    deleteExtractor();
}

void CodecDecoderTest::setUpAudioReference(const char* refFile) {
    FILE* fp = fopen(refFile, "rbe");
    struct stat buf {};
    if (fp && !fstat(fileno(fp), &buf)) {
        deleteReference();
        mRefLength = buf.st_size;
        mRefData = new uint8_t[mRefLength];
        fread(mRefData, sizeof(uint8_t), mRefLength, fp);
    } else {
        ALOGE("unable to open input file %s", refFile);
    }
    if (fp) fclose(fp);
}

void CodecDecoderTest::deleteReference() {
    if (mRefData) {
        delete[] mRefData;
        mRefData = nullptr;
    }
    mRefLength = 0;
}

bool CodecDecoderTest::setUpExtractor(const char* srcFile) {
    FILE* fp = fopen(srcFile, "rbe");
    struct stat buf {};
    if (fp && !fstat(fileno(fp), &buf)) {
        deleteExtractor();
        mExtractor = AMediaExtractor_new();
        media_status_t res =
                AMediaExtractor_setDataSourceFd(mExtractor, fileno(fp), 0, buf.st_size);
        if (res != AMEDIA_OK) {
            deleteExtractor();
        } else {
            for (size_t trackID = 0; trackID < AMediaExtractor_getTrackCount(mExtractor);
                 trackID++) {
                AMediaFormat* currFormat = AMediaExtractor_getTrackFormat(mExtractor, trackID);
                const char* mime = nullptr;
                AMediaFormat_getString(currFormat, AMEDIAFORMAT_KEY_MIME, &mime);
                if (mime && strcmp(mMime, mime) == 0) {
                    AMediaExtractor_selectTrack(mExtractor, trackID);
                    if (!mIsAudio) {
                        AMediaFormat_setInt32(currFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                              COLOR_FormatYUV420Flexible);
                    }
                    mInpDecFormat = currFormat;
                    break;
                }
                AMediaFormat_delete(currFormat);
            }
        }
    }
    if (fp) fclose(fp);
    return mInpDecFormat != nullptr;
}

void CodecDecoderTest::deleteExtractor() {
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }
    if (mInpDecFormat) {
        AMediaFormat_delete(mInpDecFormat);
        mInpDecFormat = nullptr;
    }
    if (mInpDecDupFormat) {
        AMediaFormat_delete(mInpDecDupFormat);
        mInpDecDupFormat = nullptr;
    }
}

bool CodecDecoderTest::configureCodec(AMediaFormat* format, bool isAsync,
                                      bool signalEOSWithLastFrame, bool isEncoder) {
    resetContext(isAsync, signalEOSWithLastFrame);
    CHECK_STATUS(mAsyncHandle.setCallBack(mCodec, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback failed");
    CHECK_STATUS(AMediaCodec_configure(mCodec, format, mWindow, nullptr,
                                       isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0),
                 "AMediaCodec_configure failed");
    return true;
}

bool CodecDecoderTest::enqueueCodecConfig(int32_t bufferIndex) {
    size_t bufSize;
    uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &bufSize);
    if (buf == nullptr) {
        ALOGE("AMediaCodec_getInputBuffer failed");
        return false;
    }
    void* csdBuffer = mCsdBuffers[mCurrCsdIdx].first;
    size_t csdSize = mCsdBuffers[mCurrCsdIdx].second;
    if (bufSize < csdSize) {
        ALOGE("csd exceeds input buffer size, csdSize: %zu bufSize: %zu", csdSize, bufSize);
        return false;
    }
    memcpy((void*)buf, csdBuffer, csdSize);
    uint32_t flags = AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG;
    CHECK_STATUS(AMediaCodec_queueInputBuffer(mCodec, bufferIndex, 0, csdSize, 0, flags),
                 "AMediaCodec_queueInputBuffer failed");
    return !hasSeenError();
}

bool CodecDecoderTest::enqueueInput(size_t bufferIndex) {
    if (AMediaExtractor_getSampleSize(mExtractor) < 0) {
        return enqueueEOS(bufferIndex);
    } else {
        uint32_t flags = 0;
        size_t bufSize;
        uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &bufSize);
        if (buf == nullptr) {
            ALOGE("AMediaCodec_getInputBuffer failed");
            return false;
        }
        ssize_t size = AMediaExtractor_getSampleSize(mExtractor);
        int64_t pts = AMediaExtractor_getSampleTime(mExtractor);
        if (size > bufSize) {
            ALOGE("extractor sample size exceeds codec input buffer size %zu %zu", size, bufSize);
            return false;
        }
        if (size != AMediaExtractor_readSampleData(mExtractor, buf, bufSize)) {
            ALOGE("AMediaExtractor_readSampleData failed");
            return false;
        }
        if (!AMediaExtractor_advance(mExtractor) && mSignalEOSWithLastFrame) {
            flags |= AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
            mSawInputEOS = true;
        }
        CHECK_STATUS(AMediaCodec_queueInputBuffer(mCodec, bufferIndex, 0, size, pts, flags),
                     "AMediaCodec_queueInputBuffer failed");
        ALOGV("input: id: %zu  size: %zu  pts: %d  flags: %d", bufferIndex, size, (int)pts, flags);
        if (size > 0) {
            mOutputBuff->saveInPTS(pts);
            mInputCount++;
        }
    }
    return !hasSeenError();
}

bool CodecDecoderTest::dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* info) {
    if ((info->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
        mSawOutputEOS = true;
    }
    if (info->size > 0 && (info->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0) {
        if (mSaveToMem) {
            size_t buffSize;
            uint8_t* buf = AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize);
            if (mIsAudio) mOutputBuff->saveToMemory(buf, info);
            else mOutputBuff->saveChecksum(buf, info);
        }
        mOutputBuff->saveOutPTS(info->presentationTimeUs);
        mOutputCount++;
    }
    ALOGV("output: id: %zu  size: %d  pts: %d  flags: %d", bufferIndex, info->size,
          (int)info->presentationTimeUs, info->flags);
    CHECK_STATUS(AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, mWindow != nullptr),
                 "AMediaCodec_releaseOutputBuffer failed");
    return !hasSeenError();
}

bool CodecDecoderTest::queueCodecConfig() {
    bool isOk = true;
    if (mIsCodecInAsyncMode) {
        for (mCurrCsdIdx = 0; !hasSeenError() && isOk && mCurrCsdIdx < mCsdBuffers.size();
             mCurrCsdIdx++) {
            callbackObject element = mAsyncHandle.getInput();
            if (element.bufferIndex >= 0) {
                isOk = enqueueCodecConfig(element.bufferIndex);
            }
        }
    } else {
        int bufferIndex;
        for (mCurrCsdIdx = 0; isOk && mCurrCsdIdx < mCsdBuffers.size(); mCurrCsdIdx++) {
            bufferIndex = AMediaCodec_dequeueInputBuffer(mCodec, -1);
            if (bufferIndex >= 0) {
                isOk = enqueueCodecConfig(bufferIndex);
            } else {
                ALOGE("unexpected return value from *_dequeueInputBuffer: %d", (int)bufferIndex);
                return false;
            }
        }
    }
    return !hasSeenError() && isOk;
}

bool CodecDecoderTest::decodeToMemory(const char* decoder, AMediaFormat* format, int frameLimit,
                                      OutputManager* ref, int64_t pts, SeekMode mode) {
    mSaveToMem = (mWindow == nullptr);
    mOutputBuff = ref;
    AMediaExtractor_seekTo(mExtractor, pts, mode);
    mCodec = AMediaCodec_createCodecByName(decoder);
    if (!mCodec) {
        ALOGE("unable to create codec %s", decoder);
        return false;
    }
    if (!configureCodec(format, false, true, false)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!doWork(frameLimit)) return false;
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
    mCodec = nullptr;
    mSaveToMem = false;
    return !hasSeenError();
}

bool CodecDecoderTest::testSimpleDecode(const char* decoder, const char* testFile,
                                        const char* refFile, float rmsError) {
    bool isPass = true;
    if (!setUpExtractor(testFile)) return false;
    mSaveToMem = (mWindow == nullptr);
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    const bool boolStates[]{true, false};
    int loopCounter = 0;
    for (auto eosType : boolStates) {
        if (!isPass) break;
        for (auto isAsync : boolStates) {
            if (!isPass) break;
            bool validateFormat = true;
            char log[1000];
            snprintf(log, sizeof(log), "codec: %s, file: %s, async mode: %s, eos type: %s:: \n",
                     decoder, testFile, (isAsync ? "async" : "sync"),
                     (eosType ? "eos with last frame" : "eos separate"));
            mOutputBuff = loopCounter == 0 ? ref : test;
            mOutputBuff->reset();
            AMediaExtractor_seekTo(mExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
            /* TODO(b/147348711) */
            /* Instead of create and delete codec at every iteration, we would like to create
             * once and use it for all iterations and delete before exiting */
            mCodec = AMediaCodec_createCodecByName(decoder);
            if (!mCodec) {
                ALOGE("unable to create codec %s", decoder);
                return false;
            }
            char* name = nullptr;
            if (AMEDIA_OK == AMediaCodec_getName(mCodec, &name)) {
                if (!name || strcmp(name, decoder) != 0) {
                    ALOGE("%s error codec-name act/got: %s/%s", log, name, decoder);
                    if (name) AMediaCodec_releaseName(mCodec, name);
                    return false;
                }
            } else {
                ALOGE("AMediaCodec_getName failed unexpectedly");
                return false;
            }
            if (name) AMediaCodec_releaseName(mCodec, name);
            if (!configureCodec(mInpDecFormat, isAsync, eosType, false)) return false;
            AMediaFormat* decFormat = AMediaCodec_getOutputFormat(mCodec);
            if (isFormatSimilar(mInpDecFormat, decFormat)) {
                ALOGD("Input format is same as default for format for %s", decoder);
                validateFormat = false;
            }
            AMediaFormat_delete(decFormat);
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
            if (!doWork(INT32_MAX)) return false;
            if (!queueEOS()) return false;
            if (!waitForAllOutputs()) return false;
            CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
            CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
            mCodec = nullptr;
            CHECK_ERR(hasSeenError(), log, "has seen error", isPass);
            CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
            CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
            CHECK_ERR(loopCounter != 0 && (!ref->equals(test)), log, "output is flaky", isPass);
            CHECK_ERR(
                    loopCounter == 0 && mIsAudio && (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)),
                    log, "pts is not strictly increasing", isPass);
            CHECK_ERR(loopCounter == 0 && !mIsAudio &&
                      (!ref->isOutPtsListIdenticalToInpPtsList(false)),
                      log, "input pts list and output pts list are not identical", isPass);
            if (validateFormat) {
                if (mIsCodecInAsyncMode ? !mAsyncHandle.hasOutputFormatChanged()
                                        : !mSignalledOutFormatChanged) {
                    ALOGE(log, "not received format change");
                    isPass = false;
                } else if (!isFormatSimilar(mInpDecFormat, mIsCodecInAsyncMode
                                                                   ? mAsyncHandle.getOutputFormat()
                                                                   : mOutFormat)) {
                    ALOGE(log, "configured format and output format are not similar");
                    isPass = false;
                }
            }
            loopCounter++;
        }
    }
    if (mSaveToMem && refFile && rmsError >= 0) {
        setUpAudioReference(refFile);
        float error = ref->getRmsError(mRefData, mRefLength);
        if (error > rmsError) {
            isPass = false;
            ALOGE("rms error too high for file %s, act/exp: %f/%f", testFile, error, rmsError);
        }
    }
    return isPass;
}

bool CodecDecoderTest::testFlush(const char* decoder, const char* testFile) {
    bool isPass = true;
    if (!setUpExtractor(testFile)) return false;
    mCsdBuffers.clear();
    for (int i = 0;; i++) {
        char csdName[16];
        void* csdBuffer;
        size_t csdSize;
        snprintf(csdName, sizeof(csdName), "csd-%d", i);
        if (AMediaFormat_getBuffer(mInpDecFormat, csdName, &csdBuffer, &csdSize)) {
            mCsdBuffers.push_back(std::make_pair(csdBuffer, csdSize));
        } else break;
    }
    const int64_t pts = 500000;
    const SeekMode mode = AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC;
    auto ref = &mRefBuff;
    if (!decodeToMemory(decoder, mInpDecFormat, INT32_MAX, ref, pts, mode)) {
        ALOGE("decodeToMemory failed for file: %s codec: %s", testFile, decoder);
        return false;
    }
    CHECK_ERR(mIsAudio && (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)), "",
              "pts is not strictly increasing", isPass);
    CHECK_ERR(!mIsAudio && (!ref->isOutPtsListIdenticalToInpPtsList(false)), "",
              "input pts list and output pts list are not identical", isPass);
    if (!isPass) return false;

    auto test = &mTestBuff;
    mOutputBuff = test;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log), "codec: %s, file: %s, async mode: %s:: \n", decoder, testFile,
                 (isAsync ? "async" : "sync"));
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(decoder);
        if (!mCodec) {
            ALOGE("unable to create codec %s", decoder);
            return false;
        }
        AMediaExtractor_seekTo(mExtractor, 0, mode);
        if (!configureCodec(mInpDecFormat, isAsync, true, false)) return false;
        AMediaFormat* defFormat = AMediaCodec_getOutputFormat(mCodec);
        bool validateFormat = true;
        if (isFormatSimilar(mInpDecFormat, defFormat)) {
            ALOGD("Input format is same as default for format for %s", decoder);
            validateFormat = false;
        }
        AMediaFormat_delete(defFormat);
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");

        /* test flush in running state before queuing input */
        if (!flushCodec()) return false;
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!queueCodecConfig()) return false; /* flushed codec too soon, resubmit csd */
        if (!doWork(1)) return false;

        if (!flushCodec()) return false;
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!queueCodecConfig()) return false; /* flushed codec too soon, resubmit csd */
        AMediaExtractor_seekTo(mExtractor, 0, mode);
        test->reset();
        if (!doWork(23)) return false;
        CHECK_ERR(!test->isPtsStrictlyIncreasing(mPrevOutputPts), "",
                  "pts is not strictly increasing", isPass);

        /* test flush in running state */
        if (!flushCodec()) return false;
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        mSaveToMem = (mWindow == nullptr);
        test->reset();
        AMediaExtractor_seekTo(mExtractor, pts, mode);
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_ERR(hasSeenError(), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!ref->equals(test)), log, "output is flaky", isPass);
        if (!isPass) continue;

        /* test flush in eos state */
        if (!flushCodec()) return false;
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        test->reset();
        AMediaExtractor_seekTo(mExtractor, pts, mode);
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
        CHECK_ERR(hasSeenError(), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!ref->equals(test)), log, "output is flaky", isPass);
        if (validateFormat) {
            if (mIsCodecInAsyncMode ? !mAsyncHandle.hasOutputFormatChanged()
                                    : !mSignalledOutFormatChanged) {
                ALOGE(log, "not received format change");
                isPass = false;
            } else if (!isFormatSimilar(mInpDecFormat, mIsCodecInAsyncMode
                                                               ? mAsyncHandle.getOutputFormat()
                                                               : mOutFormat)) {
                ALOGE(log, "configured format and output format are not similar");
                isPass = false;
            }
        }
        mSaveToMem = false;
    }
    return isPass;
}

bool CodecDecoderTest::testOnlyEos(const char* decoder, const char* testFile) {
    bool isPass = true;
    if (!setUpExtractor(testFile)) return false;
    mSaveToMem = (mWindow == nullptr);
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    const bool boolStates[]{true, false};
    int loopCounter = 0;
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log), "codec: %s, file: %s, async mode: %s:: \n", decoder, testFile,
                 (isAsync ? "async" : "sync"));
        mOutputBuff = loopCounter == 0 ? ref : test;
        mOutputBuff->reset();
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(decoder);
        if (!mCodec) {
            ALOGE("unable to create codec %s", decoder);
            return false;
        }
        if (!configureCodec(mInpDecFormat, isAsync, false, false)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
        CHECK_ERR(hasSeenError(), log, "has seen error", isPass);
        CHECK_ERR(loopCounter != 0 && (!ref->equals(test)), log, "output is flaky", isPass);
        CHECK_ERR(loopCounter == 0 && mIsAudio && (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)),
                  log, "pts is not strictly increasing", isPass);
        CHECK_ERR(loopCounter == 0 && !mIsAudio && (!ref->isOutPtsListIdenticalToInpPtsList(false)),
                  log, "input pts list and output pts list are not identical", isPass);
        loopCounter++;
    }
    return isPass;
}

bool CodecDecoderTest::testSimpleDecodeQueueCSD(const char* decoder, const char* testFile) {
    bool isPass = true;
    if (!setUpExtractor(testFile)) return false;
    std::vector<AMediaFormat*> formats;
    formats.push_back(mInpDecFormat);
    mInpDecDupFormat = AMediaFormat_new();
    AMediaFormat_copy(mInpDecDupFormat, mInpDecFormat);
    formats.push_back(mInpDecDupFormat);
    mCsdBuffers.clear();
    for (int i = 0;; i++) {
        char csdName[16];
        void* csdBuffer;
        size_t csdSize;
        snprintf(csdName, sizeof(csdName), "csd-%d", i);
        if (AMediaFormat_getBuffer(mInpDecDupFormat, csdName, &csdBuffer, &csdSize)) {
            mCsdBuffers.push_back(std::make_pair(csdBuffer, csdSize));
            AMediaFormat_setBuffer(mInpDecFormat, csdName, nullptr, 0);
        } else break;
    }

    const bool boolStates[]{true, false};
    mSaveToMem = true;
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    int loopCounter = 0;
    for (int i = 0; i < formats.size() && isPass; i++) {
        auto fmt = formats[i];
        for (auto eosType : boolStates) {
            if (!isPass) break;
            for (auto isAsync : boolStates) {
                if (!isPass) break;
                bool validateFormat = true;
                char log[1000];
                snprintf(log, sizeof(log), "codec: %s, file: %s, async mode: %s, eos type: %s:: \n",
                         decoder, testFile, (isAsync ? "async" : "sync"),
                         (eosType ? "eos with last frame" : "eos separate"));
                mOutputBuff = loopCounter == 0 ? ref : test;
                mOutputBuff->reset();
                /* TODO(b/147348711) */
                /* Instead of create and delete codec at every iteration, we would like to create
                 * once and use it for all iterations and delete before exiting */
                mCodec = AMediaCodec_createCodecByName(decoder);
                if (!mCodec) {
                    ALOGE("unable to create codec");
                    return false;
                }
                AMediaExtractor_seekTo(mExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
                if (!configureCodec(fmt, isAsync, eosType, false)) return false;
                AMediaFormat* defFormat = AMediaCodec_getOutputFormat(mCodec);
                if (isFormatSimilar(defFormat, mInpDecFormat)) {
                    ALOGD("Input format is same as default for format for %s", decoder);
                    validateFormat = false;
                }
                AMediaFormat_delete(defFormat);
                CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
                /* formats[0] doesn't contain csd-data, so queuing csd separately, formats[1]
                 * contain csd-data */
                if (i == 0 && !queueCodecConfig()) return false;
                if (!doWork(INT32_MAX)) return false;
                if (!queueEOS()) return false;
                if (!waitForAllOutputs()) return false;
                CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
                CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
                mCodec = nullptr;
                CHECK_ERR(hasSeenError(), log, "has seen error", isPass);
                CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
                CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
                CHECK_ERR(loopCounter != 0 && (!ref->equals(test)), log, "output is flaky", isPass);
                CHECK_ERR(loopCounter == 0 && mIsAudio &&
                          (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)),
                          log, "pts is not strictly increasing", isPass);
                CHECK_ERR(loopCounter == 0 && !mIsAudio &&
                                  (!ref->isOutPtsListIdenticalToInpPtsList(false)),
                          log, "input pts list and output pts list are not identical", isPass);
                if (validateFormat) {
                    if (mIsCodecInAsyncMode ? !mAsyncHandle.hasOutputFormatChanged()
                                            : !mSignalledOutFormatChanged) {
                        ALOGE(log, "not received format change");
                        isPass = false;
                    } else if (!isFormatSimilar(mInpDecFormat,
                                                mIsCodecInAsyncMode ? mAsyncHandle.getOutputFormat()
                                                                    : mOutFormat)) {
                        ALOGE(log, "configured format and output format are not similar");
                        isPass = false;
                    }
                }
                loopCounter++;
            }
        }
    }
    mSaveToMem = false;
    return isPass;
}

static jboolean nativeTestSimpleDecode(JNIEnv* env, jobject, jstring jDecoder, jobject surface,
                                       jstring jMime, jstring jtestFile, jstring jrefFile,
                                       jfloat jrmsError) {
    const char* cDecoder = env->GetStringUTFChars(jDecoder, nullptr);
    const char* cMime = env->GetStringUTFChars(jMime, nullptr);
    const char* cTestFile = env->GetStringUTFChars(jtestFile, nullptr);
    const char* cRefFile = env->GetStringUTFChars(jrefFile, nullptr);
    float cRmsError = jrmsError;
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    auto* codecDecoderTest = new CodecDecoderTest(cMime, window);
    bool isPass = codecDecoderTest->testSimpleDecode(cDecoder, cTestFile, cRefFile, cRmsError);
    delete codecDecoderTest;
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    env->ReleaseStringUTFChars(jDecoder, cDecoder);
    env->ReleaseStringUTFChars(jMime, cMime);
    env->ReleaseStringUTFChars(jtestFile, cTestFile);
    env->ReleaseStringUTFChars(jrefFile, cRefFile);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestOnlyEos(JNIEnv* env, jobject, jstring jDecoder, jstring jMime,
                                  jstring jtestFile) {
    const char* cDecoder = env->GetStringUTFChars(jDecoder, nullptr);
    const char* cMime = env->GetStringUTFChars(jMime, nullptr);
    const char* cTestFile = env->GetStringUTFChars(jtestFile, nullptr);
    auto* codecDecoderTest = new CodecDecoderTest(cMime, nullptr);
    bool isPass = codecDecoderTest->testOnlyEos(cDecoder, cTestFile);
    delete codecDecoderTest;
    env->ReleaseStringUTFChars(jDecoder, cDecoder);
    env->ReleaseStringUTFChars(jMime, cMime);
    env->ReleaseStringUTFChars(jtestFile, cTestFile);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFlush(JNIEnv* env, jobject, jstring jDecoder, jobject surface,
                                jstring jMime, jstring jtestFile) {
    const char* cDecoder = env->GetStringUTFChars(jDecoder, nullptr);
    const char* cMime = env->GetStringUTFChars(jMime, nullptr);
    const char* cTestFile = env->GetStringUTFChars(jtestFile, nullptr);
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    auto* codecDecoderTest = new CodecDecoderTest(cMime, window);
    bool isPass = codecDecoderTest->testFlush(cDecoder, cTestFile);
    delete codecDecoderTest;
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    env->ReleaseStringUTFChars(jDecoder, cDecoder);
    env->ReleaseStringUTFChars(jMime, cMime);
    env->ReleaseStringUTFChars(jtestFile, cTestFile);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSimpleDecodeQueueCSD(JNIEnv* env, jobject, jstring jDecoder,
                                               jstring jMime, jstring jtestFile) {
    const char* cDecoder = env->GetStringUTFChars(jDecoder, nullptr);
    const char* cMime = env->GetStringUTFChars(jMime, nullptr);
    const char* cTestFile = env->GetStringUTFChars(jtestFile, nullptr);
    auto codecDecoderTest = new CodecDecoderTest(cMime, nullptr);
    bool isPass = codecDecoderTest->testSimpleDecodeQueueCSD(cDecoder, cTestFile);
    delete codecDecoderTest;
    env->ReleaseStringUTFChars(jDecoder, cDecoder);
    env->ReleaseStringUTFChars(jMime, cMime);
    env->ReleaseStringUTFChars(jtestFile, cTestFile);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsDecoderTest(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSimpleDecode",
             "(Ljava/lang/String;Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;Ljava/"
             "lang/String;F)Z",
             (void*)nativeTestSimpleDecode},
            {"nativeTestOnlyEos", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestOnlyEos},
            {"nativeTestFlush",
             "(Ljava/lang/String;Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestFlush},
            {"nativeTestSimpleDecodeQueueCSD",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestSimpleDecodeQueueCSD},
    };
    jclass c = env->FindClass("android/mediav2/cts/CodecDecoderTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

int registerAndroidMediaV2CtsDecoderSurfaceTest(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSimpleDecode",
             "(Ljava/lang/String;Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;Ljava/"
             "lang/String;F)Z",
             (void*)nativeTestSimpleDecode},
            {"nativeTestFlush",
             "(Ljava/lang/String;Landroid/view/Surface;Ljava/lang/String;Ljava/lang/String;)Z",
             (void*)nativeTestFlush},
    };
    jclass c = env->FindClass("android/mediav2/cts/CodecDecoderSurfaceTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}
