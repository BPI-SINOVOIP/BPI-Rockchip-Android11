/*
**
** Copyright 2012, The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaPlayerFactory"
#include <utils/Log.h>

#include <cutils/properties.h>
#include <datasource/FileSource.h>
#include <media/DataSource.h>
#include <media/IMediaPlayer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/Errors.h>
#include <utils/misc.h>

#include "MediaPlayerFactory.h"

#include "TestPlayerStub.h"
#include "nuplayer/NuPlayerDriver.h"
#include "RockitPlayerInterface.h"

namespace android {

Mutex MediaPlayerFactory::sLock;
MediaPlayerFactory::tFactoryMap MediaPlayerFactory::sFactoryMap;
bool MediaPlayerFactory::sInitComplete = false;

static status_t getFileName(int fd,String8 *FilePath) {
    static ssize_t link_dest_size;
    static char link_dest[PATH_MAX];
    const char *ptr = NULL;
    String8 path;
    path.appendFormat("/proc/%d/fd/%d", getpid(), fd);

    if ((link_dest_size = readlink(path.string(), link_dest, sizeof(link_dest)-1)) < 0) {
        return errno;
    } else {
        link_dest[link_dest_size] = '\0';
    }

    path = link_dest;
    ptr = path.string();
    *FilePath = String8(ptr);

    return OK;
}

status_t MediaPlayerFactory::registerFactory_l(IFactory* factory,
                                               player_type type) {
    if (NULL == factory) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, factory is"
              " NULL.", type);
        return BAD_VALUE;
    }

    if (sFactoryMap.indexOfKey(type) >= 0) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, type is"
              " already registered.", type);
        return ALREADY_EXISTS;
    }

    if (sFactoryMap.add(type, factory) < 0) {
        ALOGE("Failed to register MediaPlayerFactory of type %d, failed to add"
              " to map.", type);
        return UNKNOWN_ERROR;
    }

    return OK;
}

static player_type getDefaultPlayerType() {
    char value[PROPERTY_VALUE_MAX];
    if (property_get("cts_gts.status", value, NULL)
        && !strcasecmp("true", value)){
        return NU_PLAYER;
    }

    if (property_get("use_nuplayer", value, NULL)
        && !strcasecmp("true", value)) {
        return NU_PLAYER;
    }
    return ROCKIT_PLAYER;
}

status_t MediaPlayerFactory::registerFactory(IFactory* factory,
                                             player_type type) {
    Mutex::Autolock lock_(&sLock);
    return registerFactory_l(factory, type);
}

void MediaPlayerFactory::unregisterFactory(player_type type) {
    Mutex::Autolock lock_(&sLock);
    sFactoryMap.removeItem(type);
}

#define GET_PLAYER_TYPE_IMPL(a...)                      \
    Mutex::Autolock lock_(&sLock);                      \
                                                        \
    player_type ret = STAGEFRIGHT_PLAYER;               \
    float bestScore = 0.0;                              \
                                                        \
    for (size_t i = 0; i < sFactoryMap.size(); ++i) {   \
                                                        \
        IFactory* v = sFactoryMap.valueAt(i);           \
        float thisScore;                                \
        CHECK(v != NULL);                               \
        thisScore = v->scoreFactory(a, bestScore);      \
        if (thisScore > bestScore) {                    \
            ret = sFactoryMap.keyAt(i);                 \
            bestScore = thisScore;                      \
        }                                               \
    }                                                   \
                                                        \
    if (0.0 == bestScore) {                             \
        ret = getDefaultPlayerType();                   \
    }                                                   \
                                                        \
    return ret;

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              const char* url) {
    if (strstr(url,".ogg")
        || strstr(url,".apk")) {
        return NU_PLAYER;
    }
    GET_PLAYER_TYPE_IMPL(client, url);
}

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              int fd,
                                              int64_t offset,
                                              int64_t length) {
    String8 filePath;
    getFileName(fd,&filePath);
    if (strstr(filePath.string(), ".ogg")
        || strstr(filePath.string(), ".mid")
        || strstr(filePath.string(), ".MID")
        || strstr(filePath.string(), ".mp3")
        || strstr(filePath.string(), ".aac")
        || strstr(filePath.string(), ".apk")
        || strstr(filePath.string(), "notification_sound_cache")
        || strstr(filePath.string(), "ringtone_cache")
        || strstr(filePath.string(), "alarm_alert_cache")) {
        return NU_PLAYER;
    }

    GET_PLAYER_TYPE_IMPL(client, fd, offset, length);
}

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              const sp<IStreamSource> &source) {
    GET_PLAYER_TYPE_IMPL(client, source);
}

player_type MediaPlayerFactory::getPlayerType(const sp<IMediaPlayer>& client,
                                              const sp<DataSource> &source) {
    GET_PLAYER_TYPE_IMPL(client, source);
}

#undef GET_PLAYER_TYPE_IMPL

sp<MediaPlayerBase> MediaPlayerFactory::createPlayer(
        player_type playerType,
        const sp<MediaPlayerBase::Listener> &listener,
        pid_t pid) {
    sp<MediaPlayerBase> p;
    IFactory* factory;
    status_t init_result;
    Mutex::Autolock lock_(&sLock);

    if (sFactoryMap.indexOfKey(playerType) < 0) {
        ALOGE("Failed to create player object of type %d, no registered"
              " factory", playerType);
        return p;
    }

    factory = sFactoryMap.valueFor(playerType);
    CHECK(NULL != factory);
    p = factory->createPlayer(pid);

    if (p == NULL) {
        ALOGE("Failed to create player object of type %d, create failed",
               playerType);
        return p;
    }

    init_result = p->initCheck();
    if (init_result == NO_ERROR) {
        p->setNotifyCallback(listener);
    } else {
        ALOGE("Failed to create player object of type %d, initCheck failed"
              " (res = %d)", playerType, init_result);
        p.clear();
    }

    return p;
}

/*****************************************************************************
 *                                                                           *
 *                     Built-In Factory Implementations                      *
 *                                                                           *
 *****************************************************************************/

class NuPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const char* url,
                               float curScore) {
        static const float kOurScore = 0.8;

        if (kOurScore <= curScore)
            return 0.0;

        if (!strncasecmp("http://", url, 7)
                || !strncasecmp("https://", url, 8)
                || !strncasecmp("file://", url, 7)) {
            size_t len = strlen(url);
            if (len >= 5 && !strcasecmp(".m3u8", &url[len - 5])) {
                return kOurScore;
            }

            if (strstr(url,"m3u8")) {
                return kOurScore;
            }

            if ((len >= 4 && !strcasecmp(".sdp", &url[len - 4])) || strstr(url, ".sdp?")) {
                return kOurScore;
            }
        }

        if (!strncasecmp("rtsp://", url, 7)) {
            return kOurScore;
        }

        return 0.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const sp<IStreamSource>& /*source*/,
                               float /*curScore*/) {
        return 1.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const sp<DataSource>& /*source*/,
                               float /*curScore*/) {
        // Only NuPlayer supports setting a DataSource source directly.
        return 1.0;
    }

    virtual sp<MediaPlayerBase> createPlayer(pid_t pid) {
        ALOGV(" create NuPlayer");
        return new NuPlayerDriver(pid);
    }
};

class TestPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const char* url,
                               float /*curScore*/) {
        if (TestPlayerStub::canBeUsed(url)) {
            return 1.0;
        }

        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer(pid_t /* pid */) {
        ALOGV("Create Test Player stub");
        return new TestPlayerStub();
    }
};


class RockitPlayerFactory : public MediaPlayerFactory::IFactory {
  public:
    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const char* url,
                               float curScore) {
        static const float kOurScore = 0.9;

        if (kOurScore <= curScore)
            return 0.0;

        if (!strncasecmp("http://", url, 7)
            || !strncasecmp("https://", url, 8)
            || !strncasecmp("file://", url, 7)) {
            char value[PROPERTY_VALUE_MAX];
            if (property_get("cts_gts.status", value, NULL)
                && !strcasecmp("true", value)){
                    return 0.0;
            }

            if (property_get("use_nuplayer", value, NULL)
                && !strcasecmp("true", value)) {
                    return 0.0;
            }

            return kOurScore;
        }

        if (!strncasecmp("rtsp://", url, 7)) {
            return kOurScore;
        }

        return 0.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const sp<IStreamSource>& /*source*/,
                               float /*curScore*/) {
        return 0.0;
    }

    virtual float scoreFactory(const sp<IMediaPlayer>& /*client*/,
                               const sp<DataSource>& /*source*/,
                               float /*curScore*/) {
        // Rockit player supports setting a DataSource source directly.
        return 0.0;
    }

    virtual sp<MediaPlayerBase> createPlayer(pid_t pid) {
        (void)pid;
        ALOGD("create Rockit Player");
        return new RockitPlayerClient();
    }
};

void MediaPlayerFactory::registerBuiltinFactories() {
    Mutex::Autolock lock_(&sLock);

    if (sInitComplete)
        return;

    IFactory* factory = new NuPlayerFactory();
    if (registerFactory_l(factory, NU_PLAYER) != OK)
        delete factory;
    factory = new TestPlayerFactory();
    if (registerFactory_l(factory, TEST_PLAYER) != OK)
        delete factory;
    factory = new RockitPlayerFactory();
    if (registerFactory_l(factory, ROCKIT_PLAYER) != OK)
        delete factory;

    sInitComplete = true;
}

}  // namespace android
