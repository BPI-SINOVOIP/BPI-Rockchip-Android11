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

#include "Lint.h"

#include <android-base/logging.h>
#include <json/json.h>
#include <iostream>
#include <tuple>

#include "Location.h"

namespace android {
Lint::Lint(LintLevel level, const Location& location, const std::string& message)
    : mLevel(level), mLocation(location), mMessage(message) {}

LintLevel Lint::getLevel() const {
    return mLevel;
}

std::string Lint::getLevelString() const {
    switch (mLevel) {
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        default:
            LOG(FATAL) << "LintLevel must be warning or error";
    }
}

const Location& Lint::getLocation() const {
    return mLocation;
}

const std::string& Lint::getMessage() const {
    return mMessage;
}

bool Lint::operator<(const Lint& other) const {
    return std::tie(mLocation, mLevel, mMessage) <
           std::tie(other.mLocation, other.mLevel, other.mMessage);
}

Lint&& Lint::operator<<(const std::string& in) {
    mMessage += in;

    return std::move(*this);
}

Json::Value Lint::asJson() const {
    Json::Value lint;

    lint["message"] = mMessage;
    lint["level"] = getLevelString();

    // Begin and end should be in the same file
    lint["filename"] = mLocation.begin().filename();
    lint["begin"]["line"] = Json::UInt(mLocation.begin().line());
    lint["begin"]["column"] = Json::UInt(mLocation.begin().column());
    lint["end"]["line"] = Json::UInt(mLocation.end().line());
    lint["end"]["column"] = Json::UInt(mLocation.end().column());

    return lint;
}

enum Color { DEFAULT = 0, RED = 31, YELLOW = 33 };

static std::string setColor(Color color, bool bold = false) {
    std::string ret;
    ret += "\033[";
    if (bold) ret += "1";
    if (bold && color != DEFAULT) ret += ";";
    if (color != DEFAULT) ret += std::to_string(color);
    ret += "m";

    return ret;
}

std::ostream& operator<<(std::ostream& os, const Lint& lint) {
    if (lint.getLevel() == WARNING) {
        os << setColor(YELLOW, true) << lint.getLevelString() << ": " << setColor(DEFAULT);
    } else {
        CHECK(lint.getLevel() == ERROR);
        os << setColor(RED, true) << lint.getLevelString() << ": " << setColor(DEFAULT);
    }

    return os << setColor(DEFAULT, true) << lint.getLocation() << setColor(DEFAULT) << ": "
              << lint.getMessage() << std::endl;
}
}  // namespace android