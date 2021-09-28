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
#pragma once

#include <array>

namespace bluetooth {
namespace security {

struct EcdhPublicKey {
  std::array<uint8_t, 32> x;
  std::array<uint8_t, 32> y;
};

/* this generates private and public Eliptic Curve Diffie Helman keys */
std::pair<std::array<uint8_t, 32>, EcdhPublicKey> GenerateECDHKeyPair();

/* This function validates that the given public key (point) lays on the special
 * Bluetooth curve */
bool ValidateECDHPoint(EcdhPublicKey pk);

std::array<uint8_t, 32> ComputeDHKey(std::array<uint8_t, 32> my_private_key, EcdhPublicKey remote_public_key);

}  // namespace security
}  // namespace bluetooth
