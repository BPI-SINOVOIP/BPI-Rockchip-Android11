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

#define LOG_TAG "VtsSecurityAvbTest"

#include <sys/utsname.h>
#include <unistd.h>

#include <array>
#include <list>
#include <map>
#include <set>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <fs_avb/fs_avb_util.h>
#include <fs_mgr/roots.h>
#include <fstab/fstab.h>
#include <gtest/gtest.h>
#include <libavb/libavb.h>
#include <libdm/dm.h>
#include <log/log.h>
#include <openssl/sha.h>

static uint8_t HexDigitToByte(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }
  return 0xff;
}

static bool HexToBytes(const std::string &hex, std::vector<uint8_t> *bytes) {
  if (hex.size() % 2 != 0) {
    return false;
  }
  bytes->resize(hex.size() / 2);
  for (unsigned i = 0; i < bytes->size(); i++) {
    uint8_t hi = HexDigitToByte(hex[i * 2]);
    uint8_t lo = HexDigitToByte(hex[i * 2 + 1]);
    if (lo > 0xf || hi > 0xf) {
      return false;
    }
    bytes->at(i) = (hi << 4) | lo;
  }
  return true;
}

// The abstract class of SHA algorithms.
class ShaHasher {
 protected:
  const uint32_t digest_size_;

  ShaHasher(uint32_t digest_size) : digest_size_(digest_size) {}

 public:
  virtual ~ShaHasher() {}

  uint32_t GetDigestSize() const { return digest_size_; }

  virtual bool CalculateDigest(const void *buffer, size_t size,
                               const void *salt, uint32_t block_length,
                               uint8_t *digest) const = 0;
};

template <typename CTX_TYPE>
class ShaHasherImpl : public ShaHasher {
 private:
  typedef int (*InitFunc)(CTX_TYPE *);
  typedef int (*UpdateFunc)(CTX_TYPE *sha, const void *data, size_t len);
  typedef int (*FinalFunc)(uint8_t *md, CTX_TYPE *sha);

  const InitFunc init_func_;
  const UpdateFunc update_func_;
  const FinalFunc final_func_;

 public:
  ShaHasherImpl(InitFunc init_func, UpdateFunc update_func,
                FinalFunc final_func, uint32_t digest_size)
      : ShaHasher(digest_size),
        init_func_(init_func),
        update_func_(update_func),
        final_func_(final_func) {}

  ~ShaHasherImpl() {}

  bool CalculateDigest(const void *buffer, size_t size, const void *salt,
                       uint32_t salt_length, uint8_t *digest) const {
    CTX_TYPE ctx;
    if (init_func_(&ctx) != 1) {
      return false;
    }
    if (update_func_(&ctx, salt, salt_length) != 1) {
      return false;
    }
    if (update_func_(&ctx, buffer, size) != 1) {
      return false;
    }
    if (final_func_(digest, &ctx) != 1) {
      return false;
    }
    return true;
  }
};

// Creates a hasher with the parameters corresponding to the algorithm name.
static std::unique_ptr<ShaHasher> CreateShaHasher(
    const std::string &algorithm) {
  if (algorithm == "sha1") {
    return std::make_unique<ShaHasherImpl<SHA_CTX>>(
        SHA1_Init, SHA1_Update, SHA1_Final, SHA_DIGEST_LENGTH);
  }
  if (algorithm == "sha256") {
    return std::make_unique<ShaHasherImpl<SHA256_CTX>>(
        SHA256_Init, SHA256_Update, SHA256_Final, SHA256_DIGEST_LENGTH);
  }
  if (algorithm == "sha512") {
    return std::make_unique<ShaHasherImpl<SHA512_CTX>>(
        SHA512_Init, SHA512_Update, SHA512_Final, SHA512_DIGEST_LENGTH);
  }
  return nullptr;
}

// Calculates the digest of a block filled with 0.
static bool CalculateZeroDigest(const ShaHasher &hasher, size_t size,
                                const void *salt, int32_t block_length,
                                uint8_t *digest) {
  const std::vector<uint8_t> buffer(size, 0);
  return hasher.CalculateDigest(buffer.data(), size, salt, block_length,
                                digest);
}

