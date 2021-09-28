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
#pragma once

#include <gtest/gtest.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace android {
namespace kernel {

class Cipher {
 public:
  virtual ~Cipher() {}
  bool Encrypt(const std::vector<uint8_t> &key, const uint8_t *iv,
               const uint8_t *src, uint8_t *dst, int nbytes) const {
    if (key.size() != keysize()) {
      ADD_FAILURE() << "Bad key size";
      return false;
    }
    return DoEncrypt(key.data(), iv, src, dst, nbytes);
  }
  virtual bool DoEncrypt(const uint8_t *key, const uint8_t *iv,
                         const uint8_t *src, uint8_t *dst,
                         int nbytes) const = 0;
  virtual int keysize() const = 0;
  virtual int ivsize() const = 0;
};

// aes_256_xts.cpp

constexpr int kAesBlockSize = 16;
constexpr int kAes256KeySize = 32;
constexpr int kAes256XtsKeySize = 2 * kAes256KeySize;

class Aes256XtsCipher : public Cipher {
 public:
  bool DoEncrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *src,
                 uint8_t *dst, int nbytes) const;
  int keysize() const { return kAes256XtsKeySize; }
  int ivsize() const { return kAesBlockSize; }
};

// adiantum.cpp

constexpr int kAdiantumKeySize = 32;

// It's variable-length in general, but the Linux kernel always uses 32.
constexpr int kAdiantumIVSize = 32;

class AdiantumCipher : public Cipher {
 public:
  bool DoEncrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *src,
                 uint8_t *dst, int nbytes) const;
  int keysize() const { return kAdiantumKeySize; }
  int ivsize() const { return kAdiantumIVSize; }
};

// utils.cpp

std::string Errno();

void DeleteRecursively(const std::string &path);

void RandomBytesForTesting(std::vector<uint8_t> &bytes);

std::vector<uint8_t> GenerateTestKey(size_t size);

std::string BytesToHex(const std::vector<uint8_t> &bytes);

template <size_t N>
static inline std::string BytesToHex(const uint8_t (&array)[N]) {
  return BytesToHex(std::vector<uint8_t>(&array[0], &array[N]));
}

bool GetFirstApiLevel(int *first_api_level);

constexpr int kFilesystemUuidSize = 16;

struct FilesystemUuid {
  uint8_t bytes[kFilesystemUuidSize];
};

struct FilesystemInfo {
  std::string fs_blk_device;
  std::string type;
  FilesystemUuid uuid;
  std::string raw_blk_device;
};

bool GetFilesystemInfo(const std::string &mountpoint, FilesystemInfo *info);

bool VerifyDataRandomness(const std::vector<uint8_t> &bytes);

bool CreateHwWrappedKey(std::vector<uint8_t> *master_key,
                        std::vector<uint8_t> *exported_key);

bool DeriveHwWrappedEncryptionKey(const std::vector<uint8_t> &master_key,
                                  std::vector<uint8_t> *enc_key);

bool DeriveHwWrappedRawSecret(const std::vector<uint8_t> &master_key,
                              std::vector<uint8_t> *secret);
}  // namespace kernel
}  // namespace android
