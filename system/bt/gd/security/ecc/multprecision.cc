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

/*******************************************************************************
 *
 *  This file contains simple pairing algorithms
 *
 ******************************************************************************/

#include "security/ecc/multprecision.h"
#include <string.h>

namespace bluetooth {
namespace security {
namespace ecc {

#define DWORD_BITS 32
#define DWORD_BYTES 4
#define DWORD_BITS_SHIFT 5

void multiprecision_init(uint32_t* c) {
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) c[i] = 0;
}

void multiprecision_copy(uint32_t* c, const uint32_t* a) {
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) c[i] = a[i];
}

int multiprecision_compare(const uint32_t* a, const uint32_t* b) {
  for (int i = KEY_LENGTH_DWORDS_P256 - 1; i >= 0; i--) {
    if (a[i] > b[i]) return 1;
    if (a[i] < b[i]) return -1;
  }
  return 0;
}

int multiprecision_iszero(const uint32_t* a) {
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++)
    if (a[i]) return 0;

  return 1;
}

uint32_t multiprecision_dword_bits(uint32_t a) {
  uint32_t i;
  for (i = 0; i < DWORD_BITS; i++, a >>= 1)
    if (a == 0) break;

  return i;
}

uint32_t multiprecision_most_signdwords(const uint32_t* a) {
  int i;
  for (i = KEY_LENGTH_DWORDS_P256 - 1; i >= 0; i--)
    if (a[i]) break;
  return (i + 1);
}

uint32_t multiprecision_most_signbits(const uint32_t* a) {
  int aMostSignDWORDs;

  aMostSignDWORDs = multiprecision_most_signdwords(a);
  if (aMostSignDWORDs == 0) return 0;

  return (((aMostSignDWORDs - 1) << DWORD_BITS_SHIFT) + multiprecision_dword_bits(a[aMostSignDWORDs - 1]));
}

uint32_t multiprecision_add(uint32_t* c, const uint32_t* a, const uint32_t* b) {
  uint32_t carrier;
  uint32_t temp;

  carrier = 0;
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) {
    temp = a[i] + carrier;
    carrier = (temp < carrier);
    temp += b[i];
    carrier |= (temp < b[i]);
    c[i] = temp;
  }

  return carrier;
}

// c=a-b
uint32_t multiprecision_sub(uint32_t* c, const uint32_t* a, const uint32_t* b) {
  uint32_t borrow;
  uint32_t temp;

  borrow = 0;
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) {
    temp = a[i] - borrow;
    borrow = (temp > a[i]);
    c[i] = temp - b[i];
    borrow |= (c[i] > temp);
  }

  return borrow;
}

// c = a << 1
void multiprecision_lshift_mod(uint32_t* c, const uint32_t* a, const uint32_t* modp) {
  uint32_t carrier = multiprecision_lshift(c, a);
  if (carrier) {
    multiprecision_sub(c, c, modp);
  } else if (multiprecision_compare(c, modp) >= 0) {
    multiprecision_sub(c, c, modp);
  }
}

// c=a>>1
void multiprecision_rshift(uint32_t* c, const uint32_t* a) {
  int j;
  uint32_t b = 1;

  j = DWORD_BITS - b;

  uint32_t carrier = 0;
  uint32_t temp;
  for (int i = KEY_LENGTH_DWORDS_P256 - 1; i >= 0; i--) {
    temp = a[i];  // in case of c==a
    c[i] = (temp >> b) | carrier;
    carrier = temp << j;
  }
}

// Curve specific optimization when p is a pseudo-Mersenns prime,
// p=2^(KEY_LENGTH_BITS)-omega
void multiprecision_mersenns_mult_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp) {
  uint32_t cc[2 * KEY_LENGTH_DWORDS_P256];

  multiprecision_mult(cc, a, b);
  multiprecision_fast_mod_P256(c, cc, modp);
}

// Curve specific optimization when p is a pseudo-Mersenns prime
void multiprecision_mersenns_squa_mod(uint32_t* c, const uint32_t* a, const uint32_t* modp) {
  multiprecision_mersenns_mult_mod(c, a, a, modp);
}

// c=(a+b) mod p, b<p, a<p
void multiprecision_add_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp) {
  uint32_t carrier = multiprecision_add(c, a, b);
  if (carrier) {
    multiprecision_sub(c, c, modp);
  } else if (multiprecision_compare(c, modp) >= 0) {
    multiprecision_sub(c, c, modp);
  }
}

