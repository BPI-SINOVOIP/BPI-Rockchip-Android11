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
// Test that file contents encryption is working, via:
//
// - Correctness tests.  These test the standard FBE settings supported by
//   Android R and higher.
//
// - Randomness test.  This runs on all devices that use FBE, even old ones.
//
// The correctness tests cover the following settings:
//
//    fileencryption=aes-256-xts:aes-256-cts:v2
//    fileencryption=aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized
//    fileencryption=aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized+wrappedkey_v0
//    fileencryption=aes-256-xts:aes-256-cts:v2+emmc_optimized
//    fileencryption=aes-256-xts:aes-256-cts:v2+emmc_optimized+wrappedkey_v0
//    fileencryption=adiantum:adiantum:v2
//
// On devices launching with R or higher those are equivalent to simply:
//
//    fileencryption=
//    fileencryption=::inlinecrypt_optimized
//    fileencryption=::inlinecrypt_optimized+wrappedkey_v0
//    fileencryption=::emmc_optimized
//    fileencryption=::emmc_optimized+wrappedkey_v0
//    fileencryption=adiantum
//
// The tests don't check which one of those settings, if any, the device is
// actually using; they just try to test everything they can.
// "fileencryption=aes-256-xts" is guaranteed to be available if the kernel
// supports any "fscrypt v2" features at all.  The others may not be available,
// so the tests take that into account and skip testing them when unavailable.
//
// None of these tests should ever fail.  In particular, vendors must not break
// any standard FBE settings, regardless of what the device actually uses.  If
// any test fails, make sure to check things like the byte order of keys.
//

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <asm/byteorder.h>
#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <limits.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <linux/fscrypt.h>
#include <openssl/evp.h>
#include <openssl/hkdf.h>
#include <openssl/siphash.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "vts_kernel_encryption.h"

#ifndef F2FS_IOCTL_MAGIC
#define F2FS_IOCTL_MAGIC 0xf5
#endif
#ifndef F2FS_IOC_SET_PIN_FILE
#define F2FS_IOC_SET_PIN_FILE _IOW(F2FS_IOCTL_MAGIC, 13, __u32)
#endif

#ifndef FS_IOC_GET_ENCRYPTION_NONCE
#define FS_IOC_GET_ENCRYPTION_NONCE _IOR('f', 27, __u8[16])
#endif

#ifndef FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32
#define FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32 0x10
#endif

