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

#include "NativeAudioAnalyzer.h"

static void convertPcm16ToFloat(const int16_t *source,
                                float *destination,
                                int32_t numSamples) {
    constexpr float scaler = 1.0f / 32768.0f;
    for (int i = 0; i < numSamples; i++) {
        destination[i] = source[i] * scaler;
    }
}

// Fill the audio output buffer.
int32_t NativeAudioAnalyzer::readFormattedData(int32_t numFrames) {
    int32_t framesRead = AAUDIO_ERROR_INVALID_FORMAT;
    if (mActualInputFormat == AAUDIO_FORMAT_PCM_I16) {
        framesRead = AAudioStream_read(mInputStream, mInputShortData,
                                       numFrames,
                                       0 /* timeoutNanoseconds */);
    } else if (mActualInputFormat == AAUDIO_FORMAT_PCM_FLOAT) {
        framesRead = AAudioStream_read(mInputStream, mInputFloatData,
                                       numFrames,
                                       0 /* timeoutNanoseconds */);
    } else {
        ALOGE("ERROR actualInputFormat = %d\n", mActualInputFormat);
        assert(false);
    }
    if (framesRead < 0) {
        // Expect INVALID_STATE if STATE_STARTING
        if (mFramesReadTotal > 0) {
            mInputError = framesRead;
            ALOGE("ERROR in read = %d = %s\n", framesRead,
                   AAudio_convertResultToText(framesRead));
        } else {
            framesRead = 0;
        }
    } else {
        mFramesReadTotal += framesRead;
    }
    return framesRead;
}

