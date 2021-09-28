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

#define LOG_TAG "RTMsgCallback"

#include "RTMsgCallback.h"
#include "RTMediaMetaKeys.h"

#define RT_KEY_LOCAL_SETTING                 102
#define RT_KEY_START_TIME                    7
#define RT_KEY_STRUCT_TEXT                   16
#define RT_TEXT_NOTIFY_MSG                   99


namespace android {

RTMsgCallback::RTMsgCallback(android::MediaPlayerInterface *player) {
    ALOGD("RTMsgCallback(%p) construct", this);
    mPlayer = player;
}

RTMsgCallback::~RTMsgCallback() {
    ALOGD("~RTMsgCallback(%p) destruct", this);
}

void RTMsgCallback::notify(int32_t msg, int32_t ext1, int32_t ext2, void* ptr) {
    ALOGV("notify msg: %d, ext1: %d, ext2: %d", msg, ext1, ext2);
    if(ptr != NULL && msg == RT_TEXT_NOTIFY_MSG) {
        Parcel txtParcel;
        const char *text;
        int64_t startTime;
        int32_t size;

        RtMetaData* textInfo = (RtMetaData *)ptr;
        textInfo->findInt64(kUserNotifyPts, &startTime);
        textInfo->findInt32(kUserNotifySize, &size);
        textInfo->findCString(kUserNotifyData, &text);

        txtParcel.writeInt32(RT_KEY_LOCAL_SETTING);
        txtParcel.writeInt32(RT_KEY_START_TIME);
        txtParcel.writeInt32((int32_t)startTime);
        txtParcel.writeInt32(RT_KEY_STRUCT_TEXT);
        txtParcel.writeInt32(size);
        txtParcel.writeByteArray(size, (const uint8_t *)text);
        mPlayer->sendEvent(msg, (int32_t)ext1, (int32_t)ext2, &txtParcel);
        txtParcel.freeData();
    } else {
        mPlayer->sendEvent(msg, (int32_t)ext1, (int32_t)ext2, NULL);
    }
}

}