// Logical structure of a hashtree:
//
// Level 2:                        [    root     ]
//                                /               \
// Level 1:              [entry_0]                 [entry_1]
//                      /   ...   \                   ...   \
// Level 0:   [entry_0_0]   ...   [entry_0_127]       ...   [entry_1_127]
//             /  ...  \           /   ...   \               /   ...   \
// Data:    blk_0 ... blk_127  blk_16256 ... blk_16383  blk_32640 ... blk_32767
//
// The digest of a data block or a hash block in level N is stored in level
// N + 1.
// The function VerifyHashtree allocates a HashtreeLevel for each level. It
// calculates the digests of the blocks in lower level and fills them in
// calculating_hash_block. When calculating_hash_block is full, it is compared
// with the hash block at comparing_tree_offset in the image. After comparison,
// calculating_hash_block is cleared and reused for the next hash block.
//
//                   comparing_tree_offset
//                   |
//                   v
// [<--------------------    level_size    -------------------->]
// [entry_0_0]  ...  [entry_0_127           ]  ...  [entry_1_127]
//
//                   [calculating_hash_block]
//                         ^
//                         |
//                         calculating_offset
struct HashtreeLevel {
  // Offset of an expected hash block to compare, relative to the beginning of
  // the hashtree in the image file.
  uint64_t comparing_tree_offset;
  // Size of this level, in bytes.
  const uint64_t level_size;
  // Offset of a digest in calculating_hash_block.
  size_t calculating_offset;
  // The hash block containing the digests calculated from the lower level.
  std::vector<uint8_t> calculating_hash_block;

  HashtreeLevel(uint64_t lv_offset, uint64_t lv_size, size_t hash_block_size)
      : comparing_tree_offset(lv_offset),
        level_size(lv_size),
        calculating_offset(0),
        calculating_hash_block(hash_block_size) {}
};