namespace android {
namespace kernel {

// Assumed size of filesystem blocks, in bytes
constexpr int kFilesystemBlockSize = 4096;

// Size of the test file in filesystem blocks
constexpr int kTestFileBlocks = 256;

// Size of the test file in bytes
constexpr int kTestFileBytes = kFilesystemBlockSize * kTestFileBlocks;

// fscrypt master key size in bytes
constexpr int kFscryptMasterKeySize = 64;

// fscrypt maximum IV size in bytes
constexpr int kFscryptMaxIVSize = 32;

// fscrypt per-file nonce size in bytes
constexpr int kFscryptFileNonceSize = 16;

// fscrypt HKDF context bytes, from kernel fs/crypto/fscrypt_private.h
enum FscryptHkdfContext {
  HKDF_CONTEXT_KEY_IDENTIFIER = 1,
  HKDF_CONTEXT_PER_FILE_ENC_KEY = 2,
  HKDF_CONTEXT_DIRECT_KEY = 3,
  HKDF_CONTEXT_IV_INO_LBLK_64_KEY = 4,
  HKDF_CONTEXT_DIRHASH_KEY = 5,
  HKDF_CONTEXT_IV_INO_LBLK_32_KEY = 6,
  HKDF_CONTEXT_INODE_HASH_KEY = 7,
};

struct FscryptFileNonce {
  uint8_t bytes[kFscryptFileNonceSize];
};

// Format of the initialization vector
union FscryptIV {
  struct {
    __le32 lblk_num;      // file logical block number, starts at 0
    __le32 inode_number;  // only used for IV_INO_LBLK_64
    uint8_t file_nonce[kFscryptFileNonceSize];  // only used for DIRECT_KEY
  };
  uint8_t bytes[kFscryptMaxIVSize];
};

struct TestFileInfo {
  std::vector<uint8_t> plaintext;
  std::vector<uint8_t> actual_ciphertext;
  uint64_t inode_number;
  FscryptFileNonce nonce;
};

static bool GetInodeNumber(const std::string &path, uint64_t *inode_number) {
  struct stat stbuf;
  if (stat(path.c_str(), &stbuf) != 0) {
    ADD_FAILURE() << "Failed to stat " << path << Errno();
    return false;
  }
  *inode_number = stbuf.st_ino;
  return true;
}

//
// Checks whether the kernel has support for the following fscrypt features:
//
// - Filesystem-level keyring (FS_IOC_ADD_ENCRYPTION_KEY and
//   FS_IOC_REMOVE_ENCRYPTION_KEY)
// - v2 encryption policies
// - The IV_INO_LBLK_64 encryption policy flag
// - The FS_IOC_GET_ENCRYPTION_NONCE ioctl
// - The IV_INO_LBLK_32 encryption policy flag
//
// To do this it's sufficient to just check whether FS_IOC_ADD_ENCRYPTION_KEY is
// available, as the other features were added in the same AOSP release.
//
// The easiest way to do this is to just execute the ioctl with a NULL argument.
// If available it will fail with EFAULT; otherwise it will fail with ENOTTY.
//
static bool IsFscryptV2Supported(const std::string &mountpoint) {
  android::base::unique_fd fd(
      open(mountpoint.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (fd < 0) {
    ADD_FAILURE() << "Failed to open " << mountpoint << Errno();
    return false;
  }

  if (ioctl(fd, FS_IOC_ADD_ENCRYPTION_KEY, nullptr) == 0) {
    ADD_FAILURE()
        << "FS_IOC_ADD_ENCRYPTION_KEY(nullptr) unexpectedly succeeded on "
        << mountpoint;
    return false;
  }
  switch (errno) {
    case EFAULT:
      return true;
    case ENOTTY:
      GTEST_LOG_(INFO) << "No support for FS_IOC_ADD_ENCRYPTION_KEY on "
                       << mountpoint;
      return false;
    default:
      ADD_FAILURE()
          << "Unexpected error from FS_IOC_ADD_ENCRYPTION_KEY(nullptr) on "
          << mountpoint << Errno();
      return false;
  }
}

// Helper class to pin / unpin a file on f2fs, to prevent f2fs from moving the
// file's blocks while the test is accessing them via the underlying device.
//
// This can be used without checking the filesystem type, since on other
// filesystem types F2FS_IOC_SET_PIN_FILE will just fail and do nothing.
class ScopedF2fsFilePinning {
 public:
  explicit ScopedF2fsFilePinning(int fd) : fd_(fd) {
    __u32 set = 1;
    ioctl(fd_, F2FS_IOC_SET_PIN_FILE, &set);
  }

  ~ScopedF2fsFilePinning() {
    __u32 set = 0;
    ioctl(fd_, F2FS_IOC_SET_PIN_FILE, &set);
  }

 private:
  int fd_;
};

// Reads the raw data of the file specified by |fd| from its underlying block
// device |blk_device|.  The file has |expected_data_size| bytes of initialized
// data; this must be a multiple of the filesystem block size
// kFilesystemBlockSize.  The file may contain holes, in which case only the
// non-holes are read; the holes are not counted in |expected_data_size|.
static bool ReadRawDataOfFile(int fd, const std::string &blk_device,
                              int expected_data_size,
                              std::vector<uint8_t> *raw_data) {
  int max_extents = expected_data_size / kFilesystemBlockSize;

  EXPECT_TRUE(expected_data_size % kFilesystemBlockSize == 0);

  // It's not entirely clear how F2FS_IOC_SET_PIN_FILE interacts with dirty
  // data, so do an extra sync here and don't just rely on FIEMAP_FLAG_SYNC.
  if (fsync(fd) != 0) {
    ADD_FAILURE() << "Failed to sync file" << Errno();
    return false;
  }

  ScopedF2fsFilePinning pinned_file(fd);  // no-op on non-f2fs

  // Query the file's extents.
  size_t allocsize = offsetof(struct fiemap, fm_extents[max_extents]);
  std::unique_ptr<struct fiemap> map(
      new (::operator new(allocsize)) struct fiemap);
  memset(map.get(), 0, allocsize);
  map->fm_flags = FIEMAP_FLAG_SYNC;
  map->fm_length = UINT64_MAX;
  map->fm_extent_count = max_extents;
  if (ioctl(fd, FS_IOC_FIEMAP, map.get()) != 0) {
    ADD_FAILURE() << "Failed to get extents of file" << Errno();
    return false;
  }

  // Read the raw data, using direct I/O to avoid getting any stale cached data.
  // Direct I/O requires using a block size aligned buffer.

  std::unique_ptr<void, void (*)(void *)> buf_mem(
      aligned_alloc(kFilesystemBlockSize, expected_data_size), free);
  if (buf_mem == nullptr) {
    ADD_FAILURE() << "Out of memory";
    return false;
  }
  uint8_t *buf = static_cast<uint8_t *>(buf_mem.get());
  int offset = 0;

  android::base::unique_fd blk_fd(
      open(blk_device.c_str(), O_RDONLY | O_DIRECT | O_CLOEXEC));
  if (blk_fd < 0) {
    ADD_FAILURE() << "Failed to open raw block device " << blk_device
                  << Errno();
    return false;
  }

  for (int i = 0; i < map->fm_mapped_extents; i++) {
    const struct fiemap_extent &extent = map->fm_extents[i];

    GTEST_LOG_(INFO) << "Extent " << i + 1 << " of " << map->fm_mapped_extents
                     << " is logical offset " << extent.fe_logical
                     << ", physical offset " << extent.fe_physical
                     << ", length " << extent.fe_length << ", flags 0x"
                     << std::hex << extent.fe_flags << std::dec;
    // Make sure the flags indicate that fe_physical is actually valid.
    if (extent.fe_flags & (FIEMAP_EXTENT_UNKNOWN | FIEMAP_EXTENT_UNWRITTEN)) {
      ADD_FAILURE() << "Unsupported extent flags: 0x" << std::hex
                    << extent.fe_flags << std::dec;
      return false;
    }
    if (extent.fe_length % kFilesystemBlockSize != 0) {
      ADD_FAILURE() << "Extent is not aligned to filesystem block size";
      return false;
    }
    if (extent.fe_length > expected_data_size - offset) {
      ADD_FAILURE() << "File is longer than expected";
      return false;
    }
    if (pread(blk_fd, &buf[offset], extent.fe_length, extent.fe_physical) !=
        extent.fe_length) {
      ADD_FAILURE() << "Error reading raw data from block device" << Errno();
      return false;
    }
    offset += extent.fe_length;
  }
  if (offset != expected_data_size) {
    ADD_FAILURE() << "File is shorter than expected";
    return false;
  }
  *raw_data = std::vector<uint8_t>(&buf[0], &buf[offset]);
  return true;
}

// Writes |plaintext| to a file |path| located on the block device |blk_device|.
// Returns in |ciphertext| the file's raw ciphertext read from |blk_device|.
static bool WriteTestFile(const std::vector<uint8_t> &plaintext,
                          const std::string &path,
                          const std::string &blk_device,
                          std::vector<uint8_t> *ciphertext) {
  GTEST_LOG_(INFO) << "Creating test file " << path << " containing "
                   << plaintext.size() << " bytes of data";
  android::base::unique_fd fd(
      open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600));
  if (fd < 0) {
    ADD_FAILURE() << "Failed to create " << path << Errno();
    return false;
  }
  if (!android::base::WriteFully(fd, plaintext.data(), plaintext.size())) {
    ADD_FAILURE() << "Error writing to " << path << Errno();
    return false;
  }

  GTEST_LOG_(INFO) << "Reading the raw ciphertext of " << path << " from disk";
  if (!ReadRawDataOfFile(fd, blk_device, plaintext.size(), ciphertext)) {
    ADD_FAILURE() << "Failed to read the raw ciphertext of " << path;
    return false;
  }
  return true;
}

class FBEPolicyTest : public ::testing::Test {
 protected:
  // Location of the test directory and file.  Since it's not possible to
  // override an existing encryption policy, in order for these tests to set
  // their own encryption policy the parent directory must be unencrypted.
  static constexpr const char *kTestMountpoint = "/data";
  static constexpr const char *kTestDir = "/data/unencrypted/vts-test-dir";
  static constexpr const char *kTestFile =
      "/data/unencrypted/vts-test-dir/file";

  void SetUp() override;
  void TearDown() override;
  bool SetMasterKey(const std::vector<uint8_t> &master_key, uint32_t flags = 0,
                    bool required = true);
  bool CreateAndSetHwWrappedKey(std::vector<uint8_t> *enc_key,
                                std::vector<uint8_t> *sw_secret);
  int GetSkipFlagsForInoBasedEncryption();
  bool SetEncryptionPolicy(int contents_mode, int filenames_mode, int flags,
                           int skip_flags);
  bool GenerateTestFile(TestFileInfo *info);
  bool VerifyKeyIdentifier(const std::vector<uint8_t> &master_key);
  bool DerivePerModeEncryptionKey(const std::vector<uint8_t> &master_key,
                                  int mode, FscryptHkdfContext context,
                                  std::vector<uint8_t> &enc_key);
  bool DerivePerFileEncryptionKey(const std::vector<uint8_t> &master_key,
                                  const FscryptFileNonce &nonce,
                                  std::vector<uint8_t> &enc_key);
  void VerifyCiphertext(const std::vector<uint8_t> &enc_key,
                        const FscryptIV &starting_iv, const Cipher &cipher,
                        const TestFileInfo &file_info);
  void TestEmmcOptimizedDunWraparound(const std::vector<uint8_t> &master_key,
                                      const std::vector<uint8_t> &enc_key);
  struct fscrypt_key_specifier master_key_specifier_;
  bool skip_test_ = false;
  bool key_added_ = false;
  FilesystemInfo fs_info_;
};

// Test setup procedure.  Creates a test directory kTestDir and does other
// preparations. skip_test_ is set to true if the test should be skipped.
void FBEPolicyTest::SetUp() {
  if (!IsFscryptV2Supported(kTestMountpoint)) {
    int first_api_level;
    ASSERT_TRUE(GetFirstApiLevel(&first_api_level));
    // Devices launching with R or higher must support fscrypt v2.
    ASSERT_LE(first_api_level, __ANDROID_API_Q__);
    GTEST_LOG_(INFO) << "Skipping test because fscrypt v2 is unsupported";
    skip_test_ = true;
    return;
  }

  ASSERT_TRUE(GetFilesystemInfo(kTestMountpoint, &fs_info_));

  DeleteRecursively(kTestDir);
  if (mkdir(kTestDir, 0700) != 0) {
    FAIL() << "Failed to create " << kTestDir << Errno();
  }
}

void FBEPolicyTest::TearDown() {
  DeleteRecursively(kTestDir);

  // Remove the test key from kTestMountpoint.
  if (key_added_) {
    android::base::unique_fd mntfd(
        open(kTestMountpoint, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (mntfd < 0) {
      FAIL() << "Failed to open " << kTestMountpoint << Errno();
    }
    struct fscrypt_remove_key_arg arg;
    memset(&arg, 0, sizeof(arg));
    arg.key_spec = master_key_specifier_;

    if (ioctl(mntfd, FS_IOC_REMOVE_ENCRYPTION_KEY, &arg) != 0) {
      FAIL() << "FS_IOC_REMOVE_ENCRYPTION_KEY failed on " << kTestMountpoint
             << Errno();
    }
  }
}

// Adds |master_key| to kTestMountpoint and places the resulting key identifier
// in master_key_specifier_.
bool FBEPolicyTest::SetMasterKey(const std::vector<uint8_t> &master_key,
                                 uint32_t flags, bool required) {
  size_t allocsize = sizeof(struct fscrypt_add_key_arg) + master_key.size();
  std::unique_ptr<struct fscrypt_add_key_arg> arg(
      new (::operator new(allocsize)) struct fscrypt_add_key_arg);
  memset(arg.get(), 0, allocsize);
  arg->key_spec.type = FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER;
  arg->__flags = flags;
  arg->raw_size = master_key.size();
  std::copy(master_key.begin(), master_key.end(), arg->raw);

  GTEST_LOG_(INFO) << "Adding fscrypt master key, flags are 0x" << std::hex
                   << flags << std::dec << ", raw bytes are "
                   << BytesToHex(master_key);
  android::base::unique_fd mntfd(
      open(kTestMountpoint, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (mntfd < 0) {
    ADD_FAILURE() << "Failed to open " << kTestMountpoint << Errno();
    return false;
  }
  if (ioctl(mntfd, FS_IOC_ADD_ENCRYPTION_KEY, arg.get()) != 0) {
    if (required || (errno != EINVAL && errno != EOPNOTSUPP)) {
      ADD_FAILURE() << "FS_IOC_ADD_ENCRYPTION_KEY failed on " << kTestMountpoint
                    << Errno();
    }
    return false;
  }
  master_key_specifier_ = arg->key_spec;
  GTEST_LOG_(INFO) << "Master key identifier is "
                   << BytesToHex(master_key_specifier_.u.identifier);
  key_added_ = true;
  if (!(flags & __FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED) &&
      !VerifyKeyIdentifier(master_key))
    return false;
  return true;
}

// Creates a hardware-wrapped key, adds it to the filesystem, and derives the
// corresponding inline encryption key |enc_key| and software secret
// |sw_secret|.  Returns false if unsuccessful (either the test failed, or the
// device doesn't support hardware-wrapped keys so the test should be skipped).
bool FBEPolicyTest::CreateAndSetHwWrappedKey(std::vector<uint8_t> *enc_key,
                                             std::vector<uint8_t> *sw_secret) {
  std::vector<uint8_t> master_key, exported_key;
  if (!CreateHwWrappedKey(&master_key, &exported_key)) return false;

  if (!SetMasterKey(exported_key, __FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED, false)) {
    if (!HasFailure()) {
      GTEST_LOG_(INFO) << "Skipping test because kernel doesn't support "
                          "hardware-wrapped keys";
    }
    return false;
  }

  if (!DeriveHwWrappedEncryptionKey(master_key, enc_key)) return false;
  if (!DeriveHwWrappedRawSecret(master_key, sw_secret)) return false;

  if (!VerifyKeyIdentifier(*sw_secret)) return false;

  return true;
}

enum {
  kSkipIfNoPolicySupport = 1 << 0,
  kSkipIfNoCryptoAPISupport = 1 << 1,
  kSkipIfNoHardwareSupport = 1 << 2,
};

// Returns 0 if encryption policies that include the inode number in the IVs
// (e.g. IV_INO_LBLK_64) are guaranteed to be settable on the test filesystem.
// Else returns kSkipIfNoPolicySupport.
//
// On f2fs, they're always settable.  On ext4, they're only settable if the
// filesystem has the 'stable_inodes' feature flag.  Android only sets
// 'stable_inodes' if the device uses one of these encryption policies "for
// real", e.g. "fileencryption=::inlinecrypt_optimized" in fstab.  Since the
// fstab could contain something else, we have to allow the tests for these
// encryption policies to be skipped on ext4.
int FBEPolicyTest::GetSkipFlagsForInoBasedEncryption() {
  if (fs_info_.type == "ext4") return kSkipIfNoPolicySupport;
  return 0;
}

// Sets a v2 encryption policy on the test directory.  The policy will use the
// test key and the specified encryption modes and flags.  If the kernel doesn't
// support setting or using the encryption policy, then a failure will be added,
// unless the reason is covered by a bit set in |skip_flags|.
bool FBEPolicyTest::SetEncryptionPolicy(int contents_mode, int filenames_mode,
                                        int flags, int skip_flags) {
  if (!key_added_) {
    ADD_FAILURE() << "SetEncryptionPolicy called but no key added";
    return false;
  }

  struct fscrypt_policy_v2 policy;
  memset(&policy, 0, sizeof(policy));
  policy.version = FSCRYPT_POLICY_V2;
  policy.contents_encryption_mode = contents_mode;
  policy.filenames_encryption_mode = filenames_mode;
  // Always give PAD_16, to match the policies that Android sets for real.
  // It doesn't affect contents encryption, though.
  policy.flags = flags | FSCRYPT_POLICY_FLAGS_PAD_16;
  memcpy(policy.master_key_identifier, master_key_specifier_.u.identifier,
         FSCRYPT_KEY_IDENTIFIER_SIZE);

  android::base::unique_fd dirfd(
      open(kTestDir, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (dirfd < 0) {
    ADD_FAILURE() << "Failed to open " << kTestDir << Errno();
    return false;
  }
  GTEST_LOG_(INFO) << "Setting encryption policy on " << kTestDir;
  if (ioctl(dirfd, FS_IOC_SET_ENCRYPTION_POLICY, &policy) != 0) {
    if (errno == EINVAL && (skip_flags & kSkipIfNoPolicySupport)) {
      GTEST_LOG_(INFO) << "Skipping test because encryption policy is "
                          "unsupported on this filesystem / kernel";
      return false;
    }
    ADD_FAILURE() << "FS_IOC_SET_ENCRYPTION_POLICY failed on " << kTestDir
                  << " using contents_mode=" << contents_mode
                  << ", filenames_mode=" << filenames_mode << ", flags=0x"
                  << std::hex << flags << std::dec << Errno();
    return false;
  }
  if (skip_flags & (kSkipIfNoCryptoAPISupport | kSkipIfNoHardwareSupport)) {
    android::base::unique_fd fd(
        open(kTestFile, O_WRONLY | O_CREAT | O_CLOEXEC, 0600));
    if (fd < 0) {
      // Setting an encryption policy that uses modes that aren't enabled in the
      // kernel's crypto API (e.g. FSCRYPT_MODE_ADIANTUM when the kernel lacks
      // CONFIG_CRYPTO_ADIANTUM) will still succeed, but actually creating a
      // file will fail with ENOPKG.  Make sure to check for this case.
      if (errno == ENOPKG && (skip_flags & kSkipIfNoCryptoAPISupport)) {
        GTEST_LOG_(INFO)
            << "Skipping test because encryption policy is "
               "unsupported on this kernel, due to missing crypto API support";
        return false;
      }
      // We get EINVAL here when using a hardware-wrapped key and the inline
      // encryption hardware supports wrapped keys but doesn't support the
      // number of DUN bytes that the file contents encryption requires.
      if (errno == EINVAL && (skip_flags & kSkipIfNoHardwareSupport)) {
        GTEST_LOG_(INFO)
            << "Skipping test because encryption policy is not compatible with "
               "this device's inline encryption hardware";
        return false;
      }
    }
    unlink(kTestFile);
  }
  return true;
}

// Generates some test data, writes it to a file in the test directory, and
// returns in |info| the file's plaintext, the file's raw ciphertext read from
// disk, and other information about the file.
bool FBEPolicyTest::GenerateTestFile(TestFileInfo *info) {
  info->plaintext.resize(kTestFileBytes);
  RandomBytesForTesting(info->plaintext);

  if (!WriteTestFile(info->plaintext, kTestFile, fs_info_.raw_blk_device,
                     &info->actual_ciphertext))
    return false;

  android::base::unique_fd fd(open(kTestFile, O_RDONLY | O_CLOEXEC));
  if (fd < 0) {
    ADD_FAILURE() << "Failed to open " << kTestFile << Errno();
    return false;
  }

  // Get the file's inode number.
  if (!GetInodeNumber(kTestFile, &info->inode_number)) return false;
  GTEST_LOG_(INFO) << "Inode number: " << info->inode_number;

  // Get the file's nonce.
  if (ioctl(fd, FS_IOC_GET_ENCRYPTION_NONCE, info->nonce.bytes) != 0) {
    ADD_FAILURE() << "FS_IOC_GET_ENCRYPTION_NONCE failed on " << kTestFile
                  << Errno();
    return false;
  }
  GTEST_LOG_(INFO) << "File nonce: " << BytesToHex(info->nonce.bytes);
  return true;
}

static std::vector<uint8_t> InitHkdfInfo(FscryptHkdfContext context) {
  return {
      'f', 's', 'c', 'r', 'y', 'p', 't', '\0', static_cast<uint8_t>(context)};
}

static bool DeriveKey(const std::vector<uint8_t> &master_key,
                      const std::vector<uint8_t> &hkdf_info,
                      std::vector<uint8_t> &out) {
  if (HKDF(out.data(), out.size(), EVP_sha512(), master_key.data(),
           master_key.size(), nullptr, 0, hkdf_info.data(),
           hkdf_info.size()) != 1) {
    ADD_FAILURE() << "BoringSSL HKDF-SHA512 call failed";
    return false;
  }
  GTEST_LOG_(INFO) << "Derived subkey " << BytesToHex(out)
                   << " using HKDF info " << BytesToHex(hkdf_info);
  return true;
}

// Derives the key identifier from |master_key| and verifies that it matches the
// value the kernel returned in |master_key_specifier_|.
bool FBEPolicyTest::VerifyKeyIdentifier(
    const std::vector<uint8_t> &master_key) {
  std::vector<uint8_t> hkdf_info = InitHkdfInfo(HKDF_CONTEXT_KEY_IDENTIFIER);
  std::vector<uint8_t> computed_key_identifier(FSCRYPT_KEY_IDENTIFIER_SIZE);
  if (!DeriveKey(master_key, hkdf_info, computed_key_identifier)) return false;

  std::vector<uint8_t> actual_key_identifier(
      std::begin(master_key_specifier_.u.identifier),
      std::end(master_key_specifier_.u.identifier));
  EXPECT_EQ(actual_key_identifier, computed_key_identifier);
  return actual_key_identifier == computed_key_identifier;
}

// Derives a per-mode encryption key from |master_key|, |mode|, |context|, and
// (if needed for the context) the filesystem UUID.
bool FBEPolicyTest::DerivePerModeEncryptionKey(
    const std::vector<uint8_t> &master_key, int mode,
    FscryptHkdfContext context, std::vector<uint8_t> &enc_key) {
  std::vector<uint8_t> hkdf_info = InitHkdfInfo(context);

  hkdf_info.push_back(mode);
  if (context == HKDF_CONTEXT_IV_INO_LBLK_64_KEY ||
      context == HKDF_CONTEXT_IV_INO_LBLK_32_KEY)
    hkdf_info.insert(hkdf_info.end(), fs_info_.uuid.bytes,
                     std::end(fs_info_.uuid.bytes));

  return DeriveKey(master_key, hkdf_info, enc_key);
}

// Derives a per-file encryption key from |master_key| and |nonce|.
bool FBEPolicyTest::DerivePerFileEncryptionKey(
    const std::vector<uint8_t> &master_key, const FscryptFileNonce &nonce,
    std::vector<uint8_t> &enc_key) {
  std::vector<uint8_t> hkdf_info = InitHkdfInfo(HKDF_CONTEXT_PER_FILE_ENC_KEY);

  hkdf_info.insert(hkdf_info.end(), nonce.bytes, std::end(nonce.bytes));

  return DeriveKey(master_key, hkdf_info, enc_key);
}

// For IV_INO_LBLK_32: Hashes the |inode_number| using the SipHash key derived
// from |master_key|.  Returns the resulting hash in |hash|.
static bool HashInodeNumber(const std::vector<uint8_t> &master_key,
                            uint64_t inode_number, uint32_t *hash) {
  union {
    uint64_t words[2];
    __le64 le_words[2];
  } siphash_key;
  union {
    __le64 inode_number;
    uint8_t bytes[8];
  } input;

  std::vector<uint8_t> hkdf_info = InitHkdfInfo(HKDF_CONTEXT_INODE_HASH_KEY);
  std::vector<uint8_t> ino_hash_key(sizeof(siphash_key));
  if (!DeriveKey(master_key, hkdf_info, ino_hash_key)) return false;

  memcpy(&siphash_key, &ino_hash_key[0], sizeof(siphash_key));
  siphash_key.words[0] = __le64_to_cpu(siphash_key.le_words[0]);
  siphash_key.words[1] = __le64_to_cpu(siphash_key.le_words[1]);

  GTEST_LOG_(INFO) << "Inode hash key is {" << std::hex << "0x"
                   << siphash_key.words[0] << ", 0x" << siphash_key.words[1]
                   << "}" << std::dec;

  input.inode_number = __cpu_to_le64(inode_number);

  *hash = SIPHASH_24(siphash_key.words, input.bytes, sizeof(input));
  GTEST_LOG_(INFO) << "Hashed inode number " << inode_number << " to 0x"
                   << std::hex << *hash << std::dec;
  return true;
}

void FBEPolicyTest::VerifyCiphertext(const std::vector<uint8_t> &enc_key,
                                     const FscryptIV &starting_iv,
                                     const Cipher &cipher,
                                     const TestFileInfo &file_info) {
  const std::vector<uint8_t> &plaintext = file_info.plaintext;

  GTEST_LOG_(INFO) << "Verifying correctness of encrypted data";
  FscryptIV iv = starting_iv;

  std::vector<uint8_t> computed_ciphertext(plaintext.size());

  // Encrypt each filesystem block of file contents.
  for (size_t i = 0; i < plaintext.size(); i += kFilesystemBlockSize) {
    int block_size =
        std::min<size_t>(kFilesystemBlockSize, plaintext.size() - i);

    ASSERT_GE(sizeof(iv.bytes), cipher.ivsize());
    ASSERT_TRUE(cipher.Encrypt(enc_key, iv.bytes, &plaintext[i],
                               &computed_ciphertext[i], block_size));

    // Update the IV by incrementing the file logical block number.
    iv.lblk_num = __cpu_to_le32(__le32_to_cpu(iv.lblk_num) + 1);
  }

  ASSERT_EQ(file_info.actual_ciphertext, computed_ciphertext);
}

static bool InitIVForPerFileKey(FscryptIV *iv) {
  memset(iv, 0, kFscryptMaxIVSize);
  return true;
}

static bool InitIVForDirectKey(const FscryptFileNonce &nonce, FscryptIV *iv) {
  memset(iv, 0, kFscryptMaxIVSize);
  memcpy(iv->file_nonce, nonce.bytes, kFscryptFileNonceSize);
  return true;
}

static bool InitIVForInoLblk64(uint64_t inode_number, FscryptIV *iv) {
  if (inode_number > UINT32_MAX) {
    ADD_FAILURE() << "inode number doesn't fit in 32 bits";
    return false;
  }
  memset(iv, 0, kFscryptMaxIVSize);
  iv->inode_number = __cpu_to_le32(inode_number);
  return true;
}

static bool InitIVForInoLblk32(const std::vector<uint8_t> &master_key,
                               uint64_t inode_number, FscryptIV *iv) {
  uint32_t hash;
  if (!HashInodeNumber(master_key, inode_number, &hash)) return false;
  memset(iv, 0, kFscryptMaxIVSize);
  iv->lblk_num = __cpu_to_le32(hash);
  return true;
}

// Tests a policy matching "fileencryption=aes-256-xts:aes-256-cts:v2"
// (or simply "fileencryption=" on devices launched with R or higher)
TEST_F(FBEPolicyTest, TestAesPerFileKeysPolicy) {
  if (skip_test_) return;

  auto master_key = GenerateTestKey(kFscryptMasterKeySize);
  ASSERT_TRUE(SetMasterKey(master_key));

  if (!SetEncryptionPolicy(FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS,
                           0, 0))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  std::vector<uint8_t> enc_key(kAes256XtsKeySize);
  ASSERT_TRUE(DerivePerFileEncryptionKey(master_key, file_info.nonce, enc_key));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForPerFileKey(&iv));
  VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);
}

// Tests a policy matching
// "fileencryption=aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized"
// (or simply "fileencryption=::inlinecrypt_optimized" on devices launched with
// R or higher)
TEST_F(FBEPolicyTest, TestAesInlineCryptOptimizedPolicy) {
  if (skip_test_) return;

  auto master_key = GenerateTestKey(kFscryptMasterKeySize);
  ASSERT_TRUE(SetMasterKey(master_key));

  if (!SetEncryptionPolicy(FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS,
                           FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64,
                           GetSkipFlagsForInoBasedEncryption()))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  std::vector<uint8_t> enc_key(kAes256XtsKeySize);
  ASSERT_TRUE(DerivePerModeEncryptionKey(master_key, FSCRYPT_MODE_AES_256_XTS,
                                         HKDF_CONTEXT_IV_INO_LBLK_64_KEY,
                                         enc_key));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForInoLblk64(file_info.inode_number, &iv));
  VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);
}

// Tests a policy matching
// "fileencryption=aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized+wrappedkey_v0"
// (or simply "fileencryption=::inlinecrypt_optimized+wrappedkey_v0" on devices
// launched with R or higher)
TEST_F(FBEPolicyTest, TestAesInlineCryptOptimizedHwWrappedKeyPolicy) {
  if (skip_test_) return;

  std::vector<uint8_t> enc_key, sw_secret;
  if (!CreateAndSetHwWrappedKey(&enc_key, &sw_secret)) return;

  if (!SetEncryptionPolicy(
          FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS,
          FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64,
          // 64-bit DUN support is not guaranteed.
          kSkipIfNoHardwareSupport | GetSkipFlagsForInoBasedEncryption()))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForInoLblk64(file_info.inode_number, &iv));
  VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);
}

// With IV_INO_LBLK_32, the DUN (IV) can wrap from UINT32_MAX to 0 in the middle
// of the file.  This method tests that this case appears to be handled
// correctly, by doing I/O across the place where the DUN wraps around.  Assumes
// that kTestDir has already been set up with an IV_INO_LBLK_32 policy.
void FBEPolicyTest::TestEmmcOptimizedDunWraparound(
    const std::vector<uint8_t> &master_key,
    const std::vector<uint8_t> &enc_key) {
  // We'll test writing 'block_count' filesystem blocks.  The first
  // 'block_count_1' blocks will have DUNs [..., UINT32_MAX - 1, UINT32_MAX].
  // The remaining 'block_count_2' blocks will have DUNs [0, 1, ...].
  constexpr uint32_t block_count_1 = 3;
  constexpr uint32_t block_count_2 = 7;
  constexpr uint32_t block_count = block_count_1 + block_count_2;
  constexpr size_t data_size = block_count * kFilesystemBlockSize;

  // Assumed maximum file size.  Unfortunately there isn't a syscall to get
  // this.  ext4 allows ~16TB and f2fs allows ~4TB.  However, an underestimate
  // works fine for our purposes, so just go with 1TB.
  constexpr off_t max_file_size = 1000000000000;
  constexpr off_t max_file_blocks = max_file_size / kFilesystemBlockSize;

  // Repeatedly create empty files until we find one that can be used for DUN
  // wraparound testing, due to SipHash(inode_number) being almost UINT32_MAX.
  std::string path;
  TestFileInfo file_info;
  uint32_t lblk_with_dun_0;
  for (int i = 0;; i++) {
    // The probability of finding a usable file is about 'max_file_blocks /
    // UINT32_MAX', or about 5.6%.  So on average we'll need about 18 tries.
    // The probability we'll need over 1000 tries is less than 1e-25.
    ASSERT_LT(i, 1000) << "Tried too many times to find a usable test file";

    path = android::base::StringPrintf("%s/file%d", kTestDir, i);
    android::base::unique_fd fd(
        open(path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600));
    ASSERT_GE(fd, 0) << "Failed to create " << path << Errno();

    ASSERT_TRUE(GetInodeNumber(path, &file_info.inode_number));
    uint32_t hash;
    ASSERT_TRUE(HashInodeNumber(master_key, file_info.inode_number, &hash));
    // Negating the hash gives the distance to DUN 0, and hence the 0-based
    // logical block number of the block which has DUN 0.
    lblk_with_dun_0 = -hash;
    if (lblk_with_dun_0 >= block_count_1 &&
        static_cast<off_t>(lblk_with_dun_0) + block_count_2 < max_file_blocks)
      break;
  }

  GTEST_LOG_(INFO) << "DUN wraparound test: path=" << path
                   << ", inode_number=" << file_info.inode_number
                   << ", lblk_with_dun_0=" << lblk_with_dun_0;

  // Write some data across the DUN wraparound boundary and verify that the
  // resulting on-disk ciphertext is as expected.  Note that we don't actually
  // have to fill the file until the boundary; we can just write to the needed
  // part and leave a hole before it.
  for (int i = 0; i < 2; i++) {
    // Try both buffered I/O and direct I/O.
    int open_flags = O_RDWR | O_CLOEXEC;
    if (i == 1) open_flags |= O_DIRECT;

    android::base::unique_fd fd(open(path.c_str(), open_flags));
    ASSERT_GE(fd, 0) << "Failed to open " << path << Errno();

    // Generate some test data.
    file_info.plaintext.resize(data_size);
    RandomBytesForTesting(file_info.plaintext);

    // Write the test data.  To support O_DIRECT, use a block-aligned buffer.
    std::unique_ptr<void, void (*)(void *)> buf_mem(
        aligned_alloc(kFilesystemBlockSize, data_size), free);
    ASSERT_TRUE(buf_mem != nullptr);
    memcpy(buf_mem.get(), &file_info.plaintext[0], data_size);
    off_t pos = static_cast<off_t>(lblk_with_dun_0 - block_count_1) *
                kFilesystemBlockSize;
    ASSERT_EQ(data_size, pwrite(fd, buf_mem.get(), data_size, pos))
        << "Error writing data to " << path << Errno();

    // Verify the ciphertext.
    ASSERT_TRUE(ReadRawDataOfFile(fd, fs_info_.raw_blk_device, data_size,
                                  &file_info.actual_ciphertext));
    FscryptIV iv;
    memset(&iv, 0, sizeof(iv));
    iv.lblk_num = __cpu_to_le32(-block_count_1);
    VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);
  }
}

// Tests a policy matching
// "fileencryption=aes-256-xts:aes-256-cts:v2+emmc_optimized" (or simply
// "fileencryption=::emmc_optimized" on devices launched with R or higher)
TEST_F(FBEPolicyTest, TestAesEmmcOptimizedPolicy) {
  if (skip_test_) return;

  auto master_key = GenerateTestKey(kFscryptMasterKeySize);
  ASSERT_TRUE(SetMasterKey(master_key));

  if (!SetEncryptionPolicy(FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS,
                           FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32,
                           GetSkipFlagsForInoBasedEncryption()))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  std::vector<uint8_t> enc_key(kAes256XtsKeySize);
  ASSERT_TRUE(DerivePerModeEncryptionKey(master_key, FSCRYPT_MODE_AES_256_XTS,
                                         HKDF_CONTEXT_IV_INO_LBLK_32_KEY,
                                         enc_key));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForInoLblk32(master_key, file_info.inode_number, &iv));
  VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);

