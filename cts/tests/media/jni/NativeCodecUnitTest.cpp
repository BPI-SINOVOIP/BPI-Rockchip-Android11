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
#define LOG_TAG "NativeCodecUnitTest"
#include <NdkMediaExtractor.h>
#include <jni.h>
#include <log/log.h>
#include <sys/stat.h>

#include <thread>

#include "NativeCodecTestBase.h"
#include "NativeMediaCommon.h"

class NativeCodecUnitTest final : CodecTestBase {
  private:
    AMediaFormat* mFormat;
    bool enqueueInput(size_t bufferIndex) override;
    bool dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* bufferInfo) override;

    const long kStallTimeMs = 1000;

  public:
    NativeCodecUnitTest(const char* mime);
    ~NativeCodecUnitTest();

    bool setupCodec(bool isAudio, bool isEncoder);

    bool testConfigureCodecForIncompleteFormat(bool isAudio, bool isEncoder);
    bool testConfigureCodecForBadFlags(bool isEncoder);
    bool testConfigureInInitState();
    bool testConfigureInRunningState();
    bool testConfigureInUnInitState();
    bool testDequeueInputBufferInInitState();
    bool testDequeueInputBufferInRunningState();
    bool testDequeueInputBufferInUnInitState();
    bool testDequeueOutputBufferInInitState();
    bool testDequeueOutputBufferInRunningState();
    bool testDequeueOutputBufferInUnInitState();
    bool testFlushInInitState();
    bool testFlushInRunningState();
    bool testFlushInUnInitState();
    bool testGetNameInInitState();
    bool testGetNameInRunningState();
    bool testGetNameInUnInitState();
    bool testSetAsyncNotifyCallbackInInitState();
    bool testSetAsyncNotifyCallbackInRunningState();
    bool testSetAsyncNotifyCallbackInUnInitState();
    bool testGetInputBufferInInitState();
    bool testGetInputBufferInRunningState();
    bool testGetInputBufferInUnInitState();
    bool testGetInputFormatInInitState();
    bool testGetInputFormatInRunningState();
    bool testGetInputFormatInUnInitState();
    bool testGetOutputBufferInInitState();
    bool testGetOutputBufferInRunningState();
    bool testGetOutputBufferInUnInitState();
    bool testGetOutputFormatInInitState();
    bool testGetOutputFormatInRunningState();
    bool testGetOutputFormatInUnInitState();
    bool testSetParametersInInitState();
    bool testSetParametersInRunningState();
    bool testSetParametersInUnInitState();
    bool testStartInRunningState();
    bool testStartInUnInitState();
    bool testStopInInitState();
    bool testStopInRunningState();
    bool testStopInUnInitState();
    bool testQueueInputBufferInInitState();
    bool testQueueInputBufferWithBadIndex();
    bool testQueueInputBufferWithBadSize();
    bool testQueueInputBufferWithBadBuffInfo();
    bool testQueueInputBufferWithBadOffset();
    bool testQueueInputBufferInUnInitState();
    bool testReleaseOutputBufferInInitState();
    bool testReleaseOutputBufferInRunningState();
    bool testReleaseOutputBufferInUnInitState();
    bool testGetBufferFormatInInitState();
    bool testGetBufferFormatInRunningState();
    bool testGetBufferFormatInUnInitState();
};

NativeCodecUnitTest::NativeCodecUnitTest(const char* mime) : CodecTestBase(mime) {
    mFormat = nullptr;
}

NativeCodecUnitTest::~NativeCodecUnitTest() {
    if (mFormat) AMediaFormat_delete(mFormat);
    mFormat = nullptr;
}

bool NativeCodecUnitTest::enqueueInput(size_t bufferIndex) {
    (void)bufferIndex;
    return false;
}

bool NativeCodecUnitTest::dequeueOutput(size_t bufferIndex, AMediaCodecBufferInfo* info) {
    if ((info->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
        mSawOutputEOS = true;
    }
    CHECK_STATUS(AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false),
                 "AMediaCodec_releaseOutputBuffer failed");
    return !hasSeenError();
}

AMediaFormat* getSampleAudioFormat() {
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, AMEDIA_MIMETYPE_AUDIO_AAC);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, 64000);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, 16000);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, 1);
    return format;
}

AMediaFormat* getSampleVideoFormat() {
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, AMEDIA_MIMETYPE_VIDEO_AVC);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, 512000);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, 352);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, 288);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, 30);
    AMediaFormat_setFloat(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1.0F);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420Flexible);
    return format;
}

bool NativeCodecUnitTest::setupCodec(bool isAudio, bool isEncoder) {
    bool isPass = true;
    mFormat = isAudio ? getSampleAudioFormat() : getSampleVideoFormat();
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    mCodec = isEncoder ? AMediaCodec_createEncoderByType(mime)
                       : AMediaCodec_createDecoderByType(mime);
    if (!mCodec) {
        ALOGE("unable to create codec %s", mime);
        isPass = false;
    }
    return isPass;
}

/* Structure to keep format key and their value to initialize format. Value can be of type
 * string(stringVal) or int(intVal). At once, only one of stringVal or intVal is initialise with
 * valid value. */
struct formatKey {
    const char* key = nullptr;
    const char* stringVal = nullptr;
    int32_t intVal = 0;
};

