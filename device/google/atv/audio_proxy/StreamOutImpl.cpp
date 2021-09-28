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

#define LOG_TAG "audio_proxy_client"

#include "StreamOutImpl.h"

#include <system/audio.h>
#include <time.h>
#include <utils/Log.h>

#include <cstring>

#include "AudioProxyStreamOut.h"

using ::android::status_t;

namespace audio_proxy {
namespace CPP_VERSION {
namespace {
// 1GB
constexpr uint32_t kMaxBufferSize = 1 << 30;

void deleteEventFlag(EventFlag* obj) {
  if (!obj) {
    return;
  }

  status_t status = EventFlag::deleteEventFlag(&obj);
  ALOGE_IF(status, "write MQ event flag deletion error: %s", strerror(-status));
}

class WriteThread : public Thread {
 public:
  // WriteThread's lifespan never exceeds StreamOut's lifespan.
  WriteThread(std::atomic<bool>* stop, AudioProxyStreamOut* stream,
              StreamOutImpl::CommandMQ* commandMQ,
              StreamOutImpl::DataMQ* dataMQ, StreamOutImpl::StatusMQ* statusMQ,
              EventFlag* eventFlag);

  ~WriteThread() override;

 private:
  bool threadLoop() override;

  IStreamOut::WriteStatus doGetLatency();
  IStreamOut::WriteStatus doGetPresentationPosition();
  IStreamOut::WriteStatus doWrite();

  std::atomic<bool>* const mStop;
  AudioProxyStreamOut* mStream;
  StreamOutImpl::CommandMQ* const mCommandMQ;
  StreamOutImpl::DataMQ* const mDataMQ;
  StreamOutImpl::StatusMQ* const mStatusMQ;
  EventFlag* const mEventFlag;
  const std::unique_ptr<uint8_t[]> mBuffer;
};

WriteThread::WriteThread(std::atomic<bool>* stop, AudioProxyStreamOut* stream,
                         StreamOutImpl::CommandMQ* commandMQ,
                         StreamOutImpl::DataMQ* dataMQ,
                         StreamOutImpl::StatusMQ* statusMQ,
                         EventFlag* eventFlag)
    : Thread(false /*canCallJava*/),
      mStop(stop),
      mStream(stream),
      mCommandMQ(commandMQ),
      mDataMQ(dataMQ),
      mStatusMQ(statusMQ),
      mEventFlag(eventFlag),
      mBuffer(new uint8_t[mDataMQ->getQuantumCount()]) {}

WriteThread::~WriteThread() = default;

IStreamOut::WriteStatus WriteThread::doWrite() {
  const size_t availToRead = mDataMQ->availableToRead();
  IStreamOut::WriteStatus status;
  status.replyTo = IStreamOut::WriteCommand::WRITE;
  status.retval = Result::OK;
  status.reply.written = 0;
  if (mDataMQ->read(&mBuffer[0], availToRead)) {
    status.reply.written = availToRead;
    ssize_t writeResult = mStream->write(&mBuffer[0], availToRead);
    if (writeResult >= 0) {
      status.reply.written = writeResult;
      ALOGW_IF(writeResult < availToRead,
               "Stream doesn't write all the bytes. Drop the unwritten bytes.");
    } else {
      status.retval = Result::INVALID_STATE;
    }
  }

  return status;
}

IStreamOut::WriteStatus WriteThread::doGetPresentationPosition() {
  IStreamOut::WriteStatus status;
  status.replyTo = IStreamOut::WriteCommand::GET_PRESENTATION_POSITION;
  status.retval = mStream->getPresentationPosition(
      &status.reply.presentationPosition.frames,
      &status.reply.presentationPosition.timeStamp);
  return status;
}

IStreamOut::WriteStatus WriteThread::doGetLatency() {
  IStreamOut::WriteStatus status;
  status.replyTo = IStreamOut::WriteCommand::GET_LATENCY;
  status.retval = Result::OK;
  status.reply.latencyMs = mStream->getLatency();
  return status;
}

bool WriteThread::threadLoop() {
  // This implementation doesn't return control back to the Thread until the
  // parent thread decides to stop, as the Thread uses mutexes, and this can
  // lead to priority inversion.
  while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
    uint32_t efState = 0;
    mEventFlag->wait(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY),
                     &efState);
    if (!(efState & static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY))) {
      continue;  // Nothing to do.
    }

