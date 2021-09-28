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
#define LOG_TAG "NativeCodecEncoderTest"
#include <log/log.h>

#include <jni.h>
#include <sys/stat.h>

#include "NativeCodecTestBase.h"
#include "NativeMediaCommon.h"

class CodecEncoderTest final : CodecTestBase {
  private:
    uint8_t* mInputData;
    size_t mInputLength;
    int mNumBytesSubmitted;
    int64_t mInputOffsetPts;
    std::vector<AMediaFormat*> mFormats;
    int mNumSyncFramesReceived;
    std::vector<int> mSyncFramesPos;

    int32_t* mBitRates;
    int mLen0;
    int32_t* mEncParamList1;
    int mLen1;
    int32_t* mEncParamList2;
    int mLen2;

    int mWidth, mHeight;
    int mChannels;
    int mSampleRate;
    int mColorFormat;
    int mMaxBFrames;
    int mDefFrameRate;
    const int kInpFrmWidth = 352;
    const int kInpFrmHeight = 288;

    void convertyuv420ptoyuv420sp();
    void setUpSource(const char* srcPath);
    void deleteSource();
    void setUpParams(int limit);
    void deleteParams();
    bool flushCodec() override;
    void resetContext(bool isAsync, bool signalEOSWithLastFrame) override;
    bool enqueueInput(size_t bufferIndex) override;
    bool dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* bufferInfo) override;
    void initFormat(AMediaFormat* format);
    bool encodeToMemory(const char* file, const char* encoder, int frameLimit, AMediaFormat* format,
                        OutputManager* ref);
    void fillByteBuffer(uint8_t* inputBuffer);
    void forceSyncFrame(AMediaFormat* format);
    void updateBitrate(AMediaFormat* format, int bitrate);

  public:
    CodecEncoderTest(const char* mime, int32_t* list0, int len0, int32_t* list1, int len1,
                     int32_t* list2, int len2, int colorFormat);
    ~CodecEncoderTest();

    bool testSimpleEncode(const char* encoder, const char* srcPath);
    bool testFlush(const char* encoder, const char* srcPath);
    bool testReconfigure(const char* encoder, const char* srcPath);
    bool testSetForceSyncFrame(const char* encoder, const char* srcPath);
    bool testAdaptiveBitRate(const char* encoder, const char* srcPath);
    bool testOnlyEos(const char* encoder);
};

CodecEncoderTest::CodecEncoderTest(const char* mime, int32_t* list0, int len0, int32_t* list1,
                                   int len1, int32_t* list2, int len2, int colorFormat)
    : CodecTestBase(mime),
      mBitRates{list0},
      mLen0{len0},
      mEncParamList1{list1},
      mLen1{len1},
      mEncParamList2{list2},
      mLen2{len2},
      mColorFormat{colorFormat} {
    mDefFrameRate = 30;
    if (!strcmp(mime, AMEDIA_MIMETYPE_VIDEO_H263)) mDefFrameRate = 12;
    else if (!strcmp(mime, AMEDIA_MIMETYPE_VIDEO_MPEG4)) mDefFrameRate = 12;
    mMaxBFrames = 0;
    mInputData = nullptr;
    mInputLength = 0;
    mNumBytesSubmitted = 0;
    mInputOffsetPts = 0;
}

CodecEncoderTest::~CodecEncoderTest() {
    deleteSource();
    deleteParams();
}

void CodecEncoderTest::convertyuv420ptoyuv420sp() {
    int ySize = kInpFrmWidth * kInpFrmHeight;
    int uSize = kInpFrmWidth * kInpFrmHeight / 4;
    int frameSize = ySize + uSize * 2;
    int totalFrames = mInputLength / frameSize;
    uint8_t* u = new uint8_t[uSize];
    uint8_t* v = new uint8_t[uSize];
    uint8_t* frameBase = mInputData;
    for (int i = 0; i < totalFrames; i++) {
        uint8_t* uvBase = frameBase + ySize;
        memcpy(u, uvBase, uSize);
        memcpy(v, uvBase + uSize, uSize);
        for (int j = 0, idx = 0; j < uSize; j++, idx += 2) {
            uvBase[idx] = u[j];
            uvBase[idx + 1] = v[j];
        }
        frameBase += frameSize;
    }
    delete[] u;
    delete[] v;
}