void setUpDefaultFormatElementsList(std::vector<formatKey*>& vec, bool isAudio, bool isEncoder) {
    if (isAudio) {
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_MIME, AMEDIA_MIMETYPE_AUDIO_AAC, -1});
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_SAMPLE_RATE, nullptr, 16000});
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_CHANNEL_COUNT, nullptr, 1});
        if (isEncoder) {
            vec.push_back(new formatKey{AMEDIAFORMAT_KEY_BIT_RATE, nullptr, 64000});
        }
    } else {
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_MIME, AMEDIA_MIMETYPE_VIDEO_AVC, -1});
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_WIDTH, nullptr, 176});
        vec.push_back(new formatKey{AMEDIAFORMAT_KEY_HEIGHT, nullptr, 144});
        if (isEncoder) {
            vec.push_back(new formatKey{AMEDIAFORMAT_KEY_FRAME_RATE, nullptr, 24});
            vec.push_back(new formatKey{AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, nullptr, 1});
            vec.push_back(new formatKey{AMEDIAFORMAT_KEY_BIT_RATE, nullptr, 256000});
            vec.push_back(new formatKey{AMEDIAFORMAT_KEY_COLOR_FORMAT, nullptr,
                                        COLOR_FormatYUV420Flexible});
        }
    }
}

void deleteDefaultFormatElementsList(std::vector<formatKey*>& vector) {
    for (int i = 0; i < vector.size(); i++) delete vector.at(i);
}

AMediaFormat* getSampleFormat(std::vector<formatKey*> vector, int skipIndex) {
    AMediaFormat* format = AMediaFormat_new();
    for (int i = 0; i < vector.size(); i++) {
        if (skipIndex == i) continue;
        formatKey* element = vector.at(i);
        if (element->stringVal) {
            AMediaFormat_setString(format, element->key, element->stringVal);
        } else {
            AMediaFormat_setInt32(format, element->key, element->intVal);
        }
    }
    return format;
}

bool NativeCodecUnitTest::testConfigureCodecForIncompleteFormat(bool isAudio, bool isEncoder) {
    const char* mime = isAudio ? AMEDIA_MIMETYPE_AUDIO_AAC : AMEDIA_MIMETYPE_VIDEO_AVC;
    mCodec = isEncoder ? AMediaCodec_createEncoderByType(mime)
                       : AMediaCodec_createDecoderByType(mime);
    if (!mCodec) {
        ALOGE("unable to create codec %s", mime);
        return false;
    }
    std::vector<formatKey*> vector;
    bool isPass = true;
    setUpDefaultFormatElementsList(vector, isAudio, isEncoder);
    AMediaFormat* format = nullptr;
    int i;
    for (i = 0; i < vector.size(); i++) {
        if (!isPass) break;
        format = getSampleFormat(vector, i);
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, format, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds for format with missing key %s", vector.at(i)->key);
            isPass = false;
        }
        AMediaFormat_delete(format);
    }
    format = getSampleFormat(vector, i);
    if (AMEDIA_OK != AMediaCodec_configure(mCodec, format, nullptr, nullptr,
                                           isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
        ALOGE("codec configure fails for valid format %s", AMediaFormat_toString(format));
        isPass = false;
    }
    AMediaFormat_delete(format);
    deleteDefaultFormatElementsList(vector);
    return isPass;
}

bool NativeCodecUnitTest::testConfigureCodecForBadFlags(bool isEncoder) {
    bool isAudio = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    bool isPass = true;
    if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                           isEncoder ? 0 : AMEDIACODEC_CONFIGURE_FLAG_ENCODE)) {
        isPass = false;
        ALOGE("codec configure succeeds with bad configure flag");
    }
    AMediaCodec_stop(mCodec);
    return isPass;
}

