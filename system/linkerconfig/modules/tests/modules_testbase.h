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

#include "linkerconfig/namespace.h"

using namespace android::linkerconfig::modules;

inline Namespace CreateNamespaceWithPaths(std::string name, bool is_isolated,
                                          bool is_visible) {
  Namespace ns(name, is_isolated, is_visible);
  ns.AddSearchPath("/search_path1", AsanPath::WITH_DATA_ASAN);
  ns.AddSearchPath("/search_path2", AsanPath::SAME_PATH);
  ns.AddSearchPath("/search_path3", AsanPath::NONE);
  ns.AddPermittedPath("/permitted_path1", AsanPath::WITH_DATA_ASAN);
  ns.AddPermittedPath("/permitted_path2", AsanPath::SAME_PATH);
  ns.AddPermittedPath("/permitted_path3", AsanPath::NONE);

  return ns;
}

inline Namespace CreateNamespaceWithLinks(std::string name, bool is_isolated,
                                          bool is_visible, std::string target_1,
                                          std::string target_2) {
  Namespace ns = CreateNamespaceWithPaths(name, is_isolated, is_visible);
  auto& link = ns.GetLink(target_1);
  link.AddSharedLib("lib1.so", "lib2.so", "lib3.so");

  ns.GetLink(target_2).AllowAllSharedLibs();
  return ns;
}
