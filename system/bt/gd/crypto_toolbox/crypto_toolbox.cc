/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "crypto_toolbox/crypto_toolbox.h"
#include "crypto_toolbox/aes.h"

#include <endian.h>
#include <algorithm>

namespace bluetooth {
namespace crypto_toolbox {

constexpr int OCTET32_LEN = 32;

Octet16 h6(const Octet16& w, std::array<uint8_t, 4> keyid) {
  return aes_cmac(w, keyid.data(), keyid.size());
}

Octet16 h7(const Octet16& salt, const Octet16& w) {
  return aes_cmac(salt, w.data(), w.size());
}

Octet16 f4(uint8_t* u, uint8_t* v, const Octet16& x, uint8_t z) {
  constexpr size_t msg_len = OCTET32_LEN /* U size */ + OCTET32_LEN /* V size */ + 1 /* Z size */;

  // DVLOG(2) << "U=" << HexEncode(u, OCTET32_LEN) << ", V=" << HexEncode(v, OCTET32_LEN)
  //          << ", X=" << HexEncode(x.data(), x.size()) << ", Z=" << std::hex << +z;

  std::array<uint8_t, msg_len> msg;
  auto it = msg.begin();
  it = std::copy(&z, &z + 1, it);
  it = std::copy(v, v + OCTET32_LEN, it);
  it = std::copy(u, u + OCTET32_LEN, it);
  return aes_cmac(x, msg.data(), msg.size());
}

/** helper for f5 */
static Octet16 calculate_mac_key_or_ltk(const Octet16& t, uint8_t counter, uint8_t* key_id, const Octet16& n1,
                                        const Octet16& n2, uint8_t* a1, uint8_t* a2, uint8_t* length) {
  constexpr size_t msg_len = 1 /* Counter size */ + 4 /* keyID size */ + OCTET16_LEN /* N1 size */ +
                             OCTET16_LEN /* N2 size */ + 7 /* A1 size*/ + 7 /* A2 size*/ + 2 /* Length size */;

  std::array<uint8_t, msg_len> msg;
  auto it = msg.begin();
  it = std::copy(length, length + 2, it);
  it = std::copy(a2, a2 + 7, it);
  it = std::copy(a1, a1 + 7, it);
  it = std::copy(n2.begin(), n2.end(), it);
  it = std::copy(n1.begin(), n1.end(), it);
  it = std::copy(key_id, key_id + 4, it);
  it = std::copy(&counter, &counter + 1, it);

  return aes_cmac(t, msg.data(), msg.size());
}

void f5(uint8_t* w, const Octet16& n1, const Octet16& n2, uint8_t* a1, uint8_t* a2, Octet16* mac_key, Octet16* ltk) {
  // DVLOG(2) << __func__ << "W=" << HexEncode(w, OCTET32_LEN) << ", N1=" << HexEncode(n1.data(), n1.size())
  //          << ", N2=" << HexEncode(n2.data(), n2.size()) << ", A1=" << HexEncode(a1, 7) << ", A2=" << HexEncode(a2,
  //          7);

  const Octet16 salt{0xBE, 0x83, 0x60, 0x5A, 0xDB, 0x0B, 0x37, 0x60, 0x38, 0xA5, 0xF5, 0xAA, 0x91, 0x83, 0x88, 0x6C};
  Octet16 t = aes_cmac(salt, w, OCTET32_LEN);

  // DVLOG(2) << "T=" << HexEncode(t.data(), t.size());

  uint8_t key_id[4] = {0x65, 0x6c, 0x74, 0x62}; /* 0x62746c65 */
  uint8_t length[2] = {0x00, 0x01};             /* 0x0100 */

  *mac_key = calculate_mac_key_or_ltk(t, 0, key_id, n1, n2, a1, a2, length);

  *ltk = calculate_mac_key_or_ltk(t, 1, key_id, n1, n2, a1, a2, length);

  // DVLOG(2) << "mac_key=" << HexEncode(mac_key->data(), mac_key->size());
  // DVLOG(2) << "ltk=" << HexEncode(ltk->data(), ltk->size());
}

Octet16 f6(const Octet16& w, const Octet16& n1, const Octet16& n2, const Octet16& r, uint8_t* iocap, uint8_t* a1,
           uint8_t* a2) {
  const uint8_t msg_len = OCTET16_LEN /* N1 size */ + OCTET16_LEN /* N2 size */ + OCTET16_LEN /* R size */ +
                          3 /* IOcap size */ + 7 /* A1 size*/ + 7 /* A2 size*/;

  // DVLOG(2) << __func__ << "W=" << HexEncode(w.data(), w.size()) << ", N1=" << HexEncode(n1.data(), n1.size())
  //          << ", N2=" << HexEncode(n2.data(), n2.size()) << ", R=" << HexEncode(r.data(), r.size())
  //          << ", IOcap=" << HexEncode(iocap, 3) << ", A1=" << HexEncode(a1, 7) << ", A2=" << HexEncode(a2, 7);

  std::array<uint8_t, msg_len> msg;
  auto it = msg.begin();
  it = std::copy(a2, a2 + 7, it);
  it = std::copy(a1, a1 + 7, it);
  it = std::copy(iocap, iocap + 3, it);
  it = std::copy(r.begin(), r.end(), it);
  it = std::copy(n2.begin(), n2.end(), it);
  it = std::copy(n1.begin(), n1.end(), it);

  return aes_cmac(w, msg.data(), msg.size());
}

uint32_t g2(uint8_t* u, uint8_t* v, const Octet16& x, const Octet16& y) {
  constexpr size_t msg_len = OCTET32_LEN /* U size */ + OCTET32_LEN /* V size */
                             + OCTET16_LEN /* Y size */;

  // DVLOG(2) << __func__ << "U=" << HexEncode(u, OCTET32_LEN) << ", V=" << HexEncode(v, OCTET32_LEN)
  //          << ", X=" << HexEncode(x.data(), x.size()) << ", Y=" << HexEncode(y.data(), y.size());

  std::array<uint8_t, msg_len> msg;
  auto it = msg.begin();
  it = std::copy(y.begin(), y.end(), it);
  it = std::copy(v, v + OCTET32_LEN, it);
  it = std::copy(u, u + OCTET32_LEN, it);

  Octet16 cmac = aes_cmac(x, msg.data(), msg.size());

  /* vres = cmac mod 2**32 mod 10**6 */
  return le32toh(*(uint32_t*)cmac.data()) % 1000000;
}

Octet16 ltk_to_link_key(const Octet16& ltk, bool use_h7) {
  Octet16 ilk; /* intermidiate link key */
  if (use_h7) {
    constexpr Octet16 salt{0x31, 0x70, 0x6D, 0x74, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ilk = h7(salt, ltk);
  } else {
    /* "tmp1" mapping to extended ASCII, little endian*/
    constexpr std::array<uint8_t, 4> keyID_tmp1 = {0x31, 0x70, 0x6D, 0x74};
    ilk = h6(ltk, keyID_tmp1);
  }

  /* "lebr" mapping to extended ASCII, little endian */
  constexpr std::array<uint8_t, 4> keyID_lebr = {0x72, 0x62, 0x65, 0x6c};
  return h6(ilk, keyID_lebr);
}

Octet16 link_key_to_ltk(const Octet16& link_key, bool use_h7) {
  Octet16 iltk; /* intermidiate long term key */
  if (use_h7) {
    constexpr Octet16 salt{0x32, 0x70, 0x6D, 0x74, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    iltk = h7(salt, link_key);
  } else {
    /* "tmp2" mapping to extended ASCII, little endian */
    constexpr std::array<uint8_t, 4> keyID_tmp2 = {0x32, 0x70, 0x6D, 0x74};
    iltk = h6(link_key, keyID_tmp2);
  }

  /* "brle" mapping to extended ASCII, little endian */
  constexpr std::array<uint8_t, 4> keyID_brle = {0x65, 0x6c, 0x72, 0x62};
  return h6(iltk, keyID_brle);
}

Octet16 c1(const Octet16& k, const Octet16& r, const uint8_t* preq, const uint8_t* pres, const uint8_t iat,
           const uint8_t* ia, const uint8_t rat, const uint8_t* ra) {
  Octet16 p1;
  auto it = p1.begin();
  it = std::copy(&iat, &iat + 1, it);
  it = std::copy(&rat, &rat + 1, it);
  it = std::copy(preq, preq + 7, it);
  it = std::copy(pres, pres + 7, it);

  for (uint8_t i = 0; i < OCTET16_LEN; i++) {
    p1[i] = r[i] ^ p1[i];
  }

  Octet16 p1bis = aes_128(k, p1);

  std::array<uint8_t, 4> padding{0};
  Octet16 p2;
  it = p2.begin();
  it = std::copy(ra, ra + 6, it);
  it = std::copy(ia, ia + 6, it);
  it = std::copy(padding.begin(), padding.end(), it);

  for (uint8_t i = 0; i < OCTET16_LEN; i++) {
    p2[i] = p1bis[i] ^ p2[i];
  }

  return aes_128(k, p2);
}

Octet16 s1(const Octet16& k, const Octet16& r1, const Octet16& r2) {
  Octet16 text{0};
  constexpr uint8_t BT_OCTET8_LEN = 8;
  memcpy(text.data(), r1.data(), BT_OCTET8_LEN);
  memcpy(text.data() + BT_OCTET8_LEN, r2.data(), BT_OCTET8_LEN);

  return aes_128(k, text);
}

}  // namespace crypto_toolbox
}  // namespace bluetooth