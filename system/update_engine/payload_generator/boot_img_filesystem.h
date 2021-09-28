//
// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef UPDATE_ENGINE_PAYLOAD_GENERATOR_BOOT_IMG_FILESYSTEM_H_
#define UPDATE_ENGINE_PAYLOAD_GENERATOR_BOOT_IMG_FILESYSTEM_H_

#include "update_engine/payload_generator/filesystem_interface.h"

#include <memory>
#include <string>
#include <vector>

namespace chromeos_update_engine {

class BootImgFilesystem : public FilesystemInterface {
 public:
  // Creates an BootImgFilesystem from an Android boot.img file.
  static std::unique_ptr<BootImgFilesystem> CreateFromFile(
      const std::string& filename);
  ~BootImgFilesystem() override = default;

  // FilesystemInterface overrides.
  size_t GetBlockSize() const override;
  size_t GetBlockCount() const override;

  // GetFiles will return one FilesystemInterface::File for kernel and one for
  // ramdisk.
  bool GetFiles(std::vector<File>* files) const override;

  bool LoadSettings(brillo::KeyValueStore* store) const override;

 private:
  friend class BootImgFilesystemTest;

  BootImgFilesystem() = default;

  File GetFile(const std::string& name, uint64_t offset, uint64_t size) const;

  // The boot.img file path.
  std::string filename_;

  uint32_t kernel_size_;  /* size in bytes */
  uint32_t ramdisk_size_; /* size in bytes */
  uint32_t page_size_;    /* flash page size we assume */

  DISALLOW_COPY_AND_ASSIGN(BootImgFilesystem);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_GENERATOR_BOOT_IMG_FILESYSTEM_H_
