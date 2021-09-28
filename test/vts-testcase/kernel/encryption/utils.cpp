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

// Utility functions for VtsKernelEncryptionTest.

#include <LzmaLib.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <errno.h>
#include <ext4_utils/ext4.h>
#include <ext4_utils/ext4_sb.h>
#include <ext4_utils/ext4_utils.h>
#include <gtest/gtest.h>
#include <libdm/dm.h>
#include <linux/magic.h>
#include <mntent.h>
#include <openssl/cmac.h>
#include <unistd.h>

#include "Keymaster.h"
#include "vts_kernel_encryption.h"

using namespace android::dm;

namespace android {
namespace kernel {

// Offset in bytes to the filesystem superblock, relative to the beginning of
// the block device
constexpr int kExt4SuperBlockOffset = 1024;
constexpr int kF2fsSuperBlockOffset = 1024;

// For F2FS: the offsets in bytes to the filesystem magic number and filesystem
// UUID, relative to the beginning of the block device
constexpr int kF2fsMagicOffset = kF2fsSuperBlockOffset;
constexpr int kF2fsUuidOffset = kF2fsSuperBlockOffset + 108;

// hw-wrapped key size in bytes
constexpr int kHwWrappedKeySize = 32;

std::string Errno() { return std::string(": ") + strerror(errno); }

// Recursively deletes the file or directory at |path|, if it exists.
void DeleteRecursively(const std::string &path) {
  if (unlink(path.c_str()) == 0 || errno == ENOENT) return;
  ASSERT_EQ(EISDIR, errno) << "Failed to unlink " << path << Errno();

  std::unique_ptr<DIR, int (*)(DIR *)> dirp(opendir(path.c_str()), closedir);
  // If the directory was assigned an encryption policy that the kernel lacks
  // crypto API support for, then opening it will fail, and it will be empty.
  // So, we have to allow opening the directory to fail.
  if (dirp != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dirp.get())) != nullptr) {
      std::string filename(entry->d_name);
      if (filename != "." && filename != "..")
        DeleteRecursively(path + "/" + filename);
    }
  }
  ASSERT_EQ(0, rmdir(path.c_str()))
      << "Failed to remove directory " << path << Errno();
}

// Generates some "random" bytes.  Not secure; this is for testing only.
void RandomBytesForTesting(std::vector<uint8_t> &bytes) {
  for (size_t i = 0; i < bytes.size(); i++) {
    bytes[i] = rand();
  }
}

// Generates a "random" key.  Not secure; this is for testing only.
std::vector<uint8_t> GenerateTestKey(size_t size) {
  std::vector<uint8_t> key(size);
  RandomBytesForTesting(key);
  return key;
}

std::string BytesToHex(const std::vector<uint8_t> &bytes) {
  std::ostringstream o;
  for (uint8_t b : bytes) {
    o << std::hex << std::setw(2) << std::setfill('0') << (int)b;
  }
  return o.str();
}

bool GetFirstApiLevel(int *first_api_level) {
  *first_api_level =
      android::base::GetIntProperty("ro.product.first_api_level", 0);
  if (*first_api_level == 0) {
    ADD_FAILURE() << "ro.product.first_api_level is unset";
    return false;
  }
  GTEST_LOG_(INFO) << "ro.product.first_api_level = " << *first_api_level;
  return true;
}

// Gets the block device and type of the filesystem mounted on |mountpoint|.
// This block device is the one on which the filesystem is directly located.  In
// the case of device-mapper that means something like /dev/mapper/dm-5, not the
// underlying device like /dev/block/by-name/userdata.
static bool GetFsBlockDeviceAndType(const std::string &mountpoint,
                                    std::string *fs_blk_device,
                                    std::string *fs_type) {
  std::unique_ptr<FILE, int (*)(FILE *)> mnts(setmntent("/proc/mounts", "re"),
                                              endmntent);
  if (!mnts) {
    ADD_FAILURE() << "Failed to open /proc/mounts" << Errno();
    return false;
  }
  struct mntent *mnt;
  while ((mnt = getmntent(mnts.get())) != nullptr) {
    if (mnt->mnt_dir == mountpoint) {
      *fs_blk_device = mnt->mnt_fsname;
      *fs_type = mnt->mnt_type;
      return true;
    }
  }
  ADD_FAILURE() << "No /proc/mounts entry found for " << mountpoint;
  return false;
}

