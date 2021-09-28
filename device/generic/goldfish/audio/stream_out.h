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
#include <atomic>
#include <android/hardware/audio/6.0/IStreamOut.h>
#include <android/hardware/audio/6.0/IDevice.h>
#include "stream_common.h"
#include "io_thread.h"

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

struct StreamOut : public IStreamOut {
    StreamOut(sp<IDevice> dev,
              void (*unrefDevice)(IDevice*),
              int32_t ioHandle,
              const DeviceAddress& device,
              const AudioConfig& config,
              hidl_bitfield<AudioOutputFlag> flags,
              const SourceMetadata& sourceMetadata);
    ~StreamOut();

    static constexpr int16_t kVolumeDenominator = 1 << 14;

    // IStream
    Return<uint64_t> getFrameSize() override;
    Return<uint64_t> getFrameCount() override;
    Return<uint64_t> getBufferSize() override;
    Return<uint32_t> getSampleRate() override;
    Return<void> getSupportedSampleRates(AudioFormat format, getSupportedSampleRates_cb _hidl_cb) override;
    Return<Result> setSampleRate(uint32_t sampleRateHz) override;
    Return<hidl_bitfield<AudioChannelMask>> getChannelMask() override;
    Return<void> getSupportedChannelMasks(AudioFormat format, getSupportedChannelMasks_cb _hidl_cb) override;
    Return<Result> setChannelMask(hidl_bitfield<AudioChannelMask> mask) override;
    Return<AudioFormat> getFormat() override;
    Return<void> getSupportedFormats(getSupportedFormats_cb _hidl_cb) override;
    Return<Result> setFormat(AudioFormat format) override;
    Return<void> getAudioProperties(getAudioProperties_cb _hidl_cb) override;
    Return<Result> addEffect(uint64_t effectId) override;
    Return<Result> removeEffect(uint64_t effectId) override;
    Return<Result> standby() override;
    Return<void> getDevices(getDevices_cb _hidl_cb) override;
    Return<Result> setDevices(const hidl_vec<DeviceAddress>& devices) override;
    Return<Result> setHwAvSync(uint32_t hwAvSync) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<Result> start() override;
    Return<Result> stop() override;
    Return<void> createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) override;
    Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
    Return<Result> close() override;

    // IStreamOut
    Return<uint32_t> getLatency() override;
    Return<Result> setVolume(float left, float right) override;
    Return<void> updateSourceMetadata(const SourceMetadata& sourceMetadata) override;
    Return<void> prepareForWriting(uint32_t frameSize, uint32_t framesCount,
                                   prepareForWriting_cb _hidl_cb) override;
    Return<void> getRenderPosition(getRenderPosition_cb _hidl_cb) override;
    Return<void> getNextWriteTimestamp(getNextWriteTimestamp_cb _hidl_cb) override;
    Return<Result> setCallback(const sp<IStreamOutCallback>& callback) override;
    Return<Result> clearCallback() override;
    Return<Result> setEventCallback(const sp<IStreamOutEventCallback>& callback) override;
    Return<void> supportsPauseAndResume(supportsPauseAndResume_cb _hidl_cb) override;
    Return<Result> pause() override;
    Return<Result> resume() override;
    Return<bool> supportsDrain() override;
    Return<Result> drain(AudioDrain type) override;
    Return<Result> flush() override;
    Return<void> getPresentationPosition(getPresentationPosition_cb _hidl_cb) override;
    Return<Result> selectPresentation(int32_t presentationId, int32_t programId) override;
    Return<void> getDualMonoMode(getDualMonoMode_cb _hidl_cb) override;
    Return<Result> setDualMonoMode(DualMonoMode mode) override;
    Return<void> getAudioDescriptionMixLevel(getAudioDescriptionMixLevel_cb _hidl_cb) override;
    Return<Result> setAudioDescriptionMixLevel(float leveldB) override;
    Return<void> getPlaybackRateParameters(getPlaybackRateParameters_cb _hidl_cb) override;
    Return<Result> setPlaybackRateParameters(const PlaybackRate &playbackRate) override;

    int16_t getVolumeNumerator() const { return mVolumeNumerator; };
    const DeviceAddress &getDeviceAddress() const { return mCommon.m_device; }
    const AudioConfig &getAudioConfig() const { return mCommon.m_config; }
    const hidl_bitfield<AudioOutputFlag> &getAudioOutputFlags() const { return mCommon.m_flags; }

    uint64_t &getFrameCounter() { return mFrames; }

private:
    Result closeImpl(bool fromDctor);

    sp<IDevice> mDev;
    void (* const mUnrefDevice)(IDevice*);
    const StreamCommon mCommon;
    const SourceMetadata mSourceMetadata;
    std::unique_ptr<IOThread> mWriteThread;
    std::atomic<int16_t> mVolumeNumerator = kVolumeDenominator;

    // The count is not reset to zero when output enters standby.
    uint64_t mFrames = 0;
};

}  // namespace implementation
}  // namespace V6_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
