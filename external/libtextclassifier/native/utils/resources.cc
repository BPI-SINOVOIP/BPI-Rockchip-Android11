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

#include "utils/resources.h"

#include "utils/base/logging.h"
#include "utils/zlib/buffer_generated.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {
namespace {
bool isWildcardMatch(const flatbuffers::String* left,
                     const std::string& right) {
  return (left == nullptr || right.empty());
}

bool isExactMatch(const flatbuffers::String* left, const std::string& right) {
  if (left == nullptr) {
    return right.empty();
  }
  return left->str() == right;
}

}  // namespace

int Resources::LocaleMatch(const Locale& locale,
                           const LanguageTag* entry_locale) const {
  int match = LOCALE_NO_MATCH;
  if (isExactMatch(entry_locale->language(), locale.Language())) {
    match |= LOCALE_LANGUAGE_MATCH;
  } else if (isWildcardMatch(entry_locale->language(), locale.Language())) {
    match |= LOCALE_LANGUAGE_WILDCARD_MATCH;
  }

  if (isExactMatch(entry_locale->script(), locale.Script())) {
    match |= LOCALE_SCRIPT_MATCH;
  } else if (isWildcardMatch(entry_locale->script(), locale.Script())) {
    match |= LOCALE_SCRIPT_WILDCARD_MATCH;
  }

  if (isExactMatch(entry_locale->region(), locale.Region())) {
    match |= LOCALE_REGION_MATCH;
  } else if (isWildcardMatch(entry_locale->region(), locale.Region())) {
    match |= LOCALE_REGION_WILDCARD_MATCH;
  }

  return match;
}

const ResourceEntry* Resources::FindResource(
    const StringPiece resource_name) const {
  if (resources_ == nullptr || resources_->resource_entry() == nullptr) {
    TC3_LOG(ERROR) << "No resources defined.";
    return nullptr;
  }
  const ResourceEntry* entry =
      resources_->resource_entry()->LookupByKey(resource_name.data());
  if (entry == nullptr) {
    TC3_LOG(ERROR) << "Resource " << resource_name.ToString() << " not found";
    return nullptr;
  }
  return entry;
}

int Resources::BestResourceForLocales(
    const ResourceEntry* resource, const std::vector<Locale>& locales) const {
  // Find best match based on locale.
  int resource_id = -1;
  int locale_match = LOCALE_NO_MATCH;
  const auto* resources = resource->resource();
  for (int user_locale = 0; user_locale < locales.size(); user_locale++) {
    if (!locales[user_locale].IsValid()) {
      continue;
    }
    for (int i = 0; i < resources->size(); i++) {
      for (const int locale_id : *resources->Get(i)->locale()) {
        const int candidate_match = LocaleMatch(
            locales[user_locale], resources_->locale()->Get(locale_id));

        // Only consider if at least the language matches.
        if ((candidate_match & LOCALE_LANGUAGE_MATCH) == 0 &&
            (candidate_match & LOCALE_LANGUAGE_WILDCARD_MATCH) == 0) {
          continue;
        }

        if (candidate_match > locale_match) {
          locale_match = candidate_match;
          resource_id = i;
        }
      }
    }

    // If the language matches exactly, we are already finished.
    // We found an exact language match.
    if (locale_match & LOCALE_LANGUAGE_MATCH) {
      return resource_id;
    }
  }
  return resource_id;
}

bool Resources::GetResourceContent(const std::vector<Locale>& locales,
                                   const StringPiece resource_name,
                                   std::string* result) const {
  const ResourceEntry* entry = FindResource(resource_name);
  if (entry == nullptr || entry->resource() == nullptr) {
    return false;
  }

  int resource_id = BestResourceForLocales(entry, locales);
  if (resource_id < 0) {
    return false;
  }
  const auto* resource = entry->resource()->Get(resource_id);
  if (resource->content() != nullptr) {
    *result = resource->content()->str();
    return true;
  } else if (resource->compressed_content() != nullptr) {
    std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance(
        resources_->compression_dictionary()->data(),
        resources_->compression_dictionary()->size());
    if (decompressor != nullptr &&
        decompressor->MaybeDecompress(resource->compressed_content(), result)) {
      return true;
    }
  }
  return false;
}

bool CompressResources(ResourcePoolT* resources,
                       const bool build_compression_dictionary,
                       const int dictionary_sample_every) {
  std::vector<unsigned char> dictionary;
  if (build_compression_dictionary) {
    {
      // Build up a compression dictionary.
      std::unique_ptr<ZlibCompressor> compressor = ZlibCompressor::Instance();
      int i = 0;
      for (auto& entry : resources->resource_entry) {
        for (auto& resource : entry->resource) {
          if (resource->content.empty()) {
            continue;
          }
          i++;

          // Use a sample of the entries to build up a custom compression
          // dictionary. Using all entries will generally not give a benefit
          // for small data sizes, so we subsample here.
          if (i % dictionary_sample_every != 0) {
            continue;
          }
          CompressedBufferT compressed_content;
          compressor->Compress(resource->content, &compressed_content);
        }
      }
      compressor->GetDictionary(&dictionary);
      resources->compression_dictionary.assign(
          dictionary.data(), dictionary.data() + dictionary.size());
    }
  }

  for (auto& entry : resources->resource_entry) {
    for (auto& resource : entry->resource) {
      if (resource->content.empty()) {
        continue;
      }
      // Try compressing the data.
      std::unique_ptr<ZlibCompressor> compressor =
          build_compression_dictionary
              ? ZlibCompressor::Instance(dictionary.data(), dictionary.size())
              : ZlibCompressor::Instance();
      if (!compressor) {
        TC3_LOG(ERROR) << "Cannot create zlib compressor.";
        return false;
      }

      CompressedBufferT compressed_content;
      compressor->Compress(resource->content, &compressed_content);

      // Only keep compressed version if smaller.
      if (compressed_content.uncompressed_size >
          compressed_content.buffer.size()) {
        resource->content.clear();
        resource->compressed_content.reset(new CompressedBufferT);
        *resource->compressed_content = compressed_content;
      }
    }
  }
  return true;
}

std::string CompressSerializedResources(const std::string& resources,
                                        const int dictionary_sample_every) {
  std::unique_ptr<ResourcePoolT> unpacked_resources(
      flatbuffers::GetRoot<ResourcePool>(resources.data())->UnPack());
  TC3_CHECK(unpacked_resources != nullptr);
  TC3_CHECK(
      CompressResources(unpacked_resources.get(), dictionary_sample_every));
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(ResourcePool::Pack(builder, unpacked_resources.get()));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

bool DecompressResources(ResourcePoolT* resources,
                         const bool build_compression_dictionary) {
  std::vector<unsigned char> dictionary;

  for (auto& entry : resources->resource_entry) {
    for (auto& resource : entry->resource) {
      if (resource->compressed_content == nullptr) {
        continue;
      }

      std::unique_ptr<ZlibDecompressor> zlib_decompressor =
          build_compression_dictionary
              ? ZlibDecompressor::Instance(dictionary.data(), dictionary.size())
              : ZlibDecompressor::Instance();
      if (!zlib_decompressor) {
        TC3_LOG(ERROR) << "Cannot initialize decompressor.";
        return false;
      }

      if (!zlib_decompressor->MaybeDecompress(
              resource->compressed_content.get(), &resource->content)) {
        TC3_LOG(ERROR) << "Cannot decompress resource.";
        return false;
      }
      resource->compressed_content.reset(nullptr);
    }
  }
  return true;
}

}  // namespace libtextclassifier3
