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
#define LOG_TAG "NativeCodecTestBase"
#include <log/log.h>

#include "NativeCodecTestBase.h"

static void onAsyncInputAvailable(AMediaCodec* codec, void* userdata, int32_t index) {
    (void)codec;
    assert(index >= 0);
    auto* aSyncHandle = static_cast<CodecAsyncHandler*>(userdata);
    callbackObject element{index};
    aSyncHandle->pushToInputList(element);
}

static void onAsyncOutputAvailable(AMediaCodec* codec, void* userdata, int32_t index,
                                   AMediaCodecBufferInfo* bufferInfo) {
    (void)codec;
    assert(index >= 0);
    auto* aSyncHandle = static_cast<CodecAsyncHandler*>(userdata);
    callbackObject element{index, bufferInfo};
    aSyncHandle->pushToOutputList(element);
}

static void onAsyncFormatChanged(AMediaCodec* codec, void* userdata, AMediaFormat* format) {
    (void)codec;
    auto* aSyncHandle = static_cast<CodecAsyncHandler*>(userdata);
    aSyncHandle->setOutputFormat(format);
    ALOGI("Output format changed: %s", AMediaFormat_toString(format));
}

static void onAsyncError(AMediaCodec* codec, void* userdata, media_status_t error,
                         int32_t actionCode, const char* detail) {
    (void)codec;
    auto* aSyncHandle = static_cast<CodecAsyncHandler*>(userdata);
    aSyncHandle->setError(true);
    ALOGE("received media codec error: %s , code : %d , action code: %d ", detail, error,
          actionCode);
}

CodecAsyncHandler::CodecAsyncHandler() {
    mOutFormat = nullptr;
    mSignalledOutFormatChanged = false;
    mSignalledError = false;
}

CodecAsyncHandler::~CodecAsyncHandler() {
    if (mOutFormat) {
        AMediaFormat_delete(mOutFormat);
        mOutFormat = nullptr;
    }
}

void CodecAsyncHandler::pushToInputList(callbackObject element) {
    std::unique_lock<std::mutex> lock{mMutex};
    mCbInputQueue.push_back(element);
    mCondition.notify_all();
}

void CodecAsyncHandler::pushToOutputList(callbackObject element) {
    std::unique_lock<std::mutex> lock{mMutex};
    mCbOutputQueue.push_back(element);
    mCondition.notify_all();
}

callbackObject CodecAsyncHandler::getInput() {
    callbackObject element{-1};
    std::unique_lock<std::mutex> lock{mMutex};
    while (!mSignalledError) {
        if (mCbInputQueue.empty()) {
            mCondition.wait(lock);
        } else {
            element = mCbInputQueue.front();
            mCbInputQueue.pop_front();
            break;
        }
    }
    return element;
}

callbackObject CodecAsyncHandler::getOutput() {
    callbackObject element;
    std::unique_lock<std::mutex> lock{mMutex};
    while (!mSignalledError) {
        if (mCbOutputQueue.empty()) {
            mCondition.wait(lock);
        } else {
            element = mCbOutputQueue.front();
            mCbOutputQueue.pop_front();
            break;
        }
    }
    return element;
}

callbackObject CodecAsyncHandler::getWork() {
    callbackObject element;
    std::unique_lock<std::mutex> lock{mMutex};
    while (!mSignalledError) {
        if (mCbInputQueue.empty() && mCbOutputQueue.empty()) {
            mCondition.wait(lock);
        } else {
            if (!mCbOutputQueue.empty()) {
                element = mCbOutputQueue.front();
                mCbOutputQueue.pop_front();
                break;
            } else {
                element = mCbInputQueue.front();
                mCbInputQueue.pop_front();
                break;
            }
        }
    }
    return element;
}

bool CodecAsyncHandler::isInputQueueEmpty() {
    std::unique_lock<std::mutex> lock{mMutex};
    return mCbInputQueue.empty();
}

void CodecAsyncHandler::clearQueues() {
    std::unique_lock<std::mutex> lock{mMutex};
    mCbInputQueue.clear();
    mCbOutputQueue.clear();
}

