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

#include <json/json.h>

#include <string>
#include <vector>

#include "Location.h"

namespace android {
struct AST;

enum LintLevel { WARNING = 0, ERROR = 1 };

struct Lint {
    Lint(LintLevel level, const Location& location, const std::string& message = "");

    Lint(Lint&& other) = default;
    Lint& operator=(Lint&& other) = default;

    Lint(const Lint&) = delete;
    Lint& operator=(const Lint&) = delete;

    Lint&& operator<<(const std::string& message);

    LintLevel getLevel() const;
    std::string getLevelString() const;
    const Location& getLocation() const;
    const std::string& getMessage() const;

    Json::Value asJson() const;
    bool operator<(const Lint& other) const;

  private:
    LintLevel mLevel;
    Location mLocation;
    std::string mMessage;
};

std::ostream& operator<<(std::ostream& os, const Lint&);
}  // namespace android