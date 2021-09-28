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

#include <map>
#include <string>
#include <vector>

// Define basic data structure for linker configuration
struct Namespace;

struct Link {
  Namespace *from, *to;
  bool allow_all_shared;
  std::vector<std::string> shared_libs;
};

struct Namespace {
  std::string name;
  bool is_isolated;
  bool is_visible;
  std::vector<std::string> search_path;
  std::vector<std::string> permitted_path;
  std::vector<std::string> asan_search_path;
  std::vector<std::string> asan_permitted_path;
  std::map<std::string, Link> links;
  std::vector<std::string> whitelisted;
};

struct Section {
  std::string name;
  std::vector<std::string> dirs;
  std::map<std::string, Namespace> namespaces;
};

struct Configuration {
  std::map<std::string, Section> sections;
};

template <class K, class V>
bool MapContainsKey(const std::map<K, V>& map, K key) {
  return map.find(key) != map.end();
}