// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_DB_APP_COMPONENT_NAME_H_
#define IORAP_SRC_DB_APP_COMPONENT_NAME_H_

namespace iorap::db {

struct AppComponentName {
  std::string package;
  std::string activity_name;

  // Turns the activity name to the fully qualified name.
  // For example, if the activity name is ".MainActivity" and the package is
  // foo.bar. Then the fully qualified name is foo.bar.MainActivity.
  AppComponentName Canonicalize() const {
    if (!activity_name.empty() && activity_name[0] == '.') {
      return {package, package + activity_name};
    }
    return {package, activity_name};
  }

  static bool HasAppComponentName(const std::string& s) {
    return s.find('/') != std::string::npos;
  }

  // "com.foo.bar/.A" -> {"com.foo.bar", ".A"}
  static AppComponentName FromString(const std::string& s) {
    constexpr const char delimiter = '/';

    if (!HasAppComponentName(s)) {
      return {std::move(s), ""};
    }

    std::string package = s.substr(0, s.find(delimiter));

    std::string activity_name = s;
    activity_name.erase(0, s.find(delimiter) + sizeof(delimiter));

    return {std::move(package), std::move(activity_name)};
  }

  // {"com.foo.bar", ".A"} -> "com.foo.bar/.A"
  std::string ToString() const {
    return package + "/" + activity_name;
  }

  /*
   * '/' is encoded into %2F
   * '%' is encoded into %25
   *
   * This allows the component name to be be used as a file name
   * ('/' is illegal due to being a path separator) with minimal
   * munging.
   */

  // "com.foo.bar%2F.A%25" -> {"com.foo.bar", ".A%"}
  static AppComponentName FromUrlEncodedString(const std::string& s) {
    std::string cpy = s;
    Replace(cpy, "%2F", "/");
    Replace(cpy, "%25", "%");

    return FromString(cpy);
  }

  // {"com.foo.bar", ".A%"} -> "com.foo.bar%2F.A%25"
  std::string ToUrlEncodedString() const {
    std::string s = ToString();
    Replace(s, "%", "%25");
    Replace(s, "/", "%2F");
    return s;
  }

  /*
   * '/' is encoded into @@
   * '%' is encoded into ^^
   *
   * Two purpose:
   * 1. This allows the package name to be used as a file name
   * ('/' is illegal due to being a path separator) with minimal
   * munging.
   * 2. This allows the package name to be used in .mk file because
   * '%' is a special char and cannot be easily escaped in Makefile.
   *
   * This is a workround for test purpose.
   * Only package name is used because activity name varies on
   * different testing framework.
   * Hopefully, the double "@@" and "^^" are not used in other cases.
   */
  // {"com.foo.bar", ".A%"} -> "com.foo.bar"
  std::string ToMakeFileSafeEncodedPkgString() const {
    std::string s = package;
    Replace(s, "/", "@@");
    Replace(s, "%", "^^");
    return s;
  }

 private:
  static bool Replace(std::string& str, const std::string& from, const std::string& to) {
    // TODO: call in a loop to replace all occurrences, not just the first one.
    const size_t start_pos = str.find(from);
    if (start_pos == std::string::npos) {
      return false;
    }

    str.replace(start_pos, from.length(), to);

    return true;
}
};

inline std::ostream& operator<<(std::ostream& os, const AppComponentName& name) {
  os << name.ToString();
  return os;
}

}  // namespace iorap::db

#endif  // IORAP_SRC_DB_APP_COMPONENT_NAME_H_
