/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "apex_file.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/scopeguard.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <google/protobuf/util/message_differencer.h>
#include <libavb/libavb.h>

#include "apex_constants.h"
#include "apex_preinstalled_data.h"
#include "apexd_utils.h"
#include "string_log.h"

using android::base::EndsWith;
using android::base::Error;
using android::base::ReadFullyAtOffset;
using android::base::Result;
using android::base::StartsWith;
using android::base::unique_fd;
using google::protobuf::util::MessageDifferencer;

namespace android {
namespace apex {
namespace {

constexpr const char* kImageFilename = "apex_payload.img";
constexpr const char* kBundledPublicKeyFilename = "apex_pubkey";

}  // namespace

Result<ApexFile> ApexFile::Open(const std::string& path) {
  int32_t image_offset;
  size_t image_size;
  std::string manifest_content;
  std::string pubkey;

  ZipArchiveHandle handle;
  auto handle_guard =
      android::base::make_scope_guard([&handle] { CloseArchive(handle); });
  int ret = OpenArchive(path.c_str(), &handle);
  if (ret < 0) {
    return Error() << "Failed to open package " << path << ": "
                   << ErrorCodeString(ret);
  }

  // Locate the mountable image within the zipfile and store offset and size.
  ZipEntry entry;
  ret = FindEntry(handle, kImageFilename, &entry);
  if (ret < 0) {
    return Error() << "Could not find entry \"" << kImageFilename
                   << "\" in package " << path << ": " << ErrorCodeString(ret);
  }
  image_offset = entry.offset;
  image_size = entry.uncompressed_length;

  ret = FindEntry(handle, kManifestFilenamePb, &entry);
  if (ret < 0) {
    return Error() << "Could not find entry \"" << kManifestFilenamePb
                   << "\" in package " << path << ": " << ErrorCodeString(ret);
  }

  uint32_t length = entry.uncompressed_length;
  manifest_content.resize(length, '\0');
  ret = ExtractToMemory(handle, &entry,
                        reinterpret_cast<uint8_t*>(&(manifest_content)[0]),
                        length);
  if (ret != 0) {
    return Error() << "Failed to extract manifest from package " << path << ": "
                   << ErrorCodeString(ret);
  }

  ret = FindEntry(handle, kBundledPublicKeyFilename, &entry);
  if (ret >= 0) {
    length = entry.uncompressed_length;
    pubkey.resize(length, '\0');
    ret = ExtractToMemory(handle, &entry,
                          reinterpret_cast<uint8_t*>(&(pubkey)[0]), length);
    if (ret != 0) {
      return Error() << "Failed to extract public key from package " << path
                     << ": " << ErrorCodeString(ret);
    }
  }

  Result<ApexManifest> manifest = ParseManifest(manifest_content);
  if (!manifest.ok()) {
    return manifest.error();
  }

  return ApexFile(path, image_offset, image_size, std::move(*manifest), pubkey,
                  isPathForBuiltinApexes(path));
}

// AVB-related code.

namespace {

static constexpr int kVbMetaMaxSize = 64 * 1024;

std::string bytes_to_hex(const uint8_t* bytes, size_t bytes_len) {
  std::ostringstream s;

  s << std::hex << std::setfill('0');
  for (size_t i = 0; i < bytes_len; i++) {
    s << std::setw(2) << static_cast<int>(bytes[i]);
  }
  return s.str();
}

std::string getSalt(const AvbHashtreeDescriptor& desc,
                    const uint8_t* trailingData) {
  const uint8_t* desc_salt = trailingData + desc.partition_name_len;

  return bytes_to_hex(desc_salt, desc.salt_len);
}

std::string getDigest(const AvbHashtreeDescriptor& desc,
                      const uint8_t* trailingData) {
  const uint8_t* desc_digest =
      trailingData + desc.partition_name_len + desc.salt_len;

  return bytes_to_hex(desc_digest, desc.root_digest_len);
}

Result<std::unique_ptr<AvbFooter>> getAvbFooter(const ApexFile& apex,
                                                const unique_fd& fd) {
  std::array<uint8_t, AVB_FOOTER_SIZE> footer_data;
  auto footer = std::make_unique<AvbFooter>();

  // The AVB footer is located in the last part of the image
  off_t offset = apex.GetImageSize() + apex.GetImageOffset() - AVB_FOOTER_SIZE;
  int ret = lseek(fd, offset, SEEK_SET);
  if (ret == -1) {
    return ErrnoError() << "Couldn't seek to AVB footer";
  }

  ret = read(fd, footer_data.data(), AVB_FOOTER_SIZE);
  if (ret != AVB_FOOTER_SIZE) {
    return ErrnoError() << "Couldn't read AVB footer";
  }

  if (!avb_footer_validate_and_byteswap((const AvbFooter*)footer_data.data(),
                                        footer.get())) {
    return Error() << "AVB footer verification failed.";
  }

  LOG(VERBOSE) << "AVB footer verification successful.";
  return footer;
}

bool CompareKeys(const uint8_t* key, size_t length,
                 const std::string& public_key_content) {
  return public_key_content.length() == length &&
         memcmp(&public_key_content[0], key, length) == 0;
}

Result<void> verifyVbMetaSignature(const ApexFile& apex, const uint8_t* data,
                                   size_t length) {
  const uint8_t* pk;
  size_t pk_len;
  AvbVBMetaVerifyResult res;

  res = avb_vbmeta_image_verify(data, length, &pk, &pk_len);
  switch (res) {
    case AVB_VBMETA_VERIFY_RESULT_OK:
      break;
    case AVB_VBMETA_VERIFY_RESULT_OK_NOT_SIGNED:
    case AVB_VBMETA_VERIFY_RESULT_HASH_MISMATCH:
    case AVB_VBMETA_VERIFY_RESULT_SIGNATURE_MISMATCH:
      return Error() << "Error verifying " << apex.GetPath() << ": "
                     << avb_vbmeta_verify_result_to_string(res);
    case AVB_VBMETA_VERIFY_RESULT_INVALID_VBMETA_HEADER:
      return Error() << "Error verifying " << apex.GetPath() << ": "
                     << "invalid vbmeta header";
    case AVB_VBMETA_VERIFY_RESULT_UNSUPPORTED_VERSION:
      return Error() << "Error verifying " << apex.GetPath() << ": "
                     << "unsupported version";
    default:
      return Errorf("Unknown vmbeta_image_verify return value");
  }

  Result<const std::string> public_key = getApexKey(apex.GetManifest().name());
  if (public_key.ok()) {
    // TODO(b/115718846)
    // We need to decide whether we need rollback protection, and whether
    // we can use the rollback protection provided by libavb.
    if (!CompareKeys(pk, pk_len, *public_key)) {
      return Error() << "Error verifying " << apex.GetPath() << ": "
                     << "public key doesn't match the pre-installed one";
    }
  } else {
    return public_key.error();
  }
  LOG(VERBOSE) << apex.GetPath() << ": public key matches.";
  return {};
}

Result<std::unique_ptr<uint8_t[]>> verifyVbMeta(const ApexFile& apex,
                                                const unique_fd& fd,
                                                const AvbFooter& footer) {
  if (footer.vbmeta_size > kVbMetaMaxSize) {
    return Errorf("VbMeta size in footer exceeds kVbMetaMaxSize.");
  }

  off_t offset = apex.GetImageOffset() + footer.vbmeta_offset;
  std::unique_ptr<uint8_t[]> vbmeta_buf(new uint8_t[footer.vbmeta_size]);

  if (!ReadFullyAtOffset(fd, vbmeta_buf.get(), footer.vbmeta_size, offset)) {
    return ErrnoError() << "Couldn't read AVB meta-data";
  }

  Result<void> st =
      verifyVbMetaSignature(apex, vbmeta_buf.get(), footer.vbmeta_size);
  if (!st.ok()) {
    return st.error();
  }

  return vbmeta_buf;
}

Result<const AvbHashtreeDescriptor*> findDescriptor(uint8_t* vbmeta_data,
                                                    size_t vbmeta_size) {
  const AvbDescriptor** descriptors;
  size_t num_descriptors;

  descriptors =
      avb_descriptor_get_all(vbmeta_data, vbmeta_size, &num_descriptors);

  // avb_descriptor_get_all() returns an internally allocated array
  // of pointers and it needs to be avb_free()ed after using it.
  auto guard = android::base::ScopeGuard(std::bind(avb_free, descriptors));

  for (size_t i = 0; i < num_descriptors; i++) {
    AvbDescriptor desc;
    if (!avb_descriptor_validate_and_byteswap(descriptors[i], &desc)) {
      return Errorf("Couldn't validate AvbDescriptor.");
    }

    if (desc.tag != AVB_DESCRIPTOR_TAG_HASHTREE) {
      // Ignore other descriptors
      continue;
    }

    // Check that hashtree descriptor actually fits into memory.
    const uint8_t* vbmeta_end = vbmeta_data + vbmeta_size;
    if ((uint8_t*)descriptors[i] + sizeof(AvbHashtreeDescriptor) > vbmeta_end) {
      return Errorf("Invalid length for AvbHashtreeDescriptor");
    }
    return (const AvbHashtreeDescriptor*)descriptors[i];
  }

  return Errorf("Couldn't find any AVB hashtree descriptors.");
}

Result<std::unique_ptr<AvbHashtreeDescriptor>> verifyDescriptor(
    const AvbHashtreeDescriptor* desc) {
  auto verifiedDesc = std::make_unique<AvbHashtreeDescriptor>();

  if (!avb_hashtree_descriptor_validate_and_byteswap(desc,
                                                     verifiedDesc.get())) {
    return Errorf("Couldn't validate AvbDescriptor.");
  }

  return verifiedDesc;
}

}  // namespace

Result<ApexVerityData> ApexFile::VerifyApexVerity() const {
  ApexVerityData verityData;

  unique_fd fd(open(GetPath().c_str(), O_RDONLY | O_CLOEXEC));
  if (fd.get() == -1) {
    return ErrnoError() << "Failed to open " << GetPath();
  }

  Result<std::unique_ptr<AvbFooter>> footer = getAvbFooter(*this, fd);
  if (!footer.ok()) {
    return footer.error();
  }

  Result<std::unique_ptr<uint8_t[]>> vbmeta_data =
      verifyVbMeta(*this, fd, **footer);
  if (!vbmeta_data.ok()) {
    return vbmeta_data.error();
  }

  Result<const AvbHashtreeDescriptor*> descriptor =
      findDescriptor(vbmeta_data->get(), (*footer)->vbmeta_size);
  if (!descriptor.ok()) {
    return descriptor.error();
  }

  Result<std::unique_ptr<AvbHashtreeDescriptor>> verifiedDescriptor =
      verifyDescriptor(*descriptor);
  if (!verifiedDescriptor.ok()) {
    return verifiedDescriptor.error();
  }
  verityData.desc = std::move(*verifiedDescriptor);

  // This area is now safe to access, because we just verified it
  const uint8_t* trailingData =
      (const uint8_t*)*descriptor + sizeof(AvbHashtreeDescriptor);
  verityData.hash_algorithm =
      reinterpret_cast<const char*>((*descriptor)->hash_algorithm);
  verityData.salt = getSalt(*verityData.desc, trailingData);
  verityData.root_digest = getDigest(*verityData.desc, trailingData);

  return verityData;
}

Result<void> ApexFile::VerifyManifestMatches(
    const std::string& mount_path) const {
  Result<ApexManifest> verifiedManifest =
      ReadManifest(mount_path + "/" + kManifestFilenamePb);
  if (!verifiedManifest.ok()) {
    return verifiedManifest.error();
  }

  if (!MessageDifferencer::Equals(manifest_, *verifiedManifest)) {
    return Errorf(
        "Manifest inside filesystem does not match manifest outside it");
  }

  return {};
}

Result<std::vector<std::string>> FindApexes(
    const std::vector<std::string>& paths) {
  std::vector<std::string> result;
  for (const auto& path : paths) {
    auto exist = PathExists(path);
    if (!exist.ok()) {
      return exist.error();
    }
    if (!*exist) continue;

    const auto& apexes = FindApexFilesByName(path);
    if (!apexes.ok()) {
      return apexes;
    }

    result.insert(result.end(), apexes->begin(), apexes->end());
  }
  return result;
}

Result<std::vector<std::string>> FindApexFilesByName(const std::string& path) {
  auto filter_fn = [](const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    if (entry.is_regular_file(ec) &&
        EndsWith(entry.path().filename().string(), kApexPackageSuffix)) {
      return true;  // APEX file, take.
    }
    return false;
  };
  return ReadDir(path, filter_fn);
}

bool isPathForBuiltinApexes(const std::string& path) {
  for (const auto& dir : kApexPackageBuiltinDirs) {
    if (StartsWith(path, dir)) {
      return true;
    }
  }
  return false;
}

}  // namespace apex
}  // namespace android
