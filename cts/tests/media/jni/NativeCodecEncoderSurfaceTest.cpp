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
#define LOG_TAG "NativeCodecEncoderSurfaceTest"
#include <log/log.h>
#include <android/native_window_jni.h>
#include <NdkMediaExtractor.h>
#include <NdkMediaMuxer.h>
#include <jni.h>
#include <sys/stat.h>

#include "NativeCodecTestBase.h"
#include "NativeMediaCommon.h"

class CodecEncoderSurfaceTest {
  private:
    const long kQDeQTimeOutUs = 5000;
    const char* mMime;
    ANativeWindow* mWindow;
    AMediaExtractor* mExtractor;
    AMediaFormat* mDecFormat;
    AMediaFormat* mEncFormat;
    AMediaMuxer* mMuxer;
    AMediaCodec* mDecoder;
    AMediaCodec* mEncoder;
    CodecAsyncHandler mAsyncHandleDecoder;
    CodecAsyncHandler mAsyncHandleEncoder;
    bool mIsCodecInAsyncMode;
    bool mSawDecInputEOS;
    bool mSawDecOutputEOS;
    bool mSawEncOutputEOS;
    bool mSignalEOSWithLastFrame;
    int mDecInputCount;
    int mDecOutputCount;
    int mEncOutputCount;
    int mEncBitrate;
    int mEncFramerate;
    int mMaxBFrames;
    int mMuxTrackID;

    OutputManager* mOutputBuff;
    OutputManager mRefBuff;
    OutputManager mTestBuff;
    bool mSaveToMem;

    bool setUpExtractor(const char* srcPath);
    void deleteExtractor();
    bool configureCodec(bool isAsync, bool signalEOSWithLastFrame);
    void resetContext(bool isAsync, bool signalEOSWithLastFrame);
    void setUpEncoderFormat();
    bool enqueueDecoderInput(size_t bufferIndex);
    bool dequeueDecoderOutput(size_t bufferIndex, AMediaCodecBufferInfo* bufferInfo);
    bool dequeueEncoderOutput(size_t bufferIndex, AMediaCodecBufferInfo* info);
    bool tryEncoderOutput(long timeOutUs);
    bool waitForAllEncoderOutputs();
    bool queueEOS();
    bool enqueueDecoderEOS(size_t bufferIndex);
    bool doWork(int frameLimit);
    bool hasSeenError() { return mAsyncHandleDecoder.getError() || mAsyncHandleEncoder.getError(); }

  public:
    CodecEncoderSurfaceTest(const char* mime, int bitrate, int framerate);
    ~CodecEncoderSurfaceTest();

    bool testSimpleEncode(const char* encoder, const char* decoder, const char* srcPath,
                          const char* muxOutPath);
};

CodecEncoderSurfaceTest::CodecEncoderSurfaceTest(const char* mime, int bitrate, int framerate)
    : mMime{mime}, mEncBitrate{bitrate}, mEncFramerate{framerate} {
    mWindow = nullptr;
    mExtractor = nullptr;
    mDecFormat = nullptr;
    mEncFormat = nullptr;
    mMuxer = nullptr;
    mDecoder = nullptr;
    mEncoder = nullptr;
    resetContext(false, false);
    mMaxBFrames = 0;
    mMuxTrackID = -1;
}

CodecEncoderSurfaceTest::~CodecEncoderSurfaceTest() {
    deleteExtractor();
    if (mWindow) {
        ANativeWindow_release(mWindow);
        mWindow = nullptr;
    }
    if (mEncFormat) {
        AMediaFormat_delete(mEncFormat);
        mEncFormat = nullptr;
    }
    if (mMuxer) {
        AMediaMuxer_delete(mMuxer);
        mMuxer = nullptr;
    }
    if (mDecoder) {
        AMediaCodec_delete(mDecoder);
        mDecoder = nullptr;
    }
    if (mEncoder) {
        AMediaCodec_delete(mEncoder);
        mEncoder = nullptr;
    }
}