void CodecEncoderTest::setUpSource(const char* srcPath) {
    FILE* fp = fopen(srcPath, "rbe");
    struct stat buf {};
    if (fp && !fstat(fileno(fp), &buf)) {
        deleteSource();
        mInputLength = buf.st_size;
        mInputData = new uint8_t[mInputLength];
        fread(mInputData, sizeof(uint8_t), mInputLength, fp);
        if (mColorFormat == COLOR_FormatYUV420SemiPlanar) {
            convertyuv420ptoyuv420sp();
        }
    } else {
        ALOGE("unable to open input file %s", srcPath);
    }
    if (fp) fclose(fp);
}

void CodecEncoderTest::deleteSource() {
    if (mInputData) {
        delete[] mInputData;
        mInputData = nullptr;
    }
    mInputLength = 0;
}

void CodecEncoderTest::setUpParams(int limit) {
    int count = 0;
    for (int k = 0; k < mLen0; k++) {
        int bitrate = mBitRates[k];
        if (mIsAudio) {
            for (int j = 0; j < mLen1; j++) {
                int rate = mEncParamList1[j];
                for (int i = 0; i < mLen2; i++) {
                    int channels = mEncParamList2[i];
                    AMediaFormat* format = AMediaFormat_new();
                    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mMime);
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, rate);
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channels);
                    mFormats.push_back(format);
                    count++;
                    if (count >= limit) break;
                }
            }
        } else {
            for (int j = 0; j < mLen1; j++) {
                int width = mEncParamList1[j];
                int height = mEncParamList2[j];
                AMediaFormat* format = AMediaFormat_new();
                AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mMime);
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, mDefFrameRate);
                AMediaFormat_setInt32(format, TBD_AMEDIACODEC_PARAMETER_KEY_MAX_B_FRAMES,
                                      mMaxBFrames);
                AMediaFormat_setFloat(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1.0F);
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, mColorFormat);
                mFormats.push_back(format);
                count++;
                if (count >= limit) break;
            }
        }
    }
}

void CodecEncoderTest::deleteParams() {
    for (auto format : mFormats) AMediaFormat_delete(format);
    mFormats.clear();
}

void CodecEncoderTest::resetContext(bool isAsync, bool signalEOSWithLastFrame) {
    CodecTestBase::resetContext(isAsync, signalEOSWithLastFrame);
    mNumBytesSubmitted = 0;
    mInputOffsetPts = 0;
    mNumSyncFramesReceived = 0;
    mSyncFramesPos.clear();
}

bool CodecEncoderTest::flushCodec() {
    bool isOk = CodecTestBase::flushCodec();
    if (mIsAudio) {
        mInputOffsetPts = (mNumBytesSubmitted + 1024) * 1000000L / (2 * mChannels * mSampleRate);
    } else {
        mInputOffsetPts = (mInputCount + 5) * 1000000L / mDefFrameRate;
    }
    mPrevOutputPts = mInputOffsetPts - 1;
    mNumBytesSubmitted = 0;
    mNumSyncFramesReceived = 0;
    mSyncFramesPos.clear();
    return isOk;
}

void CodecEncoderTest::fillByteBuffer(uint8_t* inputBuffer) {
    int width, height, tileWidth, tileHeight;
    int offset = 0, frmOffset = mNumBytesSubmitted;
    int numOfPlanes;
    if (mColorFormat == COLOR_FormatYUV420SemiPlanar) {
        numOfPlanes = 2;
    } else {
        numOfPlanes = 3;
    }
    for (int plane = 0; plane < numOfPlanes; plane++) {
        if (plane == 0) {
            width = mWidth;
            height = mHeight;
            tileWidth = kInpFrmWidth;
            tileHeight = kInpFrmHeight;
        } else {
            if (mColorFormat == COLOR_FormatYUV420SemiPlanar) {
                width = mWidth;
                tileWidth = kInpFrmWidth;
            } else {
                width = mWidth / 2;
                tileWidth = kInpFrmWidth / 2;
            }
            height = mHeight / 2;
            tileHeight = kInpFrmHeight / 2;
        }
        for (int k = 0; k < height; k += tileHeight) {
            int rowsToCopy = std::min(height - k, tileHeight);
            for (int j = 0; j < rowsToCopy; j++) {
                for (int i = 0; i < width; i += tileWidth) {
                    int colsToCopy = std::min(width - i, tileWidth);
                    memcpy(inputBuffer + (offset + (k + j) * width + i),
                           mInputData + (frmOffset + j * tileWidth), colsToCopy);
                }
            }
        }
        offset += width * height;
        frmOffset += tileWidth * tileHeight;
    }
}