void CodecAsyncHandler::setOutputFormat(AMediaFormat* format) {
    assert(format != nullptr);
    if (mOutFormat) {
        AMediaFormat_delete(mOutFormat);
        mOutFormat = nullptr;
    }
    mOutFormat = format;
    mSignalledOutFormatChanged = true;
}

AMediaFormat* CodecAsyncHandler::getOutputFormat() {
    return mOutFormat;
}

bool CodecAsyncHandler::hasOutputFormatChanged() {
    return mSignalledOutFormatChanged;
}

void CodecAsyncHandler::setError(bool status) {
    std::unique_lock<std::mutex> lock{mMutex};
    mSignalledError = status;
    mCondition.notify_all();
}

bool CodecAsyncHandler::getError() {
    return mSignalledError;
}

void CodecAsyncHandler::resetContext() {
    clearQueues();
    if (mOutFormat) {
        AMediaFormat_delete(mOutFormat);
        mOutFormat = nullptr;
    }
    mSignalledOutFormatChanged = false;
    mSignalledError = false;
}

media_status_t CodecAsyncHandler::setCallBack(AMediaCodec* codec, bool isCodecInAsyncMode) {
    media_status_t status = AMEDIA_OK;
    if (isCodecInAsyncMode) {
        AMediaCodecOnAsyncNotifyCallback callBack = {onAsyncInputAvailable, onAsyncOutputAvailable,
                                                     onAsyncFormatChanged, onAsyncError};
        status = AMediaCodec_setAsyncNotifyCallback(codec, callBack, this);
    }
    return status;
}

uint32_t OutputManager::adler32(const uint8_t* input, int offset, int len) {
    constexpr uint32_t modAdler = 65521;
    constexpr uint32_t overflowLimit = 5500;
    uint32_t a = 1;
    uint32_t b = 0;
    for (int i = offset, count = 0; i < len; i++) {
        a += input[i];
        b += a;
        count++;
        if (count > overflowLimit) {
            a %= modAdler;
            b %= modAdler;
            count = 0;
        }
    }
    return b * 65536 + a;
}

bool OutputManager::isPtsStrictlyIncreasing(int64_t lastPts) {
    bool result = true;
    for (auto it1 = outPtsArray.cbegin(); it1 < outPtsArray.cend(); it1++) {
        if (lastPts < *it1) {
            lastPts = *it1;
        } else {
            ALOGE("Timestamp ordering check failed: last timestamp: %d / current timestamp: %d",
                  (int)lastPts, (int)*it1);
            result = false;
            break;
        }
    }
    return result;
}

bool OutputManager::isOutPtsListIdenticalToInpPtsList(bool requireSorting) {
    bool isEqual = true;
    std::sort(inpPtsArray.begin(), inpPtsArray.end());
    if (requireSorting) {
        std::sort(outPtsArray.begin(), outPtsArray.end());
    }
    if (outPtsArray != inpPtsArray) {
        if (outPtsArray.size() != inpPtsArray.size()) {
            ALOGE("input and output presentation timestamp list sizes are not identical sizes "
                  "exp/rec %d/%d", (int)inpPtsArray.size(), (int)outPtsArray.size());
            isEqual = false;
        } else {
            int count = 0;
            for (auto it1 = outPtsArray.cbegin(), it2 = inpPtsArray.cbegin();
                 it1 < outPtsArray.cend(); it1++, it2++) {
                if (*it1 != *it2) {
                    ALOGE("input output pts mismatch, exp/rec %d/%d", (int)*it2, (int)*it1);
                    count++;
                }
                if (count == 20) {
                    ALOGE("stopping after 20 mismatches ... ");
                    break;
                }
            }
            if (count != 0) isEqual = false;
        }
    }
    return isEqual;
}

