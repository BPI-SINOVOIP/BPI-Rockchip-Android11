/*
 * Copyright (C) 2020 The Android Open Source Project
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

//
// Test that metadata encryption is working, via:
//
// - Correctness tests.  These test the standard metadata encryption formats
//   supported by Android R and higher via dm-default-key v2.
//
// - Randomness test.  This runs on all devices that use metadata encryption.
//
// The correctness tests create a temporary default-key mapping over the raw
// userdata partition, read from it, and verify that the data got decrypted
// correctly.  This only tests decryption, since this avoids having to find a
// region on disk that can safely be modified.  This should be good enough since
// the device wouldn't work anyway if decryption didn't invert encryption.
//
// Note that this temporary default-key mapping will overlap the device's "real"
// default-key mapping, if the device has one.  The kernel allows this.  The
// tests don't use a loopback device instead, since dm-default-key over a
// loopback device can't use the real inline encryption hardware.
//
// The correctness tests cover the following settings:
//
//    metadata_encryption=aes-256-xts
//    metadata_encryption=adiantum
//    metadata_encryption=aes-256-xts:wrappedkey_v0
//
// The tests don't check which one of those settings, if any, the device is
// actually using; they just try to test everything they can.
//
// These tests don't specifically test that file contents aren't encrypted
// twice.  That's already implied by the file-based encryption test cases,
// provided that the device actually has metadata encryption enabled.
//

#include <android-base/file.h>
#include <android-base/unique_fd.h>
#include <asm/byteorder.h>
#include <fcntl.h>
#include <fstab/fstab.h>
#include <gtest/gtest.h>
#include <libdm/dm.h>
#include <linux/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <chrono>

#include "vts_kernel_encryption.h"

using namespace android::dm;

namespace android {
namespace kernel {

#define cpu_to_le64 __cpu_to_le64
#define le64_to_cpu __le64_to_cpu

// Alignment to use for direct I/O reads of block devices
static constexpr int kDirectIOAlignment = 4096;

// Assumed size of filesystem blocks, in bytes
static constexpr int kFilesystemBlockSize = 4096;

// Checks whether the kernel supports version 2 or higher of dm-default-key.
static bool IsDmDefaultKeyV2Supported(DeviceMapper &dm) {
  DmTargetTypeInfo info;
  if (!dm.GetTargetByName("default-key", &info)) {
    GTEST_LOG_(INFO) << "dm-default-key not enabled";
    return false;
  }
  if (!info.IsAtLeast(2, 0, 0)) {
    // The legacy version of dm-default-key (which was never supported by the
    // Android common kernels) used a vendor-specific on-disk format, so it's
    // not testable by a vendor-independent test.
    GTEST_LOG_(INFO) << "Detected legacy dm-default-key";
    return false;
  }
  return true;
}

// Reads |count| bytes from the beginning of |blk_device|, using direct I/O to
// avoid getting any stale cached data.  Direct I/O requires using a hardware
// sector size aligned buffer.
static bool ReadBlockDevice(const std::string &blk_device, size_t count,
                            std::vector<uint8_t> *data) {
  GTEST_LOG_(INFO) << "Reading " << count << " bytes from " << blk_device;
  std::unique_ptr<void, void (*)(void *)> buf_mem(
      aligned_alloc(kDirectIOAlignment, count), free);
  if (buf_mem == nullptr) {
    ADD_FAILURE() << "out of memory";
    return false;
  }
  uint8_t *buffer = static_cast<uint8_t *>(buf_mem.get());

  android::base::unique_fd fd(
      open(blk_device.c_str(), O_RDONLY | O_DIRECT | O_CLOEXEC));
  if (fd < 0) {
    ADD_FAILURE() << "Failed to open " << blk_device << Errno();
    return false;
  }
  if (!android::base::ReadFully(fd, buffer, count)) {
    ADD_FAILURE() << "Failed to read from " << blk_device << Errno();
    return false;
  }

  *data = std::vector<uint8_t>(buffer, buffer + count);
  return true;
}

class DmDefaultKeyTest : public ::testing::Test {
  // Name to assign to the dm-default-key test device
  static constexpr const char *kTestDmDeviceName = "vts-test-default-key";

  // Filesystem whose underlying partition the test will use
  static constexpr const char *kTestMountpoint = "/data";

  // Size of the dm-default-key crypto sector size (data unit size) in bytes
  static constexpr int kCryptoSectorSize = 4096;

  // Size of the test data in crypto sectors
  static constexpr int kTestDataSectors = 256;

  // Size of the test data in bytes
  static constexpr int kTestDataBytes = kTestDataSectors * kCryptoSectorSize;

  // Device-mapper API sector size in bytes.
  // This is unrelated to the crypto sector size.
  static constexpr int kDmApiSectorSize = 512;

 protected:
  void SetUp() override;
  void TearDown() override;
  bool CreateTestDevice(const std::string &cipher,
                        const std::vector<uint8_t> &key, bool is_wrapped_key);
  void VerifyDecryption(const std::vector<uint8_t> &key, const Cipher &cipher);
  void DoTest(const std::string &cipher_string, const Cipher &cipher);
  bool skip_test_ = false;
  DeviceMapper *dm_ = nullptr;
  std::string raw_blk_device_;
  std::string dm_device_path_;
};

// Test setup procedure.  Checks for the needed kernel support, finds the raw
// partition to use, and does other preparations.  skip_test_ is set to true if
// the test should be skipped.
void DmDefaultKeyTest::SetUp() {
  dm_ = &DeviceMapper::Instance();

  if (!IsDmDefaultKeyV2Supported(*dm_)) {
    int first_api_level;
    ASSERT_TRUE(GetFirstApiLevel(&first_api_level));
    // Devices launching with R or higher must support dm-default-key v2.
    ASSERT_LE(first_api_level, __ANDROID_API_Q__);
    GTEST_LOG_(INFO)
        << "Skipping test because dm-default-key v2 is unsupported";
    skip_test_ = true;
    return;
  }

  FilesystemInfo fs_info;
  ASSERT_TRUE(GetFilesystemInfo(kTestMountpoint, &fs_info));
  raw_blk_device_ = fs_info.raw_blk_device;

  dm_->DeleteDevice(kTestDmDeviceName);
}

void DmDefaultKeyTest::TearDown() { dm_->DeleteDevice(kTestDmDeviceName); }

// Creates the test dm-default-key mapping using the given key and settings.
// If the dm device creation fails, then it is assumed the kernel doesn't
// support the given encryption settings, and a failure is not added.
bool DmDefaultKeyTest::CreateTestDevice(const std::string &cipher,
                                        const std::vector<uint8_t> &key,
                                        bool is_wrapped_key) {
  static_assert(kTestDataBytes % kDmApiSectorSize == 0);
  std::unique_ptr<DmTargetDefaultKey> target =
      std::make_unique<DmTargetDefaultKey>(0, kTestDataBytes / kDmApiSectorSize,
                                           cipher.c_str(), BytesToHex(key),
                                           raw_blk_device_, 0);
  target->SetSetDun();
  if (is_wrapped_key) target->SetWrappedKeyV0();

  DmTable table;
  if (!table.AddTarget(std::move(target))) {
    ADD_FAILURE() << "Failed to add default-key target to table";
    return false;
  }
  if (!table.valid()) {
    ADD_FAILURE() << "Device-mapper table failed to validate";
    return false;
  }
  if (!dm_->CreateDevice(kTestDmDeviceName, table, &dm_device_path_,
                         std::chrono::seconds(5))) {
    GTEST_LOG_(INFO) << "Unable to create default-key mapping" << Errno()
                     << ".  Assuming that the encryption settings cipher=\""
                     << cipher << "\", is_wrapped_key=" << is_wrapped_key
                     << " are unsupported and skipping the test.";
    return false;
  }
  GTEST_LOG_(INFO) << "Created default-key mapping at " << dm_device_path_
                   << " using cipher=\"" << cipher
                   << "\", key=" << BytesToHex(key)
                   << ", is_wrapped_key=" << is_wrapped_key;
  return true;
}

void DmDefaultKeyTest::VerifyDecryption(const std::vector<uint8_t> &key,
                                        const Cipher &cipher) {
  std::vector<uint8_t> raw_data;
  std::vector<uint8_t> decrypted_data;

  ASSERT_TRUE(ReadBlockDevice(raw_blk_device_, kTestDataBytes, &raw_data));
  ASSERT_TRUE(
      ReadBlockDevice(dm_device_path_, kTestDataBytes, &decrypted_data));

  // Verify that the decrypted data encrypts to the raw data.

  GTEST_LOG_(INFO) << "Verifying correctness of decrypted data";

  // Initialize the IV for crypto sector 0.
  ASSERT_GE(cipher.ivsize(), sizeof(__le64));
  std::unique_ptr<__le64> iv(new (::operator new(cipher.ivsize())) __le64);
  memset(iv.get(), 0, cipher.ivsize());

  // Encrypt each sector.
  std::vector<uint8_t> encrypted_data(kTestDataBytes);
  static_assert(kTestDataBytes % kCryptoSectorSize == 0);
  for (size_t i = 0; i < kTestDataBytes; i += kCryptoSectorSize) {
    ASSERT_TRUE(cipher.Encrypt(key, reinterpret_cast<const uint8_t *>(iv.get()),
                               &decrypted_data[i], &encrypted_data[i],
                               kCryptoSectorSize));

    // Update the IV by incrementing the crypto sector number.
    *iv = cpu_to_le64(le64_to_cpu(*iv) + 1);
  }

  ASSERT_EQ(encrypted_data, raw_data);
}

void DmDefaultKeyTest::DoTest(const std::string &cipher_string,
                              const Cipher &cipher) {
  if (skip_test_) return;

  std::vector<uint8_t> key = GenerateTestKey(cipher.keysize());

  if (!CreateTestDevice(cipher_string, key, false)) return;

  VerifyDecryption(key, cipher);
}

// Tests dm-default-key parameters matching metadata_encryption=aes-256-xts.
TEST_F(DmDefaultKeyTest, TestAes256Xts) {
  DoTest("aes-xts-plain64", Aes256XtsCipher());
}

// Tests dm-default-key parameters matching metadata_encryption=adiantum.
TEST_F(DmDefaultKeyTest, TestAdiantum) {
  DoTest("xchacha12,aes-adiantum-plain64", AdiantumCipher());
}

// Tests dm-default-key parameters matching
// metadata_encryption=aes-256-xts:wrappedkey_v0.
TEST_F(DmDefaultKeyTest, TestHwWrappedKey) {
  if (skip_test_) return;

  std::vector<uint8_t> master_key, exported_key;
  if (!CreateHwWrappedKey(&master_key, &exported_key)) return;

  if (!CreateTestDevice("aes-xts-plain64", exported_key, true)) return;

  std::vector<uint8_t> enc_key;
  ASSERT_TRUE(DeriveHwWrappedEncryptionKey(master_key, &enc_key));

  VerifyDecryption(enc_key, Aes256XtsCipher());
}

// Tests that if the device uses metadata encryption, then the first
// kFilesystemBlockSize bytes of the userdata partition appear random.  For ext4
// and f2fs, this block should contain the filesystem superblock; it therefore
// should be initialized and metadata-encrypted.  Ideally we'd check additional
// blocks too, but that would require awareness of the filesystem structure.
//
// This isn't as strong a test as the correctness tests, but it's useful because
// it applies regardless of the encryption format and key.  Thus it runs even on
// old devices, including ones that used a vendor-specific encryption format.
TEST(MetadataEncryptionTest, TestRandomness) {
  constexpr const char *mountpoint = "/data";

  android::fs_mgr::Fstab fstab;
  ASSERT_TRUE(android::fs_mgr::ReadDefaultFstab(&fstab));
  const fs_mgr::FstabEntry *entry = GetEntryForMountPoint(&fstab, mountpoint);
  ASSERT_TRUE(entry != nullptr);

  if (entry->metadata_key_dir.empty()) {
    int first_api_level;
    ASSERT_TRUE(GetFirstApiLevel(&first_api_level));
    ASSERT_LE(first_api_level, __ANDROID_API_Q__)
        << "Metadata encryption is required";
    GTEST_LOG_(INFO)
        << "Skipping test because device doesn't use metadata encryption";
    return;
  }

  GTEST_LOG_(INFO) << "Verifying randomness of ciphertext";
  std::vector<uint8_t> raw_data;
  FilesystemInfo fs_info;
  ASSERT_TRUE(GetFilesystemInfo(mountpoint, &fs_info));
  ASSERT_TRUE(
      ReadBlockDevice(fs_info.raw_blk_device, kFilesystemBlockSize, &raw_data));
  ASSERT_TRUE(VerifyDataRandomness(raw_data));
}

}  // namespace kernel
}  // namespace android