// Calculates and verifies the image's hashtree.
//
// Arguments:
//   image_fd: The raw image file.
//   image_size, data_block_size, hash_block_size, tree_offset, tree_size: The
//       fields in AvbHashtreeDescriptor.
//   salt: The binary value of the salt in FsAvbHashtreeDescriptor.
//   hasher: The ShaHasher object.
//   root_digest: The binary value of the root_digest in
//       FsAvbHashtreeDescriptor.
//
// Returns:
//   An empty string if the function succeeds.
//   Otherwise it returns the error message.
static std::string VerifyHashtree(int image_fd, uint64_t image_size,
                                  const std::vector<uint8_t> &salt,
                                  uint32_t data_block_size,
                                  uint32_t hash_block_size,
                                  uint64_t tree_offset, uint64_t tree_size,
                                  const ShaHasher &hasher,
                                  const std::vector<uint8_t> &root_digest) {
  uint32_t digest_size = hasher.GetDigestSize();
  uint32_t padded_digest_size = 1;
  while (padded_digest_size < digest_size) {
    padded_digest_size *= 2;
  }

  if (image_size % data_block_size != 0) {
    return "Image size is not a multiple of data block size";
  }

  uint64_t data_block_count = image_size / data_block_size;
  uint32_t digests_per_block = hash_block_size / padded_digest_size;

  // Initialize HashtreeLevel in bottom-up order.
  std::list<HashtreeLevel> levels;
  {
    uint64_t hash_block_count = 0;
    uint32_t level_block_count = data_block_count;
    // Calculate the hashtree until the root hash is reached.
    while (level_block_count > 1) {
      uint32_t next_level_block_count =
          (level_block_count + digests_per_block - 1) / digests_per_block;
      hash_block_count += next_level_block_count;
      // comparing_tree_offset will be initialized later.
      levels.emplace_back(0 /* comparing_tree_offset */,
                          next_level_block_count * hash_block_size,
                          hash_block_size);
      level_block_count = next_level_block_count;
    }
    if (hash_block_count * hash_block_size != tree_size) {
      return "Block count and tree size mismatch";
    }
    // Append the root digest. Its level_size is unused.
    levels.emplace_back(0 /* comparing_tree_offset */, 0 /* level_size */,
                        digest_size);

    // Initialize comparing_tree_offset of each level
    for (auto level = std::prev(levels.end()); level != levels.begin();
         level--) {
      std::prev(level)->comparing_tree_offset =
          level->comparing_tree_offset + level->level_size;
    }
  }

  std::vector<uint8_t> padded_zero_digest(padded_digest_size, 0);
  if (!CalculateZeroDigest(hasher, data_block_size, salt.data(), salt.size(),
                           padded_zero_digest.data())) {
    return "CalculateZeroDigest fails";
  }

  std::vector<uint8_t> data_block(data_block_size);
  std::vector<uint8_t> tree_block(hash_block_size);
  for (uint64_t image_offset = 0; image_offset < image_size;
       image_offset += data_block_size) {
    ssize_t read_size = TEMP_FAILURE_RETRY(
        pread64(image_fd, data_block.data(), data_block.size(), image_offset));
    if (read_size != data_block.size()) {
      return android::base::StringPrintf(
          "Fail to read data block at offset %llu",
          (unsigned long long)image_offset);
    }

    bool is_last_data = (image_offset + data_block.size() == image_size);
    // The block to be digested
    std::vector<uint8_t> *current_block = &data_block;
    for (auto level = levels.begin(); true; level++) {
      uint8_t *current_digest =
          level->calculating_hash_block.data() + level->calculating_offset;
      if (!hasher.CalculateDigest(current_block->data(), current_block->size(),
                                  salt.data(), salt.size(), current_digest)) {
        return "CalculateDigest fails";
      }
      // Stop at root digest
      if (std::next(level) == levels.end()) {
        break;
      }

      // Pad the digest
      memset(current_digest + digest_size, 0, padded_digest_size - digest_size);
      level->calculating_offset += padded_digest_size;
      // Pad the last hash block of this level
      if (is_last_data) {
        memset(
            level->calculating_hash_block.data() + level->calculating_offset, 0,
            level->calculating_hash_block.size() - level->calculating_offset);
      } else if (level->calculating_offset <
                 level->calculating_hash_block.size()) {
        // Stop at this level if the hash block is not full, continue to read
        // more data_blocks from the outside loop for digest calculation
        break;
      }
      // Verify the full hash block
      // current_block may point to tree_block. Since the following pread64
      // changes tree_block, do not read current_block in the rest of this
      // code block.
      current_block = nullptr;
      read_size = TEMP_FAILURE_RETRY(
          pread64(image_fd, tree_block.data(), tree_block.size(),
                  tree_offset + level->comparing_tree_offset));
      if (read_size != tree_block.size()) {
        return android::base::StringPrintf(
            "Fail to read tree block at offset %llu",
            (unsigned long long)tree_offset + level->comparing_tree_offset);
      }

      for (uint32_t offset = 0; offset < tree_block.size();
           offset += padded_digest_size) {
        // If the digest in the hashtree is equal to the digest of zero block,
        // it indicates the corresponding data block is in DONT_CARE chunk in
        // sparse image. The block should not be verified.
        if (level == levels.begin() &&
            memcmp(tree_block.data() + offset, padded_zero_digest.data(),
                   padded_digest_size) == 0) {
          continue;
        }
        if (memcmp(tree_block.data() + offset,
                   level->calculating_hash_block.data() + offset,
                   padded_digest_size) != 0) {
          return android::base::StringPrintf(
              "Hash blocks mismatch, block offset = %llu, digest offset = %u",
              (unsigned long long)tree_offset + level->comparing_tree_offset,
              offset);
        }
      }

      level->calculating_offset = 0;
      level->comparing_tree_offset += hash_block_size;
      if (level->comparing_tree_offset > tree_size) {
        return "Tree offset is out of bound";
      }
      // Prepare for next/upper level, to calculate the digest of this
      // hash_block for comparison
      current_block = &tree_block;
    }
  }

  if (levels.back().calculating_hash_block != root_digest) {
    return "Root digests mismatch";
  }
  return "";
}

