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

#ifndef LIBTEXTCLASSIFIER_UTILS_STRINGS_SUBSTITUTE_H_
#define LIBTEXTCLASSIFIER_UTILS_STRINGS_SUBSTITUTE_H_

#include <string>
#include <vector>

#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {
namespace strings {

// Formats a string with argument-binding.
// Uses a format string that contains positional identifiers indicated by a
// dollar sign ($) and a signle digit positional id to indicate which
// substitution arguments to use at that location within the format string.
// A '$$' sequence in the format string means output a literal '$' character.
// Returns whether the substitution was successful.
bool Substitute(const StringPiece format, const std::vector<StringPiece>& args,
                std::string* output);

std::string Substitute(const StringPiece format,
                       const std::vector<StringPiece>& args);

}  // namespace strings
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_STRINGS_SUBSTITUTE_H_
