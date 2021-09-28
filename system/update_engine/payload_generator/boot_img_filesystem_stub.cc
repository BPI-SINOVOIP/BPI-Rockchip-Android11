//
// Copyright (C) 2020 The Android Open Source Project
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

#include "update_engine/payload_generator/boot_img_filesystem.h"

namespace chromeos_update_engine {
std::unique_ptr<BootImgFilesystem> BootImgFilesystem::CreateFromFile(
    const std::string& /* filename */) {
  return nullptr;
}

size_t BootImgFilesystem::GetBlockSize() const {
  return 4096;
}

size_t BootImgFilesystem::GetBlockCount() const {
  return 0;
}

FilesystemInterface::File BootImgFilesystem::GetFile(
    const std::string& /* name */,
    uint64_t /* offset */,
    uint64_t /* size */) const {
  return {};
}

bool BootImgFilesystem::GetFiles(std::vector<File>* /* files */) const {
  return false;
}

bool BootImgFilesystem::LoadSettings(brillo::KeyValueStore* /* store */) const {
  return false;
}

}  // namespace chromeos_update_engine