// Gets the UUID of the filesystem of type |fs_type| that's located on
// |fs_blk_device|.
//
// Unfortunately there's no kernel API to get the UUID; instead we have to read
// it from the filesystem superblock.
static bool GetFilesystemUuid(const std::string &fs_blk_device,
                              const std::string &fs_type,
                              FilesystemUuid *fs_uuid) {
  android::base::unique_fd fd(
      open(fs_blk_device.c_str(), O_RDONLY | O_CLOEXEC));
  if (fd < 0) {
    ADD_FAILURE() << "Failed to open fs block device " << fs_blk_device
                  << Errno();
    return false;
  }

  if (fs_type == "ext4") {
    struct ext4_super_block sb;

    if (pread(fd, &sb, sizeof(sb), kExt4SuperBlockOffset) != sizeof(sb)) {
      ADD_FAILURE() << "Error reading ext4 superblock from " << fs_blk_device
                    << Errno();
      return false;
    }
    if (sb.s_magic != cpu_to_le16(EXT4_SUPER_MAGIC)) {
      ADD_FAILURE() << "Failed to find ext4 superblock on " << fs_blk_device;
      return false;
    }
    static_assert(sizeof(sb.s_uuid) == kFilesystemUuidSize);
    memcpy(fs_uuid->bytes, sb.s_uuid, kFilesystemUuidSize);
  } else if (fs_type == "f2fs") {
    // Android doesn't have an f2fs equivalent of libext4_utils, so we have to
    // hard-code the offset to the magic number and UUID.

    __le32 magic;
    if (pread(fd, &magic, sizeof(magic), kF2fsMagicOffset) != sizeof(magic)) {
      ADD_FAILURE() << "Error reading f2fs superblock from " << fs_blk_device
                    << Errno();
      return false;
    }
    if (magic != cpu_to_le32(F2FS_SUPER_MAGIC)) {
      ADD_FAILURE() << "Failed to find f2fs superblock on " << fs_blk_device;
      return false;
    }
    if (pread(fd, fs_uuid->bytes, kFilesystemUuidSize, kF2fsUuidOffset) !=
        kFilesystemUuidSize) {
      ADD_FAILURE() << "Failed to read f2fs filesystem UUID from "
                    << fs_blk_device << Errno();
      return false;
    }
  } else {
    ADD_FAILURE() << "Unknown filesystem type " << fs_type;
    return false;
  }
  return true;
}

// Gets the raw block device of the filesystem that is mounted from
// |fs_blk_device|.  By "raw block device" we mean a block device from which we
// can read the encrypted file contents and filesystem metadata.  When metadata
// encryption is disabled, this is simply |fs_blk_device|.  When metadata
// encryption is enabled, then |fs_blk_device| is a dm-default-key device and
// the "raw block device" is the parent of this dm-default-key device.
//
// We don't just use the block device listed in the fstab, because (a) it can be
// a logical partition name which needs extra code to map to a block device, and
// (b) due to block-level checkpointing, there can be a dm-bow device between
// the fstab partition and dm-default-key.  dm-bow can remap sectors, but for
// encryption testing we don't want any sector remapping.  So the correct block
// device to read ciphertext from is the one directly underneath dm-default-key.
static bool GetRawBlockDevice(const std::string &fs_blk_device,
                              std::string *raw_blk_device) {
  DeviceMapper &dm = DeviceMapper::Instance();

  if (!dm.IsDmBlockDevice(fs_blk_device)) {
    GTEST_LOG_(INFO)
        << fs_blk_device
        << " is not a device-mapper device; metadata encryption is disabled";
    *raw_blk_device = fs_blk_device;
    return true;
  }
  const std::optional<std::string> name =
      dm.GetDmDeviceNameByPath(fs_blk_device);
  if (!name) {
    ADD_FAILURE() << "Failed to get name of device-mapper device "
                  << fs_blk_device;
    return false;
  }

  std::vector<DeviceMapper::TargetInfo> table;
  if (!dm.GetTableInfo(*name, &table)) {
    ADD_FAILURE() << "Failed to get table of device-mapper device " << *name;
    return false;
  }
  if (table.size() != 1) {
    GTEST_LOG_(INFO) << fs_blk_device
                     << " has multiple device-mapper targets; assuming "
                        "metadata encryption is disabled";
    *raw_blk_device = fs_blk_device;
    return true;
  }
  const std::string target_type = dm.GetTargetType(table[0].spec);
  if (target_type != "default-key") {
    GTEST_LOG_(INFO) << fs_blk_device << " is a dm-" << target_type
                     << " device, not dm-default-key; assuming metadata "
                        "encryption is disabled";
    *raw_blk_device = fs_blk_device;
    return true;
  }
  std::optional<std::string> parent =
      dm.GetParentBlockDeviceByPath(fs_blk_device);
  if (!parent) {
    ADD_FAILURE() << "Failed to get parent of dm-default-key device " << *name;
    return false;
  }
  *raw_blk_device = *parent;
  return true;
}

