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
#include "utils/i18n/locale.h"
#include "utils/resources_generated.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

class ResourcesTest
    : public testing::TestWithParam<testing::tuple<bool, bool>> {
 protected:
  ResourcesTest() {}

  std::string BuildTestResources(bool add_default_language = true) const {
    ResourcePoolT test_resources;

    // Test locales.
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "en";
    test_resources.locale.back()->region = "US";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "en";
    test_resources.locale.back()->region = "GB";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "de";
    test_resources.locale.back()->region = "DE";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "fr";
    test_resources.locale.back()->region = "FR";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "pt";
    test_resources.locale.back()->region = "PT";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "pt";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "zh";
    test_resources.locale.back()->script = "Hans";
    test_resources.locale.back()->region = "CN";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "zh";
    test_resources.locale.emplace_back(new LanguageTagT);
    test_resources.locale.back()->language = "fr";
    test_resources.locale.back()->language = "fr-CA";
    if (add_default_language) {
      test_resources.locale.emplace_back(new LanguageTagT);  // default
    }

    // Test entries.
    test_resources.resource_entry.emplace_back(new ResourceEntryT);
    test_resources.resource_entry.back()->name = /*resource_name=*/"A";

    // en-US, default
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content = "localize";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(0);
    if (add_default_language) {
      test_resources.resource_entry.back()->resource.back()->locale.push_back(
          9);
    }

    // en-GB
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content = "localise";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(1);

    // de-DE
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content =
        "lokalisieren";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(2);

    // fr-FR, fr-CA
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content =
        "localiser";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(3);
    test_resources.resource_entry.back()->resource.back()->locale.push_back(8);

    // pt-PT
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content =
        "localizar";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(4);

    // pt
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content =
        "concentrar";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(5);

    // zh-Hans-CN
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content = "龙";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(6);

    // zh
    test_resources.resource_entry.back()->resource.emplace_back(new ResourceT);
    test_resources.resource_entry.back()->resource.back()->content = "龍";
    test_resources.resource_entry.back()->resource.back()->locale.push_back(7);

    if (compress()) {
      EXPECT_TRUE(CompressResources(
          &test_resources,
          /*build_compression_dictionary=*/build_dictionary()));
    }

    flatbuffers::FlatBufferBuilder builder;
    builder.Finish(ResourcePool::Pack(builder, &test_resources));

    return std::string(
        reinterpret_cast<const char*>(builder.GetBufferPointer()),
        builder.GetSize());
  }

  bool compress() const { return testing::get<0>(GetParam()); }

  bool build_dictionary() const { return testing::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(Compression, ResourcesTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ResourcesTest, CorrectlyHandlesExactMatch) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  std::string content;
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("en-US")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localize", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("en-GB")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localise", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("pt-PT")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localizar", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-Hans-CN")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龙", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龍", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("fr-CA")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localiser", content);
}

TEST_P(ResourcesTest, CorrectlyHandlesTie) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  // Uses first best match in case of a tie.
  std::string content;
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("en-CA")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localize", content);
}

TEST_P(ResourcesTest, RequiresLanguageMatch) {
  {
    std::string test_resources =
        BuildTestResources(/*add_default_language=*/false);
    Resources resources(
        flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
    EXPECT_FALSE(resources.GetResourceContent({Locale::FromBCP47("es-US")},
                                              /*resource_name=*/"A",
                                              /*result=*/nullptr));
  }
  {
    std::string test_resources =
        BuildTestResources(/*add_default_language=*/true);
    Resources resources(
        flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
    std::string content;
    EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("es-US")},
                                             /*resource_name=*/"A",
                                             /*result=*/&content));
    EXPECT_EQ("localize", content);
  }
}

TEST_P(ResourcesTest, HandlesFallback) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  std::string content;
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("fr-CH")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localiser", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-Hans")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龙", content);
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-Hans-ZZ")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龙", content);

  // Fallback to default, en-US.
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("ru")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("localize", content);
}

TEST_P(ResourcesTest, HandlesFallbackMultipleLocales) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  std::string content;

  // Still use inexact match with primary locale if language matches,
  // even though secondary locale would match exactly.
  EXPECT_TRUE(resources.GetResourceContent(
      {Locale::FromBCP47("fr-CH"), Locale::FromBCP47("en-US")},
      /*resource_name=*/"A", &content));
  EXPECT_EQ("localiser", content);

  // Use secondary language instead of default fallback if that is an exact
  // language match.
  EXPECT_TRUE(resources.GetResourceContent(
      {Locale::FromBCP47("ru"), Locale::FromBCP47("de")},
      /*resource_name=*/"A", &content));
  EXPECT_EQ("lokalisieren", content);

  // Use tertiary language.
  EXPECT_TRUE(resources.GetResourceContent(
      {Locale::FromBCP47("ru"), Locale::FromBCP47("it-IT"),
       Locale::FromBCP47("de")},
      /*resource_name=*/"A", &content));
  EXPECT_EQ("lokalisieren", content);

  // Default fallback if no locale matches.
  EXPECT_TRUE(resources.GetResourceContent(
      {Locale::FromBCP47("ru"), Locale::FromBCP47("it-IT"),
       Locale::FromBCP47("es")},
      /*resource_name=*/"A", &content));
  EXPECT_EQ("localize", content);
}

TEST_P(ResourcesTest, PreferGenericCallback) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  std::string content;
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("pt-BR")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("concentrar", content);  // Falls back to pt, not pt-PT.
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-Hant")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龍", content);  // Falls back to zh, not zh-Hans-CN.
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-Hant-CN")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龍", content);  // Falls back to zh, not zh-Hans-CN.
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("zh-CN")},
                                           /*resource_name=*/"A", &content));
  EXPECT_EQ("龍", content);  // Falls back to zh, not zh-Hans-CN.
}

TEST_P(ResourcesTest, PreferGenericWhenGeneric) {
  std::string test_resources = BuildTestResources();
  Resources resources(
      flatbuffers::GetRoot<ResourcePool>(test_resources.data()));
  std::string content;
  EXPECT_TRUE(resources.GetResourceContent({Locale::FromBCP47("pt")},
                                           /*resource_name=*/"A", &content));

  // Uses pt, not pt-PT.
  EXPECT_EQ("concentrar", content);
}

}  // namespace
}  // namespace libtextclassifier3
