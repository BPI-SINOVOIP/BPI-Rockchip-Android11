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

#ifndef MEDIACTSNATIVE_NATIVE_CODEC_TEST_BASE_H
#define MEDIACTSNATIVE_NATIVE_CODEC_TEST_BASE_H

#include <NdkMediaCodec.h>

#include <cmath>
#include <cstdint>
#include <list>
#include <mutex>
#include <vector>

#define CHECK_STATUS(status, str)                  \
    {                                              \
        media_status_t val = (status);             \
        if (AMEDIA_OK != val) {                    \
            ALOGE("%s with error %d", (str), val); \
            return false;                          \
        }                                          \
    }

#define CHECK_ERR(val, strA, strB, result) \
    if ((val)) {                           \
        (result) = false;                  \
        ALOGE("%s %s", (strA), (strB));    \
    }

struct callbackObject {
    AMediaCodecBufferInfo bufferInfo;
    int32_t bufferIndex;
    bool isInput;

    callbackObject(int32_t index, AMediaCodecBufferInfo* info)
        : bufferInfo{*info}, bufferIndex{index}, isInput{false} {}

    callbackObject(int32_t index) : bufferIndex{index}, isInput{true} {}

    callbackObject() : bufferIndex{-1}, isInput{false} {}
};

class CodecAsyncHandler {
  private:
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::list<callbackObject> mCbInputQueue;
    std::list<callbackObject> mCbOutputQueue;
    AMediaFormat* mOutFormat;
    bool mSignalledOutFormatChanged;
    volatile bool mSignalledError;

  public:
    CodecAsyncHandler();
    ~CodecAsyncHandler();
    void pushToInputList(callbackObject element);
    void pushToOutputList(callbackObject element);
    callbackObject getInput();
    callbackObject getOutput();
    callbackObject getWork();
    bool isInputQueueEmpty();
    void clearQueues();
    void setOutputFormat(AMediaFormat* format);
    AMediaFormat* getOutputFormat();
    bool hasOutputFormatChanged();
    void setError(bool status);
    bool getError();
    void resetContext();
    media_status_t setCallBack(AMediaCodec* codec, bool isCodecInAsyncMode);
};

class OutputManager {
  private:
    std::vector<int64_t> inpPtsArray;
    std::vector<int64_t> outPtsArray;
    std::vector<uint8_t> memory;
    std::vector<uint32_t> checksum;

    uint32_t adler32(const uint8_t* input, int offset, int len);

  public:
    void saveInPTS(int64_t pts) {
        // Add only Unique timeStamp, discarding any duplicate frame / non-display frame
        if(0 == std::count(inpPtsArray.begin(), inpPtsArray.end(), pts)) {
            inpPtsArray.push_back(pts);
        }
    }
    void saveOutPTS(int64_t pts) { outPtsArray.push_back(pts); }
    bool isPtsStrictlyIncreasing(int64_t lastPts);
    bool isOutPtsListIdenticalToInpPtsList(bool requireSorting);
    void saveToMemory(uint8_t* buf, AMediaCodecBufferInfo* info) {
        memory.insert(memory.end(), buf + info->offset, buf + info->size);
    }
    void saveChecksum(uint8_t* buf, AMediaCodecBufferInfo* info) {
        checksum.push_back(adler32(buf, info->offset, info->size));
    }
    void reset() {
        inpPtsArray.clear();
        outPtsArray.clear();
        memory.clear();
        checksum.clear();
    }
    bool equals(const OutputManager* that);
    float getRmsError(uint8_t* refData, int length);
    int getOutStreamSize() { return memory.size(); }
};

class CodecTestBase {
  protected:
    const long kQDeQTimeOutUs = 5000;
    const char* mMime;
    bool mIsAudio;
    CodecAsyncHandler mAsyncHandle;
    bool mIsCodecInAsyncMode;
    bool mSawInputEOS;
    bool mSawOutputEOS;
    bool mSignalEOSWithLastFrame;
    int mInputCount;
    int mOutputCount;
    int64_t mPrevOutputPts;
    bool mSignalledOutFormatChanged;
    AMediaFormat* mOutFormat;

    bool mSaveToMem;
    OutputManager* mOutputBuff;
    OutputManager mRefBuff;
    OutputManager mTestBuff;
    OutputManager mReconfBuff;

    AMediaCodec* mCodec;

    CodecTestBase(const char* mime);
    ~CodecTestBase();
    virtual bool configureCodec(AMediaFormat* format, bool isAsync, bool signalEOSWithLastFrame,
                                bool isEncoder);
    virtual bool flushCodec();
    bool reConfigureCodec(AMediaFormat* format, bool isAsync, bool signalEOSWithLastFrame,
                          bool isEncoder);
    virtual void resetContext(bool isAsync, bool signalEOSWithLastFrame);
    virtual bool enqueueInput(size_t bufferIndex) = 0;
    virtual bool dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* bufferInfo) = 0;
    bool enqueueEOS(size_t bufferIndex);
    bool doWork(int frameLimit);
    bool queueEOS();
    bool waitForAllOutputs();
    int getWidth(AMediaFormat* format);
    int getHeight(AMediaFormat* format);
    bool isFormatSimilar(AMediaFormat* inpFormat, AMediaFormat* outFormat);
    bool hasSeenError() { return mAsyncHandle.getError(); }
};

#endif  // MEDIACTSNATIVE_NATIVE_CODEC_TEST_BASE_H
