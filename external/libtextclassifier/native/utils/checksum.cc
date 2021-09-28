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

#include "utils/checksum.h"
#include "utils/strings/numbers.h"

namespace libtextclassifier3 {

bool VerifyLuhnChecksum(const std::string& input, bool ignore_whitespace) {
  int sum = 0;
  int num_digits = 0;
  bool is_odd = true;

  // http://en.wikipedia.org/wiki/Luhn_algorithm
  static const int kPrecomputedSumsOfDoubledDigits[] = {0, 2, 4, 6, 8,
                                                        1, 3, 5, 7, 9};
  for (int i = input.size() - 1; i >= 0; i--) {
    const char c = input[i];
    if (ignore_whitespace && c == ' ') {
      continue;
    }
    if (!isdigit(c)) {
      return false;
    }
    ++num_digits;
    const int digit = c - '0';
    if (is_odd) {
      sum += digit;
    } else {
      sum += kPrecomputedSumsOfDoubledDigits[digit];
    }
    is_odd = !is_odd;
  }
  return (num_digits > 1 && sum % 10 == 0);
}

}  // namespace libtextclassifier3
