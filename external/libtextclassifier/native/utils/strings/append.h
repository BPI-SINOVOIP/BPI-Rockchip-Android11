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

#ifndef LIBTEXTCLASSIFIER_UTILS_STRINGS_APPEND_H_
#define LIBTEXTCLASSIFIER_UTILS_STRINGS_APPEND_H_

#include <string>
#include <vector>

namespace libtextclassifier3 {
namespace strings {

// Append vsnprintf to strp. If bufsize hint is > 0 it is
// used. Otherwise we compute the required bufsize (which is somewhat
// expensive).
void SStringAppendV(std::string *strp, int bufsize, const char *fmt,
                           va_list arglist);

void SStringAppendF(std::string *strp, int bufsize, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

std::string StringPrintf(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

std::string JoinStrings(const char *delim, const std::vector<std::string> &vec);

}  // namespace strings
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_STRINGS_APPEND_H_
