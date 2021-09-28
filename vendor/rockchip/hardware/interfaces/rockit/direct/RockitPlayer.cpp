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
#define LOG_TAG "RockitPlayer"

#include <stdint.h>
#include <dlfcn.h>

#include "RockitPlayer.h"
#include "RTMediaMetaKeys.h"
#include "media/mediaplayer.h"

namespace android {

/* must keep sync with rockit */
typedef enum  _RTInvokeIds {
    RT_INVOKE_ID_GET_TRACK_INFO = 1,
    RT_INVOKE_ID_ADD_EXTERNAL_SOURCE = 2,
    RT_INVOKE_ID_ADD_EXTERNAL_SOURCE_FD = 3,
    RT_INVOKE_ID_SELECT_TRACK = 4,
    RT_INVOKE_ID_UNSELECT_TRACK = 5,
    RT_INVOKE_ID_SET_VIDEO_SCALING_MODE = 6,
    RT_INVOKE_ID_GET_SELECTED_TRACK = 7
} RTInvokeIds;

RockitPlayer::RockitPlayer()
              : mPlayerImpl(NULL),
                mPlayerLibFd(NULL),
                mCreatePlayerFunc(NULL),
                mDestroyPlayerFunc(NULL),
                mCreateMetaDataFunc(NULL),
                mDestroyMetaDataFunc(NULL) {
    ALOGD("RockitPlayer(%p) construct", this);
}

RockitPlayer::~RockitPlayer() {
    ALOGD("~RockitPlayer(%p) destruct", this);
}

status_t RockitPlayer::createPlayer() {
    ALOGV("createPlayer");

    mPlayerLibFd = dlopen(ROCKIT_PLAYER_LIB_NAME, RTLD_LAZY);
    if (mPlayerLibFd == NULL) {
        ALOGE("Cannot load library %s dlerror: %s", ROCKIT_PLAYER_LIB_NAME, dlerror());
    }

    mCreatePlayerFunc = (createRockitPlayerFunc *)dlsym(mPlayerLibFd,
                                                        CREATE_PLAYER_FUNC_NAME);
    if(mCreatePlayerFunc == NULL) {
        ALOGE("dlsym for create player failed, dlerror: %s", dlerror());
    }

    mDestroyPlayerFunc = (destroyRockitPlayerFunc *)dlsym(mPlayerLibFd,
                                                        DESTROY_PLAYER_FUNC_NAME);
    if(mDestroyPlayerFunc == NULL) {
        ALOGE("dlsym for destroy player failed, dlerror: %s", dlerror());
    }

    mCreateMetaDataFunc = (createRockitPlayerFunc *)dlsym(mPlayerLibFd,
                                                            CREATE_METADATA_FUNC_NAME);
    if(mCreateMetaDataFunc == NULL) {
        ALOGE("dlsym for create meta data failed, dlerror: %s", dlerror());
    }

    mDestroyMetaDataFunc = (destroyRockitPlayerFunc *)dlsym(mPlayerLibFd,
                                                        DESTROY_METADATA_FUNC_NAME);
    if(mDestroyMetaDataFunc == NULL) {
        ALOGE("dlsym for destroy meta data failed, dlerror: %s", dlerror());
    }

    mPlayerImpl = (RTMediaPlayerInterface *)mCreatePlayerFunc();
    if (mPlayerImpl == NULL) {
        ALOGE("create player failed, player is null");
    }
    ALOGV("player : %p", mPlayerImpl);
    return OK;
}

status_t RockitPlayer::destroyPlayer() {
    ALOGV("destroyPlayer");
    mDestroyPlayerFunc((void **)&mPlayerImpl);
    mPlayerImpl = NULL;
    dlclose(mPlayerLibFd);
    return OK;
}

status_t RockitPlayer::initCheck() {
    ALOGV("initCheck in");
    return OK;
}

status_t RockitPlayer::setDataSource(
            void *httpService,
            const char *url,
            void *headers) {
    (void)headers;
    (void)httpService;

    ALOGV("setDataSource url: %s", url);
    mPlayerImpl->setDataSource(url, NULL);
    return OK;
}

rt_status RockitPlayer::setDataSource(
            int fd,
            INT64 offset,
            INT64 length) {
    ALOGV("setDataSource url: fd = %d", fd);
    return mPlayerImpl->setDataSource(fd, offset, length);
}

rt_status RockitPlayer::start() {
    ALOGD("%s %d in", __FUNCTION__, __LINE__);
    mPlayerImpl->start();
    return OK;
}

rt_status RockitPlayer::prepare() {
    ALOGV("prepare in");
    mPlayerImpl->prepare();
    return OK;
}

rt_status RockitPlayer::prepareAsync() {
    ALOGV("prepareAsync in");
    mPlayerImpl->prepareAsync();
    return OK;
}

rt_status RockitPlayer::stop() {
    ALOGV("stop in");
    mPlayerImpl->stop();
    return OK;
}

rt_status RockitPlayer::pause() {
    ALOGV("pause in");
    mPlayerImpl->pause();
    return OK;
}

bool RockitPlayer::isPlaying() {
    ALOGV("isPlaying in state: %d", mPlayerImpl->getState());
    return (mPlayerImpl->getState() == 1 << 4/*RT_STATE_STARTED*/) ? true : false;
}

rt_status RockitPlayer::seekTo(INT32 msec, UINT32 mode) {
    ALOGD("seekTo time: %d, mode: %d", msec, mode);
    mPlayerImpl->seekTo(((INT64)msec) * 1000);
    return OK;
}

rt_status RockitPlayer::getCurrentPosition(int *msec) {
    INT64 usec = 0;
    mPlayerImpl->getCurrentPosition(&usec);
    ALOGV("getCurrentPosition usec: %lld in", (long long)usec);
    *msec = (INT32)(usec / 1000);
    return OK;
}

rt_status RockitPlayer::getDuration(int *msec) {
    INT64 usec = 0;
    mPlayerImpl->getDuration(&usec);
    ALOGV("getDuration usec: %lld in", (long long)usec);
    *msec = (INT32)(usec / 1000);
    return OK;
}

rt_status RockitPlayer::reset() {
    ALOGV("reset in");
    mPlayerImpl->reset();
    return OK;
}

rt_status RockitPlayer::setLooping(INT32 loop) {
    ALOGV("setLooping loop: %d", loop);
    mPlayerImpl->setLooping(loop);
    return OK;
}

INT32 RockitPlayer::playerType() {
    ALOGV("playerType in");
    return 6;
}

INT32 RockitPlayer::fillInvokeRequest(const Parcel &request, RtMetaData* meta, INT32& event) {
    INT32 methodId;
    status_t ret = request.readInt32(&methodId);
    if (ret != OK) {
        return ret;
    }

    event = methodId;
    switch (methodId) {
        case INVOKE_ID_GET_TRACK_INFO: {
            meta->setInt32(kUserInvokeCmd, RT_INVOKE_ID_GET_TRACK_INFO);
        } break;

        case INVOKE_ID_SELECT_TRACK:
        case INVOKE_ID_UNSELECT_TRACK:    {
            int index = request.readInt32();
            int cmd = (methodId == INVOKE_ID_SELECT_TRACK)?
                    RT_INVOKE_ID_SELECT_TRACK:RT_INVOKE_ID_UNSELECT_TRACK;
            meta->setInt32(kUserInvokeCmd, cmd);
            meta->setInt32(kUserInvokeTracksIdx, index);
        } break;

        case INVOKE_ID_SET_VIDEO_SCALING_MODE: {
            int mode = request.readInt32();
            meta->setInt32(kUserInvokeCmd, RT_INVOKE_ID_SET_VIDEO_SCALING_MODE);
            meta->setInt32(kUserInvokeVideoScallingMode, mode);
        } break;

        case INVOKE_ID_GET_SELECTED_TRACK: {
            INT32 andrType = request.readInt32();
            INT32 rtType = translateMediaType(andrType, false);
            meta->setInt32(kUserInvokeCmd, RT_INVOKE_ID_GET_SELECTED_TRACK);
            meta->setInt32(kUserInvokeGetSelectTrack, rtType);
        } break;

        default:
            ALOGD("RockitPlayer::fillInvokeRequest: methodid = %d not supprot, add codes here", methodId);
            ret = BAD_VALUE;
            break;
    }

    return ret;
}

INT32 RockitPlayer::translateMediaType(INT32 sourceType, bool rtType) {
    struct TrackTypeMap {
        INT32 AndrType;
        INT32 RTType;
    };

    static const TrackTypeMap kTrackTypeMap[] = {
        { MEDIA_TRACK_TYPE_UNKNOWN,      RTTRACK_TYPE_UNKNOWN  },
        { MEDIA_TRACK_TYPE_VIDEO,        RTTRACK_TYPE_VIDEO  },
        { MEDIA_TRACK_TYPE_AUDIO,        RTTRACK_TYPE_AUDIO  },
        { MEDIA_TRACK_TYPE_TIMEDTEXT,    RTTRACK_TYPE_SUBTITLE  },
        { MEDIA_TRACK_TYPE_SUBTITLE,     RTTRACK_TYPE_SUBTITLE  },
        { MEDIA_TRACK_TYPE_METADATA,     RTTRACK_TYPE_ATTACHMENT  },
    };

    static const size_t kNumTrackTypeMap =
        sizeof(kTrackTypeMap) / sizeof(kTrackTypeMap[0]);

    INT32 i;
    for (i = 0; i < kNumTrackTypeMap; ++i) {
        INT32 trackType =
            rtType ? kTrackTypeMap[i].RTType : kTrackTypeMap[i].AndrType;
        if (sourceType == trackType)
            break;
    }

    if (i == kNumTrackTypeMap)
        return rtType ? MEDIA_TRACK_TYPE_UNKNOWN : RTTRACK_TYPE_UNKNOWN;

    return rtType ? kTrackTypeMap[i].AndrType : kTrackTypeMap[i].RTType;
}

void RockitPlayer::fillTrackInfor(Parcel *reply, int type,String16& mime, String16& lang) {
    if(reply == NULL){
        return;
    }
  //  ALOGV("type = %d, mine = %s, lang = %s", type, mime.string(), lang.string());
    reply->writeInt32(3);
    /* track type */
    reply->writeInt32(type);
    /* mine */
    reply->writeString16(mime);
    /* language */
    reply->writeString16(lang);

    if(type == (int)MEDIA_TRACK_TYPE_SUBTITLE){
        reply->writeInt32(0); // KEY_IS_AUTOSELECT
        reply->writeInt32(0); // KEY_IS_DEFAULT
        reply->writeInt32(0); // KEY_IS_FORCED_SUBTITLE
    }
}

rt_status RockitPlayer::fillTrackInfoReply(RtMetaData* meta, Parcel* reply) {
    int counter = 0;
    void* tracks = NULL;

    RT_BOOL status = meta->findInt32(kUserInvokeTracksCount, &counter);
    if(status == RT_FALSE) {
        ALOGE("fillTrackInfoReply : not find track in meta,counter = %d", counter);
        return -1;
    }
    status = meta->findPointer(kUserInvokeTracksInfor, &tracks);
    if(status == RT_FALSE) {
        ALOGE("fillTrackInfoReply : not find trackInfor in meta");
        return -1;
    }

    reply->writeInt32(counter);

    char desc[100];
    String16 mine, lang;
    RockitTrackInfor* trackInfor = (RockitTrackInfor*)tracks;
    for (INT32 i = 0; i < counter; ++i) {
        int codecType = translateMediaType(trackInfor[i].mCodecType, true);
        switch (codecType) {
            case MEDIA_TRACK_TYPE_VIDEO: {
                    snprintf(desc, 100, ",%dx%d,%f", trackInfor[i].mWidth,
                             trackInfor[i].mHeight, trackInfor[i].mFrameRate);
                    String16 mine = String16(trackInfor[i].mine);
                    String16 lang = String16(desc) + mine;
                    fillTrackInfor(reply, MEDIA_TRACK_TYPE_VIDEO, mine, lang);
                } break;

            case MEDIA_TRACK_TYPE_AUDIO: {
                snprintf(desc, 100, ",%d,%d,", trackInfor[i].mSampleRate,
                          trackInfor[i].mChannels);
                String16 mine = String16(trackInfor[i].mine);
                String16 lang = mine + String16(desc);
                fillTrackInfor(reply, (int)MEDIA_TRACK_TYPE_AUDIO, mine, lang);
            } break;

            case MEDIA_TRACK_TYPE_SUBTITLE: {
                mine = String16(trackInfor[i].mine);
                lang = String16(trackInfor[i].lang);
                fillTrackInfor(reply, (int)MEDIA_TRACK_TYPE_SUBTITLE, mine, lang);
            } break;

            case MEDIA_TRACK_TYPE_TIMEDTEXT: {
                mine = String16(trackInfor[i].mine);
                lang = String16(trackInfor[i].lang);
                fillTrackInfor(reply, (int)MEDIA_TRACK_TYPE_TIMEDTEXT, mine, lang);
            } break;
        }
    }

    return 0;
}

rt_status RockitPlayer::fillGetSelectedTrackReply(RtMetaData* meta, Parcel* reply) {
    int idx = 0;
    RT_BOOL status = meta->findInt32(kUserInvokeTracksIdx, &idx);
    if(status == RT_FALSE) {
        ALOGE("fillTrackInfoReply : not find track in meta,counter = %d", idx);
        idx = -1;
    }

    reply->writeInt32(idx);
    return OK;
}

rt_status RockitPlayer::fillInvokeReply(INT32 event, RtMetaData* meta, Parcel* reply) {
    rt_status ret = OK;
    switch (event) {
        case INVOKE_ID_GET_TRACK_INFO: {
            ret = fillTrackInfoReply(meta, reply);
        } break;

        case INVOKE_ID_GET_SELECTED_TRACK: {
            ret = fillGetSelectedTrackReply(meta, reply);
        } break;
    }

    return ret;
}

rt_status RockitPlayer::invoke(const Parcel &request, Parcel *reply) {
    ALOGD("RockitPlayer::invoke");
    if (reply == NULL) {
        ALOGD("RockitPlayer::invoke, reply == NULL");
        return OK;
    }

    RtMetaData* in = (RtMetaData *)mCreateMetaDataFunc();
    RtMetaData* out = (RtMetaData *)mCreateMetaDataFunc();
    INT32 event = -1;
    // tranlate cmd to rockit can understand
    fillInvokeRequest(request, in, event);

    mPlayerImpl->invoke(in, out);

    // tranlate result to mediaplayer can understand
    fillInvokeReply(event, out, reply);

    mDestroyMetaDataFunc((void **)&in);
    mDestroyMetaDataFunc((void **)&out);

    return OK;
}

rt_status RockitPlayer::setVideoSink(const void *videoSink) {
    ALOGV("setVideoSink videoSink: %p", videoSink);
    return mPlayerImpl->setVideoSink(videoSink);
}

rt_status RockitPlayer::setAudioSink(const void *audioSink) {
    ALOGV("setAudioSink audioSink: %p", audioSink);
    return mPlayerImpl->setAudioSink(audioSink);
}
rt_status RockitPlayer::setSubteSink(const void *subteSink) {
    ALOGV("setSubteSink subteSink: %p", subteSink);
    return mPlayerImpl->setSubteSink(subteSink);
}

rt_status RockitPlayer::setParameter(INT32 key, const Parcel &request) {
    (void)request;
    ALOGV("setParameter key: %d", key);
    return OK;
}

rt_status RockitPlayer::setListener(RTPlayerListener *listener) {
    return mPlayerImpl->setListener(listener);
}

rt_status RockitPlayer::setPlaybackSettings(const AudioPlaybackRate& rate) {
    RtMetaData* meta = (RtMetaData *)mCreateMetaDataFunc();

    meta->setInt32(kUserInvokeCmd, RT_INVOKE_SET_PLAY_SPEED);
    meta->setFloat(kUserInvokeSetPlaybackRate, rate.mSpeed);

    rt_status status = mPlayerImpl->invoke(meta, NULL);

    mDestroyMetaDataFunc((void **)&meta);
    return status;
}

}

