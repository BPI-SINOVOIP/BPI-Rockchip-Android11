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

#include "linkerconfig/namespace.h"

#include <string>
#include <vector>

#include <android-base/strings.h>
#include <gtest/gtest.h>

#include "linkerconfig/configwriter.h"
#include "linkerconfig/link.h"
#include "modules_testbase.h"

using namespace android::linkerconfig::modules;
using namespace android::base;

constexpr const char* kExpectedSimpleNamespaceConfig =
    R"(namespace.test_namespace.isolated = false
namespace.test_namespace.search.paths = /search_path1
namespace.test_namespace.search.paths += /search_path2
namespace.test_namespace.search.paths += /search_path3
namespace.test_namespace.permitted.paths = /permitted_path1
namespace.test_namespace.permitted.paths += /permitted_path2
namespace.test_namespace.permitted.paths += /permitted_path3
namespace.test_namespace.asan.search.paths = /data/asan/search_path1
namespace.test_namespace.asan.search.paths += /search_path1
namespace.test_namespace.asan.search.paths += /search_path2
namespace.test_namespace.asan.permitted.paths = /data/asan/permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path2
)";

constexpr const char* kExpectedNamespaceWithLinkConfig =
    R"(namespace.test_namespace.isolated = true
namespace.test_namespace.visible = true
namespace.test_namespace.search.paths = /search_path1
namespace.test_namespace.search.paths += /search_path2
namespace.test_namespace.search.paths += /search_path3
namespace.test_namespace.permitted.paths = /permitted_path1
namespace.test_namespace.permitted.paths += /permitted_path2
namespace.test_namespace.permitted.paths += /permitted_path3
namespace.test_namespace.asan.search.paths = /data/asan/search_path1
namespace.test_namespace.asan.search.paths += /search_path1
namespace.test_namespace.asan.search.paths += /search_path2
namespace.test_namespace.asan.permitted.paths = /data/asan/permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path2
namespace.test_namespace.links = target_namespace1,target_namespace2
namespace.test_namespace.link.target_namespace1.shared_libs = lib1.so
namespace.test_namespace.link.target_namespace1.shared_libs += lib2.so
namespace.test_namespace.link.target_namespace1.shared_libs += lib3.so
namespace.test_namespace.link.target_namespace2.allow_all_shared_libs = true
)";

constexpr const char* kExpectedNamespaceWithWhitelisted =
    R"(namespace.test_namespace.isolated = false
namespace.test_namespace.search.paths = /search_path1
namespace.test_namespace.search.paths += /search_path2
namespace.test_namespace.search.paths += /search_path3
namespace.test_namespace.permitted.paths = /permitted_path1
namespace.test_namespace.permitted.paths += /permitted_path2
namespace.test_namespace.permitted.paths += /permitted_path3
namespace.test_namespace.asan.search.paths = /data/asan/search_path1
namespace.test_namespace.asan.search.paths += /search_path1
namespace.test_namespace.asan.search.paths += /search_path2
namespace.test_namespace.asan.permitted.paths = /data/asan/permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path1
namespace.test_namespace.asan.permitted.paths += /permitted_path2
namespace.test_namespace.whitelisted = whitelisted_path1
namespace.test_namespace.whitelisted += whitelisted_path2
)";

TEST(linkerconfig_namespace, simple_namespace) {
  ConfigWriter writer;
  auto ns = CreateNamespaceWithPaths("test_namespace", false, false);
  ns.WriteConfig(writer);
  auto config = writer.ToString();

  ASSERT_EQ(config, kExpectedSimpleNamespaceConfig);
}

TEST(linkerconfig_namespace, namespace_with_links) {
  ConfigWriter writer;

  auto ns = CreateNamespaceWithLinks("test_namespace", true, true,
                                     "target_namespace1", "target_namespace2");
  ns.WriteConfig(writer);
  auto config = writer.ToString();

  ASSERT_EQ(config, kExpectedNamespaceWithLinkConfig);
}

TEST(linkerconfig_namespace, namespace_with_whitelisted) {
  ConfigWriter writer;
  auto ns = CreateNamespaceWithPaths("test_namespace", false, false);
  ns.AddWhitelisted("whitelisted_path1");
  ns.AddWhitelisted("whitelisted_path2");
  ns.WriteConfig(writer);

  auto config = writer.ToString();

  ASSERT_EQ(config, kExpectedNamespaceWithWhitelisted);
}

TEST(linkerconfig_namespace, namespace_links_should_be_ordered) {
  std::vector<std::string> expected_links = {"z", "a", "o"};

  Namespace ns("test_namespace");
  for (auto link : expected_links) {
    ns.GetLink(link);
  }

  ConfigWriter writer;
  ns.WriteConfig(writer);

  std::string actual_links;
  for (auto line : Split(writer.ToString(), "\n")) {
    if (StartsWith(line, "namespace.test_namespace.links")) {
      actual_links = Split(line, " ").back();
    }
  }
  ASSERT_EQ(android::base::Join(expected_links, ","), actual_links);
}
