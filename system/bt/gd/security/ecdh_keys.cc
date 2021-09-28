/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "security/ecdh_keys.h"

/**********************************************************************************************************************
 TODO: We should have random number management in separate file, and we
       should honour all the random number requirements from the spec!!
**********************************************************************************************************************/
#include <chrono>
#include <cstdlib>

#include "security/ecc/p_256_ecc_pp.h"

namespace {

static bool srand_initiated = false;

template <size_t SIZE>
static std::array<uint8_t, SIZE> GenerateRandom() {
  if (!srand_initiated) {
    srand_initiated = true;
    // TODO:  We need a proper  random number generator here.
    // use current time as seed for random generator
    std::srand(std::time(nullptr));
  }

  std::array<uint8_t, SIZE> r;
  for (size_t i = 0; i < SIZE; i++) r[i] = std::rand();
  return r;
}
}  // namespace
/*********************************************************************************************************************/

namespace bluetooth {
namespace security {

std::pair<std::array<uint8_t, 32>, EcdhPublicKey> GenerateECDHKeyPair() {
  std::array<uint8_t, 32> private_key = GenerateRandom<32>();
  std::array<uint8_t, 32> private_key_copy = private_key;
  ecc::Point public_key;

  ECC_PointMult(&public_key, &(ecc::curve_p256.G), (uint32_t*)private_key_copy.data());

  EcdhPublicKey pk;
  memcpy(pk.x.data(), public_key.x, 32);
  memcpy(pk.y.data(), public_key.y, 32);

  /* private_key, public key pair */
  return std::make_pair<std::array<uint8_t, 32>, EcdhPublicKey>(std::move(private_key), std::move(pk));
}

bool ValidateECDHPoint(EcdhPublicKey pk) {
  ecc::Point public_key;
  memcpy(public_key.x, pk.x.data(), 32);
  memcpy(public_key.y, pk.y.data(), 32);
  memset(public_key.z, 0, 32);
  return ECC_ValidatePoint(public_key);
}

std::array<uint8_t, 32> ComputeDHKey(std::array<uint8_t, 32> my_private_key, EcdhPublicKey remote_public_key) {
  ecc::Point peer_publ_key, new_publ_key;
  uint32_t private_key[8];
  memcpy(private_key, my_private_key.data(), 32);
  memcpy(peer_publ_key.x, remote_public_key.x.data(), 32);
  memcpy(peer_publ_key.y, remote_public_key.y.data(), 32);
  memset(peer_publ_key.z, 0, 32);
  peer_publ_key.z[0] = 1;

  ECC_PointMult(&new_publ_key, &peer_publ_key, (uint32_t*)private_key);

  std::array<uint8_t, 32> dhkey;
  memcpy(dhkey.data(), new_publ_key.x, 32);
  return dhkey;
}

}  // namespace security
}  // namespace bluetooth