bool CodecEncoderTest::enqueueInput(size_t bufferIndex) {
    if (mNumBytesSubmitted >= mInputLength) {
        return enqueueEOS(bufferIndex);
    } else {
        int size = 0;
        int flags = 0;
        int64_t pts = mInputOffsetPts;
        size_t buffSize;
        uint8_t* inputBuffer = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &buffSize);
        if (mIsAudio) {
            pts += mNumBytesSubmitted * 1000000L / (2 * mChannels * mSampleRate);
            size = std::min(buffSize, mInputLength - mNumBytesSubmitted);
            memcpy(inputBuffer, mInputData + mNumBytesSubmitted, size);
            if (mNumBytesSubmitted + size >= mInputLength && mSignalEOSWithLastFrame) {
                flags |= AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
                mSawInputEOS = true;
            }
            mNumBytesSubmitted += size;
        } else {
            pts += mInputCount * 1000000L / mDefFrameRate;
            size = mWidth * mHeight * 3 / 2;
            int frmSize = kInpFrmWidth * kInpFrmHeight * 3 / 2;
            if (mNumBytesSubmitted + frmSize > mInputLength) {
                ALOGE("received partial frame to encode");
                return false;
            } else if (size > buffSize) {
                ALOGE("frame size exceeds buffer capacity of input buffer %d %zu", size, buffSize);
                return false;
            } else {
                if (mWidth == kInpFrmWidth && mHeight == kInpFrmHeight) {
                    memcpy(inputBuffer, mInputData + mNumBytesSubmitted, size);
                } else {
                    fillByteBuffer(inputBuffer);
                }
            }
            if (mNumBytesSubmitted + frmSize >= mInputLength && mSignalEOSWithLastFrame) {
                flags |= AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
                mSawInputEOS = true;
            }
            mNumBytesSubmitted += frmSize;
        }
        CHECK_STATUS(AMediaCodec_queueInputBuffer(mCodec, bufferIndex, 0, size, pts, flags),
                     "AMediaCodec_queueInputBuffer failed");
        ALOGV("input: id: %zu  size: %d  pts: %d  flags: %d", bufferIndex, size, (int)pts, flags);
        mOutputBuff->saveInPTS(pts);
        mInputCount++;
    }
    return !hasSeenError();
}

bool CodecEncoderTest::dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* info) {
    if ((info->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
        mSawOutputEOS = true;
    }
    if (info->size > 0) {
        if (mSaveToMem) {
            size_t buffSize;
            uint8_t* buf = AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize);
            mOutputBuff->saveToMemory(buf, info);
        }
        if ((info->flags & TBD_AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0) {
            mNumSyncFramesReceived += 1;
            mSyncFramesPos.push_back(mOutputCount);
        }
        if ((info->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0) {
            mOutputBuff->saveOutPTS(info->presentationTimeUs);
            mOutputCount++;
        }
    }
    ALOGV("output: id: %zu  size: %d  pts: %d  flags: %d", bufferIndex, info->size,
          (int)info->presentationTimeUs, info->flags);
    CHECK_STATUS(AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false),
                 "AMediaCodec_releaseOutputBuffer failed");
    return !hasSeenError();
}

void CodecEncoderTest::initFormat(AMediaFormat* format) {
    if (mIsAudio) {
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &mSampleRate);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &mChannels);
    } else {
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mWidth);
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mHeight);
    }
}

