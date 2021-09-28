/*
 * Copyright 2019 The Android Open Source Project
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

#include <initializer_list>
#include <iostream>
#include <sstream>
#include <vector>

#include "parse_location.h"

class Loggable {
 public:
  virtual ~Loggable() = default;
  virtual std::string GetDebugName() const = 0;
  virtual ParseLocation GetLocation() const = 0;
};

class LogMessage {
 public:
  LogMessage(ParseLocation loc, std::initializer_list<const Loggable*> tokens)
      : debug_(false), loc_(loc), tokens_(tokens) {
    Init();
  }

  LogMessage(bool debug, std::initializer_list<const Loggable*> tokens) : debug_(debug), tokens_(tokens) {
    Init();
  }

  void Init() {
    if (loc_.GetLine() != -1) {
      stream_ << "\033[1mLine " << loc_.GetLine() << ": ";
    }

    if (!debug_) {
      stream_ << "\033[1;31m";
      stream_ << "ERROR: ";
      stream_ << "\033[0m";
    } else {
      stream_ << "\033[1;m";
      stream_ << "DEBUG: ";
      stream_ << "\033[0m";
    }
  }

  ~LogMessage() {
    if (debug_ && suppress_debug_) return;

    std::cerr << stream_.str() << "\n";
    for (const auto& token : tokens_) {
      // Bold line number
      std::cerr << "\033[1m";
      std::cerr << "  Line " << token->GetLocation().GetLine() << ": ";
      std::cerr << "\033[0m";
      std::cerr << token->GetDebugName() << "\n";
    }

    if (!debug_) {
      abort();
    }
  }

  std::ostream& stream() {
    return stream_;
  }

 private:
  std::ostringstream stream_;
  bool debug_;
  bool suppress_debug_{true};
  ParseLocation loc_;
  std::vector<const Loggable*> tokens_;
};

// Error Log stream. Aborts the program after the message is printed.
// The arguments to the macro is a list of Loggable objects that are printed when the error is printed.
#define ERROR(...) LogMessage(false, {__VA_ARGS__}).stream()

// ParseLocation error log, the first argument is a location.
#define ERRORLOC(_1, ...) LogMessage(_1, {__VA_ARGS__}).stream()

#define DEBUG(...) LogMessage(true, {__VA_ARGS__}).stream()
