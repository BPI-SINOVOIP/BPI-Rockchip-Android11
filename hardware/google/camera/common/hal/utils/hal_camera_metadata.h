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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_CAMERA_METADATA_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_CAMERA_METADATA_H_

#include <system/camera_metadata.h>
#include <utils/Errors.h>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace android {
namespace google_camera_hal {

using ::android::status_t;

// kOnlyTagEntry: Only tag entry information
// kTagEntryWith16Data: Tag entry information plus at most 16 data values
// kAllInformation: All information
enum class MetadataDumpVerbosity : uint32_t {
  kOnlyTagEntry = 0,
  kTagEntryWith16Data,
  kAllInformation,
};

class HalCameraMetadata {
 public:
  // Create a HalCameraMetadata and allocate camera_metadata.
  // entry_capacity is the number of entries in the camera_metadata.
  // data_capacity is the number of bytes of the data in the camera_metadata.
  static std::unique_ptr<HalCameraMetadata> Create(size_t entry_capacity,
                                                   size_t data_capacity);

  // Create a HalCameraMetadata to own camera_metadata.
  // metadata will be owned by this HalCameraMetadata.
  // This will return nullptr if metadata is nullptr.
  // If this returns nullptr, the caller still has the ownership for metadata.
  static std::unique_ptr<HalCameraMetadata> Create(camera_metadata_t* metadata);

  // Create a HalCameraMetadata and clone the metadata.
  // metadata will be cloned and still owned by the caller.
  // This will return nullptr if metadata is nullptr.
  static std::unique_ptr<HalCameraMetadata> Clone(
      const camera_metadata_t* metadata);

  // Create a HalCameraMetadata and clone the metadata.
  // hal_metadata will be cloned and still owned by the caller.
  // This will return nullptr if metadata is nullptr.
  static std::unique_ptr<HalCameraMetadata> Clone(
      const HalCameraMetadata* hal_metadata);

  virtual ~HalCameraMetadata();

  // Return the camera_metadata owned by this HalCameraMetadata and transfer
  // the ownership to the caller. Caller is responsible for freeing the
  // camera metadata.
  camera_metadata_t* ReleaseCameraMetadata();

  // Returns the raw camera metadata pointer bound to this class. Note that it
  // would be an error to free this metadata or to change it in any way outside
  // this class while it's still owned by this class.
  const camera_metadata_t* GetRawCameraMetadata() const;

  // Get the size of the metadata in the metadata in bytes.
  size_t GetCameraMetadataSize() const;

  // Set metadata entry for setting a key's value
  status_t Set(uint32_t tag, const uint8_t* data, uint32_t data_count);
  status_t Set(uint32_t tag, const int32_t* data, uint32_t data_count);
  status_t Set(uint32_t tag, const float* data, uint32_t data_count);
  status_t Set(uint32_t tag, const int64_t* data, uint32_t data_count);
  status_t Set(uint32_t tag, const double* data, uint32_t data_count);
  status_t Set(uint32_t tag, const camera_metadata_rational_t* data,
               uint32_t data_count);
  status_t Set(uint32_t tag, const std::string& string);
  // This is a helper function, it will call related Set(...).
  // You don't need to set type in the entry, it gets type from tag id.
  status_t Set(const camera_metadata_ro_entry& entry);

  // Get a key's value by tag. Returns NAME_NOT_FOUND if the tag does not exist
  status_t Get(uint32_t tag, camera_metadata_ro_entry* entry) const;

  // Get a key's value by entry index. Returns NAME_NOT_FOUND if the tag does not exist
  status_t GetByIndex(camera_metadata_ro_entry* entry, size_t entry_index) const;

  // Erase a key. This is an expensive operation resulting in revalidation of
  // the entire metadata structure
  status_t Erase(uint32_t tag);

  // Erase all the given keys. This is an expensive operation and will result in
  // de-allocating memory for the original metadata container and allocating a
  // new smaller container to shrink the size. This will be considerably more
  // efficient than calling the 'Erase(uint32_t tag)' counterpart multiple
  // times, but less efficient if the number of tags being erased is very small.
  status_t Erase(const std::unordered_set<uint32_t>& tags);

  // Dump metadata
  // fd >= 0, dump metadata to file
  // fd < 0, dump metadata to logcat
  void Dump(int32_t fd,
            MetadataDumpVerbosity verbosity =
                MetadataDumpVerbosity::kTagEntryWith16Data,
            uint32_t indentation = 0) const;

  // Append metadata from HalCameraMetadata object
  status_t Append(std::unique_ptr<HalCameraMetadata> hal_metadata);

  // Append metadata from a raw camera_metadata buffer
  status_t Append(camera_metadata_t* metadata);

  // Get metadata entry size
  size_t GetEntryCount() const;

 protected:
  HalCameraMetadata(camera_metadata_t* metadata);

 private:
  // For dump metadata
  static void PrintData(const uint8_t* data, int32_t type, int32_t count,
                        uint32_t indentation);

  bool IsTypeValid(uint32_t tag, int32_t expected_type);

  // Base Set entry method.
  status_t SetMetadataRaw(uint32_t tag, const void* data, size_t data_count);

  status_t ResizeIfNeeded(size_t extra_entries, size_t extra_data);

  // Copy entry at the given index from source buffer to destination buffer
  status_t CopyEntry(const camera_metadata_t* src, camera_metadata_t* dest,
                     size_t entry_index) const;

  // Camera metadata owned by this HalCameraMetadata.
  mutable std::mutex metadata_lock_;
  camera_metadata_t* metadata_ = nullptr;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_HAL_CAMERA_METADATA_H_