// Gets information about the filesystem mounted on |mountpoint|.
bool GetFilesystemInfo(const std::string &mountpoint, FilesystemInfo *info) {
  if (!GetFsBlockDeviceAndType(mountpoint, &info->fs_blk_device, &info->type))
    return false;

  if (!GetFilesystemUuid(info->fs_blk_device, info->type, &info->uuid))
    return false;

  if (!GetRawBlockDevice(info->fs_blk_device, &info->raw_blk_device))
    return false;

  GTEST_LOG_(INFO) << info->fs_blk_device << " is mounted on " << mountpoint
                   << " with type " << info->type << "; UUID is "
                   << BytesToHex(info->uuid.bytes) << ", raw block device is "
                   << info->raw_blk_device;
  return true;
}

// Returns true if the given data seems to be random.
//
// Check compressibility rather than byte frequencies.  Compressibility is a
// stronger test since it also detects repetitions.
//
// To check compressibility, use LZMA rather than DEFLATE/zlib/gzip because LZMA
// compression is stronger and supports a much larger dictionary.  DEFLATE is
// limited to a 32 KiB dictionary.  So, data repeating after 32 KiB (or more)
// would not be detected with DEFLATE.  But LZMA can detect it.
bool VerifyDataRandomness(const std::vector<uint8_t> &bytes) {
  // To avoid flakiness, allow the data to be compressed a tiny bit by chance.
  // There is at most a 2^-32 chance that random data can be compressed to be 4
  // bytes shorter.  In practice it's even lower due to compression overhead.
  size_t destLen = bytes.size() - std::min<size_t>(4, bytes.size());
  std::vector<uint8_t> dest(destLen);
  uint8_t outProps[LZMA_PROPS_SIZE];
  size_t outPropsSize = LZMA_PROPS_SIZE;
  int ret;

  ret = LzmaCompress(dest.data(), &destLen, bytes.data(), bytes.size(),
                     outProps, &outPropsSize,
                     6,               // compression level (0 <= level <= 9)
                     bytes.size(),    // dictionary size
                     -1, -1, -1, -1,  // lc, lp, bp, fb (-1 selects the default)
                     1);              // number of threads

  if (ret == SZ_ERROR_OUTPUT_EOF) return true;  // incompressible

  if (ret == SZ_OK) {
    ADD_FAILURE() << "Data is not random!  Compressed " << bytes.size()
                  << " to " << destLen << " bytes";
  } else {
    ADD_FAILURE() << "LZMA compression error: ret=" << ret;
  }
  return false;
}

static bool TryPrepareHwWrappedKey(Keymaster &keymaster,
                                   const std::string &master_key_string,
                                   std::string *exported_key_string,
                                   bool rollback_resistance) {
  // This key is used to drive a CMAC-based KDF
  auto paramBuilder =
      km::AuthorizationSetBuilder().AesEncryptionKey(kHwWrappedKeySize * 8);
  if (rollback_resistance) {
    paramBuilder.Authorization(km::TAG_ROLLBACK_RESISTANCE);
  }
  paramBuilder.Authorization(km::TAG_STORAGE_KEY);

  std::string wrapped_key_blob;
  if (keymaster.importKey(paramBuilder, km::KeyFormat::RAW, master_key_string,
                          &wrapped_key_blob) &&
      keymaster.exportKey(wrapped_key_blob, exported_key_string)) {
    return true;
  }
  // It's fine for Keymaster not to support hardware-wrapped keys, but
  // if generateKey works, importKey must too.
  if (keymaster.generateKey(paramBuilder, &wrapped_key_blob) &&
      keymaster.exportKey(wrapped_key_blob, exported_key_string)) {
    ADD_FAILURE() << "generateKey succeeded but importKey failed";
  }
  return false;
}

