/*
 * Copyright (C) 2019 The Android Open Source Project
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

#pragma once

// generated code prints std::string* which is disallowed
// by android-base/logging.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuser-defined-warnings"
#include "hidl-gen_y.h"
#pragma clang diagnostic pop

namespace android {
struct Location;
struct AST;
}  // namespace android

android::Location convertYYLoc(const yy::parser::location_type& loc, const android::AST* ast);
