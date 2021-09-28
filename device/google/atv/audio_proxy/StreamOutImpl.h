// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

// clang-format off
#include PATH(android/hardware/audio/FILE_VERSION/IStreamOut.h)
// clang-format on

#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <utils/Thread.h>

using ::android::sp;
using ::android::Thread;
using ::android::hardware::EventFlag;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::kSynchronizedReadWrite;
using ::android::hardware::MessageQueue;
using ::android::hardware::Return;
using ::android::hardware::Void;
using namespace ::android::hardware::audio::common::CPP_VERSION;
using namespace ::android::hardware::audio::CPP_VERSION;

namespace audio_proxy {
namespace CPP_VERSION {
class AudioProxyStreamOut;

class StreamOutImpl : public IStreamOut {
 public:
  using CommandMQ = MessageQueue<WriteCommand, kSynchronizedReadWrite>;
  using DataMQ = MessageQueue<uint8_t, kSynchronizedReadWrite>;
  using StatusMQ = MessageQueue<WriteStatus, kSynchronizedReadWrite>;

  explicit StreamOutImpl(std::unique_ptr<AudioProxyStreamOut> stream);
  ~StreamOutImpl() override;

  // Methods from ::android::hardware::audio::CPP_VERSION::IStream follow.
  Return<uint64_t> getFrameSize() override;
  Return<uint64_t> getFrameCount() override;
  Return<uint64_t> getBufferSize() override;
  Return<uint32_t> getSampleRate() override;
  Return<void> getSupportedSampleRates(
      AudioFormat format, getSupportedSampleRates_cb _hidl_cb) override;
  Return<void> getSupportedChannelMasks(
      AudioFormat format, getSupportedChannelMasks_cb _hidl_cb) override;
  Return<Result> setSampleRate(uint32_t sampleRateHz) override;
  Return<hidl_bitfield<AudioChannelMask>> getChannelMask() override;
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
  Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                             const hidl_vec<hidl_string>& keys,
                             getParameters_cb _hidl_cb) override;
  Return<Result> setParameters(
      const hidl_vec<ParameterValue>& context,
      const hidl_vec<ParameterValue>& parameters) override;
  Return<Result> setHwAvSync(uint32_t hwAvSync) override;
  Return<Result> close() override;

  // Methods from ::android::hardware::audio::CPP_VERSION::IStreamOut follow.
  Return<uint32_t> getLatency() override;
  Return<Result> setVolume(float left, float right) override;
  Return<void> prepareForWriting(uint32_t frameSize, uint32_t framesCount,
                                 prepareForWriting_cb _hidl_cb) override;
  Return<void> getRenderPosition(getRenderPosition_cb _hidl_cb) override;
  Return<void> getNextWriteTimestamp(
      getNextWriteTimestamp_cb _hidl_cb) override;
  Return<Result> setCallback(const sp<IStreamOutCallback>& callback) override;
  Return<Result> clearCallback() override;
  Return<void> supportsPauseAndResume(
      supportsPauseAndResume_cb _hidl_cb) override;
  Return<Result> pause() override;
  Return<Result> resume() override;
  Return<bool> supportsDrain() override;
  Return<Result> drain(AudioDrain type) override;
  Return<Result> flush() override;
  Return<void> getPresentationPosition(
      getPresentationPosition_cb _hidl_cb) override;
  Return<Result> start() override;
  Return<Result> stop() override;
  Return<void> createMmapBuffer(int32_t minSizeFrames,
                                createMmapBuffer_cb _hidl_cb) override;
  Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
  Return<void> updateSourceMetadata(
      const SourceMetadata& sourceMetadata) override;
  Return<Result> selectPresentation(int32_t presentationId,
                                    int32_t programId) override;

 private:
  typedef void (*EventFlagDeleter)(EventFlag*);

  Result closeImpl();

  std::unique_ptr<AudioProxyStreamOut> mStream;

  std::unique_ptr<CommandMQ> mCommandMQ;
  std::unique_ptr<DataMQ> mDataMQ;
  std::unique_ptr<StatusMQ> mStatusMQ;
  std::unique_ptr<EventFlag, EventFlagDeleter> mEventFlag;
  std::atomic<bool> mStopWriteThread = false;
  sp<Thread> mWriteThread;
};

}  // namespace CPP_VERSION
}  // namespace audio_proxy
