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
#include <android-base/file.h>
#include <gtest/gtest.h>

#include "linkerconfig/librarylistloader.h"

using namespace android::linkerconfig::generator;

const std::string kLibraryListA = android::base::GetExecutableDirectory() +
                                  "/generator/tests/data/library_list_a.txt";
const std::string kLibraryListB = android::base::GetExecutableDirectory() +
                                  "/generator/tests/data/library_list_b.txt";
const std::string kLibraryListC = android::base::GetExecutableDirectory() +
                                  "/generator/tests/data/library_list_c.txt";
const std::string kLibraryListInvalid =
    android::base::GetExecutableDirectory() +
    "/generator/tests/data/library_list_invalid.txt";

TEST(linkerconfig_librarylistloader, get_libraries) {
  const auto& library_list = GetLibrariesString(kLibraryListA);
  ASSERT_EQ("a.so:b.so:c.so:d.so:e.so:f.so", library_list);

  const auto& library_list_invalid = GetLibrariesString(kLibraryListInvalid);
  ASSERT_TRUE(library_list_invalid.empty());

  const auto& library_list_empty = GetLibrariesString(kLibraryListC);
  ASSERT_EQ("", library_list_empty);
}

TEST(linkerconfig_librarylistloader, get_public_libraries) {
  const auto& public_library_list =
      GetPublicLibrariesString(kLibraryListA, kLibraryListB);
  ASSERT_EQ("a.so:b.so:c.so:d.so", public_library_list);

  const auto& all_private_library_list =
      GetPublicLibrariesString(kLibraryListA, kLibraryListA);
  ASSERT_TRUE(all_private_library_list.empty());

  const auto& invalid_library_list =
      GetPublicLibrariesString(kLibraryListInvalid, kLibraryListB);
  ASSERT_TRUE(invalid_library_list.empty());

  const auto& private_library_invalid_list =
      GetPublicLibrariesString(kLibraryListA, kLibraryListInvalid);
  ASSERT_EQ("a.so:b.so:c.so:d.so:e.so:f.so", private_library_invalid_list);

  const auto& empty_library_list =
      GetPublicLibrariesString(kLibraryListC, kLibraryListA);
  ASSERT_EQ("", empty_library_list);
}

TEST(linkerconfig_librarylistloader, get_private_libraries) {
  const auto& private_library_list =
      GetPrivateLibrariesString(kLibraryListA, kLibraryListB);
  ASSERT_EQ("e.so:f.so", private_library_list);

  const auto& all_private_library_list =
      GetPrivateLibrariesString(kLibraryListA, kLibraryListA);
  ASSERT_EQ("a.so:b.so:c.so:d.so:e.so:f.so", all_private_library_list);

  const auto& invalid_library_list =
      GetPrivateLibrariesString(kLibraryListInvalid, kLibraryListB);
  ASSERT_TRUE(invalid_library_list.empty());

  const auto& private_library_invalid_list =
      GetPrivateLibrariesString(kLibraryListA, kLibraryListInvalid);
  ASSERT_TRUE(private_library_invalid_list.empty());
}
