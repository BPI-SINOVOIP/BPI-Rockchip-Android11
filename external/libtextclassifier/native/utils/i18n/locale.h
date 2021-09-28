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

#ifndef LIBTEXTCLASSIFIER_UTILS_I18N_LOCALE_H_
#define LIBTEXTCLASSIFIER_UTILS_I18N_LOCALE_H_

#include <string>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/i18n/language-tag_generated.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

class Locale {
 public:
  // Constructs the object from a valid BCP47 tag. If the tag is invalid,
  // an object is created that gives false when IsInvalid() is called.
  static Locale FromBCP47(const std::string& locale_tag);

  // Constructs the object from a flatbuffer language tag.
  static Locale FromLanguageTag(const LanguageTag* language_tag);

  // Creates a prototypical invalid locale object.
  static Locale Invalid() {
    Locale locale(/*language=*/"", /*script=*/"", /*region=*/"");
    locale.is_valid_ = false;
    return locale;
  }

  std::string Language() const { return language_; }

  std::string Script() const { return script_; }

  std::string Region() const { return region_; }

  bool IsValid() const { return is_valid_; }
  bool IsUnknown() const;

  // Returns whether any of the given locales is supported by any of the
  // supported locales. Returns default value if the given 'locales' list, or
  // 'supported_locales' list is empty or an unknown locale is found.
  // Locale::FromBCP47("*") means any locale.
  static bool IsAnyLocaleSupported(const std::vector<Locale>& locales,
                                   const std::vector<Locale>& supported_locales,
                                   bool default_value);

 private:
  Locale(const std::string& language, const std::string& script,
         const std::string& region)
      : language_(language),
        script_(script),
        region_(region),
        is_valid_(true) {}

  static bool IsLocaleSupported(const Locale& locale,
                                const std::vector<Locale>& supported_locales,
                                bool default_value);

  std::string language_;
  std::string script_;
  std::string region_;
  bool is_valid_;
};

// Pretty-printing function for Locale.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Locale& locale);

// Parses a comma-separated list of BCP47 tags.
bool ParseLocales(StringPiece locales_list, std::vector<Locale>* locales);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_I18N_LOCALE_H_
