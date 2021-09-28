/*
 * Copyright (C) 2009 The Android Open Source Project
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
#define LOG_TAG "RockitPlayerClient"
#include "RockitPlayerInterface.h"
#include "RockitPlayerManager.h"

#include <utils/Log.h>
#include <dlfcn.h>
#include <fcntl.h>

#include <media/Metadata.h>
#include <media/MediaHTTPService.h>
#include <media/mediaplayer.h>
#include <gui/Surface.h>
#include <system/window.h>
#include <string.h>


namespace android {


RockitPlayerClient::RockitPlayerClient() {
    ALOGD("RockitPlayerClient(%p) construct", this);
    mPlayer = (void*)new RockitPlayerManager(this);
}

RockitPlayerClient::~RockitPlayerClient() {
    ALOGD("~RockitPlayerClient(%p) destruct", this);
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    if (player != NULL) {
        delete player;
    }
    mPlayer = NULL;
}

status_t RockitPlayerClient::initCheck() {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->initCheck();
    }

    return status;
}

status_t RockitPlayerClient::setUID(uid_t uid) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setUID(uid);
    }

    return status;
}

status_t RockitPlayerClient::setDataSource(
        const sp<IMediaHTTPService> &httpService,
        const char *url,
        const KeyedVector<String8, String8> *headers) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setDataSource(httpService, url, headers);
    }

    return status;
}

// Warning: The filedescriptor passed into this method will only be valid until
// the method returns, if you want to keep it, dup it!
status_t RockitPlayerClient::setDataSource(int fd, int64_t offset, int64_t length) {
    ALOGV("setDataSource(%d, %lld, %lld)", fd, (long long)offset, (long long)length);
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer; 
    status_t status = OK;
    if (player != NULL) {
        status = player->setDataSource(fd, offset, length);
    }

    return status;
}

status_t RockitPlayerClient::setDataSource(const sp<IStreamSource> &source) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        player->setDataSource(source);
    }

    return status;
}

status_t RockitPlayerClient::setVideoSurfaceTexture(
        const sp<IGraphicBufferProducer> &bufferProducer) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setVideoSurfaceTexture(bufferProducer);
    }

    return status;
}

status_t RockitPlayerClient::prepare() {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->prepare();
    }

    return status;
}

status_t RockitPlayerClient::prepareAsync() {
    ALOGV("prepareAsync");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->prepareAsync();
    }

    return status;
}

status_t RockitPlayerClient::start() {
    ALOGV("start");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->start();
    }

    return status;
}

status_t RockitPlayerClient::stop() {
    ALOGV("stop");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->stop();
    }

    return status;
}

status_t RockitPlayerClient::pause() {
    ALOGV("pause");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->pause();
    }

    return status;
}

bool RockitPlayerClient::isPlaying() {
    ALOGV("isPlaying");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    bool playing = false;
    if (player != NULL) {
        playing = player->isPlaying();
    }

    return playing;
}

status_t RockitPlayerClient::seekTo(int msec, MediaPlayerSeekMode mode) {
    ALOGV("seekTo %.2f secs", msec / 1E3);
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->seekTo(msec, mode);
    }

    return status;
}

status_t RockitPlayerClient::getCurrentPosition(int *msec) {
    ALOGV("getCurrentPosition");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->getCurrentPosition(msec);
    }

    return status;
}

status_t RockitPlayerClient::getDuration(int *msec) {
    ALOGV("getDuration");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->getDuration(msec);
    }

    return status;
}

status_t RockitPlayerClient::reset() {
    ALOGV("reset");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->reset();
    }

    return status;
}

status_t RockitPlayerClient::setLooping(int loop) {
    ALOGV("setLooping");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setLooping(loop);
    }

    return status;
}

player_type RockitPlayerClient::playerType() {
    ALOGV("playerType");
    return ROCKIT_PLAYER;
}

status_t RockitPlayerClient::invoke(const Parcel &request, Parcel *reply) {
    ALOGV("RockitPlayerClient::invoke");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->invoke(request, reply);
    }

    return status;
}

void RockitPlayerClient::setAudioSink(const sp<AudioSink> &audioSink) {
    ALOGV("setAudioSink audiosink: %p", audioSink.get());
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    if (player != NULL) {
        player->setAudioSink(audioSink);
    }
}

status_t RockitPlayerClient::setParameter(int key, const Parcel &request) {
    ALOGV("setParameter(key=%d)", key);
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setParameter(key, request);
    }

    return status;
}

status_t RockitPlayerClient::getParameter(int key, Parcel *reply) {
    ALOGV("getParameter");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->getParameter(key, reply);
    }

    return status;
}

status_t RockitPlayerClient::getMetadata(
        const media::Metadata::Filter& ids, Parcel *records) {
    ALOGV("getMetadata");
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->getMetadata(ids, records);
    }

    return status;
}

status_t RockitPlayerClient::getPlaybackSettings(AudioPlaybackRate* rate) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->getPlaybackSettings(rate);
    }

    return status;
}

status_t RockitPlayerClient::setPlaybackSettings(const AudioPlaybackRate& rate) {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->setPlaybackSettings(rate);
    }

    return status;
}

status_t RockitPlayerClient::dump(int fd, const Vector<String16> &args) const {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    status_t status = OK;
    if (player != NULL) {
        status = player->dump(fd, args);
    }

    return status;
}

sp<MediaPlayerBase::AudioSink> RockitPlayerClient::getAudioSink() {
    RockitPlayerManager* player = (RockitPlayerManager*)mPlayer;
    if (player != NULL) {
        return player->getAudioSink();
    } else {
        return NULL;
    }
}

}  // namespace android


