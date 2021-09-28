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

// Common definitions used in the grammar system.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TYPES_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TYPES_H_

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"

namespace libtextclassifier3::grammar {

// A nonterminal identifier.
typedef uint32 Nonterm;

// This special Nonterm value is never used as a real Nonterm, but used as
// a standin of an unassigned or unspecified nonterminal.
const Nonterm kUnassignedNonterm = 0;

typedef int32 CallbackId;  // `kNoCallback` is reserved for "no callback"
enum class DefaultCallback : CallbackId {
  kSetType = -1,
  kAssertion = -2,
  kMapping = -3,
  kExclusion = -4,
  kRootRule = 1,
};

// Special CallbackId indicating that there's no callback associated with a
// rule.
const int32 kNoCallback = 0;

// A pair of nonterminals.
using TwoNonterms = std::pair<Nonterm, Nonterm>;

static uint32 hash_int32(uint32 a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

struct BinaryRuleHasher {
  inline uint64 operator()(const TwoNonterms& x) const {
    // the hash_int32 maps a int to a random int, then treat two ints as a
    // rational number, then use cantor pairing function to calculate the
    // order of rational number.
    uint32 t1 = hash_int32(x.first);
    uint32 t2 = hash_int32(x.second);
    uint64 t = t1 + t2;
    t *= (t + 1);
    t >>= 1;
    return t + t1;
  }
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TYPES_H_
