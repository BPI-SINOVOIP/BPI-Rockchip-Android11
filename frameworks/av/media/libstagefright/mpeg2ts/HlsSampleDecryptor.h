/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef HLS_SAMPLE_AES_PROCESSOR_H_

#define HLS_SAMPLE_AES_PROCESSOR_H_

#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>

#include <openssl/aes.h>

#include <utils/Errors.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/Vector.h>

#include "SampleDecryptor.h"

namespace android {

struct HlsSampleDecryptor : SampleDecryptor {

    HlsSampleDecryptor();
    explicit HlsSampleDecryptor(const sp<AMessage> &sampleAesKeyItem);

    virtual void signalNewSampleAesKey(const sp<AMessage> &sampleAesKeyItem);

    virtual size_t processNal(uint8_t *nalData, size_t nalSize);
    virtual void processAAC(size_t adtsHdrSize, uint8_t *data, size_t size);
    virtual void processAC3(uint8_t *data, size_t size);

    static AString aesBlockToStr(uint8_t block[AES_BLOCK_SIZE]);

private:
    size_t unescapeStream(uint8_t *data, size_t limit) const;
    size_t findNextUnescapeIndex(uint8_t *data, size_t offset, size_t limit) const;
    status_t decryptBlock(uint8_t *buffer, size_t size, uint8_t AESInitVec[AES_BLOCK_SIZE]);

    static const int VIDEO_CLEAR_LEAD = 32;
    static const int AUDIO_CLEAR_LEAD = 16;

    AES_KEY mAesKey;
    uint8_t mAESInitVec[AES_BLOCK_SIZE];
    bool mValidKeyInfo;

    DISALLOW_EVIL_CONSTRUCTORS(HlsSampleDecryptor);
};

}  // namespace android

#endif // HLS_SAMPLE_AES_PROCESSOR_H_
