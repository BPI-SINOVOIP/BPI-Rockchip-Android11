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

#pragma once
#include <android/hardware/audio/6.0/IPrimaryDevice.h>
#include <atomic>
#include <unordered_map>
#include "talsa.h"

namespace android {
namespace hardware {
namespace audio {
namespace V6_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;

using namespace ::android::hardware::audio::common::V6_0;
using namespace ::android::hardware::audio::V6_0;

struct PrimaryDevice : public IPrimaryDevice {
    PrimaryDevice();

    Return<Result> initCheck() override;
    Return<Result> setMasterVolume(float volume) override;
    Return<void> getMasterVolume(getMasterVolume_cb _hidl_cb) override;
    Return<Result> setMicMute(bool mute) override;
    Return<void> getMicMute(getMicMute_cb _hidl_cb) override;
    Return<Result> setMasterMute(bool mute) override;
    Return<void> getMasterMute(getMasterMute_cb _hidl_cb) override;
    Return<void> getInputBufferSize(const AudioConfig& config,
                                    getInputBufferSize_cb _hidl_cb) override;
    Return<void> openOutputStream(int32_t ioHandle,
                                  const DeviceAddress& device,
                                  const AudioConfig& config,
                                  hidl_bitfield<AudioOutputFlag> flags,
                                  const SourceMetadata& sourceMetadata,
                                  openOutputStream_cb _hidl_cb) override;
    Return<void> openInputStream(int32_t ioHandle,
                                 const DeviceAddress& device,
                                 const AudioConfig& config,
                                 hidl_bitfield<AudioInputFlag> flags,
                                 const SinkMetadata& sinkMetadata,
                                 openInputStream_cb _hidl_cb) override;
    Return<bool> supportsAudioPatches() override;
    Return<void> createAudioPatch(const hidl_vec<AudioPortConfig>& sources,
                                  const hidl_vec<AudioPortConfig>& sinks,
                                  createAudioPatch_cb _hidl_cb) override;
    Return<void> updateAudioPatch(AudioPatchHandle previousPatch,
                                  const hidl_vec<AudioPortConfig>& sources,
                                  const hidl_vec<AudioPortConfig>& sinks,
                                  updateAudioPatch_cb _hidl_cb) override;
    Return<Result> releaseAudioPatch(AudioPatchHandle patch) override;
    Return<void> getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) override;
    Return<Result> setAudioPortConfig(const AudioPortConfig& config) override;
    Return<Result> setScreenState(bool turnedOn) override;
    Return<void> getHwAvSync(getHwAvSync_cb _hidl_cb) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<void> getMicrophones(getMicrophones_cb _hidl_cb) override;
    Return<Result> setConnectedState(const DeviceAddress& address, bool connected) override;
    Return<Result> close() override;
    Return<Result> addDeviceEffect(AudioPortHandle device, uint64_t effectId) override;
    Return<Result> removeDeviceEffect(AudioPortHandle device, uint64_t effectId) override;
    Return<Result> setVoiceVolume(float volume) override;
    Return<Result> setMode(AudioMode mode) override;
    Return<Result> setBtScoHeadsetDebugName(const hidl_string& name) override;
    Return<void> getBtScoNrecEnabled(getBtScoNrecEnabled_cb _hidl_cb) override;
    Return<Result> setBtScoNrecEnabled(bool enabled) override;
    Return<void> getBtScoWidebandEnabled(getBtScoWidebandEnabled_cb _hidl_cb) override;
    Return<Result> setBtScoWidebandEnabled(bool enabled) override;
    Return<void> getTtyMode(getTtyMode_cb _hidl_cb) override;
    Return<Result> setTtyMode(IPrimaryDevice::TtyMode mode) override;
    Return<void> getHacEnabled(getHacEnabled_cb _hidl_cb) override;
    Return<Result> setHacEnabled(bool enabled) override;
    Return<void> getBtHfpEnabled(getBtHfpEnabled_cb _hidl_cb) override;
    Return<Result> setBtHfpEnabled(bool enabled) override;
    Return<Result> setBtHfpSampleRate(uint32_t sampleRateHz) override;
    Return<Result> setBtHfpVolume(float volume) override;
    Return<Result> updateRotation(IPrimaryDevice::Rotation rotation) override;

private:
    static void unrefDevice(IDevice*);
    void unrefDeviceImpl();

    talsa::MixerPtr     mMixer;
    talsa::mixer_ctl_t  *mMixerMasterVolumeCtl = nullptr;
    talsa::mixer_ctl_t  *mMixerCaptureVolumeCtl = nullptr;
    talsa::mixer_ctl_t  *mMixerMasterPaybackSwitchCtl = nullptr;
    talsa::mixer_ctl_t  *mMixerCaptureSwitchCtl = nullptr;
    float               mMasterVolume = 1.0f;
    std::atomic<int>    mNStreams = 0;

    struct AudioPatch {
        AudioPortConfig source;
        AudioPortConfig sink;
    };

    AudioPatchHandle    mNextAudioPatchHandle = 0;
    std::unordered_map<AudioPatchHandle, AudioPatch> mAudioPatches;
};

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
