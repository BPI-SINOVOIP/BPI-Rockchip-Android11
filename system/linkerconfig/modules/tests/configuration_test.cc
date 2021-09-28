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
#include <gtest/gtest.h>

#include <vector>

#include "linkerconfig/configuration.h"
#include "linkerconfig/variables.h"
#include "modules_testbase.h"

using namespace android::linkerconfig::modules;

constexpr const char* kExpectedConfiguration =
    R"(dir.system = /system/bin
dir.system = /system/xbin
dir.system = /product/bin
dir.vendor = /odm/bin
dir.vendor = /vendor/bin
dir.vendor = /system/bin/vendor
dir.vendor = /product/bin/vendor
[system]
additional.namespaces = namespace1,namespace2
namespace.default.isolated = false
namespace.default.search.paths = /search_path1
namespace.default.search.paths += /search_path2
namespace.default.search.paths += /search_path3
namespace.default.permitted.paths = /permitted_path1
namespace.default.permitted.paths += /permitted_path2
namespace.default.permitted.paths += /permitted_path3
namespace.default.asan.search.paths = /data/asan/search_path1
namespace.default.asan.search.paths += /search_path1
namespace.default.asan.search.paths += /search_path2
namespace.default.asan.permitted.paths = /data/asan/permitted_path1
namespace.default.asan.permitted.paths += /permitted_path1
namespace.default.asan.permitted.paths += /permitted_path2
namespace.default.links = namespace1,namespace2
namespace.default.link.namespace1.shared_libs = lib1.so
namespace.default.link.namespace1.shared_libs += lib2.so
namespace.default.link.namespace1.shared_libs += lib3.so
namespace.default.link.namespace2.allow_all_shared_libs = true
namespace.namespace1.isolated = false
namespace.namespace1.search.paths = /search_path1
namespace.namespace1.search.paths += /search_path2
namespace.namespace1.search.paths += /search_path3
namespace.namespace1.permitted.paths = /permitted_path1
namespace.namespace1.permitted.paths += /permitted_path2
namespace.namespace1.permitted.paths += /permitted_path3
namespace.namespace1.asan.search.paths = /data/asan/search_path1
namespace.namespace1.asan.search.paths += /search_path1
namespace.namespace1.asan.search.paths += /search_path2
namespace.namespace1.asan.permitted.paths = /data/asan/permitted_path1
namespace.namespace1.asan.permitted.paths += /permitted_path1
namespace.namespace1.asan.permitted.paths += /permitted_path2
namespace.namespace2.isolated = false
namespace.namespace2.search.paths = /search_path1
namespace.namespace2.search.paths += /search_path2
namespace.namespace2.search.paths += /search_path3
namespace.namespace2.permitted.paths = /permitted_path1
namespace.namespace2.permitted.paths += /permitted_path2
namespace.namespace2.permitted.paths += /permitted_path3
namespace.namespace2.asan.search.paths = /data/asan/search_path1
namespace.namespace2.asan.search.paths += /search_path1
namespace.namespace2.asan.search.paths += /search_path2
namespace.namespace2.asan.permitted.paths = /data/asan/permitted_path1
namespace.namespace2.asan.permitted.paths += /permitted_path1
namespace.namespace2.asan.permitted.paths += /permitted_path2
[vendor]
namespace.default.isolated = false
namespace.default.search.paths = /search_path1
namespace.default.search.paths += /search_path2
namespace.default.search.paths += /search_path3
namespace.default.permitted.paths = /permitted_path1
namespace.default.permitted.paths += /permitted_path2
namespace.default.permitted.paths += /permitted_path3
namespace.default.asan.search.paths = /data/asan/search_path1
namespace.default.asan.search.paths += /search_path1
namespace.default.asan.search.paths += /search_path2
namespace.default.asan.permitted.paths = /data/asan/permitted_path1
namespace.default.asan.permitted.paths += /permitted_path1
namespace.default.asan.permitted.paths += /permitted_path2
)";

TEST(linkerconfig_configuration, generate_configuration) {
  std::vector<Section> sections;

  std::vector<DirToSection> dir_to_sections = {
      {"/system/bin", "system"},
      {"/system/xbin", "system"},
      {"/product/bin", "system"},
      {"/odm/bin", "vendor"},
      {"/vendor/bin", "vendor"},
      {"/system/bin/vendor", "vendor"},
      {"/product/bin/vendor", "vendor"},
      {"/product/bin", "vendor"},  // note that this is ignored
  };

  std::vector<Namespace> system_namespaces;

  system_namespaces.emplace_back(CreateNamespaceWithLinks(
      "default", false, false, "namespace1", "namespace2"));
  system_namespaces.emplace_back(
      CreateNamespaceWithPaths("namespace1", false, false));
  system_namespaces.emplace_back(
      CreateNamespaceWithPaths("namespace2", false, false));

  Section system_section("system", std::move(system_namespaces));
  sections.emplace_back(std::move(system_section));

  std::vector<Namespace> vendor_namespaces;

  vendor_namespaces.emplace_back(
      CreateNamespaceWithPaths("default", false, false));

  Section vendor_section("vendor", std::move(vendor_namespaces));
  sections.emplace_back(std::move(vendor_section));

  Configuration conf(std::move(sections), dir_to_sections);

  android::linkerconfig::modules::ConfigWriter writer;
  conf.WriteConfig(writer);

  ASSERT_EQ(kExpectedConfiguration, writer.ToString());
}