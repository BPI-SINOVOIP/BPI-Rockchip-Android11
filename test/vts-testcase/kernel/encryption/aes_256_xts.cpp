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

#include <gtest/gtest.h>
#include <openssl/cipher.h>
#include <openssl/evp.h>
#include <string.h>

#include "vts_kernel_encryption.h"

namespace android {
namespace kernel {

static void DoXtsMasking(uint8_t *data, int num_blocks,
                         const uint8_t tweak[kAesBlockSize]) {
  uint8_t mask[kAesBlockSize];

  memcpy(mask, tweak, kAesBlockSize);

  for (int i = 0; i < num_blocks; i++) {
    // XOR the next block with the current mask.
    for (int j = 0; j < kAesBlockSize; j++) {
      data[i * kAesBlockSize + j] ^= mask[j];
    }
    // Multipy the mask by 'x' in GF(2^128).
    int carry = 0;
    for (int j = 0; j < kAesBlockSize; j++) {
      int next_carry = mask[j] >> 7;

      mask[j] = (mask[j] << 1) ^ carry;
      carry = next_carry;
    }
    if (carry != 0) {
      mask[0] ^= 0x87;
    }
  }
}

bool Aes256XtsCipher::DoEncrypt(const uint8_t key[kAes256XtsKeySize],
                                const uint8_t iv[kAesBlockSize],
                                const uint8_t *src, uint8_t *dst,
                                int nbytes) const {
  std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX *)> ctx(
      EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  int outl;

  if (ctx == nullptr) {
    ADD_FAILURE() << "Failed to allocate BoringSSL cipher context";
    return false;
  }
  if (nbytes % kAesBlockSize != 0) {
    ADD_FAILURE() << "Bad input size";
    return false;
  }

  // For some reason, BoringSSL considers AES-256-XTS to be deprecated, and it's
  // in a directory of deprecated algorithms that's not compiled on Android.
  // AES-256-ECB is still available though, so just implement XTS manually...

  // Encrypt the IV.  This uses the second half of the AES-256-XTS key.
  uint8_t tweak[kAesBlockSize];
  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr,
                         key + kAes256KeySize, nullptr) != 1) {
    ADD_FAILURE() << "Failed to initialize BoringSSL AES context";
    return false;
  }
  if (EVP_EncryptUpdate(ctx.get(), tweak, &outl, iv, kAesBlockSize) != 1 ||
      outl != kAesBlockSize) {
    ADD_FAILURE() << "BoringSSL AES encryption failed (tweak)";
    return false;
  }

  // Copy plaintext to output buffer, so that we can just transform it in-place.
  memmove(dst, src, nbytes);

  // Mask the data pre-encryption.
  DoXtsMasking(dst, nbytes / kAesBlockSize, tweak);

  // Encrypt the data.
  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ecb(), nullptr, key, nullptr) !=
      1) {
    ADD_FAILURE() << "Failed to reinitialize BoringSSL AES context";
    return false;
  }
  if (EVP_EncryptUpdate(ctx.get(), dst, &outl, dst, nbytes) != 1 ||
      outl != nbytes) {
    ADD_FAILURE() << "BoringSSL AES encryption failed (data)";
    return false;
  }
  // Mask the data post-encryption.
  DoXtsMasking(dst, nbytes / kAesBlockSize, tweak);
  return true;
}

}  // namespace kernel
}  // namespace android
