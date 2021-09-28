/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef IORAP_UTILS_PRINTER_H_
#define IORAP_UTILS_PRINTER_H_

#include <utils/Printer.h>

#include <string.h>

namespace iorap::common {

class StderrLogPrinter : public ::android::Printer {
public:
    // Create a printer using the specified logcat and log priority
    // - Unless ignoreBlankLines is false, print blank lines to logcat
    // (Note that the default ALOG behavior is to ignore blank lines)
    StderrLogPrinter(const char* logtag,
                     android_LogPriority priority = ANDROID_LOG_DEBUG,
                     const char* prefix = nullptr,
                     bool ignore_blank_lines = false)
      : log_printer_{logtag, priority, prefix, ignore_blank_lines} {
      logtag_ = logtag;
      prefix_ = prefix;
      ignore_blank_lines_ = ignore_blank_lines;
    }

    // Print the specified line to logcat. No \n at the end is necessary.
    virtual void printLine(const char* string) override {
      if (ignore_blank_lines_ && strlen(string) == 0) {
        return;
      }
      std::cerr << logtag_ << ": ";
      if (prefix_ != nullptr) {
        std::cerr << prefix_;
      }
      std::cerr << string << std::endl;
      log_printer_.printLine(string);
    }
 private:
    ::android::LogPrinter log_printer_;
    const char* logtag_;
    const char* prefix_;
    bool ignore_blank_lines_;
};

}  // namespace iorap::common

#endif  // IORAP_UTILS_PRINTER_H_
