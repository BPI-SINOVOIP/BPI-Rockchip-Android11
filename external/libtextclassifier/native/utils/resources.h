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

#ifndef LIBTEXTCLASSIFIER_UTILS_RESOURCES_H_
#define LIBTEXTCLASSIFIER_UTILS_RESOURCES_H_

#include <vector>

#include "utils/i18n/language-tag_generated.h"
#include "utils/i18n/locale.h"
#include "utils/resources_generated.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Class for accessing localized model resources.
class Resources {
 public:
  explicit Resources(const ResourcePool* resources) : resources_(resources) {}

  // Returns the string value associated with the particular resource.
  // `locales` are locales in preference order.
  bool GetResourceContent(const std::vector<Locale>& locales,
                          const StringPiece resource_name,
                          std::string* result) const;

 private:
  // Match priorities: language > script > region with wildcard matches being
  // weaker than an exact match.
  // For a resource lookup, at least language needs to (weakly) match.
  // c.f. developer.android.com/guide/topics/resources/multilingual-support
  enum LocaleMatch {
    LOCALE_NO_MATCH = 0,
    LOCALE_REGION_WILDCARD_MATCH = 1 << 0,
    LOCALE_REGION_MATCH = 1 << 1,
    LOCALE_SCRIPT_WILDCARD_MATCH = 1 << 2,
    LOCALE_SCRIPT_MATCH = 1 << 3,
    LOCALE_LANGUAGE_WILDCARD_MATCH = 1 << 4,
    LOCALE_LANGUAGE_MATCH = 1 << 5
  };
  int LocaleMatch(const Locale& locale, const LanguageTag* entry_locale) const;

  // Finds a resource entry by name.
  const ResourceEntry* FindResource(const StringPiece resource_name) const;

  // Finds the best locale matching resource from a resource entry.
  int BestResourceForLocales(const ResourceEntry* resource,
                             const std::vector<Locale>& locales) const;

  const ResourcePool* resources_;
};

// Compresses resources in place.
bool CompressResources(ResourcePoolT* resources,
                       const bool build_compression_dictionary = false,
                       const int dictionary_sample_every = 1);
std::string CompressSerializedResources(
    const std::string& resources,
    const bool build_compression_dictionary = false,
    const int dictionary_sample_every = 1);

bool DecompressResources(ResourcePoolT* resources,
                         const bool build_compression_dictionary = false);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_RESOURCES_H_
