/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_HARDWARE_SOUNDTRIGGER_V2_2_SOUNDTRIGGERHW_H
#define ANDROID_HARDWARE_SOUNDTRIGGER_V2_2_SOUNDTRIGGERHW_H

#include <android/hardware/soundtrigger/2.0/ISoundTriggerHw.h>
#include <android/hardware/soundtrigger/2.0/ISoundTriggerHwCallback.h>
#include <android/hardware/soundtrigger/2.2/ISoundTriggerHw.h>
#include <hardware/sound_trigger.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <stdatomic.h>
#include <system/sound_trigger.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>

namespace android {
namespace hardware {
namespace soundtrigger {
namespace V2_2 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::audio::common::V2_0::Uuid;
using ::android::hardware::soundtrigger::V2_0::ISoundTriggerHwCallback;

/**
 * According to the HIDL C++ Users Guide: client and server implementations
 * should never directly refer to anything other than the interface header
 * generated from the HIDL definition file (ie. ISoundTriggerHw.hal), so
 * this V2_2 implementation copies the V2_0 and V2_1 implementations and
 * then adds the new V2_2 implementation.
 */
struct SoundTriggerHw : public ISoundTriggerHw {
    // Methods from V2_0::ISoundTriggerHw follow.
    Return<void> getProperties(getProperties_cb _hidl_cb) override;
    Return<void> loadSoundModel(const V2_0::ISoundTriggerHw::SoundModel& soundModel,
                                const sp<V2_0::ISoundTriggerHwCallback>& callback, int32_t cookie,
                                loadSoundModel_cb _hidl_cb) override;
    Return<void> loadPhraseSoundModel(const V2_0::ISoundTriggerHw::PhraseSoundModel& soundModel,
                                      const sp<V2_0::ISoundTriggerHwCallback>& callback,
                                      int32_t cookie, loadPhraseSoundModel_cb _hidl_cb) override;
    Return<int32_t> unloadSoundModel(int32_t modelHandle) override;
    Return<int32_t> startRecognition(int32_t modelHandle,
                                     const V2_0::ISoundTriggerHw::RecognitionConfig& config,
                                     const sp<V2_0::ISoundTriggerHwCallback>& callback,
                                     int32_t cookie) override;
    Return<int32_t> stopRecognition(int32_t modelHandle) override;
    Return<int32_t> stopAllRecognitions() override;

    // Methods from V2_1::ISoundTriggerHw follow.
    Return<void> loadSoundModel_2_1(const V2_1::ISoundTriggerHw::SoundModel& soundModel,
                                    const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                    int32_t cookie, loadSoundModel_2_1_cb _hidl_cb) override;
    Return<void> loadPhraseSoundModel_2_1(const V2_1::ISoundTriggerHw::PhraseSoundModel& soundModel,
                                          const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                          int32_t cookie,
                                          loadPhraseSoundModel_2_1_cb _hidl_cb) override;
    Return<int32_t> startRecognition_2_1(int32_t modelHandle,
                                         const V2_1::ISoundTriggerHw::RecognitionConfig& config,
                                         const sp<V2_1::ISoundTriggerHwCallback>& callback,
                                         int32_t cookie) override;

    // Methods from V2_2::ISoundTriggerHw follow.
    Return<int32_t> getModelState(int32_t modelHandle) override;

    SoundTriggerHw();

    // Copied from hardware/interfaces/soundtrigger/2.0/default/SoundTriggerHalImpl.h
    class SoundModelClient : public RefBase {
       public:
        SoundModelClient(uint32_t id, V2_0::ISoundTriggerHwCallback::CallbackCookie cookie)
            : mId(id), mCookie(cookie) {}
        virtual ~SoundModelClient() {}

        uint32_t getId() const { return mId; }
        sound_model_handle_t getHalHandle() const { return mHalHandle; }
        void setHalHandle(sound_model_handle_t handle) { mHalHandle = handle; }

        virtual void recognitionCallback(struct sound_trigger_recognition_event* halEvent) = 0;
        virtual void soundModelCallback(struct sound_trigger_model_event* halEvent) = 0;

       protected:
        const uint32_t mId;
        sound_model_handle_t mHalHandle;
        V2_0::ISoundTriggerHwCallback::CallbackCookie mCookie;
    };

