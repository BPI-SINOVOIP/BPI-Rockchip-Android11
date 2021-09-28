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

#include <set>
#include <string>
#include <vector>

#include "linkerconfig/apex.h"
#include "linkerconfig/configwriter.h"
#include "linkerconfig/link.h"

namespace android {
namespace linkerconfig {
namespace modules {

/**
 * Explains if the path should be also added for ASAN
 *
 * NONE : the path should not be added for ASAN
 * SAME_PATH : the path should be added for ASAN
 * WITH_DATA_ASAN : the path and /data/asan/<path> should be added for ASAN
 */
enum class AsanPath {
  NONE,
  SAME_PATH,
  WITH_DATA_ASAN,
};

class Namespace {
 public:
  explicit Namespace(std::string name, bool is_isolated = false,
                     bool is_visible = false)
      : is_isolated_(is_isolated),
        is_visible_(is_visible),
        name_(std::move(name)) {
  }

  Namespace(const Namespace& ns) = delete;
  Namespace(Namespace&& ns) = default;
  Namespace& operator=(Namespace&& ns) = default;

  // Add path to search path
  // This function will add path to namespace.<<namespace>>.search.paths
  // If path_from_asan is SAME_PATH, this will add path also to
  // namespace.<<namespace>>.asan.search.paths
  // If path_from_asan is WITH_DATA_ASAN,
  // this will also add asan path starts with /data/asan
  //
  // AddSearchPath("/system/${LIB}", AsanPath::NONE) :
  //    namespace.xxx.search.paths += /system/${LIB}
  // AddSearchPath("/system/${LIB}", AsanPath::SAME_PATH) :
  //    namespace.xxx.search.paths += /system/${LIB}
  //    namespace.xxx.asan.search.paths += /system/${LIB}
  // AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN) :
  //    namespace.xxx.search.paths += /system/${LIB}
  //    namespace.xxx.asan.search.paths += /data/asan/system/${LIB}
  //    namespace.xxx.asan.search.paths += /system/${LIB}
  void AddSearchPath(const std::string& path,
                     AsanPath path_from_asan = AsanPath::SAME_PATH);

  // Add path to permitted path
  // This function will add path to namespace.<<namespace>>.permitted.paths
  // If path_from_asan is SAME_PATH, this will add path also to
  // namespace.<<namespace>>.asan.permitted.paths
  // If path_from_asan is WITH_DATA_ASAN,
  // this will also add asan path starts with /data/asan
  //
  // AddSearchPath("/system/${LIB}", AsanPath::NONE) :
  //    namespace.xxx.permitted.paths += /system/${LIB}
  // AddSearchPath("/system/${LIB}", AsanPath::SAME_PATH) :
  //    namespace.xxx.permitted.paths += /system/${LIB}
  //    namespace.xxx.asan.permitted.paths += /system/${LIB}
  // AddSearchPath("/system/${LIB}", AsanPath::WITH_DATA_ASAN) :
  //    namespace.xxx.permitted.paths += /system/${LIB}
  //    namespace.xxx.asan.permitted.paths += /data/asan/system/${LIB}
  //    namespace.xxx.asan.permitted.paths += /system/${LIB}
  void AddPermittedPath(const std::string& path,
                        AsanPath path_from_asan = AsanPath::SAME_PATH);

  // Returns a link from this namespace to the given one. If one already exists
  // it is returned, otherwise one is created and pushed back to tail.
  Link& GetLink(const std::string& target_namespace);

  void WriteConfig(ConfigWriter& writer);
  void AddWhitelisted(const std::string& path);

  std::string GetName() const;

  void SetVisible(bool visible) {
    is_visible_ = visible;
  }

  // For test usage
  const std::vector<Link>& Links() const {
    return links_;
  }
  std::vector<std::string> SearchPaths() const {
    return search_paths_;
  }
  bool ContainsSearchPath(const std::string& path, AsanPath path_from_asan);
  bool ContainsPermittedPath(const std::string& path, AsanPath path_from_asan);

  template <typename Vec>
  void AddProvides(const Vec& list) {
    provides_.insert(list.begin(), list.end());
  }
  template <typename Vec>
  void AddRequires(const Vec& list) {
    requires_.insert(list.begin(), list.end());
  }
  const std::set<std::string>& GetProvides() const {
    return provides_;
  }
  const std::set<std::string>& GetRequires() const {
    return requires_;
  }

 private:
  bool is_isolated_;
  bool is_visible_;
  std::string name_;
  std::vector<std::string> search_paths_;
  std::vector<std::string> permitted_paths_;
  std::vector<std::string> asan_search_paths_;
  std::vector<std::string> asan_permitted_paths_;
  std::vector<std::string> whitelisted_;
  std::vector<Link> links_;
  std::set<std::string> provides_;
  std::set<std::string> requires_;

  void WritePathString(ConfigWriter& writer, const std::string& path_type,
                       const std::vector<std::string>& path_list);
};

void InitializeWithApex(Namespace& ns, const ApexInfo& apex_info);
}  // namespace modules
}  // namespace linkerconfig
}  // namespace android
