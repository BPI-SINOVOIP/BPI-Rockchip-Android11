/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ANDROID_MEDIA_STREAMS_H_
#define _ANDROID_MEDIA_STREAMS_H_

#include "src/piex_types.h"
#include "src/piex.h"

#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>

namespace android {

class FileStream : public piex::StreamInterface {
private:
    FILE *mFile;
    size_t mPosition;

public:
    explicit FileStream(const int fd);
    explicit FileStream(const String8 filename);
    ~FileStream();

    // Reads 'length' amount of bytes from 'offset' to 'data'. The 'data' buffer
    // provided by the caller, guaranteed to be at least "length" bytes long.
    // On 'kOk' the 'data' pointer contains 'length' valid bytes beginning at
    // 'offset' bytes from the start of the stream.
    // Returns 'kFail' if 'offset' + 'length' exceeds the stream and does not
    // change the contents of 'data'.
    piex::Error GetData(
            const size_t offset, const size_t length, std::uint8_t* data) override;
    bool exists() const;
};

// Reads EXIF metadata from a given raw image via piex.
// And returns true if the operation is successful; otherwise, false.
bool GetExifFromRawImage(
        piex::StreamInterface* stream, const String8& filename, piex::PreviewImageData& image_data);

// Returns true if the conversion is successful; otherwise, false.
bool ConvertKeyValueArraysToKeyedVector(
        JNIEnv *env, jobjectArray keys, jobjectArray values,
        KeyedVector<String8, String8>* vector);

struct AMessage;
status_t ConvertMessageToMap(
        JNIEnv *env, const sp<AMessage> &msg, jobject *map);

status_t ConvertKeyValueArraysToMessage(
        JNIEnv *env, jobjectArray keys, jobjectArray values,
        sp<AMessage> *msg);

};  // namespace android

#endif //  _ANDROID_MEDIA_STREAMS_H_