bool CodecEncoderSurfaceTest::setUpExtractor(const char* srcFile) {
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
                if (mime && strncmp(mime, "video/", strlen("video/")) == 0) {
                    AMediaExtractor_selectTrack(mExtractor, trackID);
                    AMediaFormat_setInt32(currFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                          COLOR_FormatYUV420Flexible);
                    mDecFormat = currFormat;
                    break;
                }
                AMediaFormat_delete(currFormat);
            }
        }
    }
    if (fp) fclose(fp);
    return mDecFormat != nullptr;
}

void CodecEncoderSurfaceTest::deleteExtractor() {
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }
    if (mDecFormat) {
        AMediaFormat_delete(mDecFormat);
        mDecFormat = nullptr;
    }
}

bool CodecEncoderSurfaceTest::configureCodec(bool isAsync, bool signalEOSWithLastFrame) {
    resetContext(isAsync, signalEOSWithLastFrame);
    CHECK_STATUS(mAsyncHandleEncoder.setCallBack(mEncoder, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback failed");
    CHECK_STATUS(AMediaCodec_configure(mEncoder, mEncFormat, nullptr, nullptr,
                                       AMEDIACODEC_CONFIGURE_FLAG_ENCODE),
                 "AMediaCodec_configure failed");
    CHECK_STATUS(AMediaCodec_createInputSurface(mEncoder, &mWindow),
                 "AMediaCodec_createInputSurface failed");
    CHECK_STATUS(mAsyncHandleDecoder.setCallBack(mDecoder, isAsync),
                 "AMediaCodec_setAsyncNotifyCallback failed");
    CHECK_STATUS(AMediaCodec_configure(mDecoder, mDecFormat, mWindow, nullptr, 0),
                 "AMediaCodec_configure failed");
    return !hasSeenError();
}

void CodecEncoderSurfaceTest::resetContext(bool isAsync, bool signalEOSWithLastFrame) {
    mAsyncHandleDecoder.resetContext();
    mAsyncHandleEncoder.resetContext();
    mIsCodecInAsyncMode = isAsync;
    mSawDecInputEOS = false;
    mSawDecOutputEOS = false;
    mSawEncOutputEOS = false;
    mSignalEOSWithLastFrame = signalEOSWithLastFrame;
    mDecInputCount = 0;
    mDecOutputCount = 0;
    mEncOutputCount = 0;
}

void CodecEncoderSurfaceTest::setUpEncoderFormat() {
    if (mEncFormat) AMediaFormat_delete(mEncFormat);
    mEncFormat = AMediaFormat_new();
    int width, height;
    AMediaFormat_getInt32(mDecFormat, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(mDecFormat, AMEDIAFORMAT_KEY_HEIGHT, &height);
    AMediaFormat_setString(mEncFormat, AMEDIAFORMAT_KEY_MIME, mMime);
    AMediaFormat_setInt32(mEncFormat, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(mEncFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(mEncFormat, AMEDIAFORMAT_KEY_BIT_RATE, mEncBitrate);
    AMediaFormat_setInt32(mEncFormat, AMEDIAFORMAT_KEY_FRAME_RATE, mEncFramerate);
    AMediaFormat_setInt32(mEncFormat, TBD_AMEDIACODEC_PARAMETER_KEY_MAX_B_FRAMES, mMaxBFrames);
    AMediaFormat_setInt32(mEncFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);
    AMediaFormat_setFloat(mEncFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1.0F);
}

bool CodecEncoderSurfaceTest::enqueueDecoderEOS(size_t bufferIndex) {
    if (!hasSeenError() && !mSawDecInputEOS) {
        CHECK_STATUS(AMediaCodec_queueInputBuffer(mDecoder, bufferIndex, 0, 0, 0,
                                                  AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM),
                     "Queued Decoder End of Stream Failed");
        mSawDecInputEOS = true;
        ALOGV("Queued Decoder End of Stream");
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::enqueueDecoderInput(size_t bufferIndex) {
    if (AMediaExtractor_getSampleSize(mExtractor) < 0) {
        return enqueueDecoderEOS(bufferIndex);
    } else {
        uint32_t flags = 0;
        size_t bufSize = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(mDecoder, bufferIndex, &bufSize);
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
            mSawDecInputEOS = true;
        }
        CHECK_STATUS(AMediaCodec_queueInputBuffer(mDecoder, bufferIndex, 0, size, pts, flags),
                     "AMediaCodec_queueInputBuffer failed");
        ALOGV("input: id: %zu  size: %zu  pts: %d  flags: %d", bufferIndex, size, (int)pts, flags);
        if (size > 0) {
            mOutputBuff->saveInPTS(pts);
            mDecInputCount++;
        }
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::dequeueDecoderOutput(size_t bufferIndex,
                                                   AMediaCodecBufferInfo* bufferInfo) {
    if ((bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
        mSawDecOutputEOS = true;
    }
    if (bufferInfo->size > 0 && (bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0) {
        mDecOutputCount++;
    }
    ALOGV("output: id: %zu  size: %d  pts: %d  flags: %d", bufferIndex, bufferInfo->size,
          (int)bufferInfo->presentationTimeUs, bufferInfo->flags);
    CHECK_STATUS(AMediaCodec_releaseOutputBuffer(mDecoder, bufferIndex, mWindow != nullptr),
                 "AMediaCodec_releaseOutputBuffer failed");
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::dequeueEncoderOutput(size_t bufferIndex,
                                                   AMediaCodecBufferInfo* info) {
    if ((info->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
        mSawEncOutputEOS = true;
    }
    if (info->size > 0) {
        size_t buffSize;
        uint8_t* buf = AMediaCodec_getOutputBuffer(mEncoder, bufferIndex, &buffSize);
        if (mSaveToMem) {
            mOutputBuff->saveToMemory(buf, info);
        }
        if (mMuxer != nullptr) {
            if (mMuxTrackID == -1) {
                mMuxTrackID = AMediaMuxer_addTrack(mMuxer, AMediaCodec_getOutputFormat(mEncoder));
                CHECK_STATUS(AMediaMuxer_start(mMuxer), "AMediaMuxer_start failed");
            }
            CHECK_STATUS(AMediaMuxer_writeSampleData(mMuxer, mMuxTrackID, buf, info),
                         "AMediaMuxer_writeSampleData failed");
        }
        if ((info->flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0) {
            mOutputBuff->saveOutPTS(info->presentationTimeUs);
            mEncOutputCount++;
        }
    }
    ALOGV("output: id: %zu  size: %d  pts: %d  flags: %d", bufferIndex, info->size,
          (int)info->presentationTimeUs, info->flags);
    CHECK_STATUS(AMediaCodec_releaseOutputBuffer(mEncoder, bufferIndex, false),
                 "AMediaCodec_releaseOutputBuffer failed");
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::tryEncoderOutput(long timeOutUs) {
    if (mIsCodecInAsyncMode) {
        if (!hasSeenError() && !mSawEncOutputEOS) {
            callbackObject element = mAsyncHandleEncoder.getOutput();
            if (element.bufferIndex >= 0) {
                if (!dequeueEncoderOutput(element.bufferIndex, &element.bufferInfo)) return false;
            }
        }
    } else {
        AMediaCodecBufferInfo outInfo;
        if (!mSawEncOutputEOS) {
            int bufferID = AMediaCodec_dequeueOutputBuffer(mEncoder, &outInfo, timeOutUs);
            if (bufferID >= 0) {
                if (!dequeueEncoderOutput(bufferID, &outInfo)) return false;
            } else if (bufferID == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            } else if (bufferID == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else if (bufferID == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            } else {
                ALOGE("unexpected return value from *_dequeueOutputBuffer: %d", (int)bufferID);
                return false;
            }
        }
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::waitForAllEncoderOutputs() {
    if (mIsCodecInAsyncMode) {
        while (!hasSeenError() && !mSawEncOutputEOS) {
            if (!tryEncoderOutput(kQDeQTimeOutUs)) return false;
        }
    } else {
        while (!mSawEncOutputEOS) {
            if (!tryEncoderOutput(kQDeQTimeOutUs)) return false;
        }
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::queueEOS() {
    if (mIsCodecInAsyncMode) {
        if (!hasSeenError() && !mSawDecInputEOS) {
            callbackObject element = mAsyncHandleDecoder.getInput();
            if (element.bufferIndex >= 0) {
                if (!enqueueDecoderEOS(element.bufferIndex)) return false;
            }
        }
    } else {
        if (!mSawDecInputEOS) {
            int bufferIndex = AMediaCodec_dequeueInputBuffer(mDecoder, -1);
            if (bufferIndex >= 0) {
                if (!enqueueDecoderEOS(bufferIndex)) return false;
            } else {
                ALOGE("unexpected return value from *_dequeueInputBufferBuffer: %d",
                      (int)bufferIndex);
                return false;
            }
        }
    }

    if (mIsCodecInAsyncMode) {
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (!hasSeenError() && !mSawDecOutputEOS) {
            callbackObject element = mAsyncHandleDecoder.getOutput();
            if (element.bufferIndex >= 0) {
                if (!dequeueDecoderOutput(element.bufferIndex, &element.bufferInfo)) return false;
            }
            if (mSawDecOutputEOS) AMediaCodec_signalEndOfInputStream(mEncoder);
            if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                if (!tryEncoderOutput(-1)) return false;
            }
        }
    } else {
        AMediaCodecBufferInfo outInfo;
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (!mSawDecOutputEOS) {
            if (!mSawDecOutputEOS) {
                ssize_t oBufferID =
                        AMediaCodec_dequeueOutputBuffer(mDecoder, &outInfo, kQDeQTimeOutUs);
                if (oBufferID >= 0) {
                    if (!dequeueDecoderOutput(oBufferID, &outInfo)) return false;
                } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                } else if (oBufferID == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
                } else {
                    ALOGE("unexpected return value from *_dequeueOutputBuffer: %d", (int)oBufferID);
                    return false;
                }
            }
            if (mSawDecOutputEOS) AMediaCodec_signalEndOfInputStream(mEncoder);
            if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                if (!tryEncoderOutput(-1)) return false;
            }
        }
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::doWork(int frameLimit) {
    int frameCnt = 0;
    if (mIsCodecInAsyncMode) {
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (!hasSeenError() && !mSawDecInputEOS && frameCnt < frameLimit) {
            callbackObject element = mAsyncHandleDecoder.getWork();
            if (element.bufferIndex >= 0) {
                if (element.isInput) {
                    if (!enqueueDecoderInput(element.bufferIndex)) return false;
                    frameCnt++;
                } else {
                    if (!dequeueDecoderOutput(element.bufferIndex, &element.bufferInfo)) {
                        return false;
                    }
                }
            }
            // check decoder EOS
            if (mSawDecOutputEOS) AMediaCodec_signalEndOfInputStream(mEncoder);
            // encoder output
            // TODO: remove fixed constant and change it according to encoder latency
            if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                if (!tryEncoderOutput(-1)) return false;
            }
        }
    } else {
        AMediaCodecBufferInfo outInfo;
        // output processing after queuing EOS is done in waitForAllOutputs()
        while (!mSawDecInputEOS && frameCnt < frameLimit) {
            ssize_t oBufferID = AMediaCodec_dequeueOutputBuffer(mDecoder, &outInfo, kQDeQTimeOutUs);
            if (oBufferID >= 0) {
                if (!dequeueDecoderOutput(oBufferID, &outInfo)) return false;
            } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            } else if (oBufferID == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else if (oBufferID == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
            } else {
                ALOGE("unexpected return value from *_dequeueOutputBuffer: %d", (int)oBufferID);
                return false;
            }
            ssize_t iBufferId = AMediaCodec_dequeueInputBuffer(mDecoder, kQDeQTimeOutUs);
            if (iBufferId >= 0) {
                if (!enqueueDecoderInput(iBufferId)) return false;
                frameCnt++;
            } else if (iBufferId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            } else {
                ALOGE("unexpected return value from *_dequeueInputBuffer: %d", (int)iBufferId);
                return false;
            }
            if (mSawDecOutputEOS) AMediaCodec_signalEndOfInputStream(mEncoder);
            // TODO: remove fixed constant and change it according to encoder latency
            if (mDecOutputCount - mEncOutputCount > mMaxBFrames) {
                if (!tryEncoderOutput(-1)) return false;
            }
        }
    }
    return !hasSeenError();
}

bool CodecEncoderSurfaceTest::testSimpleEncode(const char* encoder, const char* decoder,
                                               const char* srcPath, const char* muxOutPath) {
    bool isPass = true;
    if (!setUpExtractor(srcPath)) {
        ALOGE("setUpExtractor failed");
        return false;
    }
    setUpEncoderFormat();
    bool muxOutput = true;

    /* TODO(b/149027258) */
    if (true) mSaveToMem = false;
    else mSaveToMem = true;
    auto ref = &mRefBuff;
    auto test = &mTestBuff;
    int loopCounter = 0;
    const bool boolStates[]{true, false};
    for (bool isAsync : boolStates) {
        if (!isPass) break;
        AMediaExtractor_seekTo(mExtractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
        mOutputBuff = loopCounter == 0 ? ref : test;
        mOutputBuff->reset();

        /* TODO(b/147348711) */
        /* Instead of create and delete codec at every iteration, we would like to create
         * once and use it for all iterations and delete before exiting */
        mEncoder = AMediaCodec_createCodecByName(encoder);
        mDecoder = AMediaCodec_createCodecByName(decoder);
        if (!mDecoder || !mEncoder) {
            ALOGE("unable to create media codec by name %s or %s", encoder, decoder);
            isPass = false;
            continue;
        }

        FILE* ofp = nullptr;
        if (muxOutput && loopCounter == 0) {
            int muxerFormat = 0;
            if (!strcmp(mMime, AMEDIA_MIMETYPE_VIDEO_VP8) ||
                !strcmp(mMime, AMEDIA_MIMETYPE_VIDEO_VP9)) {
                muxerFormat = OUTPUT_FORMAT_WEBM;
            } else {
                muxerFormat = OUTPUT_FORMAT_MPEG_4;
            }
            ofp = fopen(muxOutPath, "wbe+");
            if (ofp) {
                mMuxer = AMediaMuxer_new(fileno(ofp), (OutputFormat)muxerFormat);
            }
        }
        if (!configureCodec(mIsCodecInAsyncMode, mSignalEOSWithLastFrame)) return false;
        CHECK_STATUS(AMediaCodec_start(mEncoder), "AMediaCodec_start failed");
        CHECK_STATUS(AMediaCodec_start(mDecoder), "AMediaCodec_start failed");
        if (!doWork(INT32_MAX)) return false;
        if (!queueEOS()) return false;
        if (!waitForAllEncoderOutputs()) return false;
        if (muxOutput) {
            if (mMuxer != nullptr) {
                CHECK_STATUS(AMediaMuxer_stop(mMuxer), "AMediaMuxer_stop failed");
                mMuxTrackID = -1;
                CHECK_STATUS(AMediaMuxer_delete(mMuxer), "AMediaMuxer_delete failed");
                mMuxer = nullptr;
            }
            if (ofp) fclose(ofp);
        }
        CHECK_STATUS(AMediaCodec_stop(mDecoder), "AMediaCodec_stop failed");
        CHECK_STATUS(AMediaCodec_stop(mEncoder), "AMediaCodec_stop failed");
        char log[1000];
        snprintf(log, sizeof(log), "format: %s \n codec: %s, file: %s, mode: %s:: ",
                 AMediaFormat_toString(mEncFormat), encoder, srcPath, (isAsync ? "async" : "sync"));
        CHECK_ERR((hasSeenError()), log, "has seen error", isPass);
        CHECK_ERR((0 == mDecInputCount), log, "no input sent", isPass);
        CHECK_ERR((0 == mDecOutputCount), log, "no decoder output received", isPass);
        CHECK_ERR((0 == mEncOutputCount), log, "no encoder output received", isPass);
        CHECK_ERR((mDecInputCount != mDecOutputCount), log, "decoder input count != output count",
                  isPass);
        /* TODO(b/153127506)
         *  Currently disabling all encoder output checks. Added checks only for encoder timeStamp
         *  is in increasing order or not.
         *  Once issue is fixed remove increasing timestamp check and enable encoder checks.
         */
        /*CHECK_ERR((mEncOutputCount != mDecOutputCount), log,
                  "encoder output count != decoder output count", isPass);
        CHECK_ERR((loopCounter != 0 && !ref->equals(test)), log, "output is flaky", isPass);
        CHECK_ERR((loopCounter == 0 && !ref->isOutPtsListIdenticalToInpPtsList(mMaxBFrames != 0)),
                  log, "input pts list and output pts list are not identical", isPass);*/
        CHECK_ERR(loopCounter == 0 && (!ref->isPtsStrictlyIncreasing(INT32_MIN)), log,
                  "Ref pts is not strictly increasing", isPass);
        CHECK_ERR(loopCounter != 0 && (!test->isPtsStrictlyIncreasing(INT32_MIN)), log,
                  "Test pts is not strictly increasing", isPass);

        loopCounter++;
        ANativeWindow_release(mWindow);
        mWindow = nullptr;
        CHECK_STATUS(AMediaCodec_delete(mEncoder), "AMediaCodec_delete failed");
        mEncoder = nullptr;
        CHECK_STATUS(AMediaCodec_delete(mDecoder), "AMediaCodec_delete failed");
        mDecoder = nullptr;
    }
    return isPass;
}

static jboolean nativeTestSimpleEncode(JNIEnv* env, jobject, jstring jEncoder, jstring jDecoder,
                                       jstring jMime, jstring jtestFile, jstring jmuxFile,
                                       jint jBitrate, jint jFramerate) {
    const char* cEncoder = env->GetStringUTFChars(jEncoder, nullptr);
    const char* cDecoder = env->GetStringUTFChars(jDecoder, nullptr);
    const char* cMime = env->GetStringUTFChars(jMime, nullptr);
    const char* cTestFile = env->GetStringUTFChars(jtestFile, nullptr);
    const char* cMuxFile = env->GetStringUTFChars(jmuxFile, nullptr);
    auto codecEncoderSurfaceTest =
            new CodecEncoderSurfaceTest(cMime, (int)jBitrate, (int)jFramerate);
    bool isPass =
            codecEncoderSurfaceTest->testSimpleEncode(cEncoder, cDecoder, cTestFile, cMuxFile);
    delete codecEncoderSurfaceTest;
    env->ReleaseStringUTFChars(jEncoder, cEncoder);
    env->ReleaseStringUTFChars(jDecoder, cDecoder);
    env->ReleaseStringUTFChars(jMime, cMime);
    env->ReleaseStringUTFChars(jtestFile, cTestFile);
    env->ReleaseStringUTFChars(jmuxFile, cMuxFile);
    return static_cast<jboolean>(isPass);
}

int registerAndroidMediaV2CtsEncoderSurfaceTest(JNIEnv* env) {
    const JNINativeMethod methodTable[] = {
            {"nativeTestSimpleEncode",
             "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
             "String;II)Z",
             (void*)nativeTestSimpleEncode},
    };
    jclass c = env->FindClass("android/mediav2/cts/CodecEncoderSurfaceTest");
    return env->RegisterNatives(c, methodTable, sizeof(methodTable) / sizeof(JNINativeMethod));
}