bool CreateHwWrappedKey(std::vector<uint8_t> *master_key,
                        std::vector<uint8_t> *exported_key) {
  *master_key = GenerateTestKey(kHwWrappedKeySize);

  Keymaster keymaster;
  if (!keymaster) {
    ADD_FAILURE() << "Unable to find keymaster";
    return false;
  }
  std::string master_key_string(master_key->begin(), master_key->end());
  std::string exported_key_string;
  // Make two attempts to create a key, first with and then without
  // rollback resistance.
  if (TryPrepareHwWrappedKey(keymaster, master_key_string, &exported_key_string,
                             true) ||
      TryPrepareHwWrappedKey(keymaster, master_key_string, &exported_key_string,
                             false)) {
    exported_key->assign(exported_key_string.begin(),
                         exported_key_string.end());
    return true;
  }
  GTEST_LOG_(INFO) << "Skipping test because device doesn't support "
                      "hardware-wrapped keys";
  return false;
}

static void PushBigEndian32(uint32_t val, std::vector<uint8_t> *vec) {
  for (int i = 24; i >= 0; i -= 8) {
    vec->push_back((val >> i) & 0xFF);
  }
}

static void GetFixedInputString(uint32_t counter,
                                const std::vector<uint8_t> &label,
                                const std::vector<uint8_t> &context,
                                uint32_t derived_key_len,
                                std::vector<uint8_t> *fixed_input_string) {
  PushBigEndian32(counter, fixed_input_string);
  fixed_input_string->insert(fixed_input_string->end(), label.begin(),
                             label.end());
  fixed_input_string->push_back(0);
  fixed_input_string->insert(fixed_input_string->end(), context.begin(),
                             context.end());
  PushBigEndian32(derived_key_len, fixed_input_string);
}

static bool AesCmacKdfHelper(const std::vector<uint8_t> &key,
                             const std::vector<uint8_t> &label,
                             const std::vector<uint8_t> &context,
                             uint32_t output_key_size,
                             std::vector<uint8_t> *output_data) {
  output_data->resize(output_key_size);
  for (size_t count = 0; count < (output_key_size / kAesBlockSize); count++) {
    std::vector<uint8_t> fixed_input_string;
    GetFixedInputString(count + 1, label, context, (output_key_size * 8),
                        &fixed_input_string);
    if (!AES_CMAC(output_data->data() + (kAesBlockSize * count), key.data(),
                  key.size(), fixed_input_string.data(),
                  fixed_input_string.size())) {
      ADD_FAILURE()
          << "AES_CMAC failed while deriving subkey from HW wrapped key";
      return false;
    }
  }
  return true;
}

bool DeriveHwWrappedEncryptionKey(const std::vector<uint8_t> &master_key,
                                  std::vector<uint8_t> *enc_key) {
  std::vector<uint8_t> label{0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x20};
  // Context in fixed input string comprises of software provided context,
  // padding to eight bytes (if required) and the key policy.
  std::vector<uint8_t> context = {
      'i', 'n', 'l', 'i', 'n', 'e', ' ', 'e',
      'n', 'c', 'r', 'y', 'p', 't', 'i', 'o',
      'n', ' ', 'k', 'e', 'y', 0x0, 0x0, 0x0,
      0x00, 0x00, 0x00, 0x02, 0x43, 0x00, 0x82, 0x50,
      0x0,  0x0,  0x0,  0x0};

  return AesCmacKdfHelper(master_key, label, context, kAes256XtsKeySize,
                          enc_key);
}

bool DeriveHwWrappedRawSecret(const std::vector<uint8_t> &master_key,
                              std::vector<uint8_t> *secret) {
  std::vector<uint8_t> label{0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x20};
  // Context in fixed input string comprises of software provided context,
  // padding to eight bytes (if required) and the key policy.
  std::vector<uint8_t> context = {'r',  'a',  'w',  ' ',  's',  'e',  'c',
                                  'r',  'e',  't',  0x0,  0x0,  0x0,  0x0,
                                  0x0,  0x0,  0x00, 0x00, 0x00, 0x02, 0x17,
                                  0x00, 0x80, 0x50, 0x0,  0x0,  0x0,  0x0};

  return AesCmacKdfHelper(master_key, label, context, kAes256KeySize, secret);
}

}  // namespace kernel
}  // namespace android
