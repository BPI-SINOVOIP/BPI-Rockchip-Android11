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

#ifndef ROCKIT_DIRECT_AUDIOSINKCALLBACK_H
#define ROCKIT_DIRECT_AUDIOSINKCALLBACK_H

#include "RTAudioSinkInterface.h"
#include "RockitPlayer.h"
#include <media/AudioTrack.h>
#include <media/AudioSystem.h>
#include <media/MediaPlayerInterface.h>
#include <media/AudioTrack.h>
#include <media/AudioResamplerPublic.h>
#include <utils/RefBase.h>


#include <system/audio.h>
#include <system/audio_policy.h>

namespace android {

class RTAudioSinkCallback : public RTAudioSinkInterface {
public:
    RTAudioSinkCallback(const sp<MediaPlayerBase::AudioSink> &audioSink);
    virtual ~RTAudioSinkCallback();
    virtual int32_t open(void *param);
    virtual int32_t start();
    virtual int32_t pause();
    virtual int32_t stop();
    virtual int32_t flush();
    virtual int32_t close();

    virtual int32_t write(const void *buffer, int32_t size, bool block);

    virtual int32_t latency();
    virtual int32_t frameSize();
    virtual int32_t getPlaybackRate(RTAudioPlaybackRate *param);
    virtual int32_t setPlaybackRate(const RTAudioPlaybackRate& param);

    virtual int64_t getPlaybackDurationUs();
    virtual int32_t setAudioChannel(RTAudioChannel mode);

private:
    sp<MediaPlayerBase::AudioSink> mAudioSink;
    RTAudioSinkParam mAudioSinkParam;
    RTAudioChannel mAudioChannelMode;
};
}
#endif  // ROCKIT_DIRECT_AUDIOSINKCALLBACK_H