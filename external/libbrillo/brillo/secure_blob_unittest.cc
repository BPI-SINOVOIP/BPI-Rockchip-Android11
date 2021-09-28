// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for SecureBlob.

#include "brillo/asan.h"
#include "brillo/secure_blob.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>

#include <base/logging.h>
#include <gtest/gtest.h>

namespace brillo {
using std::string;

// Tests BlobToString() and BlobFromString().
TEST(BlobTest, StringConversions) {
  const char kTestBytes[] = {'\0', '\x1', 'a', std::numeric_limits<char>::min(),
                             std::numeric_limits<char>::max()};
  const Blob blob(std::begin(kTestBytes), std::end(kTestBytes));
  const string obtained_string = BlobToString(blob);
  EXPECT_EQ(string(std::begin(kTestBytes), std::end(kTestBytes)),
            obtained_string);
  const Blob obtained_blob = BlobFromString(obtained_string);
  EXPECT_EQ(blob, obtained_blob);
}

// Tests CombineBlobs().
TEST(BlobTest, CombineBlobs) {
  const Blob kEmpty;
  const Blob kBlob1 = {1};
  const Blob kBlob2 = {2};
  const Blob kBlob3 = {3};
  const Blob kBlob12 = {1, 2};
  const Blob kBlob123 = {1, 2, 3};
  EXPECT_EQ(kBlob123, CombineBlobs({kBlob12, kBlob3}));
  EXPECT_EQ(kBlob123, CombineBlobs({kBlob1, kBlob2, kBlob3}));
  EXPECT_EQ(kBlob12, CombineBlobs({kBlob12}));
  EXPECT_EQ(kBlob12, CombineBlobs({kEmpty, kBlob1, kEmpty, kBlob2, kEmpty}));
  EXPECT_EQ(kEmpty, CombineBlobs({}));
}

class SecureBlobTest : public ::testing::Test {
 public:
  SecureBlobTest() {}
  virtual ~SecureBlobTest() {}

  static bool FindBlobInBlob(const brillo::SecureBlob& haystack,
                             const brillo::SecureBlob& needle) {
    auto pos = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end());
    return (pos != haystack.end());
  }

  static int FindBlobIndexInBlob(const brillo::SecureBlob& haystack,
                                 const brillo::SecureBlob& needle) {
    auto pos = std::search(
        haystack.begin(), haystack.end(), needle.begin(), needle.end());
    if (pos == haystack.end()) {
      return -1;
    }
    return std::distance(haystack.begin(), pos);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureBlobTest);
};

TEST_F(SecureBlobTest, AllocationSizeTest) {
  // Checks that allocating a SecureBlob of a specified size works.
  SecureBlob blob(32);

  EXPECT_EQ(32, blob.size());
}

TEST_F(SecureBlobTest, ConstructorCountValueTest) {
  // Checks that constructing a SecureBlob with |count| copies of |value| works.
  SecureBlob blob(32, 'a');

  for (size_t i = 0; i < blob.size(); i++) {
    EXPECT_EQ('a', blob[i]);
  }
}

TEST_F(SecureBlobTest, ConstructorAmbiguousTest) {
  // This test will become important once SecureBlob stops inheriting from Blob.
  SecureBlob blob(32, 0);

  for (size_t i = 0; i < blob.size(); i++) {
    EXPECT_EQ(0, blob[i]);
  }
}

TEST_F(SecureBlobTest, ConstructorIteratorTest) {
  // Checks that constructing a SecureBlob with an iterator works.
  unsigned char from_data[32];
  std::iota(std::begin(from_data), std::end(from_data), 0);

  SecureBlob blob(std::begin(from_data), std::end(from_data));

  EXPECT_EQ(sizeof(from_data), blob.size());

  for (unsigned int i = 0; i < sizeof(from_data); i++) {
    EXPECT_EQ(from_data[i], blob[i]);
  }
}

TEST_F(SecureBlobTest, BlobConstructorTest) {
  // Check that constructing a SecureBlob from a Blob works.
  const std::vector<uint8_t> bytes = {0, 1, 255};
  const Blob blob(bytes);
  const SecureBlob secure_blob(blob);
  EXPECT_EQ(bytes,
            std::vector<uint8_t>(secure_blob.begin(), secure_blob.end()));
}

