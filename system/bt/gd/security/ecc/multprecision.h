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
#pragma once

#include <cstdint>

namespace bluetooth {
namespace security {
namespace ecc {

#define KEY_LENGTH_DWORDS_P256 8
/* Arithmetic Operations*/
int multiprecision_compare(const uint32_t* a, const uint32_t* b);
int multiprecision_iszero(const uint32_t* a);
void multiprecision_init(uint32_t* c);
void multiprecision_copy(uint32_t* c, const uint32_t* a);
uint32_t multiprecision_dword_bits(uint32_t a);
uint32_t multiprecision_most_signdwords(const uint32_t* a);
uint32_t multiprecision_most_signbits(const uint32_t* a);
void multiprecision_inv_mod(uint32_t* aminus, uint32_t* a, const uint32_t* modp);
uint32_t multiprecision_add(uint32_t* c, const uint32_t* a, const uint32_t* b);  // c=a+b
void multiprecision_add_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp);
uint32_t multiprecision_sub(uint32_t* c, const uint32_t* a, const uint32_t* b);  // c=a-b
void multiprecision_sub_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp);
void multiprecision_rshift(uint32_t* c, const uint32_t* a);                            // c=a>>1, return carrier
void multiprecision_lshift_mod(uint32_t* c, const uint32_t* a, const uint32_t* modp);  // c=a<<b, return carrier
uint32_t multiprecision_lshift(uint32_t* c, const uint32_t* a);
void multiprecision_mult(uint32_t* c, const uint32_t* a, const uint32_t* b);  // c=a*b
void multiprecision_mersenns_mult_mod(uint32_t* c, const uint32_t* a, const uint32_t* b, const uint32_t* modp);
void multiprecision_mersenns_squa_mod(uint32_t* c, const uint32_t* a, const uint32_t* modp);
void multiprecision_fast_mod_P256(uint32_t* c, const uint32_t* a, const uint32_t* modp);

}  // namespace ecc
}  // namespace security
}  // namespace bluetooth