// c=(a-b) mod p, a<p, b<p
void multiprecision_sub_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp) {
  uint32_t borrow;

  borrow = multiprecision_sub(c, a, b);
  if (borrow) multiprecision_add(c, c, modp);
}

// c=a<<b, b<DWORD_BITS, c has a buffer size of Numuint32_ts+1
uint32_t multiprecision_lshift(uint32_t* c, const uint32_t* a) {
  int j;
  uint32_t b = 1;
  j = DWORD_BITS - b;

  uint32_t carrier = 0;
  uint32_t temp;

  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) {
    temp = a[i];  // in case c==a
    c[i] = (temp << b) | carrier;
    carrier = temp >> j;
  }

  return carrier;
}

// c=a*b; c must have a buffer of 2*Key_LENGTH_uint32_tS, c != a != b
void multiprecision_mult(uint32_t* c, const uint32_t* a, const uint32_t* b) {
  uint32_t W;
  uint32_t U;
  uint32_t V;

  U = V = W = 0;
  multiprecision_init(c);

  // assume little endian right now
  for (uint32_t i = 0; i < KEY_LENGTH_DWORDS_P256; i++) {
    U = 0;
    for (uint32_t j = 0; j < KEY_LENGTH_DWORDS_P256; j++) {
      uint64_t result;
      result = ((uint64_t)a[i]) * ((uint64_t)b[j]);
      W = result >> 32;
      V = a[i] * b[j];
      V = V + U;
      U = (V < U);
      U += W;
      V = V + c[i + j];
      U += (V < c[i + j]);
      c[i + j] = V;
    }
    c[i + KEY_LENGTH_DWORDS_P256] = U;
  }
}

void multiprecision_fast_mod_P256(uint32_t* c, const uint32_t* a, const uint32_t* modp) {
  uint32_t A;
  uint32_t B;
  uint32_t C;
  uint32_t D;
  uint32_t E;
  uint32_t F;
  uint32_t G;
  uint8_t UA;
  uint8_t UB;
  uint8_t UC;
  uint8_t UD;
  uint8_t UE;
  uint8_t UF;
  uint8_t UG;
  uint32_t U;

  // C = a[13] + a[14] + a[15];
  C = a[13];
  C += a[14];
  UC = (C < a[14]);
  C += a[15];
  UC += (C < a[15]);

  // E = a[8] + a[9];
  E = a[8];
  E += a[9];
  UE = (E < a[9]);

  // F = a[9] + a[10];
  F = a[9];
  F += a[10];
  UF = (F < a[10]);

  // G = a[10] + a[11]
  G = a[10];
  G += a[11];
  UG = (G < a[11]);

  // B = a[12] + a[13] + a[14] + a[15] == C + a[12]
  B = C;
  UB = UC;
  B += a[12];
  UB += (B < a[12]);

  // A = a[11] + a[12] + a[13] + a[14] == B + a[11] - a[15]
  A = B;
  UA = UB;
  A += a[11];
  UA += (A < a[11]);
  UA -= (A < a[15]);
  A -= a[15];

  // D = a[10] + a[11] + a[12] + a[13] == A + a[10] - a[14]
  D = A;
  UD = UA;
  D += a[10];
  UD += (D < a[10]);
  UD -= (D < a[14]);
  D -= a[14];

  c[0] = a[0];
  c[0] += E;
  U = (c[0] < E);
  U += UE;
  U -= (c[0] < A);
  U -= UA;
  c[0] -= A;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[1] < UU);
    c[1] = a[1] - UU;
  } else {
    c[1] = a[1] + U;
    U = (c[1] < a[1]);
  }

  c[1] += F;
  U += (c[1] < F);
  U += UF;
  U -= (c[1] < B);
  U -= UB;
  c[1] -= B;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[2] < UU);
    c[2] = a[2] - UU;
  } else {
    c[2] = a[2] + U;
    U = (c[2] < a[2]);
  }

  c[2] += G;
  U += (c[2] < G);
  U += UG;
  U -= (c[2] < C);
  U -= UC;
  c[2] -= C;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[3] < UU);
    c[3] = a[3] - UU;
  } else {
    c[3] = a[3] + U;
    U = (c[3] < a[3]);
  }

  c[3] += A;
  U += (c[3] < A);
  U += UA;
  c[3] += a[11];
  U += (c[3] < a[11]);
  c[3] += a[12];
  U += (c[3] < a[12]);
  U -= (c[3] < a[14]);
  c[3] -= a[14];
  U -= (c[3] < a[15]);
  c[3] -= a[15];
  U -= (c[3] < E);
  U -= UE;
  c[3] -= E;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[4] < UU);
    c[4] = a[4] - UU;
  } else {
    c[4] = a[4] + U;
    U = (c[4] < a[4]);
  }

  c[4] += B;
  U += (c[4] < B);
  U += UB;
  U -= (c[4] < a[15]);
  c[4] -= a[15];
  c[4] += a[12];
  U += (c[4] < a[12]);
  c[4] += a[13];
  U += (c[4] < a[13]);
  U -= (c[4] < F);
  U -= UF;
  c[4] -= F;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[5] < UU);
    c[5] = a[5] - UU;
  } else {
    c[5] = a[5] + U;
    U = (c[5] < a[5]);
  }

  c[5] += C;
  U += (c[5] < C);
  U += UC;
  c[5] += a[13];
  U += (c[5] < a[13]);
  c[5] += a[14];
  U += (c[5] < a[14]);
  U -= (c[5] < G);
  U -= UG;
  c[5] -= G;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[6] < UU);
    c[6] = a[6] - UU;
  } else {
    c[6] = a[6] + U;
    U = (c[6] < a[6]);
  }

  c[6] += C;
  U += (c[6] < C);
  U += UC;
  c[6] += a[14];
  U += (c[6] < a[14]);
  c[6] += a[14];
  U += (c[6] < a[14]);
  c[6] += a[15];
  U += (c[6] < a[15]);
  U -= (c[6] < E);
  U -= UE;
  c[6] -= E;

  if (U & 0x80000000) {
    uint32_t UU;
    UU = 0 - U;
    U = (a[7] < UU);
    c[7] = a[7] - UU;
  } else {
    c[7] = a[7] + U;
    U = (c[7] < a[7]);
  }

  c[7] += a[15];
  U += (c[7] < a[15]);
  c[7] += a[15];
  U += (c[7] < a[15]);
  c[7] += a[15];
  U += (c[7] < a[15]);
  c[7] += a[8];
  U += (c[7] < a[8]);
  U -= (c[7] < D);
  U -= UD;
  c[7] -= D;

  if (U & 0x80000000) {
    while (U) {
      multiprecision_add(c, c, modp);
      U++;
    }
  } else if (U) {
    while (U) {
      multiprecision_sub(c, c, modp);
      U--;
    }
  }

  if (multiprecision_compare(c, modp) >= 0) multiprecision_sub(c, c, modp);
}