// Converts descriptor.hash_algorithm to std::string.
static std::string GetHashAlgorithm(const AvbHashtreeDescriptor &descriptor) {
  return std::string(reinterpret_cast<const char *>(descriptor.hash_algorithm));
}

// Converts descriptor.hash_algorithm to std::string.
static std::string GetHashAlgorithm(const AvbHashDescriptor &descriptor) {
  return std::string(reinterpret_cast<const char *>(descriptor.hash_algorithm));
}

// Checks whether the public key is an official GSI key or not.
static bool ValidatePublicKeyBlob(const std::string &key_blob_to_validate) {
  if (key_blob_to_validate.empty()) {
    ALOGE("Failed to validate an empty key");
    return false;
  }

  std::string allowed_key_blob;
  std::vector<std::string> allowed_key_paths = {
      "/data/local/tmp/q-gsi.avbpubkey", "/data/local/tmp/r-gsi.avbpubkey",
      "/data/local/tmp/s-gsi.avbpubkey"};
  for (const auto &path : allowed_key_paths) {
    if (android::base::ReadFileToString(path, &allowed_key_blob)) {
      if (key_blob_to_validate == allowed_key_blob) {
        ALOGE("Found matching GSI key: %s", path.c_str());
        return true;
      }
    }
  }
  return false;
}

// Gets the system partition's AvbHashtreeDescriptor and device file path.
//
// Arguments:
//  out_verify_result: The result of vbmeta verification.
//  out_system_path: The system's device file path.
//
// Returns:
//   The pointer to the system's AvbHashtreeDescriptor.
//   nullptr if any operation fails.
static std::unique_ptr<android::fs_mgr::FsAvbHashtreeDescriptor>
GetSystemHashtreeDescriptor(
    android::fs_mgr::VBMetaVerifyResult *out_verify_result,
    std::string *out_system_path) {
  android::fs_mgr::Fstab default_fstab;
  bool ok = ReadDefaultFstab(&default_fstab);
  if (!ok) {
    ALOGE("ReadDefaultFstab fails");
    return nullptr;
  }
  android::fs_mgr::FstabEntry *system_fstab_entry =
      GetEntryForPath(&default_fstab, "/system");
  if (system_fstab_entry == nullptr) {
    ALOGE("GetEntryForPath fails");
    return nullptr;
  }

  ok = fs_mgr_update_logical_partition(system_fstab_entry);
  if (!ok) {
    ALOGE("fs_mgr_update_logical_partition fails");
    return nullptr;
  }

  CHECK(out_system_path != nullptr);
  *out_system_path = system_fstab_entry->blk_device;

  std::string out_public_key_data;
  std::string out_avb_partition_name;
  std::unique_ptr<android::fs_mgr::VBMetaData> vbmeta =
      android::fs_mgr::LoadAndVerifyVbmeta(
          *system_fstab_entry, "" /* expected_key_blob */, &out_public_key_data,
          &out_avb_partition_name, out_verify_result);
  if (vbmeta == nullptr) {
    ALOGE("LoadAndVerifyVbmeta fails");
    return nullptr;
  }

  if (out_public_key_data.empty()) {
    ALOGE("The GSI image is not signed");
    return nullptr;
  }

  if (!ValidatePublicKeyBlob(out_public_key_data)) {
    ALOGE("The GSI image is not signed by an official key");
    return nullptr;
  }

  std::unique_ptr<android::fs_mgr::FsAvbHashtreeDescriptor> descriptor =
      android::fs_mgr::GetHashtreeDescriptor("system", std::move(*vbmeta));
  if (descriptor == nullptr) {
    ALOGE("GetHashtreeDescriptor fails");
    return nullptr;
  }

  return descriptor;
}

