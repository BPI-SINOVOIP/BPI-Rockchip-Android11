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
#pragma once

#include <unordered_set>

#include "gtest/gtest.h"
#include "linkerconfigparser.h"
#include "modules.h"

namespace {
inline void TraverseLink(const Namespace& ns, std::unordered_set<std::string>& visible_ns) {
  if (visible_ns.count(ns.name) != 0) {
    return;
  }

  visible_ns.insert(ns.name);

  for (auto& [_, link] : ns.links) {
    TraverseLink(*link.to, visible_ns);
  }
}

inline void ValidateAllNamespacesAreVisible(const Section& section) {
  std::unordered_set<std::string> visible_ns;
  for (auto& [_, ns] : section.namespaces) {
    if (ns.name == "default" || ns.is_visible) {
      TraverseLink(ns, visible_ns);
    }
  }

  for (auto& [_, ns] : section.namespaces) {
    EXPECT_EQ(1u, visible_ns.count(ns.name))
        << "Namespace " << ns.name << " is not visible from section " << section.name;
  }
}

inline void ValidateNamespace(const Namespace& target_namespace, const Section& parent_section) {
  EXPECT_FALSE(target_namespace.name.empty()) << "Namespace name should not be empty";
  EXPECT_FALSE(target_namespace.search_path.empty() && target_namespace.permitted_path.empty())
      << "Search path or permitted path should be defined in namespace " << target_namespace.name
      << " from section " << parent_section.name;
}

inline void ValidateSection(const Section& section) {
  EXPECT_FALSE(section.name.empty()) << "Section name should not be empty";
  EXPECT_NE(0u, section.namespaces.size())
      << "Section " << section.name << " should contain at least one namespace";
  EXPECT_NE(0u, section.dirs.size())
      << "Section " << section.name << "does not contain any directory as executable path";
  EXPECT_TRUE(MapContainsKey(section.namespaces, std::string("default")))
      << "Section " << section.name << " should contain namespace named 'default'";

  for (auto& [_, target_namespace] : section.namespaces) {
    ValidateNamespace(target_namespace, section);
  }

  ValidateAllNamespacesAreVisible(section);
}

inline void ValidateConfiguration(const Configuration& conf) {
  EXPECT_NE(0u, conf.sections.size());
  for (auto& [_, section] : conf.sections) {
    ValidateSection(section);
  }
}
}  // namespace

inline void VerifyConfiguration(const std::string& configuration_str) {
  Configuration conf;
  ParseConfiguration(configuration_str, conf);
  ValidateConfiguration(conf);
}