/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "ECOData"

#include "eco/ECOData.h"

#include <binder/Parcel.h>
#include <inttypes.h>
#include <utils/Errors.h>
#include <utils/Log.h>

#include <string>

#include "eco/ECODataKey.h"
#include "eco/ECOUtils.h"

namespace android {
namespace media {
namespace eco {

using namespace ::android;

status_t ECOData::readFromParcel(const Parcel* parcel) {
    if (parcel == nullptr) {
        ALOGE("readFromParcel failed. Parcel pointer can not be null");
        return BAD_VALUE;
    }

    // Reads the data type and time.
    RETURN_STATUS_IF_ERROR(parcel->readInt32(&mDataType));
    RETURN_STATUS_IF_ERROR(parcel->readInt64(&mDataTimeUs));

    // Reads the number of items.
    uint32_t numOfItems = 0;
    RETURN_STATUS_IF_ERROR(parcel->readUint32(&numOfItems));

    // Reads the key-value pairs one by one.
    for (size_t i = 0; i < numOfItems; ++i) {
        // Reads the name of the key.
        const char* name = parcel->readCString();
        if (name == NULL) {
            ALOGE("Failed reading name for the key. Parsing aborted.");
            return NAME_NOT_FOUND;
        }

        int32_t type;
        RETURN_STATUS_IF_ERROR(parcel->readInt32(&type));
        switch (static_cast<ValueType>(type)) {
        case kTypeInt32: {
            int32_t value32;
            RETURN_STATUS_IF_ERROR(parcel->readInt32(&value32));
            setInt32(std::string(name), value32);
            break;
        }
        case kTypeInt64: {
            int64_t value64;
            RETURN_STATUS_IF_ERROR(parcel->readInt64(&value64));
            setInt64(std::string(name), value64);
            break;
        }
        case kTypeSize: {
            int32_t valueSize;
            RETURN_STATUS_IF_ERROR(parcel->readInt32(&valueSize));
            setInt32(std::string(name), valueSize);
            break;
        }
        case kTypeFloat: {
            float valueFloat;
            RETURN_STATUS_IF_ERROR(parcel->readFloat(&valueFloat));
            setFloat(std::string(name), valueFloat);
            break;
        }
        case kTypeDouble: {
            double valueDouble;
            RETURN_STATUS_IF_ERROR(parcel->readDouble(&valueDouble));
            setDouble(std::string(name), valueDouble);
            break;
        }
        case kTypeString: {
            const char* valueStr = parcel->readCString();
            if (valueStr == NULL) {
                ALOGE("Failed reading name for the key. Parsing aborted.");
                return NAME_NOT_FOUND;
            }
            setString(std::string(name), valueStr);
            break;
        }
        case kTypeInt8: {
            int8_t value8;
            RETURN_STATUS_IF_ERROR(parcel->readByte(&value8));
            setInt8(std::string(name), value8);
            break;
        }
        default: {
            return BAD_TYPE;
        }
        }
    }

    return NO_ERROR;
}

status_t ECOData::writeToParcel(Parcel* parcel) const {
    if (parcel == nullptr) {
        ALOGE("writeToParcel failed. Parcel pointer can not be null");
        return BAD_VALUE;
    }

    // Writes out the data type and time.
    RETURN_STATUS_IF_ERROR(parcel->writeInt32(mDataType));
    RETURN_STATUS_IF_ERROR(parcel->writeInt64(mDataTimeUs));

    // Writes out number of items.
    RETURN_STATUS_IF_ERROR(parcel->writeUint32(int32_t(mKeyValueStore.size())));

    // Writes out the key-value pairs one by one.
    for (const auto& it : mKeyValueStore) {
        // Writes out the key.
        RETURN_STATUS_IF_ERROR(parcel->writeCString(it.first.c_str()));

        // Writes out the data type.
        const ECODataValueType& value = it.second;
        RETURN_STATUS_IF_ERROR(parcel->writeInt32(static_cast<int32_t>(value.index())));
        switch (static_cast<ValueType>(value.index())) {
        case kTypeInt32:
            RETURN_STATUS_IF_ERROR(parcel->writeInt32(std::get<int32_t>(it.second)));
            break;

        case kTypeInt64:
            RETURN_STATUS_IF_ERROR(parcel->writeInt64(std::get<int64_t>(it.second)));
            break;

        case kTypeSize:
            RETURN_STATUS_IF_ERROR(parcel->writeUint32(std::get<size_t>(it.second)));
            break;

        case kTypeFloat:
            RETURN_STATUS_IF_ERROR(parcel->writeFloat(std::get<float>(it.second)));
            break;

        case kTypeDouble:
            RETURN_STATUS_IF_ERROR(parcel->writeDouble(std::get<double>(it.second)));
            break;

        case kTypeString:
            RETURN_STATUS_IF_ERROR(parcel->writeCString(std::get<std::string>(it.second).c_str()));
            break;

        case kTypeInt8:
            RETURN_STATUS_IF_ERROR(parcel->writeByte(std::get<int8_t>(it.second)));
            break;

        default:
            return BAD_TYPE;
        }
    }

    return NO_ERROR;
}

int32_t ECOData::getDataType() const {
    return mDataType;
}

int64_t ECOData::getDataTimeUs() const {
    return mDataTimeUs;
}

// Inserts a new key into store if the key does not exist yet. Otherwise, this will override the
// existing key's value.
ECODataStatus ECOData::setString(const std::string& key, const std::string& value) {
    if (key.empty() || value.empty()) {
        return ECODataStatus::INVALID_ARGUMENT;
    }

    mKeyValueStore[key] = value;

    // TODO(hkuang): Check the valueType is valid for the key.
    return ECODataStatus::OK;
}

ECODataStatus ECOData::findString(const std::string& key, std::string* value) const {
    if (key.empty()) {
        return ECODataStatus::INVALID_ARGUMENT;
    }

    // Check if the key exists.
    if (mKeyValueStore.find(key) == mKeyValueStore.end()) {
        return ECODataStatus::KEY_NOT_EXIST;
    }

    // Safely access the value.
    const std::string& entryValue = std::get<std::string>(mKeyValueStore.at(key));
    value->assign(entryValue);

    return ECODataStatus::OK;
}

// Inserts a new key into store if the key does not exist yet. Otherwise, this will override the
// existing key's value.
template <typename T>
ECODataStatus ECOData::setValue(const std::string& key, T value) {
    if (key.empty()) {
        return ECODataStatus::INVALID_ARGUMENT;
    }

    mKeyValueStore[key] = value;
    return ECODataStatus::OK;
}

template <typename T>
ECODataStatus ECOData::findValue(const std::string& key, T* out) const {
    if (key.empty() || out == nullptr) {
        return ECODataStatus::INVALID_ARGUMENT;
    }

    if (mKeyValueStore.find(key) == mKeyValueStore.end()) {
        return ECODataStatus::KEY_NOT_EXIST;
    }

    // Safely access the value.
    *out = std::get<T>(mKeyValueStore.at(key));

    return ECODataStatus::OK;
}

ECODataStatus ECOData::setInt32(const std::string& key, int32_t value) {
    return setValue<int32_t>(key, value);
}

ECODataStatus ECOData::findInt32(const std::string& key, int32_t* out) const {
    return findValue<int32_t>(key, out);
}

ECODataStatus ECOData::setInt64(const std::string& key, int64_t value) {
    return setValue<int64_t>(key, value);
}

ECODataStatus ECOData::findInt64(const std::string& key, int64_t* out) const {
    return findValue<int64_t>(key, out);
}

ECODataStatus ECOData::setDouble(const std::string& key, double value) {
    return setValue<double>(key, value);
}

ECODataStatus ECOData::findDouble(const std::string& key, double* out) const {
    return findValue<double>(key, out);
}

ECODataStatus ECOData::setSize(const std::string& key, size_t value) {
    return setValue<size_t>(key, value);
}

ECODataStatus ECOData::findSize(const std::string& key, size_t* out) const {
    return findValue<size_t>(key, out);
}

ECODataStatus ECOData::setFloat(const std::string& key, float value) {
    return setValue<float>(key, value);
}

ECODataStatus ECOData::findFloat(const std::string& key, float* out) const {
    return findValue<float>(key, out);
}

ECODataStatus ECOData::setInt8(const std::string& key, int8_t value) {
    return setValue<int8_t>(key, value);
}

ECODataStatus ECOData::findInt8(const std::string& key, int8_t* out) const {
    return findValue<int8_t>(key, out);
}

ECODataStatus ECOData::set(const std::string& key, const ECOData::ECODataValueType& value) {
    if (key.empty()) {
        return ECODataStatus::INVALID_ARGUMENT;
    }
    mKeyValueStore[key] = value;
    return ECODataStatus::OK;
}

ECODataStatus ECOData::find(const std::string& key, ECOData::ECODataValueType* out) const {
    if (key.empty() || out == nullptr) {
        return ECODataStatus::INVALID_ARGUMENT;
    }

    if (mKeyValueStore.find(key) == mKeyValueStore.end()) {
        return ECODataStatus::KEY_NOT_EXIST;
    }

    // Safely access the value.
    *out = mKeyValueStore.at(key);

    return ECODataStatus::OK;
}

std::string ECOData::getDataTypeString() const {
    switch (mDataType) {
    case DATA_TYPE_UNKNOWN:
        return "DATA_TYPE_UNKNOWN";
    case DATA_TYPE_STATS:
        return "DATA_TYPE_STATS";
    case DATA_TYPE_INFO:
        return "DATA_TYPE_INFO";
    case DATA_TYPE_STATS_PROVIDER_CONFIG:
        return "DATA_TYPE_STATS_PROVIDER_CONFIG";
    case DATA_TYPE_INFO_LISTENER_CONFIG:
        return "DATA_TYPE_INFO_LISTENER_CONFIG";
    }
    return {};
}

// TODO(hkuang): Add test for this.
bool ECODataKeyValueIterator::hasNext() {
    if (mIterator == mKeyValueStore.end()) return false;

    if (!mBeginReturned) {
        // mIterator has been initialized to the beginning and
        // hasn't been returned. Do not advance:
        mBeginReturned = true;
    } else {
        std::advance(mIterator, 1);
    }
    return mIterator != mKeyValueStore.end();
}

// TODO(hkuang): Add test for this.
ECOData::ECODataKeyValuePair ECODataKeyValueIterator::next() const {
    return ECOData::ECODataKeyValuePair(mIterator->first, mIterator->second);
}

std::string ECOData::debugString() const {
    std::string s = "ECOData(type = ";

    std::string tmp;
    switch (mDataType) {
    case DATA_TYPE_UNKNOWN:
        tmp = "Unknown";
        break;
    case DATA_TYPE_STATS:
        tmp = "Stats";
        break;
    case DATA_TYPE_INFO:
        tmp = "Info";
        break;
    case DATA_TYPE_STATS_PROVIDER_CONFIG:
        tmp = "Stats provider config";
        break;
    case DATA_TYPE_INFO_LISTENER_CONFIG:
        tmp = "Info listener config";
        break;
    default:
        break;
    }
    s.append(tmp);
    s.append(") = {\n  ");

    // Writes out the key-value pairs one by one.
    for (const auto& it : mKeyValueStore) {
        const size_t SIZE = 100;
        char keyValue[SIZE];
        const ECODataValueType& value = it.second;
        switch (static_cast<ValueType>(value.index())) {
        case kTypeInt32:
            snprintf(keyValue, SIZE, "int32_t %s = %d, ", it.first.c_str(),
                     std::get<int32_t>(it.second));
            break;
        case kTypeInt64:
            snprintf(keyValue, SIZE, "int64_t %s = %" PRId64 ", ", it.first.c_str(),
                     std::get<int64_t>(it.second));
            break;
        case kTypeSize:
            snprintf(keyValue, SIZE, "size_t %s = %zu, ", it.first.c_str(),
                     std::get<size_t>(it.second));
            break;
        case kTypeFloat:
            snprintf(keyValue, SIZE, "float %s = %f, ", it.first.c_str(),
                     std::get<float>(it.second));
            break;
        case kTypeDouble:
            snprintf(keyValue, SIZE, "double %s = %f, ", it.first.c_str(),
                     std::get<double>(it.second));
            break;
        case kTypeString:
            snprintf(keyValue, SIZE, "string %s = %s, ", it.first.c_str(),
                     std::get<std::string>(it.second).c_str());
            break;
        case kTypeInt8:
            snprintf(keyValue, SIZE, "int8_t %s = %d, ", it.first.c_str(),
                     std::get<int8_t>(it.second));
            break;
        default:
            break;
        }
        s.append(keyValue);
    }

    s.append("\n }");

    return s;
}

}  // namespace eco
}  // namespace media
}  // namespace android