    IStreamOut::WriteCommand replyTo;
    if (!mCommandMQ->read(&replyTo)) {
      continue;  // Nothing to do.
    }

    IStreamOut::WriteStatus status;
    switch (replyTo) {
      case IStreamOut::WriteCommand::WRITE:
        status = doWrite();
        break;
      case IStreamOut::WriteCommand::GET_PRESENTATION_POSITION:
        status = doGetPresentationPosition();
        break;
      case IStreamOut::WriteCommand::GET_LATENCY:
        status = doGetLatency();
        break;
      default:
        ALOGE("Unknown write thread command code %d", replyTo);
        status.retval = Result::NOT_SUPPORTED;
        break;
    }
    if (!mStatusMQ->write(&status)) {
      ALOGE("status message queue write failed");
    }
    mEventFlag->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL));
  }

  return false;
}

}  // namespace

StreamOutImpl::StreamOutImpl(std::unique_ptr<AudioProxyStreamOut> stream)
    : mStream(std::move(stream)), mEventFlag(nullptr, deleteEventFlag) {}

StreamOutImpl::~StreamOutImpl() {
  closeImpl();

  if (mWriteThread) {
    status_t status = mWriteThread->join();
    ALOGE_IF(status, "write thread exit error: %s", strerror(-status));
  }

  mEventFlag.reset();
}

Return<uint64_t> StreamOutImpl::getFrameSize() {
  audio_format_t format = static_cast<audio_format_t>(mStream->getFormat());

  if (!audio_has_proportional_frames(format)) {
    return sizeof(int8_t);
  }

  size_t channel_sample_size = audio_bytes_per_sample(format);
  return audio_channel_count_from_out_mask(
             static_cast<audio_channel_mask_t>(mStream->getChannelMask())) *
         channel_sample_size;
}

Return<uint64_t> StreamOutImpl::getFrameCount() {
  return mStream->getFrameCount();
}

Return<uint64_t> StreamOutImpl::getBufferSize() {
  return mStream->getBufferSize();
}

Return<uint32_t> StreamOutImpl::getSampleRate() {
  return mStream->getSampleRate();
}

Return<void> StreamOutImpl::getSupportedSampleRates(
    AudioFormat format, getSupportedSampleRates_cb _hidl_cb) {
  _hidl_cb(Result::OK, mStream->getSupportedSampleRates(format));
  return Void();
}

Return<void> StreamOutImpl::getSupportedChannelMasks(
    AudioFormat format, getSupportedChannelMasks_cb _hidl_cb) {
  _hidl_cb(Result::OK, mStream->getSupportedChannelMasks(format));
  return Void();
}

Return<Result> StreamOutImpl::setSampleRate(uint32_t sampleRateHz) {
  return mStream->setSampleRate(sampleRateHz);
}

Return<hidl_bitfield<AudioChannelMask>> StreamOutImpl::getChannelMask() {
  return hidl_bitfield<AudioChannelMask>(mStream->getChannelMask());
}

Return<Result> StreamOutImpl::setChannelMask(
    hidl_bitfield<AudioChannelMask> mask) {
  return mStream->setChannelMask(mask);
}

Return<AudioFormat> StreamOutImpl::getFormat() { return mStream->getFormat(); }

Return<void> StreamOutImpl::getSupportedFormats(
    getSupportedFormats_cb _hidl_cb) {
  _hidl_cb(mStream->getSupportedFormats());
  return Void();
}

Return<Result> StreamOutImpl::setFormat(AudioFormat format) {
  return mStream->setFormat(format);
}