   protected:
    static void convertPhaseRecognitionEventFromHal(
        V2_0::ISoundTriggerHwCallback::PhraseRecognitionEvent* event,
        const struct sound_trigger_phrase_recognition_event* halEvent);
    static void convertRecognitionEventFromHal(
        V2_0::ISoundTriggerHwCallback::RecognitionEvent* event,
        const struct sound_trigger_recognition_event* halEvent);
    static void convertSoundModelEventFromHal(V2_0::ISoundTriggerHwCallback::ModelEvent* event,
                                              const struct sound_trigger_model_event* halEvent);

    virtual ~SoundTriggerHw();

    uint32_t nextUniqueModelId();
    int doLoadSoundModel(const V2_0::ISoundTriggerHw::SoundModel& soundModel,
                         sp<SoundModelClient> client);

    // RefBase
    void onFirstRef() override;

   private:
    class SoundModelClient_2_0 : public SoundModelClient {
       public:
        SoundModelClient_2_0(uint32_t id, V2_0::ISoundTriggerHwCallback::CallbackCookie cookie,
                             sp<V2_0::ISoundTriggerHwCallback> callback)
            : SoundModelClient(id, cookie), mCallback(callback) {}

        void recognitionCallback(struct sound_trigger_recognition_event* halEvent) override;
        void soundModelCallback(struct sound_trigger_model_event* halEvent) override;

       private:
        sp<V2_0::ISoundTriggerHwCallback> mCallback;
    };

    void convertUuidFromHal(Uuid* uuid, const sound_trigger_uuid_t* halUuid);
    void convertUuidToHal(sound_trigger_uuid_t* halUuid, const Uuid* uuid);
    void convertPropertiesFromHal(V2_0::ISoundTriggerHw::Properties* properties,
                                  const struct sound_trigger_properties* halProperties);
    void convertTriggerPhraseToHal(struct sound_trigger_phrase* halTriggerPhrase,
                                   const V2_0::ISoundTriggerHw::Phrase* triggerPhrase);
    // returned HAL sound model must be freed by caller
    struct sound_trigger_sound_model* convertSoundModelToHal(
        const V2_0::ISoundTriggerHw::SoundModel* soundModel);
    void convertPhraseRecognitionExtraToHal(struct sound_trigger_phrase_recognition_extra* halExtra,
                                            const V2_0::PhraseRecognitionExtra* extra);
    // returned recognition config must be freed by caller
    struct sound_trigger_recognition_config* convertRecognitionConfigToHal(
        const V2_0::ISoundTriggerHw::RecognitionConfig* config);

    static void convertPhraseRecognitionExtraFromHal(
        V2_0::PhraseRecognitionExtra* extra,
        const struct sound_trigger_phrase_recognition_extra* halExtra);

    static void soundModelCallback(struct sound_trigger_model_event* halEvent, void* cookie);
    static void recognitionCallback(struct sound_trigger_recognition_event* halEvent, void* cookie);

    const char* mModuleName;
    struct sound_trigger_hw_device* mHwDevice;
    volatile atomic_uint_fast32_t mNextModelId;
    DefaultKeyedVector<int32_t, sp<SoundModelClient> > mClients;
    Mutex mLock;

    // Copied from hardware/interfaces/soundtrigger/2.1/default/SoundTriggerHw.h
    class SoundModelClient_2_1 : public SoundModelClient {
       public:
        SoundModelClient_2_1(uint32_t id, V2_1::ISoundTriggerHwCallback::CallbackCookie cookie,
                             sp<V2_1::ISoundTriggerHwCallback> callback)
            : SoundModelClient(id, cookie), mCallback(callback) {}

        void recognitionCallback(struct sound_trigger_recognition_event* halEvent) override;
        void soundModelCallback(struct sound_trigger_model_event* halEvent) override;

       private:
        sp<V2_1::ISoundTriggerHwCallback> mCallback;
    };
};

extern "C" ISoundTriggerHw* HIDL_FETCH_ISoundTriggerHw(const char* name);

}  // namespace implementation
}  // namespace V2_2
}  // namespace soundtrigger
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_SOUNDTRIGGER_V2_2_SOUNDTRIGGERHW_H
