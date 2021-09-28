/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "RTAudioSinkCallback"

#include <dlfcn.h>
#include "RockitPlayer.h"
#include "RTAudioSinkCallback.h"
#include "cutils/properties.h"

using namespace ::android;

static inline int32_t getAudioSinkPcmMsSetting() {
    return property_get_int32(
            "media.rockit.audio.sink", 500 /* default_value */);
}

static inline int32_t getEnableAudioSetting(){
    return property_get_int32(
            "media.rockit.audio.setting", 1 /* default_value */);
}

RTAudioSinkCallback::RTAudioSinkCallback(const sp<MediaPlayerBase::AudioSink> &audioSink) {
    mAudioSink = audioSink;
    memset(&mAudioSinkParam, 0, sizeof(RTAudioSinkParam));
    mAudioChannelMode = RT_AUDIO_CHANNEL_STEREO;
    ALOGD("RTAudioSinkCallback(%p) construct", this);
}

RTAudioSinkCallback::~RTAudioSinkCallback() {
    ALOGD("~RTAudioSinkCallback(%p) destruct", this);
}

int32_t RTAudioSinkCallback::open(void *param) {
    ALOGV("RTAudioSinkCallback open in");
    memcpy(&mAudioSinkParam, param, sizeof(RTAudioSinkParam));

    uint32_t frameCount = 0;
    int bufferCount = DEFAULT_AUDIOSINK_BUFFERCOUNT;
    int32_t enable = getEnableAudioSetting();

    /*  if current audio stream is mixer output, we set more buffer for audioTrack */
    audio_output_flags_t flags = (audio_output_flags_t)mAudioSinkParam.flags;
    if((flags == AUDIO_OUTPUT_FLAG_NONE) && enable) {
       frameCount = ((unsigned long long)mAudioSinkParam.sampleRate * getAudioSinkPcmMsSetting()) / 1000;
       bufferCount = 0;
    }

    return (int32_t)mAudioSink->open(mAudioSinkParam.sampleRate,
                                     mAudioSinkParam.channels,
                                     mAudioSinkParam.channelMask,
                                     (audio_format_t)mAudioSinkParam.format,
                                     bufferCount, /* buffer count */
                                     NULL, /* callback */
                                     NULL, /* cookie */
                                     (audio_output_flags_t)mAudioSinkParam.flags,
                                     NULL, /* offload info */
                                     false, /* do not reconnect */
                                     frameCount);
}

int32_t RTAudioSinkCallback::start() {
    ALOGV("RTAudioSinkCallback start in");
    return (int32_t)mAudioSink->start();
}

int32_t RTAudioSinkCallback::pause() {
    ALOGV("RTAudioSinkCallback pause in");
    mAudioSink->pause();
    return 0;
}

int32_t RTAudioSinkCallback::stop() {
    ALOGV("RTAudioSinkCallback stop in");
    mAudioSink->stop();
    return 0;
}

int32_t RTAudioSinkCallback::flush() {
    ALOGV("RTAudioSinkCallback flush in");
    mAudioSink->flush();
    return 0;
}

int32_t RTAudioSinkCallback::close() {
    ALOGV("RTAudioSinkCallback close in");
    mAudioSink->close();
    return 0;
}

int32_t RTAudioSinkCallback::latency() {
    ALOGV("RTAudioSinkCallback latency in");
    return mAudioSink->latency();
}

int32_t RTAudioSinkCallback::write(const void *buffer, int32_t size, bool block) {
    ALOGV("RTAudioSinkCallback write audio(data=%p, size=%d)", buffer, size);
    int32_t consumedLen = 0;
    short *pcmData = (short *)buffer;
    short *pcmDataEnd = (short *)buffer + (size/2);

    if ((mAudioSinkParam.flags == AAUDIO_OUTPUT_FLAG_NONE)
        && mAudioSinkParam.channels == 2) {
        if (mAudioChannelMode == RT_AUDIO_CHANNEL_LEFT) {
            while(pcmData < pcmDataEnd) {
                *(pcmData + 1) =  *pcmData;
                pcmData = pcmData + 2;
            }
        } else if (mAudioChannelMode == RT_AUDIO_CHANNEL_RIGHT) {
            while(pcmData < pcmDataEnd) {
                *pcmData =  *(pcmData + 1);
                pcmData = pcmData + 2;
            }
        }
    }
    consumedLen = mAudioSink->write(buffer, size, block);
    return consumedLen;
}

int32_t RTAudioSinkCallback::frameSize() {
    ALOGV("RTAudioSinkCallback frameSize in");
    return mAudioSink->frameSize();
}

int32_t RTAudioSinkCallback::getPlaybackRate(RTAudioPlaybackRate *param) {
    ALOGV("RTAudioSinkCallback start in");
    android::AudioPlaybackRate rate;
    int32_t status = mAudioSink->getPlaybackRate(&rate);
    if (status != NO_ERROR) {
        ALOGW("AudioSink not prepared yet, set deault rate value.");
        param->mSpeed = 1.0f;
        param->mPitch = 1.0f;
        param->mStretchMode = AAUDIO_TIMESTRETCH_STRETCH_DEFAULT;
        param->mFallbackMode = AAUDIO_TIMESTRETCH_FALLBACK_DEFAULT;
    } else {
        param->mSpeed = rate.mSpeed;
        param->mPitch = rate.mPitch;
        param->mStretchMode = (AAudioTimestretchStretchMode)rate.mStretchMode;
        param->mFallbackMode = (AAudioTimestretchFallbackMode)rate.mFallbackMode;
    }
    return NO_ERROR;
}


int32_t RTAudioSinkCallback::setPlaybackRate(const RTAudioPlaybackRate& param) {
    ALOGV("RTAudioSinkCallback start in");
    AudioPlaybackRate rate;
    rate.mSpeed = param.mSpeed;
    rate.mPitch = param.mPitch;
    rate.mStretchMode = (AudioTimestretchStretchMode)param.mStretchMode;
    rate.mFallbackMode = (AudioTimestretchFallbackMode)param.mFallbackMode;
    return mAudioSink->setPlaybackRate(rate);
}

int64_t RTAudioSinkCallback::getPlaybackDurationUs() {
    ALOGV("getPlaybackDurationUs in");
    return mAudioSink->getPlayedOutDurationUs(systemTime(SYSTEM_TIME_MONOTONIC) / 1000ll);
}

int32_t RTAudioSinkCallback::setAudioChannel(RTAudioChannel mode) {
    mAudioChannelMode = mode;
    return 0;
}

#if 0
void RTAudioSinkCallback::AudioCallback(int event, void *user, void *info)
{
   ((RTAudioSinkCallback *)user)->AudioCallback(event, info);
}
void RTAudioSinkCallback::AudioCallback(int event, void *info)
{

    if (event != AudioTrack::EVENT_MORE_DATA) {
        if (event == AudioTrack::EVENT_UNDERRUN)
            ALOGE("Audio Track underrun event, no more enough data!");
        return;
    }

    AudioTrack::Buffer *buffer = (AudioTrack::Buffer *)info;
    size_t numBytesWritten = fillBuffer(buffer->raw, buffer->size);
    buffer->size = numBytesWritten;

}
#endif