Return<void> StreamOutImpl::getAudioProperties(getAudioProperties_cb _hidl_cb) {
  _hidl_cb(mStream->getSampleRate(), mStream->getChannelMask(),
           mStream->getFormat());
  return Void();
}

Return<Result> StreamOutImpl::addEffect(uint64_t effectId) {
  return Result::NOT_SUPPORTED;
}

Return<Result> StreamOutImpl::removeEffect(uint64_t effectId) {
  return Result::NOT_SUPPORTED;
}

Return<Result> StreamOutImpl::standby() { return mStream->standby(); }

Return<void> StreamOutImpl::getDevices(getDevices_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, {});
  return Void();
}

Return<Result> StreamOutImpl::setDevices(
    const hidl_vec<DeviceAddress>& devices) {
  return Result::NOT_SUPPORTED;
}

Return<void> StreamOutImpl::getParameters(
    const hidl_vec<ParameterValue>& context, const hidl_vec<hidl_string>& keys,
    getParameters_cb _hidl_cb) {
  _hidl_cb(Result::OK, mStream->getParameters(context, keys));
  return Void();
}

Return<Result> StreamOutImpl::setParameters(
    const hidl_vec<ParameterValue>& context,
    const hidl_vec<ParameterValue>& parameters) {
  return mStream->setParameters(context, parameters);
}

Return<Result> StreamOutImpl::setHwAvSync(uint32_t hwAvSync) {
  return Result::NOT_SUPPORTED;
}

Return<Result> StreamOutImpl::close() { return closeImpl(); }

Result StreamOutImpl::closeImpl() {
  if (mStopWriteThread.load(
          std::memory_order_relaxed)) {  // only this thread writes
    return Result::INVALID_STATE;
  }
  mStopWriteThread.store(true, std::memory_order_release);
  if (mEventFlag) {
    mEventFlag->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY));
  }
  return Result::OK;
}

Return<uint32_t> StreamOutImpl::getLatency() { return mStream->getLatency(); }

Return<Result> StreamOutImpl::setVolume(float left, float right) {
  return mStream->setVolume(left, right);
}

Return<void> StreamOutImpl::prepareForWriting(uint32_t frameSize,
                                              uint32_t framesCount,
                                              prepareForWriting_cb _hidl_cb) {
  ThreadInfo threadInfo = {0, 0};

  // Wrap the _hidl_cb to return an error
  auto sendError = [&threadInfo, &_hidl_cb](Result result) -> Return<void> {
    _hidl_cb(result, CommandMQ::Descriptor(), DataMQ::Descriptor(),
             StatusMQ::Descriptor(), threadInfo);
    return Void();
  };

  if (mDataMQ) {
    ALOGE("the client attempted to call prepareForWriting twice");
    return sendError(Result::INVALID_STATE);
  }

  if (frameSize == 0 || framesCount == 0) {
    ALOGE("Invalid frameSize (%u) or framesCount (%u)", frameSize, framesCount);
    return sendError(Result::INVALID_ARGUMENTS);
  }

  if (frameSize > kMaxBufferSize / framesCount) {
    ALOGE("Buffer too big: %u*%u bytes > MAX_BUFFER_SIZE (%u)", frameSize,
          framesCount, kMaxBufferSize);
    return sendError(Result::INVALID_ARGUMENTS);
  }

  auto commandMQ = std::make_unique<CommandMQ>(1);
  if (!commandMQ->isValid()) {
    ALOGE("command MQ is invalid");
    return sendError(Result::INVALID_ARGUMENTS);
  }

  auto dataMQ =
      std::make_unique<DataMQ>(frameSize * framesCount, true /* EventFlag */);
  if (!dataMQ->isValid()) {
    ALOGE("data MQ is invalid");
    return sendError(Result::INVALID_ARGUMENTS);
  }

  auto statusMQ = std::make_unique<StatusMQ>(1);
  if (!statusMQ->isValid()) {
    ALOGE("status MQ is invalid");
    return sendError(Result::INVALID_ARGUMENTS);
  }

  EventFlag* rawEventFlag = nullptr;
  status_t status =
      EventFlag::createEventFlag(dataMQ->getEventFlagWord(), &rawEventFlag);
  std::unique_ptr<EventFlag, EventFlagDeleter> eventFlag(rawEventFlag,
                                                         deleteEventFlag);
  if (status != ::android::OK || !eventFlag) {
    ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
    return sendError(Result::INVALID_ARGUMENTS);
  }

  sp<WriteThread> writeThread =
      new WriteThread(&mStopWriteThread, mStream.get(), commandMQ.get(),
                      dataMQ.get(), statusMQ.get(), eventFlag.get());
  status = writeThread->run("writer", ::android::PRIORITY_URGENT_AUDIO);
  if (status != ::android::OK) {
    ALOGW("failed to start writer thread: %s", strerror(-status));
    return sendError(Result::INVALID_ARGUMENTS);
  }

  mCommandMQ = std::move(commandMQ);
  mDataMQ = std::move(dataMQ);
  mStatusMQ = std::move(statusMQ);
  mEventFlag = std::move(eventFlag);
  mWriteThread = std::move(writeThread);
  threadInfo.pid = getpid();
  threadInfo.tid = mWriteThread->getTid();
  _hidl_cb(Result::OK, *mCommandMQ->getDesc(), *mDataMQ->getDesc(),
           *mStatusMQ->getDesc(), threadInfo);
  return Void();
}