TEST_F(SecureBlobTest, IteratorTest) {
  // Checks that SecureBlob::begin(), SecureBlob::end() work.
  unsigned char from_data[32];
  std::iota(std::begin(from_data), std::end(from_data), 0);

  SecureBlob blob(std::begin(from_data), std::end(from_data));

  EXPECT_EQ(sizeof(from_data), blob.size());

  size_t i = 0;
  for (auto it = blob.begin(); it != blob.end(); ++it) {
    EXPECT_EQ(from_data[i], *it);
    ++i;
  }
}

TEST_F(SecureBlobTest, AssignTest) {
  // Checks that .assign() works.
  unsigned char from_data[32];
  std::iota(std::begin(from_data), std::end(from_data), 0);

  SecureBlob blob;
  blob.assign(std::begin(from_data), std::end(from_data));

  EXPECT_EQ(sizeof(from_data), blob.size());

  size_t i = 0;
  for (auto it = blob.begin(); it != blob.end(); ++it) {
    EXPECT_EQ(from_data[i], *it);
    ++i;
  }

  SecureBlob blob2;
  blob2.assign(blob.begin(), blob.end());

  EXPECT_EQ(blob, blob2);
}

// Disable ResizeTest with Address Sanitizer.
// https://crbug.com/806013
#ifndef BRILLO_ASAN_BUILD
TEST_F(SecureBlobTest, ResizeTest) {
  // Check that resizing a SecureBlob wipes the excess memory. The test assumes
  // that resize() down by one will not re-allocate the memory, so the last byte
  // will still be part of the SecureBlob's allocation.
  size_t length = 1024;
  SecureBlob blob(length);
  void* original_data = blob.data();
  for (size_t i = 0; i < length; i++) {
    blob[i] = i;
  }

  blob.resize(length - 1);

  EXPECT_EQ(original_data, blob.data());
  EXPECT_EQ(length - 1, blob.size());
  EXPECT_EQ(0, blob.data()[length - 1]);
}
#endif

TEST_F(SecureBlobTest, CombineTest) {
  SecureBlob blob1(32);
  SecureBlob blob2(32);
  std::iota(blob1.begin(), blob1.end(), 0);
  std::iota(blob2.begin(), blob2.end(), 32);
  SecureBlob combined_blob = SecureBlob::Combine(blob1, blob2);
  EXPECT_EQ(combined_blob.size(), (blob1.size() + blob2.size()));
  EXPECT_TRUE(SecureBlobTest::FindBlobInBlob(combined_blob, blob1));
  EXPECT_TRUE(SecureBlobTest::FindBlobInBlob(combined_blob, blob2));
  int blob1_index = SecureBlobTest::FindBlobIndexInBlob(combined_blob, blob1);
  int blob2_index = SecureBlobTest::FindBlobIndexInBlob(combined_blob, blob2);
  EXPECT_EQ(blob1_index, 0);
  EXPECT_EQ(blob2_index, 32);
}

TEST_F(SecureBlobTest, BlobToStringTest) {
  std::string test_string("Test String");
  SecureBlob blob = SecureBlob(test_string.begin(), test_string.end());
  EXPECT_EQ(blob.size(), test_string.length());
  std::string result_string = blob.to_string();
  EXPECT_EQ(test_string.compare(result_string), 0);
}

TEST_F(SecureBlobTest, HexStringToSecureBlob) {
  std::string hex_string("112233445566778899aabbccddeeff0f");

  SecureBlob blob;
  SecureBlob::HexStringToSecureBlob(hex_string, &blob);

  EXPECT_EQ(blob.size(), 16u);
  EXPECT_EQ(blob[0],  0x11);
  EXPECT_EQ(blob[1],  0x22);
  EXPECT_EQ(blob[2],  0x33);
  EXPECT_EQ(blob[3],  0x44);
  EXPECT_EQ(blob[4],  0x55);
  EXPECT_EQ(blob[5],  0x66);
  EXPECT_EQ(blob[6],  0x77);
  EXPECT_EQ(blob[7],  0x88);
  EXPECT_EQ(blob[8],  0x99);
  EXPECT_EQ(blob[9],  0xaa);
  EXPECT_EQ(blob[10], 0xbb);
  EXPECT_EQ(blob[11], 0xcc);
  EXPECT_EQ(blob[12], 0xdd);
  EXPECT_EQ(blob[13], 0xee);
  EXPECT_EQ(blob[14], 0xff);
  EXPECT_EQ(blob[15], 0x0f);
}

}  // namespace brillo
