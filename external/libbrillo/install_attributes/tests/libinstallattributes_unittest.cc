// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "install_attributes/libinstallattributes.h"

#include <string>

#include <gtest/gtest.h>

// Allows to override the install attributes path while preserving all the
// functionality of the original class.
class MockInstallAttributesReader : public InstallAttributesReader {
 public:
  void SetPath(const std::string& filename) {
    install_attributes_path_ = base::FilePath(filename);
  }
  size_t GetAttributesCount() const { return attributes_.size(); }
};

TEST(InstallAttributesTest, ReadNonexistingAttributes) {
  MockInstallAttributesReader reader;
  reader.SetPath("non-existing.pb");
  ASSERT_FALSE(reader.IsLocked());
  ASSERT_EQ(0, reader.GetAttributesCount());
}

// corrupt.pb is an invalid proto.
TEST(InstallAttributesTest, ReadCorruptAttributes) {
  MockInstallAttributesReader reader;
  reader.SetPath("install_attributes/tests/corrupt.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ(0, reader.GetAttributesCount());
}

// consumer.pb is a valid proto containing no attributes.
TEST(InstallAttributesTest, ReadEmptyAttributes) {
  MockInstallAttributesReader reader;
  reader.SetPath("install_attributes/tests/consumer.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ(0, reader.GetAttributesCount());
}

// managed.pb is a valid proto containing the usual enterprise enrollment
// attributes.
TEST(InstallAttributesTest, ReadManagedAttributes) {
  MockInstallAttributesReader reader;
  reader.SetPath("install_attributes/tests/managed.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ(std::string(), reader.GetAttribute("non-existing"));
  ASSERT_EQ("enterprise", reader.GetAttribute("enterprise.mode"));
}

// Going from non-existing attributes file to existing attributes file must
// work, i.e. the non-existence of the attributes file must not be cached.
TEST(InstallAttributesTest, ProgressionFromNonExistingToManaged) {
  MockInstallAttributesReader reader;
  reader.SetPath("non-existing.pb");
  ASSERT_FALSE(reader.IsLocked());
  ASSERT_EQ(0, reader.GetAttributesCount());

  reader.SetPath("install_attributes/tests/managed.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ("enterprise", reader.GetAttribute("enterprise.mode"));
}

// Going from empty attributes file to non-empty attributes file must not work,
// i.e. the non-existence of the attributes must be cached.
TEST(InstallAttributesTest, NoProgressionFromEmptyToManaged) {
  MockInstallAttributesReader reader;
  reader.SetPath("install_attributes/tests/consumer.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ(0, reader.GetAttributesCount());

  reader.SetPath("install_attributes/tests/managed.pb");
  ASSERT_TRUE(reader.IsLocked());
  ASSERT_EQ(std::string(), reader.GetAttribute("enterprise.mode"));
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