bool CodecEncoderTest::encodeToMemory(const char* file, const char* encoder, int32_t frameLimit,
                                      AMediaFormat* format, OutputManager* ref) {
    /* TODO(b/149027258) */
    if (true) mSaveToMem = false;
    else mSaveToMem = true;
    mOutputBuff = ref;
    mCodec = AMediaCodec_createCodecByName(encoder);
    if (!mCodec) {
        ALOGE("unable to create codec %s", encoder);
        return false;
    }
    setUpSource(file);
    if (!mInputData) return false;
    if (!configureCodec(format, false, true, true)) return false;
    initFormat(format);
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

void CodecEncoderTest::forceSyncFrame(AMediaFormat* format) {
    AMediaFormat_setInt32(format, TBD_AMEDIACODEC_PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
    ALOGV("requesting key frame");
    AMediaCodec_setParameters(mCodec, format);
}

void CodecEncoderTest::updateBitrate(AMediaFormat* format, int bitrate) {
    AMediaFormat_setInt32(format, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate);
    ALOGV("requesting bitrate to be changed to %d", bitrate);
    AMediaCodec_setParameters(mCodec, format);
}

bool CodecEncoderTest::testSimpleEncode(const char* encoder, const char* srcPath) {
    bool isPass = true;
    setUpSource(srcPath);
    if (!mInputData) return false;
    setUpParams(INT32_MAX);
    /* TODO(b/149027258) */
    if (true) mSaveToMem = false;
    else mSaveToMem = true;
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    const bool boolStates[]{true, false};
    for (int i = 0; i < mFormats.size() && isPass; i++) {
        AMediaFormat* format = mFormats[i];
        initFormat(format);
        int loopCounter = 0;
        for (auto eosType : boolStates) {
            if (!isPass) break;
            for (auto isAsync : boolStates) {
                if (!isPass) break;
                char log[1000];
                snprintf(log, sizeof(log),
                         "format: %s \n codec: %s, file: %s, mode: %s, eos type: %s:: ",
                         AMediaFormat_toString(format), encoder, srcPath,
                         (isAsync ? "async" : "sync"),
                         (eosType ? "eos with last frame" : "eos separate"));
                mOutputBuff = loopCounter == 0 ? ref : test;
                mOutputBuff->reset();
                /* TODO(b/147348711) */
                /* Instead of create and delete codec at every iteration, we would like to create
                 * once and use it for all iterations and delete before exiting */
                mCodec = AMediaCodec_createCodecByName(encoder);
                if (!mCodec) {
                    ALOGE("%s unable to create media codec by name %s", log, encoder);
                    isPass = false;
                    continue;
                }
                char* name = nullptr;
                if (AMEDIA_OK == AMediaCodec_getName(mCodec, &name)) {
                    if (!name || strcmp(name, encoder) != 0) {
                        ALOGE("%s error codec-name act/got: %s/%s", log, name, encoder);
                        if (name) AMediaCodec_releaseName(mCodec, name);
                        return false;
                    }
                } else {
                    ALOGE("AMediaCodec_getName failed unexpectedly");
                    return false;
                }
                if (name) AMediaCodec_releaseName(mCodec, name);
                if (!configureCodec(format, isAsync, eosType, true)) return false;
                CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
                if (!doWork(INT32_MAX)) return false;
                if (!queueEOS()) return false;
                if (!waitForAllOutputs()) return false;
                CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
                CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
                mCodec = nullptr;
                CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
                CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
                CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
                CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log,
                          "input cnt != output cnt", isPass);
                CHECK_ERR((loopCounter != 0 && !ref->equals(test)), log, "output is flaky", isPass);
                CHECK_ERR((loopCounter == 0 && mIsAudio &&
                           !ref->isPtsStrictlyIncreasing(mPrevOutputPts)),
                          log, "pts is not strictly increasing", isPass);
                CHECK_ERR((loopCounter == 0 && !mIsAudio &&
                           !ref->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                          log, "input pts list and output pts list are not identical", isPass);
                loopCounter++;
            }
        }
    }
    return isPass;
}

bool CodecEncoderTest::testFlush(const char* encoder, const char* srcPath) {
    bool isPass = true;
    setUpSource(srcPath);
    if (!mInputData) return false;
    setUpParams(1);
    mOutputBuff = &mTestBuff;
    AMediaFormat* format = mFormats[0];
    initFormat(format);
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log),
                 "format: %s \n codec: %s, file: %s, mode: %s:: ", AMediaFormat_toString(format),
                 encoder, srcPath, (isAsync ? "async" : "sync"));
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(encoder);
        if (!mCodec) {
            ALOGE("unable to create media codec by name %s", encoder);
            isPass = false;
            continue;
        }
        if (!configureCodec(format, isAsync, true, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");

        /* test flush in running state before queuing input */
        if (!flushCodec()) return false;
        mOutputBuff->reset();
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!doWork(23)) return false;
        CHECK_ERR((!mOutputBuff->isPtsStrictlyIncreasing(mPrevOutputPts)), log,
                  "pts is not strictly increasing", isPass);
        if (!isPass) continue;

        /* test flush in running state */
        if (!flushCodec()) return false;
        mOutputBuff->reset();
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((mIsAudio && !mOutputBuff->isPtsStrictlyIncreasing(mPrevOutputPts)), log,
                  "pts is not strictly increasing", isPass);
        CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR((!mIsAudio && !mOutputBuff->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                  log, "input pts list and output pts list are not identical", isPass);
        if (!isPass) continue;

        /* test flush in eos state */
        if (!flushCodec()) return false;
        mOutputBuff->reset();
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((mIsAudio && !mOutputBuff->isPtsStrictlyIncreasing(mPrevOutputPts)), log,
                  "pts is not strictly increasing", isPass);
        CHECK_ERR(!mIsAudio && (mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR(!mIsAudio && (!mOutputBuff->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                  log, "input pts list and output pts list are not identical", isPass);
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
    }
    return isPass;
}

bool CodecEncoderTest::testReconfigure(const char* encoder, const char* srcPath) {
    bool isPass = true;
    setUpSource(srcPath);
    if (!mInputData) return false;
    setUpParams(2);
    auto configRef = &mReconfBuff;
    if (mFormats.size() > 1) {
        auto format = mFormats[1];
        if (!encodeToMemory(srcPath, encoder, INT32_MAX, format, configRef)) {
            ALOGE("encodeToMemory failed for file: %s codec: %s \n format: %s", srcPath, encoder,
                  AMediaFormat_toString(format));
            return false;
        }
        CHECK_ERR(mIsAudio && (!configRef->isPtsStrictlyIncreasing(mPrevOutputPts)), "",
                  "pts is not strictly increasing", isPass);
        CHECK_ERR(!mIsAudio && (!configRef->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                  "", "input pts list and output pts list are not identical", isPass);
        if (!isPass) return false;
    }
    auto format = mFormats[0];
    auto ref = &mRefBuff;
    if (!encodeToMemory(srcPath, encoder, INT32_MAX, format, ref)) {
        ALOGE("encodeToMemory failed for file: %s codec: %s \n format: %s", srcPath, encoder,
              AMediaFormat_toString(format));
        return false;
    }
    CHECK_ERR(mIsAudio && (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)), "",
              "pts is not strictly increasing", isPass);
    CHECK_ERR(!mIsAudio && (!ref->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)), "",
              "input pts list and output pts list are not identical", isPass);
    if (!isPass) return false;

    auto test = &mTestBuff;
    mOutputBuff = test;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log),
                 "format: %s \n codec: %s, file: %s, mode: %s:: ", AMediaFormat_toString(format),
                 encoder, srcPath, (isAsync ? "async" : "sync"));
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(encoder);
        if (!mCodec) {
            ALOGE("%s unable to create media codec by name %s", log, encoder);
            isPass = false;
            continue;
        }
        if (!configureCodec(format, isAsync, true, true)) return false;
        /* test reconfigure in init state */
        if (!reConfigureCodec(format, !isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");

        /* test reconfigure in running state before queuing input */
        if (!reConfigureCodec(format, !isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (!doWork(23)) return false;

        /* test reconfigure codec in running state */
        if (!reConfigureCodec(format, isAsync, true, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");

        /* TODO(b/149027258) */
        if (true) mSaveToMem = false;
        else mSaveToMem = true;
        test->reset();
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR((!ref->equals(test)), log, "output is flaky", isPass);
        if (!isPass) continue;

        /* test reconfigure codec at eos state */
        if (!reConfigureCodec(format, !isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        test->reset();
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR((!ref->equals(test)), log, "output is flaky", isPass);

        /* test reconfigure codec for new format */
        if (mFormats.size() > 1) {
            if (!reConfigureCodec(mFormats[1], isAsync, false, true)) return false;
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
            test->reset();
            if (!doWork(INT32_MAX)) return false;
            if (!queueEOS()) return false;
            if (!waitForAllOutputs()) return false;
            CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
            CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
            CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
            CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
            CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                      isPass);
            CHECK_ERR((!configRef->equals(test)), log, "output is flaky", isPass);
        }
        mSaveToMem = false;
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
    }
    return isPass;
}

bool CodecEncoderTest::testOnlyEos(const char* encoder) {
    bool isPass = true;
    setUpParams(1);
    /* TODO(b/149027258) */
    if (true) mSaveToMem = false;
    else mSaveToMem = true;
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    const bool boolStates[]{true, false};
    AMediaFormat* format = mFormats[0];
    int loopCounter = 0;
    for (int k = 0; (k < (sizeof(boolStates) / sizeof(boolStates[0]))) && isPass; k++) {
        bool isAsync = boolStates[k];
        char log[1000];
        snprintf(log, sizeof(log),
                 "format: %s \n codec: %s, mode: %s:: ", AMediaFormat_toString(format), encoder,
                 (isAsync ? "async" : "sync"));
        mOutputBuff = loopCounter == 0 ? ref : test;
        mOutputBuff->reset();
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(encoder);
        if (!mCodec) {
            ALOGE("unable to create media codec by name %s", encoder);
            isPass = false;
            continue;
        }
        if (!configureCodec(format, isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR(loopCounter != 0 && (!ref->equals(test)), log, "output is flaky", isPass);
        CHECK_ERR(loopCounter == 0 && mIsAudio && (!ref->isPtsStrictlyIncreasing(mPrevOutputPts)),
                  log, "pts is not strictly increasing", isPass);
        CHECK_ERR(loopCounter == 0 && !mIsAudio &&
                  (!ref->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                  log, "input pts list and output pts list are not identical", isPass);
        loopCounter++;
    }
    return isPass;
}

bool CodecEncoderTest::testSetForceSyncFrame(const char* encoder, const char* srcPath) {
    bool isPass = true;
    setUpSource(srcPath);
    if (!mInputData) return false;
    setUpParams(1);
    AMediaFormat* format = mFormats[0];
    AMediaFormat_setFloat(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 500.f);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mWidth);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mHeight);
    // Maximum allowed key frame interval variation from the target value.
    int kMaxKeyFrameIntervalVariation = 3;
    int kKeyFrameInterval = 2;  // force key frame every 2 seconds.
    int kKeyFramePos = mDefFrameRate * kKeyFrameInterval;
    int kNumKeyFrameRequests = 7;
    AMediaFormat* params = AMediaFormat_new();
    mFormats.push_back(params);
    mOutputBuff = &mTestBuff;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log),
                 "format: %s \n codec: %s, file: %s, mode: %s:: ", AMediaFormat_toString(format),
                 encoder, srcPath, (isAsync ? "async" : "sync"));
        mOutputBuff->reset();
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(encoder);
        if (!mCodec) {
            ALOGE("%s unable to create media codec by name %s", log, encoder);
            isPass = false;
            continue;
        }
        if (!configureCodec(format, isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        for (int i = 0; i < kNumKeyFrameRequests; i++) {
            if (!doWork(kKeyFramePos)) return false;
            assert(!mSawInputEOS);
            forceSyncFrame(params);
            mNumBytesSubmitted = 0;
        }
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR((!mOutputBuff->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)), log,
                  "input pts list and output pts list are not identical", isPass);
        CHECK_ERR((mNumSyncFramesReceived < kNumKeyFrameRequests), log,
                  "Num Sync Frames Received != Num Key Frame Requested", isPass);
        ALOGD("mNumSyncFramesReceived %d", mNumSyncFramesReceived);
        for (int i = 0, expPos = 0, index = 0; i < kNumKeyFrameRequests; i++) {
            int j = index;
            for (; j < mSyncFramesPos.size(); j++) {
                // Check key frame intervals:
                // key frame position should not be greater than target value + 3
                // key frame position should not be less than target value - 3
                if (abs(expPos - mSyncFramesPos.at(j)) <= kMaxKeyFrameIntervalVariation) {
                    index = j;
                    break;
                }
            }
            if (j == mSyncFramesPos.size()) {
                ALOGW("requested key frame at frame index %d none found near by", expPos);
            }
            expPos += kKeyFramePos;
        }
    }
    return isPass;
}

bool CodecEncoderTest::testAdaptiveBitRate(const char* encoder, const char* srcPath) {
    bool isPass = true;
    setUpSource(srcPath);
    if (!mInputData) return false;
    setUpParams(1);
    AMediaFormat* format = mFormats[0];
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &mWidth);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &mHeight);
    int kAdaptiveBitrateInterval = 3;  // change bitrate every 3 seconds.
    int kAdaptiveBitrateDurationFrame = mDefFrameRate * kAdaptiveBitrateInterval;
    int kBitrateChangeRequests = 7;
    AMediaFormat* params = AMediaFormat_new();
    mFormats.push_back(params);
    // Setting in CBR Mode
    AMediaFormat_setInt32(format, TBD_AMEDIAFORMAT_KEY_BIT_RATE_MODE, kBitrateModeConstant);
    mOutputBuff = &mTestBuff;
    mSaveToMem = true;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!isPass) break;
        char log[1000];
        snprintf(log, sizeof(log),
                 "format: %s \n codec: %s, file: %s, mode: %s:: ", AMediaFormat_toString(format),
                 encoder, srcPath, (isAsync ? "async" : "sync"));
        mOutputBuff->reset();
        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mCodec = AMediaCodec_createCodecByName(encoder);
        if (!mCodec) {
            ALOGE("%s unable to create media codec by name %s", log, encoder);
            isPass = false;
            continue;
        }
        if (!configureCodec(format, isAsync, false, true)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        int expOutSize = 0;
        int bitrate;
        AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, &bitrate);
        for (int i = 0; i < kBitrateChangeRequests; i++) {
            if (!doWork(kAdaptiveBitrateDurationFrame)) return false;
            assert(!mSawInputEOS);
            expOutSize += kAdaptiveBitrateInterval * bitrate;
            if ((i & 1) == 1) bitrate *= 2;
            else bitrate /= 2;
            updateBitrate(params, bitrate);
            mNumBytesSubmitted = 0;
        }
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_delete(mCodec), "AMediaCodec_delete failed");
        mCodec = nullptr;
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mInputCount), log, "queued 0 inputs", isPass);
        CHECK_ERR((0 == mOutputCount), log, "received 0 outputs", isPass);
        CHECK_ERR((!mIsAudio && mInputCount != mOutputCount), log, "input cnt != output cnt",
                  isPass);
        CHECK_ERR((!mOutputBuff->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)), log,
                  "input pts list and output pts list are not identical", isPass);
        /* TODO: validate output br with sliding window constraints Sec 5.2 cdd */
        int outSize = mOutputBuff->getOutStreamSize() * 8;
        float brDev = abs(expOutSize - outSize) * 100.0f / expOutSize;
        ALOGD("%s relative bitrate error is %f %%", log, brDev);
        if (brDev > 50) {
            ALOGE("%s relative bitrate error is is too large %f %%", log, brDev);
            return false;
        }
    }
    return isPass;
}