  TestEmmcOptimizedDunWraparound(master_key, enc_key);
}

// Tests a policy matching
// "fileencryption=aes-256-xts:aes-256-cts:v2+emmc_optimized+wrappedkey_v0"
// (or simply "fileencryption=::emmc_optimized+wrappedkey_v0" on devices
// launched with R or higher)
TEST_F(FBEPolicyTest, TestAesEmmcOptimizedHwWrappedKeyPolicy) {
  if (skip_test_) return;

  std::vector<uint8_t> enc_key, sw_secret;
  if (!CreateAndSetHwWrappedKey(&enc_key, &sw_secret)) return;

  if (!SetEncryptionPolicy(FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS,
                           FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32,
                           GetSkipFlagsForInoBasedEncryption()))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForInoLblk32(sw_secret, file_info.inode_number, &iv));
  VerifyCiphertext(enc_key, iv, Aes256XtsCipher(), file_info);

  TestEmmcOptimizedDunWraparound(sw_secret, enc_key);
}

// Tests a policy matching "fileencryption=adiantum:adiantum:v2" (or simply
// "fileencryption=adiantum" on devices launched with R or higher)
TEST_F(FBEPolicyTest, TestAdiantumPolicy) {
  if (skip_test_) return;

  auto master_key = GenerateTestKey(kFscryptMasterKeySize);
  ASSERT_TRUE(SetMasterKey(master_key));

  // Adiantum support isn't required (since CONFIG_CRYPTO_ADIANTUM can be unset
  // in the kernel config), so we may skip the test here.
  //
  // We don't need to use GetSkipFlagsForInoBasedEncryption() here, since the
  // "DIRECT_KEY" IV generation method doesn't include inode numbers in the IVs.
  if (!SetEncryptionPolicy(FSCRYPT_MODE_ADIANTUM, FSCRYPT_MODE_ADIANTUM,
                           FSCRYPT_POLICY_FLAG_DIRECT_KEY,
                           kSkipIfNoCryptoAPISupport))
    return;

  TestFileInfo file_info;
  ASSERT_TRUE(GenerateTestFile(&file_info));

  std::vector<uint8_t> enc_key(kAdiantumKeySize);
  ASSERT_TRUE(DerivePerModeEncryptionKey(master_key, FSCRYPT_MODE_ADIANTUM,
                                         HKDF_CONTEXT_DIRECT_KEY, enc_key));

  FscryptIV iv;
  ASSERT_TRUE(InitIVForDirectKey(file_info.nonce, &iv));
  VerifyCiphertext(enc_key, iv, AdiantumCipher(), file_info);
}

