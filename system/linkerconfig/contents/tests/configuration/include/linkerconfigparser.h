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

#include <cstring>
#include <regex>

#include "gtest/gtest.h"
#include "modules.h"

namespace {
constexpr const char* kSectionNameRegex = "\\[\\s*(\\w+)\\s*\\]";
constexpr const char* kDirRegex = "dir\\.(\\w+)\\s*=\\s*([\\w_\\-/]+)";
constexpr const char* kNamespaceBaseRegex =
    "namespace\\.(\\w+)\\.([^\\s=]+)\\s*(=|\\+=)\\s*([^\\s]+)";
constexpr const char* kAdditionalNamespacesRegex =
    "additional\\.namespaces\\s*=\\s*((?:[\\w]+)(?:,[\\w]+)*)";

// Functions to parse configuration string and verify syntax

inline void ParseDirPath(const std::string& line, Configuration& conf) {
  static std::regex dir_regex(kDirRegex);
  std::smatch match;

  ASSERT_TRUE(std::regex_match(line, match, dir_regex)) << line;
  ASSERT_EQ(3u, match.size()) << line;
  std::string section_name = match[1];
  std::string dir_path = match[2];

  if (!MapContainsKey(conf.sections, section_name)) {
    conf.sections[section_name].name = section_name;
    conf.sections[section_name].namespaces["default"].name = "default";
  }

  conf.sections[section_name].dirs.push_back(dir_path);
}

inline void ParseAdditionalNamespaces(const std::smatch& match,
                                      Section& current_section) {
  // additional.namespace = a,b,c,e,d
  ASSERT_EQ(2u, match.size());
  std::stringstream namespaces(match[1]);
  for (std::string namespace_name;
       std::getline(namespaces, namespace_name, ',');) {
    EXPECT_FALSE(MapContainsKey(current_section.namespaces, namespace_name))
        << "Namespace " << namespace_name << " already exists";
    Namespace new_namespace;
    new_namespace.name = namespace_name;
    current_section.namespaces[namespace_name] = new_namespace;
  }
}

inline void ParseNamespacePath(const std::vector<std::string>& property_descs,
                               const bool is_additional, const std::string& path,
                               Namespace& current_namespace,
                               const std::string& line) {
  // namespace.test.(asan.)search|permitted.path =|+= /path/to/${LIB}/dir
  ASSERT_EQ(property_descs[0] == "asan" ? 3u : 2u, property_descs.size());

  std::vector<std::string>* target_path = nullptr;
  if (property_descs[0] == "search") {
    target_path = &current_namespace.search_path;
  } else if (property_descs[0] == "permitted") {
    target_path = &current_namespace.permitted_path;
  } else if (property_descs[0] == "asan" && property_descs[1] == "search") {
    target_path = &current_namespace.asan_search_path;
  } else if (property_descs[0] == "asan" && property_descs[1] == "permitted") {
    target_path = &current_namespace.asan_permitted_path;
  }

  ASSERT_NE(nullptr, target_path) << line;
  EXPECT_EQ(is_additional, target_path->size() != 0)
      << "Path should be marked as = if and only if it is mentioned first : "
      << line;

  target_path->push_back(path);
}

inline void ParseLinkList(const std::vector<std::string>& property_descs,
                          const std::string& target_namespaces,
                          Namespace& current_namespace,
                          Section& current_section, const std::string& line) {
  // namespace.test.links = a,b,c,d,e
  EXPECT_EQ(1u, property_descs.size());
  std::stringstream namespaces(target_namespaces);
  for (std::string namespace_to; std::getline(namespaces, namespace_to, ',');) {
    EXPECT_FALSE(MapContainsKey(current_namespace.links, namespace_to))
        << "Link to " << namespace_to << " is already defined : " << line;
    EXPECT_TRUE(MapContainsKey(current_section.namespaces, namespace_to))
        << "Target namespace is not defined in section : " << line;

    current_namespace.links[namespace_to].from = &current_namespace;
    current_namespace.links[namespace_to].to =
        &current_section.namespaces[namespace_to];
    current_namespace.links[namespace_to].allow_all_shared = false;
  }
}

inline void ParseLink(const std::vector<std::string>& property_descs,
                      const bool is_additional, const std::string& value,
                      Namespace& current_namespace, Section& current_section,
                      const std::string& line) {
  // namespace.from.link.to.shared_libs = a.so
  // namespace.from.link.to.allow_all_shared_libs = true
  ASSERT_EQ(3u, property_descs.size()) << line;
  ASSERT_TRUE(property_descs[2] == "shared_libs" ||
              property_descs[2] == "allow_all_shared_libs")
      << line;
  std::string namespace_to = property_descs[1];

  ASSERT_TRUE(MapContainsKey(current_section.namespaces, namespace_to))
      << "To namespace does not exist in section " << current_section.name
      << " : " << line;

  if (property_descs[2] == "shared_libs") {
    EXPECT_EQ(is_additional,
              current_namespace.links[namespace_to].shared_libs.size() != 0)
        << "Link should be defined with = if and only if it is first link "
           "between two namespaces : "
        << line;

    current_namespace.links[namespace_to].shared_libs.push_back(value);
  } else {
    EXPECT_EQ("true", value) << line;
    current_namespace.links[namespace_to].allow_all_shared = true;
  }
}

inline void ParseNamespaceCommand(const std::string& namespace_name,
                                  const std::string& property_desc,
                                  const bool is_additional_property,
                                  const std::string& value,
                                  Section& current_section,
                                  const std::string& line) {
  ASSERT_TRUE(MapContainsKey(current_section.namespaces, namespace_name))
      << "Namespace " << namespace_name << " does not exist in section "
      << current_section.name << " : " << line;
  Namespace& current_namespace = current_section.namespaces[namespace_name];

  std::vector<std::string> property_descs;
  std::stringstream property_desc_stream(property_desc);
  for (std::string property;
       std::getline(property_desc_stream, property, '.');) {
    property_descs.push_back(property);
  }

  ASSERT_TRUE(property_descs.size() > 0)
      << "There should be at least one property description after namespace."
      << namespace_name << " : " << line;

  if (property_descs[0].compare("isolated") == 0) {
    // namespace.test.isolated = true
    EXPECT_EQ(1u, property_descs.size()) << line;
    EXPECT_TRUE(value == "true" || value == "false") << line;
    current_namespace.is_isolated = value == "true";
  } else if (property_descs[0].compare("visible") == 0) {
    // namespace.test.visible = true
    EXPECT_EQ(1u, property_descs.size()) << line;
    EXPECT_TRUE(value == "true" || value == "false") << line;
    current_namespace.is_visible = value == "true";
  } else if (property_descs[property_descs.size() - 1] == "paths") {
    // namespace.test.search.path += /system/lib
    ParseNamespacePath(
        property_descs, is_additional_property, value, current_namespace, line);
  } else if (property_descs[0] == "links") {
    // namespace.test.links = a,b,c
    ParseLinkList(
        property_descs, value, current_namespace, current_section, line);
  } else if (property_descs[0] == "link") {
    // namespace.test.link.a = libc.so
    ParseLink(property_descs,
              is_additional_property,
              value,
              current_namespace,
              current_section,
              line);
  } else if (property_descs[0] == "whitelisted") {
    EXPECT_EQ(1u, property_descs.size()) << line;
    current_namespace.whitelisted.push_back(value);
  } else {
    EXPECT_TRUE(false) << "Failed to parse line : " << line;
  }
}
}  // namespace