bool NativeCodecUnitTest::testConfigureInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, true, isEncoder)) return false;
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testConfigureInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds in initialized state");
            return false;
        }
        if (!flushCodec()) return false;
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds in flush state");
            return false;
        }
        if (mIsCodecInAsyncMode) {
            CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        }
        if (!queueEOS()) return false;
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds in running state");
            return false;
        }
        if (!waitForAllOutputs()) return false;
        if (AMEDIA_OK == AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                               isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0)) {
            ALOGE("codec configure succeeds in eos state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testConfigureInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr,
                                           isEncoder ? AMEDIACODEC_CONFIGURE_FLAG_ENCODE : 0),
                     "codec configure fails in uninitialized state");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueInputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        if (AMediaCodec_dequeueInputBuffer(mCodec, kQDeQTimeOutUs) >=
            AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            ALOGE("dequeue input buffer succeeds in uninitialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueInputBufferInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (mIsCodecInAsyncMode) {
            if (AMediaCodec_dequeueInputBuffer(mCodec, kQDeQTimeOutUs) >=
                AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                ALOGE("dequeue input buffer succeeds in running state in async mode");
                return false;
            }
        }
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueInputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (AMediaCodec_dequeueInputBuffer(mCodec, kQDeQTimeOutUs) >=
            AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            ALOGE("dequeue input buffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMediaCodec_dequeueInputBuffer(mCodec, kQDeQTimeOutUs) >= -1) {
            ALOGE("dequeue input buffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueOutputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        AMediaCodecBufferInfo outInfo;
        if (AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs) >=
            AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            ALOGE("dequeue output buffer succeeds in uninitialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueOutputBufferInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (mIsCodecInAsyncMode) {
            AMediaCodecBufferInfo outInfo;
            if (AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs) >=
                AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                ALOGE("dequeue output buffer succeeds in running state in async mode");
                return false;
            }
        }
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testDequeueOutputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        AMediaCodecBufferInfo outInfo;
        if (AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs) >=
            AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            ALOGE("dequeue output buffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs) >=
            AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            ALOGE("dequeue output buffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testFlushInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        if (flushCodec()) {
            ALOGE("codec flush succeeds in uninitialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testFlushInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    bool isAsync = true;
    if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!flushCodec()) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(kStallTimeMs));
    if (!mAsyncHandle.isInputQueueEmpty()) {
        ALOGE("received input buffer callback before start");
        return false;
    }
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(kStallTimeMs));
    if (mAsyncHandle.isInputQueueEmpty()) {
        ALOGE("did not receive input buffer callback after start");
        return false;
    }
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    return !hasSeenError();
}

bool NativeCodecUnitTest::testFlushInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (flushCodec()) {
            ALOGE("codec flush succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (flushCodec()) {
            ALOGE("codec flush succeeds in uninitialized state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetNameInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        char* name = nullptr;
        if (AMEDIA_OK != AMediaCodec_getName(mCodec, &name) || !name) {
            ALOGE("codec get metadata call fails in initialized state");
            if (name) AMediaCodec_releaseName(mCodec, name);
            return false;
        }
        if (name) AMediaCodec_releaseName(mCodec, name);
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetNameInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        char* name = nullptr;
        if (AMEDIA_OK != AMediaCodec_getName(mCodec, &name) || !name) {
            ALOGE("codec get metadata call fails in running state");
            if (name) AMediaCodec_releaseName(mCodec, name);
            return false;
        }
        if (name) AMediaCodec_releaseName(mCodec, name);
        name = nullptr;
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        if (AMEDIA_OK != AMediaCodec_getName(mCodec, &name) || !name) {
            ALOGE("codec get metadata call fails in running state");
            if (name) AMediaCodec_releaseName(mCodec, name);
            return false;
        }
        if (name) AMediaCodec_releaseName(mCodec, name);
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetNameInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    char* name = nullptr;
    if (AMEDIA_OK != AMediaCodec_getName(mCodec, &name) || !name) {
        ALOGE("codec get metadata call fails in uninitialized state");
        if (name) AMediaCodec_releaseName(mCodec, name);
        return false;
    }
    if (name) AMediaCodec_releaseName(mCodec, name);
    name = nullptr;
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMEDIA_OK != AMediaCodec_getName(mCodec, &name) || !name) {
            ALOGE("codec get metadata call fails in uninitialized state");
            if (name) AMediaCodec_releaseName(mCodec, name);
            return false;
        }
        if (name) AMediaCodec_releaseName(mCodec, name);
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetAsyncNotifyCallbackInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    bool isAsync = true;

    // configure component in sync mode
    if (!configureCodec(mFormat, !isAsync, false, isEncoder)) return false;
    // setCallBack in async mode
    CHECK_STATUS(mAsyncHandle.setCallBack(mCodec, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback failed");
    mIsCodecInAsyncMode = true;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");

    // configure component in async mode
    if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");

    // configure component in async mode
    if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    // configure component in sync mode
    if (!reConfigureCodec(mFormat, !isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetAsyncNotifyCallbackInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        // setCallBack in async mode
        if (AMEDIA_OK == mAsyncHandle.setCallBack(mCodec, true)) {
            ALOGE("setAsyncNotifyCallback call succeeds in running state");
            return false;
        }
        if (!queueEOS()) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetAsyncNotifyCallbackInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    bool isAsync = true;
    // setCallBack in async mode
    CHECK_STATUS(mAsyncHandle.setCallBack(mCodec, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback fails in uninitalized state");
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    // configure component in sync mode
    if (!reConfigureCodec(mFormat, !isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");

    // setCallBack in async mode
    CHECK_STATUS(mAsyncHandle.setCallBack(mCodec, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback fails in stopped state");
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    // configure component in sync mode
    if (!reConfigureCodec(mFormat, !isAsync, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (!queueEOS()) return false;
    if (!waitForAllOutputs()) return false;
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        size_t bufSize;
        uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, 0, &bufSize);
        if (buf != nullptr) {
            ALOGE("getInputBuffer succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputBufferInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        size_t bufSize;
        if (AMediaCodec_getInputBuffer(mCodec, -1, &bufSize) != nullptr) {
            ALOGE("getInputBuffer succeeds for bad buffer index -1");
            return false;
        }
        int bufferIndex = mIsCodecInAsyncMode ? mAsyncHandle.getInput().bufferIndex
                                              : AMediaCodec_dequeueInputBuffer(mCodec, -1);
        uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &bufSize);
        if (buf == nullptr) {
            ALOGE("getInputBuffer fails for valid index");
            return false;
        }
        if (!enqueueEOS(bufferIndex)) return false;
        if (!waitForAllOutputs()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        size_t bufSize;
        if (AMediaCodec_getInputBuffer(mCodec, 0, &bufSize) != nullptr) {
            ALOGE("getInputBuffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMediaCodec_getInputBuffer(mCodec, 0, &bufSize) != nullptr) {
            ALOGE("getInputBuffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputFormatInInitState() {
    bool isAudio = true;
    bool isEncoder = false;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        AMediaFormat* dupFormat = AMediaCodec_getInputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (!dupMime || strcmp(dupMime, mime) != 0) {
            ALOGE("getInputFormat fails in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputFormatInRunningState() {
    bool isAudio = true;
    bool isEncoder = false;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        AMediaFormat* dupFormat = AMediaCodec_getInputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (!dupMime || strcmp(dupMime, mime) != 0) {
            ALOGE("getInputFormat fails in running state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetInputFormatInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    for (auto isAsync : boolStates) {
        AMediaFormat* dupFormat = AMediaCodec_getInputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("getInputFormat succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        dupFormat = AMediaCodec_getInputFormat(mCodec);
        dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("getInputFormat succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetOutputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        size_t bufSize;
        if (AMediaCodec_getOutputBuffer(mCodec, 0, &bufSize) != nullptr) {
            ALOGE("GetOutputBuffer succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return true;
}

bool NativeCodecUnitTest::testGetOutputBufferInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    AMediaCodecBufferInfo outInfo;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        size_t bufSize;
        if (AMediaCodec_getOutputBuffer(mCodec, -1, &bufSize)) {
            ALOGE("GetOutputBuffer succeeds for bad buffer index -1");
            return false;
        }
        if (!queueEOS()) return false;
        bool isOk = true;
        if (!hasSeenError()) {
            int bufferIndex = 0;
            size_t buffSize;
            while (!mSawOutputEOS && isOk) {
                if (mIsCodecInAsyncMode) {
                    callbackObject element = mAsyncHandle.getOutput();
                    bufferIndex = element.bufferIndex;
                    if (element.bufferIndex >= 0) {
                        if (!AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize)) {
                            ALOGE("GetOutputBuffer fails for valid bufffer index");
                            return false;
                        }
                        isOk = dequeueOutput(element.bufferIndex, &element.bufferInfo);
                    }
                } else {
                    bufferIndex = AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs);
                    if (bufferIndex >= 0) {
                        if (!AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize)) {
                            ALOGE("GetOutputBuffer fails for valid bufffer index");
                            return false;
                        }
                        isOk = dequeueOutput(bufferIndex, &outInfo);
                    }
                }
                if (hasSeenError() || !isOk) {
                    ALOGE("Got unexpected error");
                    return false;
                }
            }
            if (AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &bufSize) != nullptr) {
                ALOGE("getOutputBuffer succeeds for buffer index not owned by client");
                return false;
            }
        } else {
            ALOGE("Got unexpected error");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetOutputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        size_t bufSize;
        if (AMediaCodec_getOutputBuffer(mCodec, 0, &bufSize) != nullptr) {
            ALOGE("GetOutputBuffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMediaCodec_getOutputBuffer(mCodec, 0, &bufSize) != nullptr) {
            ALOGE("GetOutputBuffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetOutputFormatInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        AMediaFormat* dupFormat = AMediaCodec_getOutputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (!dupMime || strcmp(dupMime, mime) != 0) {
            ALOGE("getOutputFormat fails in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetOutputFormatInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        AMediaFormat* dupFormat = AMediaCodec_getOutputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (!dupMime || strcmp(dupMime, mime) != 0) {
            ALOGE("getOutputFormat fails in running state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetOutputFormatInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        AMediaFormat* dupFormat = AMediaCodec_getOutputFormat(mCodec);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("getOutputFormat succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        dupFormat = AMediaCodec_getOutputFormat(mCodec);
        dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("getOutputFormat succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetParametersInInitState() {
    bool isAudio = false;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        int bitrate;
        AMediaFormat_getInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bitrate);
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate >> 1);
        if (AMEDIA_OK == AMediaCodec_setParameters(mCodec, params)) {
            ALOGE("SetParameters succeeds in initialized state");
            AMediaFormat_delete(params);
            return false;
        }
        AMediaFormat_delete(params);
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetParametersInRunningState() {
    bool isAudio = false;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    int bitrate;
    AMediaFormat_getInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bitrate);
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        // behaviour of setParams with null argument is acceptable according to SDK
        AMediaCodec_setParameters(mCodec, nullptr);
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate >> 1);
        if (AMEDIA_OK != AMediaCodec_setParameters(mCodec, params)) {
            ALOGE("SetParameters fails in running state");
            AMediaFormat_delete(params);
            return false;
        }
        if (!queueEOS()) return false;
        AMediaCodec_setParameters(mCodec, nullptr);
        AMediaFormat_setInt32(mFormat, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate << 1);
        if (AMEDIA_OK != AMediaCodec_setParameters(mCodec, mFormat)) {
            ALOGE("SetParameters fails in running state");
            AMediaFormat_delete(params);
            return false;
        }
        if (!waitForAllOutputs()) return false;
        AMediaCodec_setParameters(mCodec, nullptr);
        AMediaFormat_setInt32(mFormat, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate);
        if (AMEDIA_OK != AMediaCodec_setParameters(mCodec, mFormat)) {
            ALOGE("SetParameters fails in running state");
            AMediaFormat_delete(params);
            return false;
        }
        AMediaFormat_delete(params);
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testSetParametersInUnInitState() {
    bool isAudio = false;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        int bitrate;
        AMediaFormat_getInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bitrate);
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, TBD_AMEDIACODEC_PARAMETER_KEY_VIDEO_BITRATE, bitrate >> 1);
        if (AMEDIA_OK == AMediaCodec_setParameters(mCodec, params)) {
            ALOGE("SetParameters succeeds in stopped state");
            AMediaFormat_delete(params);
            return false;
        }
        AMediaFormat_delete(params);
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testStartInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    if (!configureCodec(mFormat, false, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    if (AMEDIA_OK == AMediaCodec_start(mCodec)) {
        ALOGE("Start succeeds in running state");
        return false;
    }
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    return !hasSeenError();
}

bool NativeCodecUnitTest::testStartInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    if (AMEDIA_OK == AMediaCodec_start(mCodec)) {
        ALOGE("codec start succeeds before initialization");
        return false;
    }
    if (!configureCodec(mFormat, false, false, isEncoder)) return false;
    CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
    CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    if (AMEDIA_OK == AMediaCodec_start(mCodec)) {
        ALOGE("codec start succeeds in stopped state");
        return false;
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testStopInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "Stop fails in initialized state");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testStopInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (!queueEOS()) return false;
        CHECK_STATUS(AMediaCodec_stop(mCodec), "Stop fails in running state");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testStopInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "Stop fails in stopped state");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, 0, 0, 0,
                                                      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            ALOGE("queueInputBuffer succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferWithBadIndex() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, -1, 0, 0, 0,
                                                      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            ALOGE("queueInputBuffer succeeds with bad buffer index :: -1");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferWithBadSize() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        int bufferIndex = mIsCodecInAsyncMode ? mAsyncHandle.getInput().bufferIndex
                                              : AMediaCodec_dequeueInputBuffer(mCodec, -1);
        size_t bufSize;
        uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &bufSize);
        if (buf == nullptr) {
            ALOGE("getInputBuffer fails for valid index");
            return false;
        } else {
            if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, 0, bufSize + 100, 0,
                                                          AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
                ALOGE("queueInputBuffer succeeds for bad size %d, buffer capacity %d, ",
                      (int)bufSize + 100, (int)bufSize);
                return false;
            }
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferWithBadBuffInfo() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        int bufferIndex = mIsCodecInAsyncMode ? mAsyncHandle.getInput().bufferIndex
                                              : AMediaCodec_dequeueInputBuffer(mCodec, -1);
        size_t bufSize;
        uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufferIndex, &bufSize);
        if (buf == nullptr) {
            ALOGE("getInputBuffer fails for valid index");
            return false;
        } else {
            if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, 16, bufSize, 0,
                                                          AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
                ALOGE("queueInputBuffer succeeds with bad offset and size param");
                return false;
            }
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferWithBadOffset() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, -1, 0, 0,
                                                      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            ALOGE("queueInputBuffer succeeds with bad buffer offset :: -1");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testQueueInputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, 0, 0, 0,
                                                      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            ALOGE("queueInputBuffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMEDIA_OK == AMediaCodec_queueInputBuffer(mCodec, 0, 0, 0, 0,
                                                      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {
            ALOGE("queueInputBuffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testReleaseOutputBufferInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        if (AMEDIA_OK == AMediaCodec_releaseOutputBuffer(mCodec, 0, false)) {
            ALOGE("ReleaseOutputBuffer succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testReleaseOutputBufferInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    AMediaCodecBufferInfo outInfo;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        if (AMEDIA_OK == AMediaCodec_releaseOutputBuffer(mCodec, -1, false)) {
            ALOGE("ReleaseOutputBuffer succeeds for bad buffer index -1");
            return false;
        }
        if (!queueEOS()) return false;
        if (!hasSeenError()) {
            int bufferIndex = 0;
            size_t buffSize;
            bool isOk = true;
            while (!mSawOutputEOS && isOk) {
                if (mIsCodecInAsyncMode) {
                    callbackObject element = mAsyncHandle.getOutput();
                    bufferIndex = element.bufferIndex;
                    if (element.bufferIndex >= 0) {
                        if (!AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize)) {
                            ALOGE("GetOutputBuffer fails for valid buffer index");
                            return false;
                        }
                        isOk = dequeueOutput(element.bufferIndex, &element.bufferInfo);
                    }
                } else {
                    bufferIndex = AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs);
                    if (bufferIndex >= 0) {
                        if (!AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &buffSize)) {
                            ALOGE("GetOutputBuffer fails for valid bufffer index");
                            return false;
                        }
                        isOk = dequeueOutput(bufferIndex, &outInfo);
                    }
                }
                if (hasSeenError() || !isOk) {
                    ALOGE("Got unexpected error");
                    return false;
                }
            }
            if (AMEDIA_OK == AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false)) {
                ALOGE("ReleaseOutputBuffer succeeds for buffer index not owned by client");
                return false;
            }
        } else {
            ALOGE("Got unexpected error");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testReleaseOutputBufferInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (AMEDIA_OK == AMediaCodec_releaseOutputBuffer(mCodec, 0, false)) {
            ALOGE("ReleaseOutputBuffer succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        if (AMEDIA_OK == AMediaCodec_releaseOutputBuffer(mCodec, 0, false)) {
            ALOGE("ReleaseOutputBuffer succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetBufferFormatInInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        AMediaFormat* dupFormat = AMediaCodec_getBufferFormat(mCodec, 0);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("GetBufferFormat succeeds in initialized state");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetBufferFormatInRunningState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const char* mime = nullptr;
    AMediaFormat_getString(mFormat, AMEDIAFORMAT_KEY_MIME, &mime);
    AMediaCodecBufferInfo outInfo;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        AMediaFormat* dupFormat = AMediaCodec_getBufferFormat(mCodec, -1);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("GetBufferFormat succeeds for bad buffer index -1");
            return false;
        }
        if (!queueEOS()) return false;
        if (!hasSeenError()) {
            int bufferIndex = 0;
            bool isOk = true;
            while (!mSawOutputEOS && isOk) {
                if (mIsCodecInAsyncMode) {
                    callbackObject element = mAsyncHandle.getOutput();
                    bufferIndex = element.bufferIndex;
                    if (element.bufferIndex >= 0) {
                        dupFormat = AMediaCodec_getBufferFormat(mCodec, bufferIndex);
                        dupMime = nullptr;
                        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
                        AMediaFormat_delete(dupFormat);
                        if (!dupMime || strcmp(dupMime, mime) != 0) {
                            ALOGE("GetBufferFormat fails in running state");
                            return false;
                        }
                        isOk = dequeueOutput(element.bufferIndex, &element.bufferInfo);
                    }
                } else {
                    bufferIndex = AMediaCodec_dequeueOutputBuffer(mCodec, &outInfo, kQDeQTimeOutUs);
                    if (bufferIndex >= 0) {
                        dupFormat = AMediaCodec_getBufferFormat(mCodec, bufferIndex);
                        dupMime = nullptr;
                        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
                        AMediaFormat_delete(dupFormat);
                        if (!dupMime || strcmp(dupMime, mime) != 0) {
                            ALOGE("GetBufferFormat fails in running state");
                            return false;
                        }
                        isOk = dequeueOutput(bufferIndex, &outInfo);
                    }
                }
                if (hasSeenError() || !isOk) {
                    ALOGE("Got unexpected error");
                    return false;
                }
            }
            dupFormat = AMediaCodec_getBufferFormat(mCodec, bufferIndex);
            dupMime = nullptr;
            AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
            AMediaFormat_delete(dupFormat);
            if (dupMime) {
                ALOGE("GetBufferFormat succeeds for buffer index not owned by client");
                return false;
            }
        } else {
            ALOGE("Got unexpected error");
            return false;
        }
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
    }
    return !hasSeenError();
}

bool NativeCodecUnitTest::testGetBufferFormatInUnInitState() {
    bool isAudio = true;
    bool isEncoder = true;
    if (!setupCodec(isAudio, isEncoder)) return false;
    const bool boolStates[]{true, false};
    for (auto isAsync : boolStates) {
        AMediaFormat* dupFormat = AMediaCodec_getBufferFormat(mCodec, 0);
        const char* dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("GetBufferFormat succeeds in uninitialized state");
            return false;
        }
        if (!configureCodec(mFormat, isAsync, false, isEncoder)) return false;
        CHECK_STATUS(AMediaCodec_start(mCodec), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_stop(mCodec), "AMediaCodec_stop failed");
        dupFormat = AMediaCodec_getBufferFormat(mCodec, 0);
        dupMime = nullptr;
        AMediaFormat_getString(dupFormat, AMEDIAFORMAT_KEY_MIME, &dupMime);
        AMediaFormat_delete(dupFormat);
        if (dupMime) {
            ALOGE("GetBufferFormat succeeds in stopped state");
            return false;
        }
    }
    return !hasSeenError();
}

static jboolean nativeTestCreateByCodecNameForNull(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createCodecByName(nullptr);
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createCodecByName succeeds with null argument");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestCreateByCodecNameForInvalidName(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createCodecByName("invalid name");
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createCodecByName succeeds with invalid name");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestCreateDecoderByTypeForNull(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createDecoderByType(nullptr);
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createDecoderByType succeeds with null argument");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestCreateDecoderByTypeForInvalidMime(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createDecoderByType("invalid name");
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createDecoderByType succeeds with invalid name");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestCreateEncoderByTypeForNull(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createEncoderByType(nullptr);
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createEncoderByType succeeds with null argument");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestCreateEncoderByTypeForInvalidMime(JNIEnv*, jobject) {
    bool isPass = true;
    AMediaCodec* codec = AMediaCodec_createEncoderByType("invalid name");
    if (codec) {
        AMediaCodec_delete(codec);
        ALOGE("AMediaCodec_createEncoderByType succeeds with invalid name");
        isPass = false;
    }
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureForNullFormat(JNIEnv*, jobject) {
    AMediaCodec* codec = AMediaCodec_createEncoderByType(AMEDIA_MIMETYPE_AUDIO_AAC);
    if (!codec) {
        ALOGE("unable to create codec %s", AMEDIA_MIMETYPE_AUDIO_AAC);
        return static_cast<jboolean>(false);
    }
    bool isPass = (AMEDIA_OK != AMediaCodec_configure(codec, nullptr, nullptr, nullptr,
                                                      AMEDIACODEC_CONFIGURE_FLAG_ENCODE));
    if (!isPass) {
        ALOGE("codec configure succeeds with null format");
    }
    AMediaCodec_delete(codec);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureForEmptyFormat(JNIEnv*, jobject) {
    AMediaCodec* codec = AMediaCodec_createEncoderByType(AMEDIA_MIMETYPE_AUDIO_AAC);
    if (!codec) {
        ALOGE("unable to create codec %s", AMEDIA_MIMETYPE_AUDIO_AAC);
        return static_cast<jboolean>(false);
    }
    AMediaFormat* format = AMediaFormat_new();
    bool isPass = (AMEDIA_OK != AMediaCodec_configure(codec, format, nullptr, nullptr,
                                                      AMEDIACODEC_CONFIGURE_FLAG_ENCODE));
    if (!isPass) {
        ALOGE("codec configure succeeds with empty format");
    }
    AMediaFormat_delete(format);
    AMediaCodec_delete(codec);
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureCodecForIncompleteFormat(JNIEnv*, jobject, bool isAudio,
                                                            bool isEncoder) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testConfigureCodecForIncompleteFormat(isAudio, isEncoder);
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureEncoderForBadFlags(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isEncoder = true;
    bool isPass = nativeCodecUnitTest->testConfigureCodecForBadFlags(isEncoder);
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureDecoderForBadFlags(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isEncoder = false;
    bool isPass = nativeCodecUnitTest->testConfigureCodecForBadFlags(isEncoder);
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testConfigureInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testConfigureInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestConfigureInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testConfigureInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueInputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueInputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueInputBufferInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueInputBufferInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueInputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueInputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueOutputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueOutputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueOutputBufferInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueOutputBufferInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestDequeueOutputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testDequeueOutputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFlushInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testFlushInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFlushInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testFlushInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestFlushInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testFlushInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetNameInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetNameInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetNameInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetNameInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetNameInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetNameInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetAsyncNotifyCallbackInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetAsyncNotifyCallbackInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetAsyncNotifyCallbackInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetAsyncNotifyCallbackInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetAsyncNotifyCallbackInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetAsyncNotifyCallbackInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputBufferInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputBufferInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputFormatInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputFormatInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputFormatInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputFormatInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetInputFormatInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetInputFormatInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputBufferInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputBufferInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputFormatInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputFormatInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputFormatInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputFormatInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetOutputFormatInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetOutputFormatInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetParametersInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetParametersInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetParametersInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetParametersInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestSetParametersInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testSetParametersInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStartInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testStartInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStartInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testStartInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStopInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testStopInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStopInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testStopInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestStopInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testStopInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferWithBadIndex(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferWithBadIndex();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferWithBadSize(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferWithBadSize();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferWithBadBuffInfo(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferWithBadBuffInfo();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferWithBadOffset(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferWithBadOffset();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestQueueInputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testQueueInputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReleaseOutputBufferInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testReleaseOutputBufferInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReleaseOutputBufferInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testReleaseOutputBufferInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestReleaseOutputBufferInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testReleaseOutputBufferInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetBufferFormatInInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetBufferFormatInInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetBufferFormatInRunningState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetBufferFormatInRunningState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

static jboolean nativeTestGetBufferFormatInUnInitState(JNIEnv*, jobject) {
    auto* nativeCodecUnitTest = new NativeCodecUnitTest(AMEDIA_MIMETYPE_AUDIO_AAC);
    bool isPass = nativeCodecUnitTest->testGetBufferFormatInUnInitState();
    delete nativeCodecUnitTest;
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsCodecUnitTest(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestCreateByCodecNameForNull", "()Z",
             (void*)nativeTestCreateByCodecNameForNull},
            {"nativeTestCreateByCodecNameForInvalidName", "()Z",
             (void*)nativeTestCreateByCodecNameForInvalidName},
            {"nativeTestCreateDecoderByTypeForNull", "()Z",
             (void*)nativeTestCreateDecoderByTypeForNull},
            {"nativeTestCreateDecoderByTypeForInvalidMime", "()Z",
             (void*)nativeTestCreateDecoderByTypeForInvalidMime},
            {"nativeTestCreateEncoderByTypeForNull", "()Z",
             (void*)nativeTestCreateEncoderByTypeForNull},
            {"nativeTestCreateEncoderByTypeForInvalidMime", "()Z",
             (void*)nativeTestCreateEncoderByTypeForInvalidMime},
            {"nativeTestConfigureForNullFormat", "()Z", (void*)nativeTestConfigureForNullFormat},
            {"nativeTestConfigureForEmptyFormat", "()Z", (void*)nativeTestConfigureForEmptyFormat},
            {"nativeTestConfigureCodecForIncompleteFormat", "(ZZ)Z",
             (void*)nativeTestConfigureCodecForIncompleteFormat},
            {"nativeTestConfigureEncoderForBadFlags", "()Z",
             (void*)nativeTestConfigureEncoderForBadFlags},
            {"nativeTestConfigureDecoderForBadFlags", "()Z",
             (void*)nativeTestConfigureDecoderForBadFlags},
            {"nativeTestConfigureInInitState", "()Z", (void*)nativeTestConfigureInInitState},
            {"nativeTestConfigureInRunningState", "()Z", (void*)nativeTestConfigureInRunningState},
            {"nativeTestConfigureInUnInitState", "()Z", (void*)nativeTestConfigureInUnInitState},
            {"nativeTestDequeueInputBufferInInitState", "()Z",
             (void*)nativeTestDequeueInputBufferInInitState},
            {"nativeTestDequeueInputBufferInRunningState", "()Z",
             (void*)nativeTestDequeueInputBufferInRunningState},
            {"nativeTestDequeueInputBufferInUnInitState", "()Z",
             (void*)nativeTestDequeueInputBufferInUnInitState},
            {"nativeTestDequeueOutputBufferInInitState", "()Z",
             (void*)nativeTestDequeueOutputBufferInInitState},
            {"nativeTestDequeueOutputBufferInRunningState", "()Z",
             (void*)nativeTestDequeueOutputBufferInRunningState},
            {"nativeTestDequeueOutputBufferInUnInitState", "()Z",
             (void*)nativeTestDequeueOutputBufferInUnInitState},
            {"nativeTestFlushInInitState", "()Z", (void*)nativeTestFlushInInitState},
            {"nativeTestFlushInRunningState", "()Z", (void*)nativeTestFlushInRunningState},
            {"nativeTestFlushInUnInitState", "()Z", (void*)nativeTestFlushInUnInitState},
            {"nativeTestGetNameInInitState", "()Z", (void*)nativeTestGetNameInInitState},
            {"nativeTestGetNameInRunningState", "()Z", (void*)nativeTestGetNameInRunningState},
            {"nativeTestGetNameInUnInitState", "()Z", (void*)nativeTestGetNameInUnInitState},
            {"nativeTestSetAsyncNotifyCallbackInInitState", "()Z",
             (void*)nativeTestSetAsyncNotifyCallbackInInitState},
            {"nativeTestSetAsyncNotifyCallbackInRunningState", "()Z",
             (void*)nativeTestSetAsyncNotifyCallbackInRunningState},
            {"nativeTestSetAsyncNotifyCallbackInUnInitState", "()Z",
             (void*)nativeTestSetAsyncNotifyCallbackInUnInitState},
            {"nativeTestGetInputBufferInInitState", "()Z",
             (void*)nativeTestGetInputBufferInInitState},
            {"nativeTestGetInputBufferInRunningState", "()Z",
             (void*)nativeTestGetInputBufferInRunningState},
            {"nativeTestGetInputBufferInUnInitState", "()Z",
             (void*)nativeTestGetInputBufferInUnInitState},
            {"nativeTestGetInputFormatInInitState", "()Z",
             (void*)nativeTestGetInputFormatInInitState},
            {"nativeTestGetInputFormatInRunningState", "()Z",
             (void*)nativeTestGetInputFormatInRunningState},
            {"nativeTestGetInputFormatInUnInitState", "()Z",
             (void*)nativeTestGetInputFormatInUnInitState},
            {"nativeTestGetOutputBufferInInitState", "()Z",
             (void*)nativeTestGetOutputBufferInInitState},
            {"nativeTestGetOutputBufferInRunningState", "()Z",
             (void*)nativeTestGetOutputBufferInRunningState},
            {"nativeTestGetOutputBufferInUnInitState", "()Z",
             (void*)nativeTestGetOutputBufferInUnInitState},
            {"nativeTestGetOutputFormatInInitState", "()Z",
             (void*)nativeTestGetOutputFormatInInitState},
            {"nativeTestGetOutputFormatInRunningState", "()Z",
             (void*)nativeTestGetOutputFormatInRunningState},
            {"nativeTestGetOutputFormatInUnInitState", "()Z",
             (void*)nativeTestGetOutputFormatInUnInitState},
            {"nativeTestSetParametersInInitState", "()Z",
             (void*)nativeTestSetParametersInInitState},
            {"nativeTestSetParametersInRunningState", "()Z",
             (void*)nativeTestSetParametersInRunningState},
            {"nativeTestSetParametersInUnInitState", "()Z",
             (void*)nativeTestSetParametersInUnInitState},
            {"nativeTestStartInRunningState", "()Z", (void*)nativeTestStartInRunningState},
            {"nativeTestStartInUnInitState", "()Z", (void*)nativeTestStartInUnInitState},
            {"nativeTestStopInInitState", "()Z", (void*)nativeTestStopInInitState},
            {"nativeTestStopInRunningState", "()Z", (void*)nativeTestStopInRunningState},
            {"nativeTestStopInUnInitState", "()Z", (void*)nativeTestStopInUnInitState},
            {"nativeTestQueueInputBufferInInitState", "()Z",
             (void*)nativeTestQueueInputBufferInInitState},
            {"nativeTestQueueInputBufferWithBadIndex", "()Z",
             (void*)nativeTestQueueInputBufferWithBadIndex},
            {"nativeTestQueueInputBufferWithBadSize", "()Z",
             (void*)nativeTestQueueInputBufferWithBadSize},
            {"nativeTestQueueInputBufferWithBadBuffInfo", "()Z",
             (void*)nativeTestQueueInputBufferWithBadBuffInfo},
            {"nativeTestQueueInputBufferWithBadOffset", "()Z",
             (void*)nativeTestQueueInputBufferWithBadOffset},
            {"nativeTestQueueInputBufferInUnInitState", "()Z",
             (void*)nativeTestQueueInputBufferInUnInitState},
            {"nativeTestReleaseOutputBufferInInitState", "()Z",
             (void*)nativeTestReleaseOutputBufferInInitState},
            {"nativeTestReleaseOutputBufferInRunningState", "()Z",
             (void*)nativeTestReleaseOutputBufferInRunningState},
            {"nativeTestReleaseOutputBufferInUnInitState", "()Z",
             (void*)nativeTestReleaseOutputBufferInUnInitState},
            {"nativeTestGetBufferFormatInInitState", "()Z",
             (void*)nativeTestGetBufferFormatInInitState},
            {"nativeTestGetBufferFormatInRunningState", "()Z",
             (void*)nativeTestGetBufferFormatInRunningState},
            {"nativeTestGetBufferFormatInUnInitState", "()Z",
             (void*)nativeTestGetBufferFormatInUnInitState},

    };
    jclass c = env->FindClass("android/mediav2/cts/CodecUnitTest$TestApiNative");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}
