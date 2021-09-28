/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef CTS_NATIVE_AUDIO_ANALYZER_H
#define CTS_NATIVE_AUDIO_ANALYZER_H

#define LOG_TAG "NativeAudioAnalyzer"
#include <android/log.h>

#ifndef MODULE_NAME
#define MODULE_NAME  "NativeAudioAnalyzer"
#endif

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, MODULE_NAME, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, MODULE_NAME, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, MODULE_NAME, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, MODULE_NAME, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, MODULE_NAME, __VA_ARGS__)
#define ALOGF(...) __android_log_print(ANDROID_LOG_FATAL, MODULE_NAME, __VA_ARGS__)

#include <aaudio/AAudio.h>

#include "analyzer/GlitchAnalyzer.h"
#include "analyzer/LatencyAnalyzer.h"

class NativeAudioAnalyzer {
public:

    /**
     * Open the audio input and output streams.
     * @return AAUDIO_OK or negative error
     */
    aaudio_result_t openAudio();

    /**
     * Start the audio input and output streams.
     * @return AAUDIO_OK or negative error
     */
    aaudio_result_t startAudio();

    /**
     * Stop the audio input and output streams.
     * @return AAUDIO_OK or negative error
     */
    aaudio_result_t stopAudio();

    /**
     * Close the audio input and output streams.
     * @return AAUDIO_OK or negative error
     */
    aaudio_result_t closeAudio();

    /**
     * @return true if enough audio input has been recorded
     */
    bool isRecordingComplete();

    /**
     * Analyze the input and measure the latency between output and input.
     * @return AAUDIO_OK or negative error
     */
    int analyze();

    /**
     * @return the measured latency in milliseconds
     */
    double getLatencyMillis();

    /**
     * @return the sample rate (in Hz) used for the measurement signal
     */
    int getSampleRate();

    /**
     * The confidence is based on a normalized correlation.
     * It ranges from 0.0 to 1.0. Higher is better.
     *
     * @return the confidence in the latency result
     */
    double getConfidence();

    aaudio_result_t getError() {
        return mInputError ? mInputError : mOutputError;
    }

    AAudioStream      *mInputStream = nullptr;
    AAudioStream      *mOutputStream = nullptr;
    aaudio_format_t    mActualInputFormat = AAUDIO_FORMAT_INVALID;
    int16_t           *mInputShortData = nullptr;
    float             *mInputFloatData = nullptr;
    int32_t            mOutputSampleRate = 0;

    aaudio_result_t    mInputError = AAUDIO_OK;
    aaudio_result_t    mOutputError = AAUDIO_OK;

aaudio_data_callback_result_t dataCallbackProc(
        void *audioData,
        int32_t numFrames);

private:

    int32_t readFormattedData(int32_t numFrames);

    GlitchAnalyzer       mSineAnalyzer;
    PulseLatencyAnalyzer mPulseLatencyAnalyzer;
    LoopbackProcessor   *mLoopbackProcessor;

    int32_t            mInputFramesMaximum = 0;
    int32_t            mActualInputChannelCount = 0;
    int32_t            mActualOutputChannelCount = 0;
    int32_t            mNumCallbacksToDrain = kNumCallbacksToDrain;
    int32_t            mNumCallbacksToNotRead = kNumCallbacksToNotRead;
    int32_t            mNumCallbacksToDiscard = kNumCallbacksToDiscard;
    int32_t            mMinNumFrames = INT32_MAX;
    int32_t            mMaxNumFrames = 0;
    int32_t            mInsufficientReadCount = 0;
    int32_t            mInsufficientReadFrames = 0;
    int32_t            mFramesReadTotal = 0;
    int32_t            mFramesWrittenTotal = 0;
    bool               mIsDone = false;

    static constexpr int kLogPeriodMillis         = 1000;
    static constexpr int kNumInputChannels        = 1;
    static constexpr int kNumCallbacksToDrain     = 20;
    static constexpr int kNumCallbacksToNotRead   = 0; // let input fill back up
    static constexpr int kNumCallbacksToDiscard   = 20;
    static constexpr int kDefaultHangTimeMillis   = 50;
    static constexpr int kMaxGlitchEventsToSave   = 32;
    static constexpr int kDefaultOutputSizeBursts = 2;
};

#endif // CTS_NATIVE_AUDIO_ANALYZER_H