bool OutputManager::equals(const OutputManager* that) {
    if (this == that) return true;
    if (outPtsArray != that->outPtsArray) {
        if (outPtsArray.size() != that->outPtsArray.size()) {
            ALOGE("ref and test outputs presentation timestamp arrays are of unequal sizes %d, %d",
                  (int)outPtsArray.size(), (int)that->outPtsArray.size());
            return false;
        } else {
            int count = 0;
            for (auto it1 = outPtsArray.cbegin(), it2 = that->outPtsArray.cbegin();
                 it1 < outPtsArray.cend(); it1++, it2++) {
                if (*it1 != *it2) {
                    ALOGE("presentation timestamp exp/rec %d/%d", (int)*it1, (int)*it2);
                    count++;
                }
                if (count == 20) {
                    ALOGE("stopping after 20 mismatches ... ");
                    break;
                }
            }
            if (count != 0) return false;
        }
    }
    if (memory != that->memory) {
        if (memory.size() != that->memory.size()) {
            ALOGE("ref and test outputs decoded buffer are of unequal sizes %d, %d",
                  (int)memory.size(), (int)that->memory.size());
            return false;
        } else {
            int count = 0;
            for (auto it1 = memory.cbegin(), it2 = that->memory.cbegin(); it1 < memory.cend();
                 it1++, it2++) {
                if (*it1 != *it2) {
                    ALOGE("decoded sample exp/rec %d/%d", (int)*it1, (int)*it2);
                    count++;
                }
                if (count == 20) {
                    ALOGE("stopping after 20 mismatches ... ");
                    break;
                }
            }
            if (count != 0) return false;
        }
    }
    if (checksum != that->checksum) {
        if (checksum.size() != that->checksum.size()) {
            ALOGE("ref and test outputs checksum arrays are of unequal sizes %d, %d",
                  (int)checksum.size(), (int)that->checksum.size());
            return false;
        } else {
            int count = 0;
            for (auto it1 = checksum.cbegin(), it2 = that->checksum.cbegin(); it1 < checksum.cend();
                 it1++, it2++) {
                if (*it1 != *it2) {
                    ALOGE("adler32 checksum exp/rec %u/%u", (int)*it1, (int)*it2);
                    count++;
                }
                if (count == 20) {
                    ALOGE("stopping after 20 mismatches ... ");
                    break;
                }
            }
            if (count != 0) return false;
        }
    }
    return true;
}

float OutputManager::getRmsError(uint8_t* refData, int length) {
    long totalErrorSquared = 0;
    if (length != memory.size()) return -1.0F;
    if ((length % 2) != 0) return -1.0F;
    auto* testData = new uint8_t[length];
    std::copy(memory.begin(), memory.end(), testData);
    auto* testDataReinterpret = reinterpret_cast<int16_t*>(testData);
    auto* refDataReinterpret = reinterpret_cast<int16_t*>(refData);
    for (int i = 0; i < length / 2; i++) {
        int d = testDataReinterpret[i] - refDataReinterpret[i];
        totalErrorSquared += d * d;
    }
    delete[] testData;
    long avgErrorSquared = (totalErrorSquared / (length / 2));
    return (float)sqrt(avgErrorSquared);
}

CodecTestBase::CodecTestBase(const char* mime) {
    mMime = mime;
    mIsAudio = strncmp(mime, "audio/", strlen("audio/")) == 0;
    mIsCodecInAsyncMode = false;
    mSawInputEOS = false;
    mSawOutputEOS = false;
    mSignalEOSWithLastFrame = false;
    mInputCount = 0;
    mOutputCount = 0;
    mPrevOutputPts = INT32_MIN;
    mSignalledOutFormatChanged = false;
    mOutFormat = nullptr;
    mSaveToMem = false;
    mOutputBuff = nullptr;
    mCodec = nullptr;
}

CodecTestBase::~CodecTestBase() {
    if (mOutFormat) {
        AMediaFormat_delete(mOutFormat);
        mOutFormat = nullptr;
    }
    if (mCodec) {
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }
}

