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

#include <crypto_utils/android_pubkey.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <string.h>
#include <memory>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>
#include <cstdio>

#define ANDROID_PUBKEY_MODULUS_SIZE_WORDS (ANDROID_PUBKEY_MODULUS_SIZE / 4)

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    if (size < 2050) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    uint8_t buffer[ANDROID_PUBKEY_ENCODED_SIZE];
    uint32_t modulus_size_words = ANDROID_PUBKEY_MODULUS_SIZE_WORDS;
    memcpy(buffer, &modulus_size_words, sizeof(uint32_t));

    uint32_t n0inv = fdp.ConsumeIntegralInRange<uint32_t>(0,2^32);
    memcpy(buffer+sizeof(uint32_t), &n0inv, sizeof(uint32_t));

    std::string s = fdp.ConsumeBytesAsString(ANDROID_PUBKEY_MODULUS_SIZE);
    uint8_t* modulus = (uint8_t*)s.c_str();
    memcpy(buffer+sizeof(uint32_t)*2, modulus,
           sizeof(uint8_t)*ANDROID_PUBKEY_MODULUS_SIZE);

    std::string ss = fdp.ConsumeBytesAsString(ANDROID_PUBKEY_MODULUS_SIZE);
    uint8_t* rr = (uint8_t*)ss.c_str();
    memcpy(buffer+sizeof(uint32_t)*2+ANDROID_PUBKEY_MODULUS_SIZE, rr,
           sizeof(uint8_t)*ANDROID_PUBKEY_MODULUS_SIZE);

    int flip = fdp.ConsumeIntegralInRange<uint32_t>(0,1);
    buffer[ANDROID_PUBKEY_ENCODED_SIZE-1] = (uint8_t)(flip == 0 ? 3 : 65537);

    RSA* new_key = nullptr;
    android_pubkey_decode(buffer, sizeof(uint8_t)*ANDROID_PUBKEY_ENCODED_SIZE, &new_key);

    uint8_t key_data[ANDROID_PUBKEY_ENCODED_SIZE];
    android_pubkey_encode(new_key, key_data, sizeof(key_data));

    assert(0 == memcmp(buffer, key_data, sizeof(buffer)));

    RSA_free(new_key);

    return 0;
}