TEST(AvbTest, Boot) {
  /* Skip for devices running kernels older than 5.4. */
  struct utsname buf;
  int ret, kernel_version_major, kernel_version_minor;
  ret = uname(&buf);
  ASSERT_EQ(ret, 0) << "Failed to get kernel version.";
  char dummy;
  ret = sscanf(buf.release, "%d.%d%c", &kernel_version_major,
               &kernel_version_minor, &dummy);
  ASSERT_GE(ret, 2) << "Failed to parse kernel version.";
  if (kernel_version_major < 5 ||
      (kernel_version_major == 5 && kernel_version_minor < 4)) {
    return;
  }

  /* load vbmeta struct from boot, verify struct integrity */
  std::string out_public_key_data;
  android::fs_mgr::VBMetaVerifyResult out_verify_result;
  std::string boot_path = "/dev/block/by-name/boot" + fs_mgr_get_slot_suffix();
  std::unique_ptr<android::fs_mgr::VBMetaData> vbmeta =
      android::fs_mgr::LoadAndVerifyVbmetaByPath(
          boot_path, "boot", "" /* expected_key_blob */,
          true /* allow verification error */, false /* rollback_protection */,
          false /* is_chained_vbmeta */, &out_public_key_data,
          nullptr /* out_verification_disabled */, &out_verify_result);

  ASSERT_TRUE(vbmeta) << "Verification of GKI vbmeta fails.";
  ASSERT_FALSE(out_public_key_data.empty()) << "The GKI image is not signed.";
  EXPECT_TRUE(ValidatePublicKeyBlob(out_public_key_data))
      << "The GKI image is not signed by an official key.";
  EXPECT_EQ(out_verify_result, android::fs_mgr::VBMetaVerifyResult::kSuccess)
      << "Verification of the GKI vbmeta structure failed.";

  /* verify boot partition according to vbmeta structure */
  std::unique_ptr<android::fs_mgr::FsAvbHashDescriptor> descriptor =
      android::fs_mgr::GetHashDescriptor("boot", std::move(*vbmeta));
  const std::string &salt_str = descriptor->salt;
  const std::string &expected_digest_str = descriptor->digest;

  android::base::unique_fd fd(open(boot_path.c_str(), O_RDONLY));
  ASSERT_GE(fd, 0) << "Fail to open boot partition. Try 'adb root'.";

  const std::string hash_algorithm(GetHashAlgorithm(*descriptor));
  ALOGI("hash_algorithm = %s", hash_algorithm.c_str());

  std::unique_ptr<ShaHasher> hasher = CreateShaHasher(hash_algorithm);
  ASSERT_TRUE(hasher);

  std::vector<uint8_t> salt, expected_digest, out_digest;
  bool ok = HexToBytes(salt_str, &salt);
  ASSERT_TRUE(ok) << "Invalid salt in descriptor: " << salt_str;
  ok = HexToBytes(expected_digest_str, &expected_digest);
  ASSERT_TRUE(ok) << "Invalid digest in descriptor: " << expected_digest_str;
  ASSERT_EQ(expected_digest.size(), hasher->GetDigestSize());

  std::vector<char> boot_partition_vector;
  boot_partition_vector.resize(descriptor->image_size);
  ASSERT_TRUE(android::base::ReadFully(fd, boot_partition_vector.data(),
                                       descriptor->image_size))
      << "Could not read boot partition to vector.";

  out_digest.resize(hasher->GetDigestSize());
  ASSERT_TRUE(hasher->CalculateDigest(
      boot_partition_vector.data(), descriptor->image_size,
      salt.data(), descriptor->salt_len, out_digest.data()))
      << "Unable to calculate boot image digest.";

  ASSERT_TRUE(out_digest.size() == expected_digest.size())
      << "Calculated GKI boot digest size does not match expected digest size.";
  ASSERT_TRUE(out_digest == expected_digest)
      << "Calculated GKI boot digest does not match expected digest.";
}