// Tests adding a corrupted wrapped key to fscrypt keyring.
// If wrapped key is corrupted, fscrypt should return a failure.
TEST_F(FBEPolicyTest, TestHwWrappedKeyCorruption) {
  if (skip_test_) return;

  std::vector<uint8_t> master_key, exported_key;
  if (!CreateHwWrappedKey(&master_key, &exported_key)) return;

  for (int i = 0; i < exported_key.size(); i++) {
    std::vector<uint8_t> corrupt_key(exported_key.begin(), exported_key.end());
    corrupt_key[i] = ~corrupt_key[i];
    ASSERT_FALSE(
        SetMasterKey(corrupt_key, __FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED, false));
  }
}

// Tests that if the device uses FBE, then the ciphertext for file contents in
// encrypted directories seems to be random.
//
// This isn't as strong a test as the correctness tests, but it's useful because
// it applies regardless of the encryption format and key.  Thus it runs even on
// old devices, including ones that used a vendor-specific encryption format.
TEST(FBETest, TestFileContentsRandomness) {
  constexpr const char *path_1 = "/data/local/tmp/vts-test-file-1";
  constexpr const char *path_2 = "/data/local/tmp/vts-test-file-2";

  if (android::base::GetProperty("ro.crypto.type", "") != "file") {
    // FBE has been required since Android Q.
    int first_api_level;
    ASSERT_TRUE(GetFirstApiLevel(&first_api_level));
    ASSERT_LE(first_api_level, __ANDROID_API_P__)
        << "File-based encryption is required";
    GTEST_LOG_(INFO)
        << "Skipping test because device doesn't use file-based encryption";
    return;
  }
  FilesystemInfo fs_info;
  ASSERT_TRUE(GetFilesystemInfo("/data", &fs_info));

  std::vector<uint8_t> zeroes(kTestFileBytes, 0);
  std::vector<uint8_t> ciphertext_1;
  std::vector<uint8_t> ciphertext_2;
  ASSERT_TRUE(
      WriteTestFile(zeroes, path_1, fs_info.raw_blk_device, &ciphertext_1));
  ASSERT_TRUE(
      WriteTestFile(zeroes, path_2, fs_info.raw_blk_device, &ciphertext_2));

  GTEST_LOG_(INFO) << "Verifying randomness of ciphertext";

  // Each individual file's ciphertext should be random.
  ASSERT_TRUE(VerifyDataRandomness(ciphertext_1));
  ASSERT_TRUE(VerifyDataRandomness(ciphertext_2));

  // The files' ciphertext concatenated should also be random.
  // I.e., each file should be encrypted differently.
  std::vector<uint8_t> concatenated_ciphertext;
  concatenated_ciphertext.insert(concatenated_ciphertext.end(),
                                 ciphertext_1.begin(), ciphertext_1.end());
  concatenated_ciphertext.insert(concatenated_ciphertext.end(),
                                 ciphertext_2.begin(), ciphertext_2.end());
  ASSERT_TRUE(VerifyDataRandomness(concatenated_ciphertext));

  ASSERT_EQ(unlink(path_1), 0);
  ASSERT_EQ(unlink(path_2), 0);
}

}  // namespace kernel
}  // namespace android