inline void ParseConfiguration(const std::string& configuration_str,
                               Configuration& conf) {
  Section* current_section = nullptr;

  static std::regex section_name_regex(kSectionNameRegex);
  static std::regex additional_namespaces_regex(kAdditionalNamespacesRegex);
  static std::regex namespace_base_regex(kNamespaceBaseRegex);

  std::smatch match;

  std::stringstream configuration_stream(configuration_str);

  for (std::string line; std::getline(configuration_stream, line);) {
    // Skip empty line
    if (line.empty()) {
      continue;
    }

    if (std::regex_match(line, match, section_name_regex)) {
      // [section_name]
      ASSERT_EQ(2u, match.size()) << line;
      std::string section_name = match[1];
      ASSERT_TRUE(MapContainsKey(conf.sections, section_name)) << line;
      current_section = &conf.sections[section_name];

      continue;
    }

    if (current_section == nullptr) {
      ParseDirPath(line, conf);
    } else {
      if (std::regex_match(line, match, additional_namespaces_regex)) {
        ParseAdditionalNamespaces(match, *current_section);
      } else {
        EXPECT_TRUE(std::regex_match(line, match, namespace_base_regex)) << line;
        ASSERT_EQ(5u, match.size()) << line;
        std::string namespace_name = match[1];
        std::string property_desc = match[2];
        bool is_additional_property = match[3] == "+=";
        std::string content = match[4];
        ParseNamespaceCommand(namespace_name,
                              property_desc,
                              is_additional_property,
                              content,
                              *current_section,
                              line);
      }
    }
  }
}