Return<void> StreamOutImpl::getRenderPosition(getRenderPosition_cb _hidl_cb) {
  uint32_t dspFrames = 0;
  Result res = mStream->getRenderPosition(&dspFrames);
  _hidl_cb(res, dspFrames);
  return Void();
}

Return<void> StreamOutImpl::getNextWriteTimestamp(
    getNextWriteTimestamp_cb _hidl_cb) {
  int64_t timestamp = 0;
  Result res = mStream->getNextWriteTimestamp(&timestamp);
  _hidl_cb(res, timestamp);
  return Void();
}

Return<Result> StreamOutImpl::setCallback(
    const sp<IStreamOutCallback>& callback) {
  return Result::NOT_SUPPORTED;
}

Return<Result> StreamOutImpl::clearCallback() { return Result::NOT_SUPPORTED; }

Return<void> StreamOutImpl::supportsPauseAndResume(
    supportsPauseAndResume_cb _hidl_cb) {
  _hidl_cb(true, true);
  return Void();
}

Return<Result> StreamOutImpl::pause() { return mStream->pause(); }

Return<Result> StreamOutImpl::resume() { return mStream->resume(); }

Return<bool> StreamOutImpl::supportsDrain() { return mStream->supportsDrain(); }

Return<Result> StreamOutImpl::drain(AudioDrain type) {
  return mStream->drain(type);
}

Return<Result> StreamOutImpl::flush() { return mStream->flush(); }

Return<void> StreamOutImpl::getPresentationPosition(
    getPresentationPosition_cb _hidl_cb) {
  uint64_t frames = 0;
  TimeSpec ts = {0, 0};
  Result result = mStream->getPresentationPosition(&frames, &ts);
  _hidl_cb(result, frames, ts);
  return Void();
}

Return<Result> StreamOutImpl::start() { return Result::NOT_SUPPORTED; }

Return<Result> StreamOutImpl::stop() { return Result::NOT_SUPPORTED; }

Return<void> StreamOutImpl::createMmapBuffer(int32_t minSizeFrames,
                                             createMmapBuffer_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, MmapBufferInfo());
  return Void();
}

Return<void> StreamOutImpl::getMmapPosition(getMmapPosition_cb _hidl_cb) {
  _hidl_cb(Result::NOT_SUPPORTED, MmapPosition());
  return Void();
}

Return<void> StreamOutImpl::updateSourceMetadata(
    const SourceMetadata& sourceMetadata) {
  return Void();
}

Return<Result> StreamOutImpl::selectPresentation(int32_t presentationId,
                                                 int32_t programId) {
  return Result::NOT_SUPPORTED;
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy
