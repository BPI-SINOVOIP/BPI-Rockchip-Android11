/*
**
** Copyright 2009, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ROCKIT_DIRECT_ROCKITPLAYERMANAGER_H
#define ROCKIT_DIRECT_ROCKITPLAYERMANAGER_H

#include <media/MediaPlayerInterface.h>
#include "RockitPlayer.h"

namespace android {

struct ROCKIT_PLAYER_CTX;
class RockitPlayerManager {
public:
    RockitPlayerManager(android::MediaPlayerInterface *player);
    virtual ~RockitPlayerManager();

    virtual status_t initCheck();

    virtual status_t setUID(uid_t uid);

    virtual status_t setDataSource(
                        const sp<IMediaHTTPService> &httpService,
                        const char *url,
                        const KeyedVector<String8, String8> *headers = NULL) ;
    virtual status_t setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t setDataSource(const sp<IStreamSource> &source);
    virtual status_t setVideoSurfaceTexture(
            const sp<IGraphicBufferProducer> &bufferProducer);
    virtual status_t prepare();
    virtual status_t prepareAsync();
    virtual status_t start();
    virtual status_t stop();
    virtual status_t pause();
    virtual bool     isPlaying();
    virtual status_t seekTo(int msec, MediaPlayerSeekMode mode = MediaPlayerSeekMode::SEEK_PREVIOUS_SYNC);
    virtual status_t getCurrentPosition(int *msec);
    virtual status_t getDuration(int *msec);
    virtual status_t reset();
    virtual status_t setLooping(int loop);
    virtual player_type playerType();
    virtual status_t invoke(const Parcel &request, Parcel *reply);
    virtual void     setAudioSink(const sp<MediaPlayerBase::AudioSink> &audioSink);
    virtual status_t setParameter(int key, const Parcel &request);
    virtual status_t getParameter(int key, Parcel *reply);

    virtual status_t getMetadata(const media::Metadata::Filter& ids, Parcel *records);

    virtual status_t getPlaybackSettings(AudioPlaybackRate* rate);
    virtual status_t setPlaybackSettings(const AudioPlaybackRate& rate);

    virtual status_t dump(int fd, const Vector<String16> &args) const;

    sp<MediaPlayerBase::AudioSink> getAudioSink();

private:
    void initPlayer(android::MediaPlayerInterface* player);
    void deinitPlayer();
    status_t getUriFromFd(int fd, char **uri);

    RockitPlayerManager(const RockitPlayerManager &);
    RockitPlayerManager &operator=(const RockitPlayerManager &);

private:
    struct ROCKIT_PLAYER_CTX *mCtx;
};

}  // namespace android

#endif  // ROCKIT_DIRECT_ROCKITPLAYERMANAGER_H