bool CodecTestBase::configureCodec(AMediaFormat* format, bool isAsync, bool signalEOSWithLastFrame,
                                   bool isEncoder) {
    resetContext(isAsync, signalEOSWithLastFrame);
    CHECK_STATUS(mAsyncHandle.setCallBack(mCodec, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback failed");
    CHECK_STATUS(AMediaCodec_configure(mCodec, format, nullptr, nullptr,
                                       isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0),
                 "AMediaCodec_configure failed");
    return true;
}

bool CodecTestBase::flushCodec() {
    CHECK_STATUS(AMediaCodec_flush(mCodec), "AMediaCodec_flush failed");
    // TODO(b/147576107): is it ok to clearQueues right away or wait for some signal
    mAsyncHandle.clearQueues();
    mSawInputEOS = false;
    mSawOutputEOS = false;
    mInputCount = 0;
    mOutputCount = 0;
    mPrevOutputPts = INT32_MIN;
    return true;
}

bool CodecTestBase::reConfigureCodec(AMediaFormat* format, bool isAsync,
                                     bool signalEOSWithLastFrame, bool isEncoder) {
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    return configureCodec(format, isAsync, signalEOSWithLastFrame, isEncoder);
}

void CodecTestBase::resetContext(bool isAsync, bool signalEOSWithLastFrame) {
    mAsyncHandle.resetContext();
    mIsCodecInAsyncMode = isAsync;
    mSawInputEOS = false;
    mSawOutputEOS = false;
    mSignalEOSWithLastFrame = signalEOSWithLastFrame;
    mInputCount = 0;
    mOutputCount = 0;
    mPrevOutputPts = INT32_MIN;
    mSignalledOutFormatChanged = false;
    if (mOutFormat) {
        AMediaFormat_delete(mOutFormat);
        mOutFormat = nullptr;
    }
}

bool CodecTestBase::enqueueEOS(size_t bufferIndex) {
    if (!hasSeenError() && !mSawInputEOS) {
        CHECK_STATUS(AMediaCodec_queueInputBuffer(mCodec, bufferIndex, 0, 0, 0,
                                                  AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM),
                     "AMediaCodec_queueInputBuffer failed");
        mSawInputEOS = true;
        ALOGV("Queued End of Stream");
    }
    return !hasSeenError();
}

bool CodecTestBase::doWork(int frameLimit) {
    bool isOk = true;
    int frameCnt = 0;
    if (mIsCodecInAsyncMode) {
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (!hasSeenError() && isOk && !mSawInputEOS && frameCnt < frameLimit) {
            callbackObject element = mAsyncHandle.getWork();
            if (element.bufferIndex >= 0) {
                if (element.isInput) {
                    isOk = enqueueInput(element.bufferIndex);
                    frameCnt++;
                } else {
                    isOk = dequeueOutput(element.bufferIndex, &element.bufferInfo);
                }
            }
        }
    } else {
        AMediaCodecBufferInfo outInfo;
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (isOk && !mSawInputEOS && frameCnt < frameLimit) {
            ssize_t oBufferID = AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs);
            if (oBufferID >= 0) {
                isOk = dequeueOutput(oBufferID, &outInfo);
            } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                if (mOutFormat) {
                    AMediaFormat_delete(mOutFormat);
                    mOutFormat = nullptr;
                }
                mOutFormat = AMediaCodec_getOutputFormat(mCodec);
                mSignalledOutFormatChanged = true;
            } else if (oBufferID == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            } else {
                ALOGE("unexpected return value from *_dequeueOutputBuffer: %d", (int)oBufferID);
                return false;
            }
            ssize_t iBufferId = AMediaCodec_dequeueInputBuffer(mCodec, kQDeQTimeOutUs);
            if (iBufferId >= 0) {
                isOk = enqueueInput(iBufferId);
                frameCnt++;
            } else if (iBufferId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else {
                ALOGE("unexpected return value from *_dequeueInputBuffer: %d", (int)iBufferId);
                return false;
            }
        }
    }
    return !hasSeenError() && isOk;
}

bool CodecTestBase::queueEOS() {
    bool isOk = true;
    if (mIsCodecInAsyncMode) {
        if (!hasSeenError() && isOk && !mSawInputEOS) {
            callbackObject element = mAsyncHandle.getInput();
            if (element.bufferIndex >= 0) {
                isOk = enqueueEOS(element.bufferIndex);
            }
        }
    } else {
        if (!mSawInputEOS) {
            int bufferIndex = AMediaCodec_dequeueInputBuffer(mCodec, -1);
            if (bufferIndex >= 0) {
                isOk = enqueueEOS(bufferIndex);
            } else {
                ALOGE("unexpected return value from *_dequeueInputBuffer: %d", (int)bufferIndex);
                return false;
            }
        }
    }
    return !hasSeenError() && isOk;
}

bool CodecTestBase::waitForAllOutputs() {
    bool isOk = true;
    if (mIsCodecInAsyncMode) {
        while (!hasSeenError() && isOk && !mSawOutputEOS) {
            callbackObject element = mAsyncHandle.getOutput();
            if (element.bufferIndex >= 0) {
                isOk = dequeueOutput(element.bufferIndex, &element.bufferInfo);
            }
        }
    } else {
        AMediaCodecBufferInfo outInfo;
        while (!mSawOutputEOS) {
            int bufferID = AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs);
            if (bufferID >= 0) {
                isOk = dequeueOutput(bufferID, &outInfo);
            } else if (bufferID == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                if (mOutFormat) {
                    AMediaFormat_delete(mOutFormat);
                    mOutFormat = nullptr;
                }
                mOutFormat = AMediaCodec_getOutputFormat(mCodec);
                mSignalledOutFormatChanged = true;
            } else if (bufferID == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else if (bufferID == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            } else {
                ALOGE("unexpected return value from *_dequeueOutputBuffer: %d", (int)bufferID);
                return false;
            }
        }
    }
    return !hasSeenError() && isOk;
}

int CodecTestBase::getWidth(AMediaFormat* format) {
    int width = -1;
    int cropLeft, cropRight, cropTop, cropBottom;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
    if (AMediaFormat_getRect(format, "crop", &cropLeft, &cropTop, &cropRight, &cropBottom) ||
        (AMediaFormat_getInt32(format, "crop-left", &cropLeft) &&
        AMediaFormat_getInt32(format, "crop-right", &cropRight))) {
        width = cropRight + 1 - cropLeft;
    }
    return width;
}

int CodecTestBase::getHeight(AMediaFormat* format) {
    int height = -1;
    int cropLeft, cropRight, cropTop, cropBottom;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
    if (AMediaFormat_getRect(format, "crop", &cropLeft, &cropTop, &cropRight, &cropBottom) ||
        (AMediaFormat_getInt32(format, "crop-top", &cropTop) &&
        AMediaFormat_getInt32(format, "crop-bottom", &cropBottom))) {
        height = cropBottom + 1 - cropTop;
    }
    return height;
}

bool CodecTestBase::isFormatSimilar(AMediaFormat* inpFormat, AMediaFormat* outFormat) {
    const char *refMime = nullptr, *testMime = nullptr;
    bool hasRefMime = AMediaFormat_getString(inpFormat, AMEDIAFORMAT_KEY_MIME, &refMime);
    bool hasTestMime = AMediaFormat_getString(outFormat, AMEDIAFORMAT_KEY_MIME, &testMime);

    if (!hasRefMime || !hasTestMime) return false;
    if (!strncmp(refMime, "audio/", strlen("audio/"))) {
        int32_t refSampleRate = -1;
        int32_t testSampleRate = -2;
        int32_t refNumChannels = -1;
        int32_t testNumChannels = -2;
        AMediaFormat_getInt32(inpFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &refSampleRate);
        AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, &testSampleRate);
        AMediaFormat_getInt32(inpFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &refNumChannels);
        AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &testNumChannels);
        return refNumChannels == testNumChannels && refSampleRate == testSampleRate &&
               (strncmp(testMime, "audio/", strlen("audio/")) == 0);
    } else if (!strncmp(refMime, "video/", strlen("video/"))) {
        int32_t refWidth = getWidth(inpFormat);
        int32_t testWidth = getWidth(outFormat);
        int32_t refHeight = getHeight(inpFormat);
        int32_t testHeight = getHeight(outFormat);
        return refWidth != -1 && refHeight != -1 && refWidth == testWidth &&
               refHeight == testHeight && (strncmp(testMime, "video/", strlen("video/")) == 0);
    }
    return true;
}
