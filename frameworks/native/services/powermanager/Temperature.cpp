/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "Temperature"

#include <android/Temperature.h>
#include <binder/Parcel.h>
#include <utils/Log.h>

namespace android {
namespace os {

status_t Temperature::readFromParcel(const android::Parcel *parcel) {
    if (parcel == nullptr) {
        ALOGE("%s: Null parcel", __FUNCTION__);
        return BAD_VALUE;
    }

    parcel->readFloat(&mValue);
    parcel->readInt32(&mType);
    parcel->readString16(&mName);
    parcel->readInt32(&mStatus);

    return OK;
}

status_t Temperature::writeToParcel(android::Parcel *parcel) const {
    if (parcel == nullptr) {
        ALOGE("%s: Null parcel", __FUNCTION__);
        return BAD_VALUE;
    }

    parcel->writeFloat(mValue);
    parcel->writeInt32(mType);
    parcel->writeString16(mName);
    parcel->writeInt32(mStatus);

    return OK;
}

} // namespace os
} // namespace android