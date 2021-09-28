/******************************************************************************
 *
 *  Copyright 2006-2015 Broadcom Corporation
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

/******************************************************************************
 *
 *  This file contains simple pairing algorithms using Elliptic Curve
 *Cryptography for private public key
 *
 ******************************************************************************/

#pragma once

#include "security/ecc/multprecision.h"

namespace bluetooth {
namespace security {
namespace ecc {

struct Point {
  uint32_t x[KEY_LENGTH_DWORDS_P256];
  uint32_t y[KEY_LENGTH_DWORDS_P256];
  uint32_t z[KEY_LENGTH_DWORDS_P256];
};

struct elliptic_curve_t {
  // curve's coefficients
  uint32_t a[KEY_LENGTH_DWORDS_P256];
  uint32_t b[KEY_LENGTH_DWORDS_P256];

  // prime modulus
  uint32_t p[KEY_LENGTH_DWORDS_P256];

  // Omega, p = 2^m -omega
  uint32_t omega[KEY_LENGTH_DWORDS_P256];

  // base point, a point on E of order r
  Point G;
};

// P-256 elliptic curve, as per BT Spec 5.1 Vol 2, Part H 7.6
static constexpr elliptic_curve_t curve_p256{
    .a = {0},
    .b = {0x27d2604b, 0x3bce3c3e, 0xcc53b0f6, 0x651d06b0, 0x769886bc, 0xb3ebbd55, 0xaa3a93e7, 0x5ac635d8},
    .p = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0, 0x0, 0x0, 0x00000001, 0xFFFFFFFF},
    .omega = {0},

    .G = {.x = {0xd898c296, 0xf4a13945, 0x2deb33a0, 0x77037d81, 0x63a440f2, 0xf8bce6e5, 0xe12c4247, 0x6b17d1f2},
          .y = {0x37bf51f5, 0xcbb64068, 0x6b315ece, 0x2bce3357, 0x7c0f9e16, 0x8ee7eb4a, 0xfe1a7f9b, 0x4fe342e2},
          .z = {0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}},
};

/* This function checks that point is on the elliptic curve*/
bool ECC_ValidatePoint(const Point& point);

void ECC_PointMult_Bin_NAF(Point* q, const Point* p, uint32_t* n);

#define ECC_PointMult(q, p, n) ECC_PointMult_Bin_NAF(q, p, n)

}  // namespace ecc
}  // namespace security
}  // namespace bluetooth
