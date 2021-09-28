/*
 * Copyright 2017 The Android Open Source Project
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
#ifndef CTS_MEDIA_TEST_AAUDIO_UTILS_H
#define CTS_MEDIA_TEST_AAUDIO_UTILS_H

#include <dlfcn.h>
#include <map>
#include <sys/system_properties.h>

#include <aaudio/AAudio.h>

int64_t getNanoseconds(clockid_t clockId = CLOCK_MONOTONIC);
const char* performanceModeToString(aaudio_performance_mode_t mode);
const char* sharingModeToString(aaudio_sharing_mode_t mode);

static constexpr const char* FEATURE_PLAYBACK = "android.hardware.audio.output";
static constexpr const char* FEATURE_RECORDING = "android.hardware.microphone";
static constexpr const char* FEATURE_LOW_LATENCY = "android.hardware.audio.low_latency";
bool deviceSupportsFeature(const char* feature);

class StreamBuilderHelper {
  public:
    struct Parameters {
        int32_t sampleRate;
        int32_t channelCount;
        aaudio_format_t dataFormat;
        aaudio_sharing_mode_t sharingMode;
        aaudio_performance_mode_t perfMode;
    };

    void initBuilder();
    void createAndVerifyStream(bool *success);
    void close();

    void startStream() {
        streamCommand(&AAudioStream_requestStart,
                AAUDIO_STREAM_STATE_STARTING, AAUDIO_STREAM_STATE_STARTED);
    }
    void pauseStream() {
        streamCommand(&AAudioStream_requestPause,
                AAUDIO_STREAM_STATE_PAUSING, AAUDIO_STREAM_STATE_PAUSED);
    }
    void stopStream() {
        streamCommand(&AAudioStream_requestStop,
                AAUDIO_STREAM_STATE_STOPPING, AAUDIO_STREAM_STATE_STOPPED);
    }
    void flushStream() {
        streamCommand(&AAudioStream_requestFlush,
                AAUDIO_STREAM_STATE_FLUSHING, AAUDIO_STREAM_STATE_FLUSHED);
    }

    AAudioStreamBuilder* builder() const { return mBuilder; }
    AAudioStream* stream() const { return mStream; }
    const Parameters& actual() const { return mActual; }
    int32_t framesPerBurst() const { return mFramesPerBurst; }

  protected:
    StreamBuilderHelper(aaudio_direction_t direction, int32_t sampleRate,
            int32_t channelCount, aaudio_format_t dataFormat,
            aaudio_sharing_mode_t sharingMode, aaudio_performance_mode_t perfMode);
    ~StreamBuilderHelper();

    typedef aaudio_result_t (StreamCommand)(AAudioStream*);
    void streamCommand(
            StreamCommand cmd, aaudio_stream_state_t fromState, aaudio_stream_state_t toState);

    static const std::map<aaudio_performance_mode_t, int64_t> sMaxFramesPerBurstMs;
    const aaudio_direction_t mDirection;
    const Parameters mRequested;
    Parameters mActual;
    int32_t mFramesPerBurst;
    AAudioStreamBuilder *mBuilder;
    AAudioStream *mStream;
};

class InputStreamBuilderHelper : public StreamBuilderHelper {
  public:
    InputStreamBuilderHelper(
            aaudio_sharing_mode_t requestedSharingMode,
            aaudio_performance_mode_t requestedPerfMode,
            aaudio_format_t requestedFormat = AAUDIO_FORMAT_PCM_FLOAT);
};

class OutputStreamBuilderHelper : public StreamBuilderHelper {
  public:
    OutputStreamBuilderHelper(
            aaudio_sharing_mode_t requestedSharingMode,
            aaudio_performance_mode_t requestedPerfMode,
            aaudio_format_t requestedFormat = AAUDIO_FORMAT_PCM_I16);
    void initBuilder();
    void createAndVerifyStream(bool *success);

  private:
    const int32_t kBufferCapacityFrames = 2000;
};


#define LIB_AAUDIO_NAME          "libaaudio.so"
#define FUNCTION_IS_MMAP         "AAudioStream_isMMapUsed"
#define FUNCTION_SET_MMAP_POLICY "AAudio_setMMapPolicy"
#define FUNCTION_GET_MMAP_POLICY "AAudio_getMMapPolicy"

enum {
    AAUDIO_POLICY_UNSPECIFIED = 0,
/* These definitions are from aaudio/AAudioTesting.h */
    AAUDIO_POLICY_NEVER = 1,
    AAUDIO_POLICY_AUTO = 2,
    AAUDIO_POLICY_ALWAYS = 3
};
typedef int32_t aaudio_policy_t;

/**
 * Call some AAudio test routines that are not part of the normal API.
 */
class AAudioExtensions {
public:
    AAudioExtensions();

    static bool isPolicyEnabled(int32_t policy) {
        return (policy == AAUDIO_POLICY_AUTO || policy == AAUDIO_POLICY_ALWAYS);
    }

    static AAudioExtensions &getInstance() {
        static AAudioExtensions instance;
        return instance;
    }

    static int getMMapPolicyProperty() {
        return getIntegerProperty("aaudio.mmap_policy", AAUDIO_POLICY_UNSPECIFIED);
    }

    aaudio_policy_t getMMapPolicy() {
        if (!mFunctionsLoaded) return -1;
        return mAAudio_getMMapPolicy();
    }

    int32_t setMMapPolicy(aaudio_policy_t policy) {
        if (!mFunctionsLoaded) return -1;
        return mAAudio_setMMapPolicy(policy);
    }

    bool isMMapUsed(AAudioStream *aaudioStream) {
        if (!mFunctionsLoaded) return false;
        return mAAudioStream_isMMap(aaudioStream);
    }

    int32_t setMMapEnabled(bool enabled) {
        return setMMapPolicy(enabled ? AAUDIO_POLICY_AUTO : AAUDIO_POLICY_NEVER);
    }

    bool isMMapEnabled() {
        return isPolicyEnabled(mAAudio_getMMapPolicy());
    }

    bool isMMapSupported() const {
        return mMMapSupported;
    }

    bool isMMapExclusiveSupported() const {
        return mMMapExclusiveSupported;
    }

private:

    static int getIntegerProperty(const char *name, int defaultValue);

    /**
     * Load some AAudio test functions.
     * This should only be called once from the constructor.
     * @return true if it succeeds
     */
    bool loadLibrary();

    bool      mFunctionsLoaded = false;
    void     *mLibHandle = nullptr;
    bool    (*mAAudioStream_isMMap)(AAudioStream *stream) = nullptr;
    int32_t (*mAAudio_setMMapPolicy)(aaudio_policy_t policy) = nullptr;
    aaudio_policy_t (*mAAudio_getMMapPolicy)() = nullptr;

    const bool   mMMapSupported;
    const bool   mMMapExclusiveSupported;
};

#endif  // CTS_MEDIA_TEST_AAUDIO_UTILS_H