aaudio_data_callback_result_t NativeAudioAnalyzer::dataCallbackProc(
        void *audioData,
        int32_t numFrames
) {
    aaudio_data_callback_result_t callbackResult = AAUDIO_CALLBACK_RESULT_CONTINUE;
    float  *outputData = (float  *) audioData;

    // Read audio data from the input stream.
    int32_t actualFramesRead;

    if (numFrames > mInputFramesMaximum) {
        ALOGE("%s() numFrames:%d > mInputFramesMaximum:%d", __func__, numFrames, mInputFramesMaximum);
        mInputError = AAUDIO_ERROR_OUT_OF_RANGE;
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    if (numFrames > mMaxNumFrames) {
        mMaxNumFrames = numFrames;
    }
    if (numFrames < mMinNumFrames) {
        mMinNumFrames = numFrames;
    }

    // Silence the output.
    int32_t numBytes = numFrames * mActualOutputChannelCount * sizeof(float);
    memset(audioData, 0 /* value */, numBytes);

    if (mNumCallbacksToDrain > 0) {
        // Drain the input FIFOs.
        int32_t totalFramesRead = 0;
        do {
            actualFramesRead = readFormattedData(numFrames);
            if (actualFramesRead > 0) {
                totalFramesRead += actualFramesRead;
            } else if (actualFramesRead < 0) {
                callbackResult = AAUDIO_CALLBACK_RESULT_STOP;
            }
            // Ignore errors because input stream may not be started yet.
        } while (actualFramesRead > 0);
        // Only counts if we actually got some data.
        if (totalFramesRead > 0) {
            mNumCallbacksToDrain--;
        }

    } else if (mNumCallbacksToNotRead > 0) {
        // Let the input fill up a bit so we are not so close to the write pointer.
        mNumCallbacksToNotRead--;
    } else if (mNumCallbacksToDiscard > 0) {
        // Ignore. Allow the input to fill back up to equilibrium with the output.
        actualFramesRead = readFormattedData(numFrames);
        if (actualFramesRead < 0) {
            callbackResult = AAUDIO_CALLBACK_RESULT_STOP;
        }
        mNumCallbacksToDiscard--;

    } else {
        // The full duplex stream is now stable so process the audio.
        int32_t numInputBytes = numFrames * mActualInputChannelCount * sizeof(float);
        memset(mInputFloatData, 0 /* value */, numInputBytes);

        int64_t inputFramesWritten = AAudioStream_getFramesWritten(mInputStream);
        int64_t inputFramesRead = AAudioStream_getFramesRead(mInputStream);
        int64_t framesAvailable = inputFramesWritten - inputFramesRead;

        // Read the INPUT data.
        actualFramesRead = readFormattedData(numFrames); // READ
        if (actualFramesRead < 0) {
            callbackResult = AAUDIO_CALLBACK_RESULT_STOP;
        } else {
            if (actualFramesRead < numFrames) {
                if(actualFramesRead < (int32_t) framesAvailable) {
                    ALOGE("insufficient for no reason, numFrames = %d"
                                   ", actualFramesRead = %d"
                                   ", inputFramesWritten = %d"
                                   ", inputFramesRead = %d"
                                   ", available = %d\n",
                           numFrames,
                           actualFramesRead,
                           (int) inputFramesWritten,
                           (int) inputFramesRead,
                           (int) framesAvailable);
                }
                mInsufficientReadCount++;
                mInsufficientReadFrames += numFrames - actualFramesRead; // deficit
                // ALOGE("Error insufficientReadCount = %d\n",(int)mInsufficientReadCount);
            }

            int32_t numSamples = actualFramesRead * mActualInputChannelCount;

            if (mActualInputFormat == AAUDIO_FORMAT_PCM_I16) {
                convertPcm16ToFloat(mInputShortData, mInputFloatData, numSamples);
            }

            // Process the INPUT and generate the OUTPUT.
            mLoopbackProcessor->process(mInputFloatData,
                                               mActualInputChannelCount,
                                               numFrames,
                                               outputData,
                                               mActualOutputChannelCount,
                                               numFrames);

            mIsDone = mLoopbackProcessor->isDone();
            if (mIsDone) {
                callbackResult = AAUDIO_CALLBACK_RESULT_STOP;
            }
        }
    }
    mFramesWrittenTotal += numFrames;

    return callbackResult;
}

static aaudio_data_callback_result_t s_MyDataCallbackProc(
        AAudioStream * /* outputStream */,
        void *userData,
        void *audioData,
        int32_t numFrames) {
    NativeAudioAnalyzer *myData = (NativeAudioAnalyzer *) userData;
    return myData->dataCallbackProc(audioData, numFrames);
}

static void s_MyErrorCallbackProc(
        AAudioStream * /* stream */,
        void * userData,
        aaudio_result_t error) {
    ALOGE("Error Callback, error: %d\n",(int)error);
    NativeAudioAnalyzer *myData = (NativeAudioAnalyzer *) userData;
    myData->mOutputError = error;
}

bool NativeAudioAnalyzer::isRecordingComplete() {
    return mPulseLatencyAnalyzer.isRecordingComplete();
}

int NativeAudioAnalyzer::analyze() {
    mPulseLatencyAnalyzer.analyze();
    return getError(); // TODO review
}

double NativeAudioAnalyzer::getLatencyMillis() {
    return mPulseLatencyAnalyzer.getMeasuredLatency() * 1000.0 / 48000;
}

double NativeAudioAnalyzer::getConfidence() {
    return mPulseLatencyAnalyzer.getMeasuredConfidence();
}

int NativeAudioAnalyzer::getSampleRate() {
    return mOutputSampleRate;
}

aaudio_result_t NativeAudioAnalyzer::openAudio() {
    AAudioStreamBuilder *builder = nullptr;

    mLoopbackProcessor = &mPulseLatencyAnalyzer; // for latency test

    // Use an AAudioStreamBuilder to contain requested parameters.
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK) {
        ALOGE("AAudio_createStreamBuilder() returned %s",
               AAudio_convertResultToText(result));
        return result;
    }

    // Create the OUTPUT stream -----------------------
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, 2); // stereo
    AAudioStreamBuilder_setDataCallback(builder, s_MyDataCallbackProc, this);
    AAudioStreamBuilder_setErrorCallback(builder, s_MyErrorCallbackProc, this);

    result = AAudioStreamBuilder_openStream(builder, &mOutputStream);
    if (result != AAUDIO_OK) {
        ALOGE("NativeAudioAnalyzer::openAudio() OUTPUT error %s",
               AAudio_convertResultToText(result));
        return result;
    }

    int32_t outputFramesPerBurst = AAudioStream_getFramesPerBurst(mOutputStream);
    (void) AAudioStream_setBufferSizeInFrames(mOutputStream, outputFramesPerBurst * kDefaultOutputSizeBursts);

    mOutputSampleRate = AAudioStream_getSampleRate(mOutputStream);
    mActualOutputChannelCount = AAudioStream_getChannelCount(mOutputStream);

    // Create the INPUT stream -----------------------
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_UNSPECIFIED);
    AAudioStreamBuilder_setSampleRate(builder, mOutputSampleRate); // must match
    AAudioStreamBuilder_setChannelCount(builder, 1); // mono
    AAudioStreamBuilder_setDataCallback(builder, nullptr, nullptr);
    AAudioStreamBuilder_setErrorCallback(builder, nullptr, nullptr);
    result = AAudioStreamBuilder_openStream(builder, &mInputStream);
    if (result != AAUDIO_OK) {
        ALOGE("NativeAudioAnalyzer::openAudio() INPUT error %s",
               AAudio_convertResultToText(result));
        return result;
    }

    int32_t actualCapacity = AAudioStream_getBufferCapacityInFrames(mInputStream);
    (void) AAudioStream_setBufferSizeInFrames(mInputStream, actualCapacity);

    // ------- Setup loopbackData -----------------------------
    mActualInputFormat = AAudioStream_getFormat(mInputStream);
    mActualInputChannelCount = AAudioStream_getChannelCount(mInputStream);

    // Allocate a buffer for the audio data.
    mInputFramesMaximum = 32 * AAudioStream_getFramesPerBurst(mInputStream);

    if (mActualInputFormat == AAUDIO_FORMAT_PCM_I16) {
        mInputShortData = new int16_t[mInputFramesMaximum * mActualInputChannelCount]{};
    }
    mInputFloatData = new float[mInputFramesMaximum * mActualInputChannelCount]{};

    return result;
}

