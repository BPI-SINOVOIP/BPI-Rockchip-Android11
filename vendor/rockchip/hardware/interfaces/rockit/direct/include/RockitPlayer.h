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

#ifndef ROCKIT_DIRECT_ROCKITPLAYER_H
#define ROCKIT_DIRECT_ROCKITPLAYER_H

#include <binder/Parcel.h>
#include <media/MediaPlayerInterface.h>
#include "rt_metadata.h"
#include "RTMediaPlayerInterface.h"
#include "RTLibDefine.h"

namespace android {


enum RTInvokeId {
    RT_INVOKE_SET_PLAY_SPEED = 10000,
    RT_INVOKE_GET_PLAY_SPEED,
};

class RockitPlayer : public RefBase {
 public:
    RockitPlayer();
    virtual ~RockitPlayer();

    rt_status createPlayer();
    rt_status destroyPlayer();

    virtual rt_status initCheck();
    virtual rt_status setDataSource(
                void *httpService,
                const char *url,
                void *headers);
    virtual rt_status setDataSource(
                INT32 fd,
                INT64 offset,
                INT64 length);
    virtual rt_status prepare();
    virtual rt_status prepareAsync();
    virtual rt_status start();
    virtual rt_status stop();
    virtual rt_status pause();
    virtual bool      isPlaying();
    virtual rt_status seekTo(INT32 msec, UINT32 mode);
    virtual rt_status getCurrentPosition(int *msec);
    virtual rt_status getDuration(int *msec);
    virtual rt_status reset();
    virtual rt_status setLooping(INT32 loop);
    virtual INT32     playerType();
    virtual rt_status invoke(const Parcel &request, Parcel *reply);
    virtual rt_status setVideoSink(const void *videoSink);
    virtual rt_status setAudioSink(const void *audioSink);
    virtual rt_status setSubteSink(const void *subteSink);
    virtual rt_status setParameter(INT32 key, const Parcel &request);
    virtual rt_status setListener(RTPlayerListener *listener);
    virtual rt_status setPlaybackSettings(const AudioPlaybackRate& rate);

 protected:
    rt_status fillInvokeRequest(const Parcel &parcel, RtMetaData* meta, INT32& event);
    rt_status fillInvokeReply(INT32 event, RtMetaData* meta, Parcel* reply);
    rt_status fillTrackInfoReply(RtMetaData* meta, Parcel* reply);
    void      fillTrackInfor(Parcel *reply, int type, String16& mime, String16& lang);
    INT32     translateMediaType(INT32 sourceType, bool rtType);
    rt_status fillGetSelectedTrackReply(RtMetaData* meta, Parcel* reply);

 private:
    RTMediaPlayerInterface      *mPlayerImpl;

    // rockit player lib impl
    void                        *mPlayerLibFd;
    createRockitPlayerFunc      *mCreatePlayerFunc;
    destroyRockitPlayerFunc     *mDestroyPlayerFunc;

    createRockitMetaDataFunc    *mCreateMetaDataFunc;
    destroyRockitMetaDataFunc   *mDestroyMetaDataFunc;
};

}

#endif  // ROCKIT_DIRECT_ROCKITPLAYER_H
