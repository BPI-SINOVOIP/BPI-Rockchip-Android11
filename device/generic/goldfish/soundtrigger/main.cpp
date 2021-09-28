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

#include <android/hardware/soundtrigger/2.2/ISoundTriggerHw.h>

namespace android {
namespace hardware {
namespace soundtrigger {
namespace V2_2 {
namespace implementation {

using ::android::hardware::soundtrigger::V2_0::SoundModelHandle;
using ::android::hardware::soundtrigger::V2_0::ISoundTriggerHwCallback;
using ::android::hardware::soundtrigger::V2_0::RecognitionMode;
using ::android::hardware::audio::common::V2_0::Uuid;

struct SoundTriggerHw : public ISoundTriggerHw {
    // Methods from V2_0::ISoundTriggerHw follow.
    Return<void> getProperties(getProperties_cb _hidl_cb) override {
        V2_0::ISoundTriggerHw::Properties props;

        props.implementor = "The Android Open Source Project";
        props.description = "The Andtoid Studio Emulator Soundtrigger no-op implementation";
        props.version = 0;
        props.uuid = (Uuid){
            .timeLow = 0x04030201,
            .timeMid = 0x0605,
            .versionAndTimeHigh = 0x0807,
            .variantAndClockSeqHigh = 0x0A09,
            .node = hidl_array<uint8_t, 6>({ 'r', 'a', 'n', 'c', 'h', 'u' }),
        };
        props.maxSoundModels = 42;
        props.maxKeyPhrases = 4242;
        props.maxUsers = 7;
        props.recognitionModes = RecognitionMode::VOICE_TRIGGER
                                 | RecognitionMode::GENERIC_TRIGGER;
        props.captureTransition = false;
        props.maxBufferMs = 0;
        props.concurrentCapture = false;
        props.triggerInEvent = true;
        props.powerConsumptionMw = 42;

        _hidl_cb(0, props);
        return Void();
    }

    Return<void> loadSoundModel(const V2_0::ISoundTriggerHw::SoundModel& soundModel,
                                const sp<V2_0::ISoundTriggerHwCallback>& callback,
                                int32_t cookie,
                                loadSoundModel_cb _hidl_cb) override {
        (void)soundModel;
        (void)callback;
        (void)cookie;
        _hidl_cb(0, genHandle());
        return Void();
    }

    Return<void> loadPhraseSoundModel(const V2_0::ISoundTriggerHw::PhraseSoundModel& soundModel,
                                      const sp<V2_0::ISoundTriggerHwCallback>& callback,
                                      int32_t cookie,
                                      loadPhraseSoundModel_cb _hidl_cb) override {
        (void)soundModel;
        (void)callback;
        (void)cookie;
        _hidl_cb(0, genHandle());
        return Void();
    }

    Return<int32_t> unloadSoundModel(int32_t modelHandle) override {
        (void)modelHandle;
        return 0;
    }

    Return<int32_t> startRecognition(int32_t modelHandle,
                                     const V2_0::ISoundTriggerHw::RecognitionConfig& config,
                                     const sp<V2_0::ISoundTriggerHwCallback>& callback,
                                     int32_t cookie) override {
        (void)modelHandle;
        (void)config;
        (void)callback;
        (void)cookie;
        return 0;
    }

    Return<int32_t> stopRecognition(int32_t modelHandle) override {
        (void)modelHandle;
        return 0;
    }

    Return<int32_t> stopAllRecognitions() override {
        return 0;
    }

    // Methods from V2_1::ISoundTriggerHw follow.
    Return<void> loadSoundModel_2_1(const V2_1::ISoundTriggerHw::SoundModel& soundModel,
                                    const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                    int32_t cookie,
                                    loadSoundModel_2_1_cb _hidl_cb) override {
        (void)soundModel;
        (void)callback;
        (void)cookie;
        _hidl_cb(0, genHandle());
        return Void();
    }

    Return<void> loadPhraseSoundModel_2_1(const V2_1::ISoundTriggerHw::PhraseSoundModel& soundModel,
                                          const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                          int32_t cookie,
                                          loadPhraseSoundModel_2_1_cb _hidl_cb) override {
        (void)soundModel;
        (void)callback;
        (void)cookie;
        _hidl_cb(0, genHandle());
        return Void();
    }

    Return<int32_t> startRecognition_2_1(int32_t modelHandle,
                                         const V2_1::ISoundTriggerHw::RecognitionConfig& config,
                                         const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                         int32_t cookie) override {
        (void)modelHandle;
        (void)config;
        (void)callback;
        (void)cookie;
        return 0;
    }

    // Methods from V2_2::ISoundTriggerHw follow.
    Return<int32_t> getModelState(int32_t modelHandle) override {
        (void)modelHandle;
        return 0;
    }

    SoundModelHandle genHandle() {
        mHandle = std::max(0, mHandle + 1);
        return mHandle;
    }

    SoundModelHandle mHandle = 0;
};

extern "C" ISoundTriggerHw* HIDL_FETCH_ISoundTriggerHw(const char* /* name */) {
    return new SoundTriggerHw();
}

}  // namespace implementation
}  // namespace V2_2
}  // namespace soundtrigger
}  // namespace hardware
}  // namespace android