// Loads contents and metadata of logical system partition, calculates
// the hashtree, and compares with the metadata.
TEST(AvbTest, SystemHashtree) {
  android::fs_mgr::VBMetaVerifyResult verify_result;
  std::string system_path;
  std::unique_ptr<android::fs_mgr::FsAvbHashtreeDescriptor> descriptor =
      GetSystemHashtreeDescriptor(&verify_result, &system_path);
  ASSERT_TRUE(descriptor);

  ALOGI("System partition is %s", system_path.c_str());

  // TODO: Skip assertion when running with non-compliance configuration.
  EXPECT_EQ(verify_result, android::fs_mgr::VBMetaVerifyResult::kSuccess);
  EXPECT_NE(verify_result,
            android::fs_mgr::VBMetaVerifyResult::kErrorVerification)
      << "The system image is not an officially signed GSI.";

  const std::string &salt_str = descriptor->salt;
  const std::string &expected_digest_str = descriptor->root_digest;

  android::base::unique_fd fd(open(system_path.c_str(), O_RDONLY));
  ASSERT_GE(fd, 0) << "Fail to open system partition. Try 'adb root'.";

  const std::string hash_algorithm(GetHashAlgorithm(*descriptor));
  ALOGI("hash_algorithm = %s", hash_algorithm.c_str());

  std::unique_ptr<ShaHasher> hasher = CreateShaHasher(hash_algorithm);
  ASSERT_TRUE(hasher);

  std::vector<uint8_t> salt, expected_digest;
  bool ok = HexToBytes(salt_str, &salt);
  ASSERT_TRUE(ok) << "Invalid salt in descriptor: " << salt_str;
  ok = HexToBytes(expected_digest_str, &expected_digest);
  ASSERT_TRUE(ok) << "Invalid digest in descriptor: " << expected_digest_str;
  ASSERT_EQ(expected_digest.size(), hasher->GetDigestSize());

  ALOGI("image_size = %llu", (unsigned long long)descriptor->image_size);
  ALOGI("data_block_size = %u", descriptor->data_block_size);
  ALOGI("hash_block_size = %u", descriptor->hash_block_size);
  ALOGI("tree_offset = %llu", (unsigned long long)descriptor->tree_offset);
  ALOGI("tree_size = %llu", (unsigned long long)descriptor->tree_size);

  std::string error_message = VerifyHashtree(
      fd, descriptor->image_size, salt, descriptor->data_block_size,
      descriptor->hash_block_size, descriptor->tree_offset,
      descriptor->tree_size, *hasher, expected_digest);
  ASSERT_EQ(error_message, "");
}

// Finds the next word consisting of non-whitespace characters in a string.
//
// Arguments:
//   str: The string to be searched for the next word.
//   pos: The starting position to search for the next word.
//        This function sets it to the past-the-end position of the word.
//
// Returns:
//   The starting position of the word.
//   If there is no next word, this function does not change pos and returns
//   std::string::npos.
static size_t NextWord(const std::string &str, size_t *pos) {
  const char *whitespaces = " \t\r\n";
  size_t start = str.find_first_not_of(whitespaces, *pos);
  if (start == std::string::npos) {
    return start;
  }
  *pos = str.find_first_of(whitespaces, start);
  if (*pos == std::string::npos) {
    *pos = str.size();
  }
  return start;
}

