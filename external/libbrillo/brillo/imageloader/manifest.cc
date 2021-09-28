// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/imageloader/manifest.h>

#include <memory>
#include <utility>

#include <base/json/json_string_value_serializer.h>
#include <base/strings/string_number_conversions.h>

namespace brillo {
namespace imageloader {

namespace {
// The current version of the manifest file.
constexpr int kCurrentManifestVersion = 1;
// The name of the version field in the manifest.
constexpr char kManifestVersionField[] = "manifest-version";
// The name of the component version field in the manifest.
constexpr char kVersionField[] = "version";
// The name of the field containing the image hash.
constexpr char kImageHashField[] = "image-sha256-hash";
// The name of the bool field indicating whether component is removable.
constexpr char kIsRemovableField[] = "is-removable";
// The name of the metadata field.
constexpr char kMetadataField[] = "metadata";
// The name of the field containing the table hash.
constexpr char kTableHashField[] = "table-sha256-hash";
// The name of the optional field containing the file system type.
constexpr char kFSType[] = "fs-type";

bool GetSHA256FromString(const std::string& hash_str,
                         std::vector<uint8_t>* bytes) {
  if (!base::HexStringToBytes(hash_str, bytes))
    return false;
  return bytes->size() == 32;
}

// Ensure the metadata entry is a dictionary mapping strings to strings and
// parse it into |out_metadata| and return true if so.
bool ParseMetadata(const base::Value* metadata_element,
                   std::map<std::string, std::string>* out_metadata) {
  DCHECK(out_metadata);

  const base::DictionaryValue* metadata_dict = nullptr;
  if (!metadata_element->GetAsDictionary(&metadata_dict))
    return false;

  base::DictionaryValue::Iterator it(*metadata_dict);
  for (; !it.IsAtEnd(); it.Advance()) {
    std::string parsed_value;
    if (!it.value().GetAsString(&parsed_value)) {
      LOG(ERROR) << "Key \"" << it.key() << "\" did not map to string value";
      return false;
    }

    (*out_metadata)[it.key()] = std::move(parsed_value);
  }

  return true;
}

}  // namespace

Manifest::Manifest() {}

bool Manifest::ParseManifest(const std::string& manifest_raw) {
  // Now deserialize the manifest json and read out the rest of the component.
  int error_code;
  std::string error_message;
  JSONStringValueDeserializer deserializer(manifest_raw);
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);

  if (!value) {
    LOG(ERROR) << "Could not deserialize the manifest file. Error "
               << error_code << ": " << error_message;
    return false;
  }

  base::DictionaryValue* manifest_dict = nullptr;
  if (!value->GetAsDictionary(&manifest_dict)) {
    LOG(ERROR) << "Could not parse manifest file as JSON.";
    return false;
  }

  // This will have to be changed if the manifest version is bumped.
  int version;
  if (!manifest_dict->GetInteger(kManifestVersionField, &version)) {
    LOG(ERROR) << "Could not parse manifest version field from manifest.";
    return false;
  }
  if (version != kCurrentManifestVersion) {
    LOG(ERROR) << "Unsupported version of the manifest.";
    return false;
  }
  manifest_version_ = version;

  std::string image_hash_str;
  if (!manifest_dict->GetString(kImageHashField, &image_hash_str)) {
    LOG(ERROR) << "Could not parse image hash from manifest.";
    return false;
  }

  if (!GetSHA256FromString(image_hash_str, &(image_sha256_))) {
    LOG(ERROR) << "Could not convert image hash to bytes.";
    return false;
  }

  std::string table_hash_str;
  if (!manifest_dict->GetString(kTableHashField, &table_hash_str)) {
    LOG(ERROR) << "Could not parse table hash from manifest.";
    return false;
  }

  if (!GetSHA256FromString(table_hash_str, &(table_sha256_))) {
    LOG(ERROR) << "Could not convert table hash to bytes.";
    return false;
  }

  if (!manifest_dict->GetString(kVersionField, &(version_))) {
    LOG(ERROR) << "Could not parse component version from manifest.";
    return false;
  }

  // The fs_type field is optional, and squashfs by default.
  fs_type_ = FileSystem::kSquashFS;
  std::string fs_type;
  if (manifest_dict->GetString(kFSType, &fs_type)) {
    if (fs_type == "ext4") {
      fs_type_ = FileSystem::kExt4;
    } else if (fs_type == "squashfs") {
      fs_type_ = FileSystem::kSquashFS;
    } else {
      LOG(ERROR) << "Unsupported file system type: " << fs_type;
      return false;
    }
  }

  if (!manifest_dict->GetBoolean(kIsRemovableField, &(is_removable_))) {
    // If is_removable field does not exist, by default it is false.
    is_removable_ = false;
  }

  // Copy out the metadata, if it's there.
  const base::Value* metadata = nullptr;
  if (manifest_dict->Get(kMetadataField, &metadata)) {
    if (!ParseMetadata(metadata, &(metadata_))) {
      LOG(ERROR) << "Manifest metadata was malformed";
      return false;
    }
  }

  return true;
}

int Manifest::manifest_version() const {
  return manifest_version_;
}

const std::vector<uint8_t>& Manifest::image_sha256() const {
  return image_sha256_;
}

const std::vector<uint8_t>& Manifest::table_sha256() const {
  return table_sha256_;
}

const std::string& Manifest::version() const {
  return version_;
}

FileSystem Manifest::fs_type() const {
  return fs_type_;
}

bool Manifest::is_removable() const {
  return is_removable_;
}

const std::map<std::string, std::string> Manifest::metadata() const {
  return metadata_;
}

}  // namespace imageloader
}  // namespace brillo