aaudio_result_t NativeAudioAnalyzer::startAudio() {
    mLoopbackProcessor->prepareToTest();

    // Start OUTPUT first so INPUT does not overflow.
    aaudio_result_t result = AAudioStream_requestStart(mOutputStream);
    if (result != AAUDIO_OK) {
        stopAudio();
        return result;
    }

    result = AAudioStream_requestStart(mInputStream);
    if (result != AAUDIO_OK) {
        stopAudio();
        return result;
    }

    return result;
}

aaudio_result_t NativeAudioAnalyzer::stopAudio() {
    aaudio_result_t result1 = AAUDIO_OK;
    aaudio_result_t result2 = AAUDIO_OK;
    ALOGD("stopAudio() , minNumFrames = %d, maxNumFrames = %d\n", mMinNumFrames, mMaxNumFrames);
    // Stop OUTPUT first because it uses INPUT.
    if (mOutputStream != nullptr) {
        result1 = AAudioStream_requestStop(mOutputStream);
    }

    // Stop INPUT.
    if (mInputStream != nullptr) {
        result2 = AAudioStream_requestStop(mInputStream);
    }
    return result1 != AAUDIO_OK ? result1 : result2;
}

aaudio_result_t NativeAudioAnalyzer::closeAudio() {
    aaudio_result_t result1 = AAUDIO_OK;
    aaudio_result_t result2 = AAUDIO_OK;
    // Stop and close OUTPUT first because it uses INPUT.
    if (mOutputStream != nullptr) {
        result1 = AAudioStream_close(mOutputStream);
        mOutputStream = nullptr;
    }

    // Stop and close INPUT.
    if (mInputStream != nullptr) {
        result2 = AAudioStream_close(mInputStream);
        mInputStream = nullptr;
    }
    return result1 != AAUDIO_OK ? result1 : result2;
}