// Compares device mapper table with system hashtree descriptor.
TEST(AvbTest, SystemDescriptor) {
  // Get system hashtree descriptor.

  android::fs_mgr::VBMetaVerifyResult verify_result;
  std::string system_path;
  std::unique_ptr<android::fs_mgr::FsAvbHashtreeDescriptor> descriptor =
      GetSystemHashtreeDescriptor(&verify_result, &system_path);
  ASSERT_TRUE(descriptor);

  // TODO: Assert when running with compliance configuration.
  // The SystemHashtree function asserts verify_result.
  if (verify_result != android::fs_mgr::VBMetaVerifyResult::kSuccess) {
    ALOGW("The system image is not an officially signed GSI.");
  }

  // Get device mapper table.
  android::dm::DeviceMapper &device_mapper =
      android::dm::DeviceMapper::Instance();
  std::vector<android::dm::DeviceMapper::TargetInfo> table;
  bool ok = device_mapper.GetTableInfo("system-verity", &table);
  ASSERT_TRUE(ok) << "GetTableInfo fails";
  ASSERT_EQ(table.size(), 1);
  const android::dm::DeviceMapper::TargetInfo &target = table[0];
  // Sample output:
  // Device mapper table for system-verity:
  // 0-1783288: verity, 1 253:0 253:0 4096 4096 222911 222911 sha1
  // 6b2b46715a2d27c53cc7f91fe63ce798ff1f3df7
  // 65bc99ca8e97379d4f7adc66664941acc0a8e682 10 restart_on_corruption
  // ignore_zero_blocks use_fec_from_device 253:0 fec_blocks 224668 fec_start
  // 224668 fec_roots 2
  ALOGI("Device mapper table for system-verity:\n%llu-%llu: %s, %s",
        target.spec.sector_start, target.spec.sector_start + target.spec.length,
        target.spec.target_type, target.data.c_str());
  EXPECT_EQ(strcmp(target.spec.target_type, "verity"), 0);

  // Compare the target's positional parameters with the descriptor. Reference:
  // https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity#mapping-table-for-verity-target
  std::array<std::string, 10> descriptor_values = {
      std::to_string(descriptor->dm_verity_version),
      "",  // skip data_dev
      "",  // skip hash_dev
      std::to_string(descriptor->data_block_size),
      std::to_string(descriptor->hash_block_size),
      std::to_string(descriptor->image_size /
                     descriptor->data_block_size),  // #blocks
      std::to_string(descriptor->image_size /
                     descriptor->data_block_size),  // hash_start
      GetHashAlgorithm(*descriptor),
      descriptor->root_digest,
      descriptor->salt,
  };

  size_t next_pos = 0;
  for (const std::string &descriptor_value : descriptor_values) {
    size_t begin_pos = NextWord(target.data, &next_pos);
    ASSERT_NE(begin_pos, std::string::npos);
    if (!descriptor_value.empty()) {
      EXPECT_EQ(target.data.compare(begin_pos, next_pos - begin_pos,
                                    descriptor_value),
                0);
    }
  }

  // Compare the target's optional parameters with the descriptor.
  unsigned long opt_param_count;
  {
    size_t begin_pos = NextWord(target.data, &next_pos);
    ASSERT_NE(begin_pos, std::string::npos);
    opt_param_count =
        std::stoul(target.data.substr(begin_pos, next_pos - begin_pos));
  }
  // https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity#optional-parameters
  std::set<std::string> opt_params = {
      "check_at_most_once",
      "ignore_corruption",
      "ignore_zero_blocks",
      "restart_on_corruption",
  };
  // https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity#optional-fec-forward-error-correction-parameters
  std::map<std::string, std::string> opt_fec_params = {
      {"fec_blocks", ""},
      {"fec_roots", ""},
      {"fec_start", ""},
      {"use_fec_from_device", ""},
  };

  for (unsigned long i = 0; i < opt_param_count; i++) {
    size_t begin_pos = NextWord(target.data, &next_pos);
    ASSERT_NE(begin_pos, std::string::npos);
    const std::string param_name(target.data, begin_pos, next_pos - begin_pos);
    if (opt_fec_params.count(param_name)) {
      i++;
      ASSERT_LT(i, opt_param_count);
      begin_pos = NextWord(target.data, &next_pos);
      ASSERT_NE(begin_pos, std::string::npos);
      opt_fec_params[param_name] =
          target.data.substr(begin_pos, next_pos - begin_pos);
    } else {
      ASSERT_NE(opt_params.count(param_name), 0)
          << "Unknown dm-verity target parameter: " << param_name;
    }
  }

  EXPECT_EQ(opt_fec_params["fec_roots"],
            std::to_string(descriptor->fec_num_roots));
  EXPECT_EQ(
      opt_fec_params["fec_blocks"],
      std::to_string(descriptor->fec_offset / descriptor->data_block_size));
  EXPECT_EQ(
      opt_fec_params["fec_start"],
      std::to_string(descriptor->fec_offset / descriptor->data_block_size));
  // skip use_fec_from_device

  ASSERT_EQ(NextWord(target.data, &next_pos), std::string::npos);
}
