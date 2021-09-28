/*
 * Copyright 2020 The Android Open Source Project
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

// #define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_Metadata"

#include <string.h>
#include <errno.h>

#include <audio_utils/Metadata.h>
#include <log/log.h>

using namespace android::audio_utils::metadata;

audio_metadata_t *audio_metadata_create() {
    return reinterpret_cast<audio_metadata_t *>
            (new(std::nothrow) Data());
}

int audio_metadata_put_int32(audio_metadata_t *metadata, const char *key, int32_t value) {
    if (metadata == nullptr || key == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, value);
    return 0;
}

int audio_metadata_put_int64(audio_metadata_t *metadata, const char *key, int64_t value) {
    if (metadata == nullptr || key == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, value);
    return 0;
}

int audio_metadata_put_float(audio_metadata_t *metadata, const char *key, float value) {
    if (metadata == nullptr || key == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, value);
    return 0;
}

int audio_metadata_put_double(audio_metadata_t *metadata, const char *key, double value) {
    if (metadata == nullptr || key == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, value);
    return 0;
}

int audio_metadata_put_string(audio_metadata_t *metadata, const char *key, const char *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, value);
    return 0;
}

int audio_metadata_put_data(
        audio_metadata_t *metadata, const char *key, audio_metadata_t *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    reinterpret_cast<Data *>(metadata)->emplace(key, *reinterpret_cast<Data *>(value));
    return 0;
}

int audio_metadata_put_unknown(
        audio_metadata_t *metadata __unused, const char *key, const void *value __unused) {
    ALOGW("Unknown data type to put with key: %s", key);
    return -EINVAL;
}

int audio_metadata_get_int32(audio_metadata_t *metadata, const char *key, int32_t *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    int32_t *val = reinterpret_cast<Data *>(metadata)->get_ptr(Key<int32_t>(key));
    if (val == nullptr) {
        return -ENOENT;
    }
    *value = *val;
    return 0;
}

int audio_metadata_get_int64(audio_metadata_t *metadata, const char *key, int64_t *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    int64_t *val = reinterpret_cast<Data *>(metadata)->get_ptr(Key<int64_t>(key));
    if (val == nullptr) {
        return -ENOENT;
    }
    *value = *val;
    return 0;
}

int audio_metadata_get_float(audio_metadata_t *metadata, const char *key, float *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    float *val = reinterpret_cast<Data *>(metadata)->get_ptr(Key<float>(key));
    if (val == nullptr) {
        return -ENOENT;
    }
    *value = *val;
    return 0;
}

int audio_metadata_get_double(audio_metadata_t *metadata, const char *key, double *value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    double *val = reinterpret_cast<Data *>(metadata)->get_ptr(Key<double>(key));
    if (val == nullptr) {
        return -ENOENT;
    }
    *value = *val;
    return 0;
}

int audio_metadata_get_string(audio_metadata_t *metadata, const char *key, char **value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    std::string *valueStr = reinterpret_cast<Data *>(metadata)->get_ptr(Key<std::string>(key));
    if (valueStr == nullptr) {
        return -ENOENT;
    }
    *value = strdup(valueStr->c_str());
    return *value == nullptr ? -ENOMEM : 0;
}

int audio_metadata_get_data(
        audio_metadata_t *metadata, const char *key, audio_metadata_t **value) {
    if (metadata == nullptr || key == nullptr || value == nullptr) {
        return -EINVAL;
    }
    Data *valueData = reinterpret_cast<Data *>(metadata)->get_ptr(Key<Data>(key));
    if (valueData == nullptr) {
        *value = nullptr;
        return -ENOENT;
    }
    *value = reinterpret_cast<audio_metadata_t *>(
            new(std::nothrow) Data(*valueData));
    return *value == nullptr ? -ENOMEM : 0;
}

int audio_metadata_get_unknown(
        audio_metadata_t *metadata __unused, const char *key, void *value __unused) {
    ALOGW("Unknown data type to get with key: %s", key);
    return -EINVAL;
}

ssize_t audio_metadata_erase(audio_metadata_t *metadata, const char *key) {
    if (metadata == nullptr || key == nullptr) {
        return -EINVAL;
    }
    return reinterpret_cast<Data *>(metadata)->erase(key);
}

void audio_metadata_destroy(audio_metadata_t *metadata) {
    delete reinterpret_cast<Data *>(metadata);
}

audio_metadata_t *audio_metadata_from_byte_string(const uint8_t *byteString, size_t length) {
    if (byteString == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<audio_metadata_t *>(
            new(std::nothrow) Data(dataFromByteString(ByteString(byteString, length))));
}

ssize_t byte_string_from_audio_metadata(audio_metadata_t *metadata, uint8_t **byteString) {
    if (metadata == nullptr || byteString == nullptr) {
        return -EINVAL;
    }
    ByteString bs = byteStringFromData(*reinterpret_cast<Data *>(metadata));
    *byteString = (uint8_t *) malloc(bs.size());
    if (*byteString == nullptr) {
        return -ENOMEM;
    }
    memcpy(*byteString, bs.c_str(), bs.size());
    return bs.size();
}
