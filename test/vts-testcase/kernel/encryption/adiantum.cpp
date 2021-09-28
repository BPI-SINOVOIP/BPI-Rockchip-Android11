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

// Adiantum encryption mode
//
// Reference: "Adiantum: length-preserving encryption for entry-level
// processors" https://tosc.iacr.org/index.php/ToSC/article/view/7360

#include <asm/byteorder.h>
#include <gtest/gtest.h>
#include <linux/types.h>
#include <openssl/aes.h>
#include <openssl/poly1305.h>
#include <string.h>

#include "vts_kernel_encryption.h"

namespace android {
namespace kernel {

#define cpu_to_le32 __cpu_to_le32
#define cpu_to_le64 __cpu_to_le64
#define le32_to_cpu __le32_to_cpu
#define le64_to_cpu __le64_to_cpu

static uint32_t get_unaligned_le32(const void *p) {
  __le32 x;

  memcpy(&x, p, sizeof(x));
  return le32_to_cpu(x);
}

static void put_unaligned_le32(uint32_t v, void *p) {
  __le32 x = cpu_to_le32(v);

  memcpy(p, &x, sizeof(x));
}

static void put_unaligned_le64(uint64_t v, void *p) {
  __le64 x = cpu_to_le64(v);

  memcpy(p, &x, sizeof(x));
}

static unsigned int round_up(unsigned int a, unsigned int b) {
  return a + -a % b;
}

static uint32_t rol32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void le128_add(uint8_t res[16], const uint8_t a[16],
                      const uint8_t b[16]) {
  int carry = 0;
  for (int i = 0; i < 16; i++) {
    int sum = a[i] + b[i] + carry;

    res[i] = sum;
    carry = sum >> 8;
  }
}

static void le128_sub(uint8_t res[16], const uint8_t a[16],
                      const uint8_t b[16]) {
  int carry = 0;
  for (int i = 0; i < 16; i++) {
    int sum = a[i] - b[i] - carry;

    res[i] = sum;
    carry = (sum < 0);
  }
}

constexpr int kChaChaKeySize = 32;
constexpr int kXChaChaKeySize = kChaChaKeySize;
constexpr int kXChaChaNonceSize = 24;

static void ChaChaInitState(uint32_t state[16],
                            const uint8_t key[kChaChaKeySize],
                            const uint8_t iv[16]) {
  static const uint8_t consts[] = "expand 32-byte k";
  int i;

  for (i = 0; i < 4; i++)
    state[i] = get_unaligned_le32(&consts[i * sizeof(__le32)]);
  for (i = 0; i < 8; i++)
    state[4 + i] = get_unaligned_le32(&key[i * sizeof(__le32)]);
  for (i = 0; i < 4; i++)
    state[12 + i] = get_unaligned_le32(&iv[i * sizeof(__le32)]);
}

#define CHACHA_QUARTERROUND(a, b, c, d) \
  do {                                  \
    a += b;                             \
    d = rol32(d ^ a, 16);               \
    c += d;                             \
    b = rol32(b ^ c, 12);               \
    a += b;                             \
    d = rol32(d ^ a, 8);                \
    c += d;                             \
    b = rol32(b ^ c, 7);                \
  } while (0)

static void ChaChaPermute(uint32_t x[16], int nrounds) {
  do {
    // column round
    CHACHA_QUARTERROUND(x[0], x[4], x[8], x[12]);
    CHACHA_QUARTERROUND(x[1], x[5], x[9], x[13]);
    CHACHA_QUARTERROUND(x[2], x[6], x[10], x[14]);
    CHACHA_QUARTERROUND(x[3], x[7], x[11], x[15]);

    // diagonal round
    CHACHA_QUARTERROUND(x[0], x[5], x[10], x[15]);
    CHACHA_QUARTERROUND(x[1], x[6], x[11], x[12]);
    CHACHA_QUARTERROUND(x[2], x[7], x[8], x[13]);
    CHACHA_QUARTERROUND(x[3], x[4], x[9], x[14]);
  } while ((nrounds -= 2) != 0);
}

static void XChaCha(const uint8_t key[kXChaChaKeySize],
                    const uint8_t nonce[kXChaChaNonceSize], const uint8_t *src,
                    uint8_t *dst, int nbytes, int nrounds) {
  uint32_t state[16];
  uint8_t real_key[kChaChaKeySize];
  uint8_t real_iv[16] = {0};
  int i, j;

  // Compute real key using original key and first 128 nonce bits
  ChaChaInitState(state, key, nonce);
  ChaChaPermute(state, nrounds);
  for (i = 0; i < 8; i++)  // state words 0..3, 12..15
    put_unaligned_le32(state[(i < 4 ? 0 : 8) + i],
                       &real_key[i * sizeof(__le32)]);

  // Now do regular ChaCha, using real key and remaining nonce bits
  memcpy(&real_iv[8], nonce + 16, 8);
  ChaChaInitState(state, real_key, real_iv);
  for (i = 0; i < nbytes; i += 64) {
    uint32_t x[16];
    union {
      __le32 words[16];
      uint8_t bytes[64];
    } keystream;

    memcpy(x, state, 64);
    ChaChaPermute(x, nrounds);
    for (j = 0; j < 16; j++) keystream.words[j] = cpu_to_le32(x[j] + state[j]);
    for (j = 0; j < std::min(nbytes - i, 64); j++)
      dst[i + j] = src[i + j] ^ keystream.bytes[j];
    if (++state[12] == 0) state[13]++;
  }
}

// XChaCha12 stream cipher
//
// References:
//   - "XChaCha: eXtended-nonce ChaCha and AEAD_XChaCha20_Poly1305"
//	https://tools.ietf.org/html/draft-arciszewski-xchacha-03
//
//   - "ChaCha, a variant of Salsa20"
//	https://cr.yp.to/chacha/chacha-20080128.pdf
//
//   - "Extending the Salsa20 nonce"
//	https://cr.yp.to/snuffle/xsalsa-20081128.pdf
static void XChaCha12(const uint8_t key[kXChaChaKeySize],
                      const uint8_t nonce[kXChaChaNonceSize],
                      const uint8_t *src, uint8_t *dst, int nbytes) {
  XChaCha(key, nonce, src, dst, nbytes, 12);
}

constexpr int kPoly1305BlockSize = 16;
constexpr int kPoly1305KeySize = 16;
constexpr int kPoly1305HashSize = 16;

static void Poly1305(const uint8_t key[kPoly1305KeySize], const uint8_t *msg,
                     int msglen, uint8_t out[kPoly1305HashSize]) {
  // Adiantum wants just the Poly1305 ε-almost-∆-universal hash function, not
  // the full MAC.  To get the correct result with BoringSSL's Poly1305 MAC
  // implementation, leave the second half of the MAC key zeroed.  (The first
  // half is the real Poly1305 key; the second half is the value which gets
  // added at the end.)
  uint8_t mac_key[2 * kPoly1305KeySize] = {0};

  memcpy(mac_key, key, kPoly1305KeySize);

  poly1305_state state;
  CRYPTO_poly1305_init(&state, mac_key);
  CRYPTO_poly1305_update(&state, msg, msglen);
  CRYPTO_poly1305_finish(&state, out);
}

constexpr int kNHBlockSize = 1024;
constexpr int kNHHashSize = 32;
constexpr int kNHKeySize = 1072;
constexpr int kNHKeyWords = kNHKeySize / sizeof(uint32_t);
constexpr int kNHMessageUnit = 16;

static uint64_t NH_Add(const uint8_t *a, uint32_t b) {
  return static_cast<uint32_t>(get_unaligned_le32(a) + b);
}

static uint64_t NH_Pass(const uint32_t *key, const uint8_t *msg, int msglen) {
  uint64_t sum = 0;

  EXPECT_TRUE(msglen % kNHMessageUnit == 0);
  while (msglen >= kNHMessageUnit) {
    sum += NH_Add(msg + 0, key[0]) * NH_Add(msg + 8, key[2]);
    sum += NH_Add(msg + 4, key[1]) * NH_Add(msg + 12, key[3]);
    key += kNHMessageUnit / sizeof(key[0]);
    msg += kNHMessageUnit;
    msglen -= kNHMessageUnit;
  }
  return sum;
}

// NH ε-almost-universal hash function
static void NH(const uint32_t *key, const uint8_t *msg, int msglen,
               uint8_t result[kNHHashSize]) {
  int i;

  for (i = 0; i < kNHHashSize; i += sizeof(__le64)) {
    put_unaligned_le64(NH_Pass(key, msg, msglen), &result[i]);
    key += kNHMessageUnit / sizeof(key[0]);
  }
}

constexpr int kAdiantumHashKeySize = (2 * kPoly1305KeySize) + kNHKeySize;

// Adiantum's ε-almost-∆-universal hash function
static void AdiantumHash(const uint8_t key[kAdiantumHashKeySize],
                         const uint8_t iv[kAdiantumIVSize], const uint8_t *msg,
                         int msglen, uint8_t result[kPoly1305HashSize]) {
  const uint8_t *header_poly_key = key;
  const uint8_t *msg_poly_key = header_poly_key + kPoly1305KeySize;
  const uint8_t *nh_key = msg_poly_key + kPoly1305KeySize;
  uint32_t nh_key_words[kNHKeyWords];
  uint8_t header[kPoly1305BlockSize + kAdiantumIVSize];
  const int num_nh_blocks = (msglen + kNHBlockSize - 1) / kNHBlockSize;
  std::unique_ptr<uint8_t> nh_hashes(new uint8_t[num_nh_blocks * kNHHashSize]);
  const int padded_msglen = round_up(msglen, kNHMessageUnit);
  std::unique_ptr<uint8_t> padded_msg(new uint8_t[padded_msglen]);
  uint8_t hash1[kPoly1305HashSize], hash2[kPoly1305HashSize];
  int i;

  for (i = 0; i < kNHKeyWords; i++)
    nh_key_words[i] = get_unaligned_le32(&nh_key[i * sizeof(uint32_t)]);

  // Hash tweak and message length with first Poly1305 key
  put_unaligned_le64(static_cast<uint64_t>(msglen) * 8, header);
  put_unaligned_le64(0, &header[sizeof(__le64)]);
  memcpy(&header[kPoly1305BlockSize], iv, kAdiantumIVSize);
  Poly1305(header_poly_key, header, sizeof(header), hash1);

  // Hash NH hashes of message blocks using second Poly1305 key
  // (using a super naive way of handling the padding)
  memcpy(padded_msg.get(), msg, msglen);
  memset(&padded_msg.get()[msglen], 0, padded_msglen - msglen);
  for (i = 0; i < num_nh_blocks; i++) {
    NH(nh_key_words, &padded_msg.get()[i * kNHBlockSize],
       std::min(kNHBlockSize, padded_msglen - (i * kNHBlockSize)),
       &nh_hashes.get()[i * kNHHashSize]);
  }
  Poly1305(msg_poly_key, nh_hashes.get(), num_nh_blocks * kNHHashSize, hash2);

  // Add the two hashes together to get the final hash
  le128_add(result, hash1, hash2);
}

bool AdiantumCipher::DoEncrypt(const uint8_t key[kAdiantumKeySize],
                               const uint8_t iv[kAdiantumIVSize],
                               const uint8_t *src, uint8_t *dst,
                               int nbytes) const {
  uint8_t rbuf[kXChaChaNonceSize] = {1};
  uint8_t hash[kPoly1305HashSize];

  static_assert(kAdiantumKeySize == kXChaChaKeySize);
  static_assert(kPoly1305HashSize == kAesBlockSize);
  static_assert(kXChaChaNonceSize > kAesBlockSize);

  if (nbytes < kAesBlockSize) {
    ADD_FAILURE() << "Bad input size";
    return false;
  }

  // Derive subkeys
  uint8_t subkeys[kAes256KeySize + kAdiantumHashKeySize] = {0};
  XChaCha12(key, rbuf, subkeys, subkeys, sizeof(subkeys));

  AES_KEY aes_key;
  if (AES_set_encrypt_key(subkeys, kAes256KeySize * 8, &aes_key) != 0) {
    ADD_FAILURE() << "Failed to set AES key";
    return false;
  }

  // Hash left part and add to right part
  const int bulk_len = nbytes - kAesBlockSize;
  AdiantumHash(&subkeys[kAes256KeySize], iv, src, bulk_len, hash);
  le128_add(rbuf, &src[bulk_len], hash);

  // Encrypt right part with block cipher
  AES_encrypt(rbuf, rbuf, &aes_key);

  // Encrypt left part with stream cipher, using the computed nonce
  rbuf[kAesBlockSize] = 1;
  XChaCha12(key, rbuf, src, dst, bulk_len);

  // Finalize right part by subtracting hash of left part
  AdiantumHash(&subkeys[kAes256KeySize], iv, dst, bulk_len, hash);
  le128_sub(&dst[bulk_len], rbuf, hash);
  return true;
}

}  // namespace kernel
}  // namespace android
