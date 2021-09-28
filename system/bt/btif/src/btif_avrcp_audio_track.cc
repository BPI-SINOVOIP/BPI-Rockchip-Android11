/*
 * Copyright 2015 The Android Open Source Project
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
#define LOG_TAG "bt_btif_avrcp_audio_track"

#include "btif_avrcp_audio_track.h"

#include <aaudio/AAudio.h>
#include <base/logging.h>
#include <utils/StrongPointer.h>

#include "bt_target.h"
#include "osi/include/log.h"

using namespace android;

typedef struct {
  AAudioStream* stream;
  int bitsPerSample;
  int channelCount;
  float* buffer;
  size_t bufferLength;
} BtifAvrcpAudioTrack;

#if (DUMP_PCM_DATA == TRUE)
FILE* outputPcmSampleFile;
char outputFilename[50] = "/data/misc/bluedroid/output_sample.pcm";
#endif

void* BtifAvrcpAudioTrackCreate(int trackFreq, int bitsPerSample,
                                int channelCount) {
  LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btCreateTrack freq %d bps %d channel %d ",
              __func__, trackFreq, bitsPerSample, channelCount);

  AAudioStreamBuilder* builder;
  AAudioStream* stream;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  AAudioStreamBuilder_setSampleRate(builder, trackFreq);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setChannelCount(builder, channelCount);
  AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  result = AAudioStreamBuilder_openStream(builder, &stream);
  CHECK(result == AAUDIO_OK);
  AAudioStreamBuilder_delete(builder);

  BtifAvrcpAudioTrack* trackHolder = new BtifAvrcpAudioTrack;
  CHECK(trackHolder != NULL);
  trackHolder->stream = stream;
  trackHolder->bitsPerSample = bitsPerSample;
  trackHolder->channelCount = channelCount;
  trackHolder->bufferLength =
      trackHolder->channelCount * AAudioStream_getBufferSizeInFrames(stream);
  trackHolder->buffer = new float[trackHolder->bufferLength]();

#if (DUMP_PCM_DATA == TRUE)
  outputPcmSampleFile = fopen(outputFilename, "ab");
#endif
  return (void*)trackHolder;
}

void BtifAvrcpAudioTrackStart(void* handle) {
  if (handle == NULL) {
    LOG_ERROR(LOG_TAG, "%s: handle is null!", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  CHECK(trackHolder != NULL);
  CHECK(trackHolder->stream != NULL);
  LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btStartTrack", __func__);
  AAudioStream_requestStart(trackHolder->stream);
}

void BtifAvrcpAudioTrackStop(void* handle) {
  if (handle == NULL) {
    LOG_DEBUG(LOG_TAG, "%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btStartTrack", __func__);
    AAudioStream_requestStop(trackHolder->stream);
  }
}

void BtifAvrcpAudioTrackDelete(void* handle) {
  if (handle == NULL) {
    LOG_DEBUG(LOG_TAG, "%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btStartTrack", __func__);
    AAudioStream_close(trackHolder->stream);
    delete trackHolder->buffer;
    delete trackHolder;
  }

#if (DUMP_PCM_DATA == TRUE)
  if (outputPcmSampleFile) {
    fclose(outputPcmSampleFile);
  }
  outputPcmSampleFile = NULL;
#endif
}

void BtifAvrcpAudioTrackPause(void* handle) {
  if (handle == NULL) {
    LOG_DEBUG(LOG_TAG, "%s handle is null.", __func__);
    return;
  }
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  if (trackHolder != NULL && trackHolder->stream != NULL) {
    LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btPauseTrack", __func__);
    AAudioStream_requestPause(trackHolder->stream);
    AAudioStream_requestFlush(trackHolder->stream);
  }
}

void BtifAvrcpSetAudioTrackGain(void* handle, float gain) {
  if (handle == NULL) {
    LOG_DEBUG(LOG_TAG, "%s handle is null.", __func__);
    return;
  }
  // Does nothing right now
}

constexpr float kScaleQ15ToFloat = 1.0f / 32768.0f;
constexpr float kScaleQ23ToFloat = 1.0f / 8388608.0f;
constexpr float kScaleQ31ToFloat = 1.0f / 2147483648.0f;

static size_t sampleSizeFor(BtifAvrcpAudioTrack* trackHolder) {
  return trackHolder->bitsPerSample / 8;
}

static size_t transcodeQ15ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  for (; i <= length / sampleSize; i++) {
    trackHolder->buffer[i] = ((int16_t*)buffer)[i] * kScaleQ15ToFloat;
  }
  return i * sampleSize;
}

static size_t transcodeQ23ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  for (; i <= length / sampleSize; i++) {
    size_t offset = i * sampleSize;
    int32_t sample = *((int32_t*)(buffer + offset - 1)) & 0x00FFFFFF;
    trackHolder->buffer[i] = sample * kScaleQ23ToFloat;
  }
  return i * sampleSize;
}

static size_t transcodeQ31ToFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  size_t sampleSize = sampleSizeFor(trackHolder);
  size_t i = 0;
  for (; i <= length / sampleSize; i++) {
    trackHolder->buffer[i] = ((int32_t*)buffer)[i] * kScaleQ31ToFloat;
  }
  return i * sampleSize;
}

static size_t transcodeToPcmFloat(uint8_t* buffer, size_t length,
                                  BtifAvrcpAudioTrack* trackHolder) {
  switch (trackHolder->bitsPerSample) {
    case 16:
      return transcodeQ15ToFloat(buffer, length, trackHolder);
    case 24:
      return transcodeQ23ToFloat(buffer, length, trackHolder);
    case 32:
      return transcodeQ31ToFloat(buffer, length, trackHolder);
  }
  return -1;
}

constexpr int64_t kTimeoutNanos = 100 * 1000 * 1000;  // 100 ms

int BtifAvrcpAudioTrackWriteData(void* handle, void* audioBuffer,
                                 int bufferLength) {
  BtifAvrcpAudioTrack* trackHolder = static_cast<BtifAvrcpAudioTrack*>(handle);
  CHECK(trackHolder != NULL);
  CHECK(trackHolder->stream != NULL);
  aaudio_result_t retval = -1;
#if (DUMP_PCM_DATA == TRUE)
  if (outputPcmSampleFile) {
    fwrite((audioBuffer), 1, (size_t)bufferLength, outputPcmSampleFile);
  }
#endif

  size_t sampleSize = sampleSizeFor(trackHolder);
  int transcodedCount = 0;
  do {
    transcodedCount +=
        transcodeToPcmFloat(((uint8_t*)audioBuffer) + transcodedCount,
                            bufferLength - transcodedCount, trackHolder);

    retval = AAudioStream_write(
        trackHolder->stream, trackHolder->buffer,
        transcodedCount / (sampleSize * trackHolder->channelCount),
        kTimeoutNanos);
    LOG_VERBOSE(LOG_TAG, "%s Track.cpp: btWriteData len = %d ret = %d",
                __func__, bufferLength, retval);
  } while (transcodedCount < bufferLength);

  return transcodedCount;
}
