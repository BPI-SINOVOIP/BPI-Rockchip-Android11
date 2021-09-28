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

#include "utils/i18n/locale.h"

#include "utils/strings/split.h"

namespace libtextclassifier3 {

namespace {
constexpr const char* kAnyMatch = "*";

// BCP 47 code for "Undetermined Language".
constexpr const char* kUnknownLanguageCode = "und";

bool CheckLanguage(StringPiece language) {
  if (language.size() == 1 && language.data()[0] == '*') {
    return true;
  }

  if (language.size() != 2 && language.size() != 3) {
    return false;
  }

  // Needs to be all lowercase.
  for (int i = 0; i < language.size(); ++i) {
    if (!std::islower(language[i])) {
      return false;
    }
  }

  return true;
}

bool CheckScript(StringPiece script) {
  if (script.size() != 4) {
    return false;
  }

  if (!std::isupper(script[0])) {
    return false;
  }

  // Needs to be all lowercase.
  for (int i = 1; i < script.size(); ++i) {
    if (!std::islower(script[i])) {
      return false;
    }
  }

  return true;
}

bool CheckRegion(StringPiece region) {
  if (region.size() == 2) {
    return std::isupper(region[0]) && std::isupper(region[1]);
  } else if (region.size() == 3) {
    return std::isdigit(region[0]) && std::isdigit(region[1]) &&
           std::isdigit(region[2]);
  } else {
    return false;
  }
}

}  // namespace

Locale Locale::FromBCP47(const std::string& locale_tag) {
  std::vector<StringPiece> parts = strings::Split(locale_tag, '-');
  if (parts.empty()) {
    return Locale::Invalid();
  }

  auto parts_it = parts.begin();
  StringPiece language = *parts_it;
  if (!CheckLanguage(language)) {
    return Locale::Invalid();
  }
  ++parts_it;

  StringPiece script;
  if (parts_it != parts.end()) {
    script = *parts_it;
    if (!CheckScript(script)) {
      script = "";
    } else {
      ++parts_it;
    }
  }

  StringPiece region;
  if (parts_it != parts.end()) {
    region = *parts_it;
    if (!CheckRegion(region)) {
      region = "";
    } else {
      ++parts_it;
    }
  }

  // NOTE: We don't parse the rest of the BCP47 tag here even if specified.

  return Locale(language.ToString(), script.ToString(), region.ToString());
}

Locale Locale::FromLanguageTag(const LanguageTag* language_tag) {
  if (language_tag == nullptr || language_tag->language() == nullptr) {
    return Locale::Invalid();
  }

  StringPiece language = language_tag->language()->c_str();
  if (!CheckLanguage(language)) {
    return Locale::Invalid();
  }

  StringPiece script;
  if (language_tag->script() != nullptr) {
    script = language_tag->script()->c_str();
    if (!CheckScript(script)) {
      script = "";
    }
  }

  StringPiece region;
  if (language_tag->region() != nullptr) {
    region = language_tag->region()->c_str();
    if (!CheckRegion(region)) {
      region = "";
    }
  }
  return Locale(language.ToString(), script.ToString(), region.ToString());
}

bool Locale::IsUnknown() const {
  return is_valid_ && language_ == kUnknownLanguageCode;
}

bool Locale::IsLocaleSupported(const Locale& locale,
                               const std::vector<Locale>& supported_locales,
                               bool default_value) {
  if (!locale.IsValid()) {
    return false;
  }
  if (locale.IsUnknown()) {
    return default_value;
  }
  for (const Locale& supported_locale : supported_locales) {
    if (!supported_locale.IsValid()) {
      continue;
    }
    const bool language_matches =
        supported_locale.Language().empty() ||
        supported_locale.Language() == kAnyMatch ||
        supported_locale.Language() == locale.Language();
    const bool script_matches = supported_locale.Script().empty() ||
                                supported_locale.Script() == kAnyMatch ||
                                locale.Script().empty() ||
                                supported_locale.Script() == locale.Script();
    const bool region_matches = supported_locale.Region().empty() ||
                                supported_locale.Region() == kAnyMatch ||
                                locale.Region().empty() ||
                                supported_locale.Region() == locale.Region();
    if (language_matches && script_matches && region_matches) {
      return true;
    }
  }
  return false;
}

bool Locale::IsAnyLocaleSupported(const std::vector<Locale>& locales,
                                  const std::vector<Locale>& supported_locales,
                                  bool default_value) {
  if (locales.empty()) {
    return default_value;
  }
  if (supported_locales.empty()) {
    return default_value;
  }
  for (const Locale& locale : locales) {
    if (IsLocaleSupported(locale, supported_locales, default_value)) {
      return true;
    }
  }
  return false;
}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Locale& locale) {
  return stream << "Locale(language=" << locale.Language()
                << ", script=" << locale.Script()
                << ", region=" << locale.Region()
                << ", is_valid=" << locale.IsValid()
                << ", is_unknown=" << locale.IsUnknown() << ")";
}

bool ParseLocales(StringPiece locales_list, std::vector<Locale>* locales) {
  for (const auto& locale_str : strings::Split(locales_list, ',')) {
    const Locale locale = Locale::FromBCP47(locale_str.ToString());
    if (!locale.IsValid()) {
      TC3_LOG(ERROR) << "Invalid locale " << locale_str.ToString();
      return false;
    }
    locales->push_back(locale);
  }
  return true;
}

}  // namespace libtextclassifier3
