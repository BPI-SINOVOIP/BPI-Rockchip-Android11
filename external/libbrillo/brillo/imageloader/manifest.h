// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_IMAGELOADER_MANIFEST_H_
#define LIBBRILLO_BRILLO_IMAGELOADER_MANIFEST_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/brillo_export.h>

namespace brillo {
namespace imageloader {

// The supported file systems for images.
enum class FileSystem { kExt4, kSquashFS };

// A class to parse and store imageloader.json manifest.
class BRILLO_EXPORT Manifest {
 public:
  Manifest();
  // Parse the manifest raw string. Return true if successful.
  bool ParseManifest(const std::string& manifest_raw);
  // Getters for manifest fields:
  int manifest_version() const;
  const std::vector<uint8_t>& image_sha256() const;
  const std::vector<uint8_t>& table_sha256() const;
  const std::string& version() const;
  FileSystem fs_type() const;
  bool is_removable() const;
  const std::map<std::string, std::string> metadata() const;

 private:
  // Manifest fields:
  int manifest_version_;
  std::vector<uint8_t> image_sha256_;
  std::vector<uint8_t> table_sha256_;
  std::string version_;
  FileSystem fs_type_;
  bool is_removable_;
  std::map<std::string, std::string> metadata_;

  DISALLOW_COPY_AND_ASSIGN(Manifest);
};

}  // namespace imageloader
}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_IMAGELOADER_MANIFEST_H_