static jboolean nativeTestSimpleEncode(JNIEnv* env, jobject, jstring jEncoder, jstring jsrcPath,
                                       jstring jMime, jintArray jList0, jintArray jList1,
                                       jintArray jList2, jint colorFormat) {
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    bool isPass = codecEncoderTest->testSimpleEncode(cEncoder, csrcPath);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFlush(JNIEnv* env, jobject, jstring jEncoder, jstring jsrcPath,
                                jstring jMime, jintArray jList0, jintArray jList1, jintArray jList2,
                                jint colorFormat) {
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    bool isPass = codecEncoderTest->testFlush(cEncoder, csrcPath);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReconfigure(JNIEnv* env, jobject, jstring jEncoder, jstring jsrcPath,
                                      jstring jMime, jintArray jList0, jintArray jList1,
                                      jintArray jList2, jint colorFormat) {
    bool isPass;
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    isPass = codecEncoderTest->testReconfigure(cEncoder, csrcPath);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetForceSyncFrame(JNIEnv* env, jobject, jstring jEncoder,
                                            jstring jsrcPath, jstring jMime, jintArray jList0,
                                            jintArray jList1, jintArray jList2, jint colorFormat) {
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    bool isPass = codecEncoderTest->testSetForceSyncFrame(cEncoder, csrcPath);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestAdaptiveBitRate(JNIEnv* env, jobject, jstring jEncoder, jstring jsrcPath,
                                          jstring jMime, jintArray jList0, jintArray jList1,
                                          jintArray jList2, jint colorFormat) {
    const char* csrcPath = env->GetStringUTFChars(jsrcPath, nullptr);
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    bool isPass = codecEncoderTest->testAdaptiveBitRate(cEncoder, csrcPath);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    env->ReleaseStringUTFChars(jsrcPath, csrcPath);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestOnlyEos(JNIEnv* env, jobject, jstring jEncoder, jstring jMime,
                                  jintArray jList0, jintArray jList1, jintArray jList2,
                                  jint colorFormat) {
    const char* cmime = env->GetStringUTFChars(jMime, nullptr);
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    jsize cLen0 = env->GetArrayLength(jList0);
    jint* cList0 = env->GetIntArrayElements(jList0, nullptr);
    jsize cLen1 = env->GetArrayLength(jList1);
    jint* cList1 = env->GetIntArrayElements(jList1, nullptr);
    jsize cLen2 = env->GetArrayLength(jList2);
    jint* cList2 = env->GetIntArrayElements(jList2, nullptr);
    auto codecEncoderTest = new CodecEncoderTest(cmime, cList0, cLen0, cList1, cLen1, cList2, cLen2,
                                                 (int)colorFormat);
    bool isPass = codecEncoderTest->testOnlyEos(cEncoder);
    delete codecEncoderTest;
    env->ReleaseIntArrayElements(jList0, cList0, 0);
    env->ReleaseIntArrayElements(jList1, cList1, 0);
    env->ReleaseIntArrayElements(jList2, cList2, 0);
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jMime, cmime);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsEncoderTest(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSimpleEncode",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestSimpleEncode},
            {"nativeTestFlush", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestFlush},
            {"nativeTestReconfigure",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestReconfigure},
            {"nativeTestSetForceSyncFrame",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestSetForceSyncFrame},
            {"nativeTestAdaptiveBitRate",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestAdaptiveBitRate},
            {"nativeTestOnlyEos", "(Ljava/lang/String;Ljava/lang/String;[I[I[II)Z",
             (void*)nativeTestOnlyEos},
    };
    jclass c = env->FindClass("android/mediav2/cts/CodecEncoderTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}

extern int registerAndroidMediaV2CtsCodecUnitTest(JNIEnv* env);
extern int registerAndroidMediaV2CtsDecoderTest(JNIEnv* env);
extern int registerAndroidMediaV2CtsDecoderSurfaceTest(JNIEnv* env);
extern int registerAndroidMediaV2CtsEncoderSurfaceTest(JNIEnv* env);

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsCodecUnitTest(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsEncoderTest(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsDecoderTest(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsDecoderSurfaceTest(env) != JNI_OK) return JNI_ERR;
    if (registerAndroidMediaV2CtsEncoderSurfaceTest(env) != JNI_OK) return JNI_ERR;
    return JNI_VERSION_1_6;
}