void multiprecision_inv_mod(uint32_t* aminus, uint32_t* u, const uint32_t* modp) {
  uint32_t v[KEY_LENGTH_DWORDS_P256];
  uint32_t A[KEY_LENGTH_DWORDS_P256 + 1];
  uint32_t C[KEY_LENGTH_DWORDS_P256 + 1];

  multiprecision_copy(v, modp);
  multiprecision_init(A);
  multiprecision_init(C);
  A[0] = 1;

  while (!multiprecision_iszero(u)) {
    while (!(u[0] & 0x01))  // u is even
    {
      multiprecision_rshift(u, u);
      if (!(A[0] & 0x01))  // A is even
        multiprecision_rshift(A, A);
      else {
        A[KEY_LENGTH_DWORDS_P256] = multiprecision_add(A, A, modp);  // A =A+p
        multiprecision_rshift(A, A);
        A[KEY_LENGTH_DWORDS_P256 - 1] |= (A[KEY_LENGTH_DWORDS_P256] << 31);
      }
    }

    while (!(v[0] & 0x01))  // v is even
    {
      multiprecision_rshift(v, v);
      if (!(C[0] & 0x01))  // C is even
      {
        multiprecision_rshift(C, C);
      } else {
        C[KEY_LENGTH_DWORDS_P256] = multiprecision_add(C, C, modp);  // C =C+p
        multiprecision_rshift(C, C);
        C[KEY_LENGTH_DWORDS_P256 - 1] |= (C[KEY_LENGTH_DWORDS_P256] << 31);
      }
    }

    if (multiprecision_compare(u, v) >= 0) {
      multiprecision_sub(u, u, v);
      multiprecision_sub_mod(A, A, C, modp);
    } else {
      multiprecision_sub(v, v, u);
      multiprecision_sub_mod(C, C, A, modp);
    }
  }

  if (multiprecision_compare(C, modp) >= 0)
    multiprecision_sub(aminus, C, modp);
  else
    multiprecision_copy(aminus, C);
}

}  // namespace ecc
}  // namespace security
}  // namespace bluetooth