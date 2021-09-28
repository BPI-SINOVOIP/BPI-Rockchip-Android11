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

//#define LOG_NDEBUG 0
#define LOG_TAG "GCH_HalCameraMetadata"
#define ATRACE_TAG ATRACE_TAG_CAMERA
#include <log/log.h>
#include <utils/Trace.h>

#include <inttypes.h>

#include "hal_camera_metadata.h"

namespace android {
namespace google_camera_hal {

std::unique_ptr<HalCameraMetadata> HalCameraMetadata::Create(
    camera_metadata_t* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata cannot be nullptr.", __FUNCTION__);
    return nullptr;
  }

  auto hal_metadata =
      std::unique_ptr<HalCameraMetadata>(new HalCameraMetadata(metadata));
  if (hal_metadata == nullptr) {
    ALOGE("%s: Creating HalCameraMetadata failed.", __FUNCTION__);
    return nullptr;
  }

  return hal_metadata;
}

std::unique_ptr<HalCameraMetadata> HalCameraMetadata::Create(
    size_t entry_capacity, size_t data_capacity) {
  camera_metadata_t* metadata =
      allocate_camera_metadata(entry_capacity, data_capacity);
  if (metadata == nullptr) {
    ALOGE("%s: Allocating camera metadata failed.", __FUNCTION__);
    return nullptr;
  }

  auto hal_metadata = Create(metadata);
  if (hal_metadata == nullptr) {
    free_camera_metadata(metadata);
    return nullptr;
  }

  return hal_metadata;
}

std::unique_ptr<HalCameraMetadata> HalCameraMetadata::Clone(
    const camera_metadata_t* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata cannot be nullptr.", __FUNCTION__);
    return nullptr;
  }

  camera_metadata_t* cloned_metadata = clone_camera_metadata(metadata);
  if (cloned_metadata == nullptr) {
    ALOGE("%s: Cloning camera metadata failed.", __FUNCTION__);
    return nullptr;
  }

  auto hal_metadata = Create(cloned_metadata);
  if (hal_metadata == nullptr) {
    free_camera_metadata(cloned_metadata);
    return nullptr;
  }

  return hal_metadata;
}

std::unique_ptr<HalCameraMetadata> HalCameraMetadata::Clone(
    const HalCameraMetadata* hal_metadata) {
  if (hal_metadata == nullptr) {
    return nullptr;
  }

  return Clone(hal_metadata->metadata_);
}

HalCameraMetadata::~HalCameraMetadata() {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ != nullptr) {
    free_camera_metadata(metadata_);
  }
}

HalCameraMetadata::HalCameraMetadata(camera_metadata_t* metadata)
    : metadata_(metadata) {
}

camera_metadata_t* HalCameraMetadata::ReleaseCameraMetadata() {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  camera_metadata_t* metadata = metadata_;
  metadata_ = nullptr;

  return metadata;
}

const camera_metadata_t* HalCameraMetadata::GetRawCameraMetadata() const {
  return metadata_;
}

size_t HalCameraMetadata::GetCameraMetadataSize() const {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    return 0;
  }

  return get_camera_metadata_size(metadata_);
}

status_t HalCameraMetadata::ResizeIfNeeded(size_t extra_entries,
                                           size_t extra_data) {
  bool resize = false;
  size_t current_entry_cap = get_camera_metadata_entry_capacity(metadata_);
  size_t new_entry_count =
      get_camera_metadata_entry_count(metadata_) + extra_entries;
  if (new_entry_count > current_entry_cap) {
    new_entry_count = new_entry_count * 2;
    resize = true;
  } else {
    new_entry_count = current_entry_cap;
  }

  size_t current_data_count = get_camera_metadata_data_count(metadata_);
  size_t current_data_cap = get_camera_metadata_data_capacity(metadata_);
  size_t new_data_count = current_data_count + extra_data;
  if (new_data_count > current_data_cap) {
    new_data_count = new_data_count * 2;
    resize = true;
  } else {
    new_data_count = current_data_cap;
  }

  if (resize) {
    camera_metadata_t* metadata = metadata_;
    metadata_ = allocate_camera_metadata(new_entry_count, new_data_count);
    if (metadata_ == nullptr) {
      ALOGE("%s: Can't allocate larger metadata buffer", __FUNCTION__);
      metadata_ = metadata;
      return NO_MEMORY;
    }
    append_camera_metadata(metadata_, metadata);
    free_camera_metadata(metadata);
  }

  return OK;
}

status_t HalCameraMetadata::SetMetadataRaw(uint32_t tag, const void* data,
                                           size_t data_count) {
  status_t res;
  int type = get_camera_metadata_tag_type(tag);
  if (type == -1) {
    ALOGE("%s: Tag %d not found", __FUNCTION__, tag);
    return BAD_VALUE;
  }
  // Safety check - ensure that data isn't pointing to this metadata, since
  // that would get invalidated if a resize is needed
  size_t buffer_size = get_camera_metadata_size(metadata_);
  uintptr_t buffer_addr = reinterpret_cast<uintptr_t>(metadata_);
  uintptr_t data_addr = reinterpret_cast<uintptr_t>(data);
  if (data_addr > buffer_addr && data_addr < (buffer_addr + buffer_size)) {
    ALOGE("%s: Update attempted with data from the same metadata buffer!",
          __FUNCTION__);
    return INVALID_OPERATION;
  }

  size_t size = calculate_camera_metadata_entry_data_size(type, data_count);
  res = ResizeIfNeeded(/*extra_entries=*/1, size);
  if (res != OK) {
    ALOGE("%s: Resize fail", __FUNCTION__);
    return res;
  }

  camera_metadata_entry_t entry;
  res = find_camera_metadata_entry(metadata_, tag, &entry);
  if (res == NAME_NOT_FOUND) {
    res = add_camera_metadata_entry(metadata_, tag, data, data_count);
  } else if (res == OK) {
    res = update_camera_metadata_entry(metadata_, entry.index, data, data_count,
                                       nullptr);
  }

  return res;
}

bool HalCameraMetadata::IsTypeValid(uint32_t tag, int32_t expected_type) {
  int32_t type = get_camera_metadata_tag_type(tag);
  if (type == -1) {
    ALOGE("%s: Unknown tag 0x%x.", __FUNCTION__, tag);
    return false;
  }
  if (type != expected_type) {
    ALOGE("%s: mismatched type (%d) from tag 0x%x. Expected type is %d",
          __FUNCTION__, type, tag, expected_type);
    return false;
  }
  return true;
}

status_t HalCameraMetadata::Set(uint32_t tag, const uint8_t* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_BYTE) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag, const int32_t* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_INT32) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag, const float* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_FLOAT) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag, const int64_t* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_INT64) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag, const double* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_DOUBLE) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag,
                                const camera_metadata_rational_t* data,
                                uint32_t data_count) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_RATIONAL) == false) {
    return INVALID_OPERATION;
  }
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(data), data_count);
}

status_t HalCameraMetadata::Set(uint32_t tag, const std::string& string) {
  std::unique_lock<std::mutex> lock(metadata_lock_);

  if (metadata_ == nullptr) {
    ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
    return INVALID_OPERATION;
  }
  if (IsTypeValid(tag, TYPE_BYTE) == false) {
    return INVALID_OPERATION;
  }
  // string length +1 for null-terminated character.
  return SetMetadataRaw(tag, reinterpret_cast<const void*>(string.c_str()),
                        string.length() + 1);
}

status_t HalCameraMetadata::Set(const camera_metadata_ro_entry& entry) {
  status_t res;

  int32_t type = get_camera_metadata_tag_type(entry.tag);

  switch (type) {
    case TYPE_BYTE:
      res = Set(entry.tag, entry.data.u8, entry.count);
      break;
    case TYPE_INT32:
      res = Set(entry.tag, entry.data.i32, entry.count);
      break;
    case TYPE_FLOAT:
      res = Set(entry.tag, entry.data.f, entry.count);
      break;
    case TYPE_INT64:
      res = Set(entry.tag, entry.data.i64, entry.count);
      break;
    case TYPE_DOUBLE:
      res = Set(entry.tag, entry.data.d, entry.count);
      break;
    case TYPE_RATIONAL:
      res = Set(entry.tag, entry.data.r, entry.count);
      break;
    default:
      ALOGE("%s: Unknown type in tag 0x%x.", __FUNCTION__, entry.tag);
      res = BAD_TYPE;
      break;
  }

  return res;
}

status_t HalCameraMetadata::Get(uint32_t tag,
                                camera_metadata_ro_entry* entry) const {
  if (entry == nullptr) {
    ALOGE("%s: entry is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(metadata_lock_);
  return find_camera_metadata_ro_entry(metadata_, tag, entry);
}

status_t HalCameraMetadata::GetByIndex(camera_metadata_ro_entry* entry,
                                       size_t entry_index) const {
  if (entry == nullptr) {
    ALOGE("%s: entry is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::unique_lock<std::mutex> lock(metadata_lock_);
  size_t entry_count = get_camera_metadata_entry_count(metadata_);
  if (entry_index >= entry_count) {
    ALOGE("%s: entry_index (%zu) >= entry_count(%zu)", __FUNCTION__,
          entry_index, entry_count);
    return BAD_VALUE;
  }

  return get_camera_metadata_ro_entry(metadata_, entry_index, entry);
}

status_t HalCameraMetadata::Erase(const std::unordered_set<uint32_t>& tags) {
  std::unique_lock<std::mutex> lock(metadata_lock_);
  camera_metadata_ro_entry_t entry;
  status_t res;

  // Metadata entries to copy over; entries whose tag IDs aren't in 'tags'
  std::vector<size_t> entry_indices;
  size_t data_count = get_camera_metadata_data_count(metadata_);
  size_t entry_count = get_camera_metadata_entry_count(metadata_);
  size_t data_count_removed = 0;
  for (size_t entry_index = 0; entry_index < entry_count; entry_index++) {
    res = get_camera_metadata_ro_entry(metadata_, entry_index, &entry);
    if (res != OK) {
      ALOGE("%s: Error getting entry at index %zu: %s %d", __FUNCTION__,
            entry_index, strerror(-res), res);
      continue;
    }

    // See if the entry at the current index has a tag ID in the list of IDs
    // that we'd like to remove
    if (tags.find(entry.tag) != tags.end()) {
      data_count_removed +=
          calculate_camera_metadata_entry_data_size(entry.type, entry.count);
    } else {
      entry_indices.push_back(entry_index);
    }
  }

  if (data_count_removed > data_count) {
    // Sanity; this should never happen unless there is an implementation error
    ALOGE("%s: Error! Cannot remove %zu bytes of data when there is only %zu",
          __FUNCTION__, data_count_removed, data_count);
    return UNKNOWN_ERROR;
  } else if (data_count_removed == 0) {
    // Nothing to remove
    return OK;
  }

  size_t new_data_count = (data_count - data_count_removed);
  size_t new_entry_count = entry_indices.size();
  // Calculate the new capacity; multiply by 2 to allow room for metadata growth
  size_t entry_capacity = (2 * new_entry_count);
  size_t data_capacity = (2 * new_data_count);

  // Allocate a new buffer with the smaller size
  camera_metadata_t* orig_metadata = metadata_;
  metadata_ = allocate_camera_metadata(entry_capacity, data_capacity);
  if (metadata_ == nullptr) {
    ALOGE("%s: Can't allocate new metadata buffer", __FUNCTION__);
    metadata_ = orig_metadata;
    return NO_MEMORY;
  }

  IF_ALOGV() {
    ALOGV(
        "%s: data capacity [%zu --> %zu], data count [%zu --> %zu], entry "
        "capacity: [%zu --> %zu] entry count: [%zu --> %zu]",
        __FUNCTION__, get_camera_metadata_data_capacity(orig_metadata),
        data_capacity, data_count, new_data_count,
        get_camera_metadata_entry_capacity(orig_metadata), entry_capacity,
        entry_count, new_entry_count);
  }

  // Loop through the original metadata buffer and add all to the new buffer,
  // except for those indices which were removed
  for (size_t entry_index : entry_indices) {
    res = CopyEntry(orig_metadata, metadata_, entry_index);
    if (res != OK) {
      ALOGE("%s: Error adding entry at index %zu failed: %s %d", __FUNCTION__,
            entry_index, strerror(-res), res);
      free_camera_metadata(metadata_);
      metadata_ = orig_metadata;
      return res;
    }
  }

  free_camera_metadata(orig_metadata);
  return OK;
}

status_t HalCameraMetadata::Erase(uint32_t tag) {
  std::unique_lock<std::mutex> lock(metadata_lock_);
  camera_metadata_entry_t entry;
  status_t res = find_camera_metadata_entry(metadata_, tag, &entry);
  if (res == NAME_NOT_FOUND) {
    return OK;
  } else if (res != OK) {
    ALOGE("%s: Error finding entry (0x%x): %s %d", __FUNCTION__, tag,
          strerror(-res), res);
    return res;
  }

  res = delete_camera_metadata_entry(metadata_, entry.index);
  if (res != OK) {
    ALOGE("%s: Error deleting entry (0x%x): %s %d", __FUNCTION__, tag,
          strerror(-res), res);
  }
  return res;
}

void HalCameraMetadata::PrintData(const uint8_t* data, int32_t type,
                                  int32_t count, uint32_t indentation) {
  const uint32_t kDumpSizePerLine = 100;
  static int32_t values_per_line[NUM_TYPES] = {
      [TYPE_BYTE] = 16, [TYPE_INT32] = 4,  [TYPE_FLOAT] = 8,
      [TYPE_INT64] = 2, [TYPE_DOUBLE] = 4, [TYPE_RATIONAL] = 2,
  };
  int32_t max_type_count = sizeof(values_per_line) / sizeof(int32_t);
  if (type >= max_type_count) {
    ALOGE("%s: unsupported type: %d", __FUNCTION__, type);
    return;
  }
  size_t type_size = camera_metadata_type_size[type];
  char string[kDumpSizePerLine];
  uint32_t string_offset;
  int32_t lines = count / values_per_line[type];
  if (count % values_per_line[type] != 0) {
    lines++;
  }

  int32_t index = 0;
  int32_t j = 0, k = 0;
  for (j = 0; j < lines; j++) {
    string_offset = 0;
    string_offset +=
        snprintf(&string[string_offset], (sizeof(string) - string_offset),
                 "%*s[ ", indentation + 4, "");
    for (k = 0; k < values_per_line[type] && count > 0;
         k++, count--, index += type_size) {
      switch (type) {
        case TYPE_BYTE: {
          string_offset +=
              snprintf(&string[string_offset], (sizeof(string) - string_offset),
                       "%hhu ", *(data + index));
          break;
        }
        case TYPE_INT32: {
          string_offset +=
              snprintf(&string[string_offset], (sizeof(string) - string_offset),
                       "%d ", *reinterpret_cast<const int32_t*>(data + index));
          break;
        }
        case TYPE_FLOAT: {
          string_offset +=
              snprintf(&string[string_offset], (sizeof(string) - string_offset),
                       "%0.8f ", *reinterpret_cast<const float*>(data + index));
          break;
        }
        case TYPE_INT64: {
          string_offset += snprintf(
              &string[string_offset], (sizeof(string) - string_offset),
              "%" PRId64 " ", *reinterpret_cast<const int64_t*>(data + index));
          break;
        }
        case TYPE_DOUBLE: {
          string_offset += snprintf(
              &string[string_offset], (sizeof(string) - string_offset),
              "%0.8f ", *reinterpret_cast<const double*>(data + index));
          break;
        }
        case TYPE_RATIONAL: {
          int32_t numerator = *(int32_t*)(data + index);
          int32_t denominator = *(int32_t*)(data + index + 4);
          string_offset +=
              snprintf(&string[string_offset], (sizeof(string) - string_offset),
                       "(%d / %d) ", numerator, denominator);
          break;
        }
        default: { break; }
      }
    }
    string_offset +=
        snprintf(&string[string_offset], (sizeof(string) - string_offset), "]");
    ALOGI("%s:%s", __FUNCTION__, string);
  }
}

void HalCameraMetadata::Dump(int32_t fd, MetadataDumpVerbosity verbosity,
                             uint32_t indentation) const {
  std::unique_lock<std::mutex> lock(metadata_lock_);
  if (fd >= 0) {
    dump_indented_camera_metadata(metadata_, fd, static_cast<int>(verbosity),
                                  indentation);
  } else {
    if (metadata_ == nullptr) {
      ALOGE("%s: metadata_ is nullptr", __FUNCTION__);
      return;
    }
    size_t entry_count = get_camera_metadata_entry_count(metadata_);
    for (uint32_t i = 0; i < entry_count; i++) {
      camera_metadata_ro_entry_t entry;
      status_t res = get_camera_metadata_ro_entry(metadata_, i, &entry);
      if (res != OK) {
        ALOGE("%s: get_camera_metadata_ro_entry (%d) fail", __FUNCTION__, i);
        continue;
      }
      const char *tag_name, *tag_section;
      tag_section = get_local_camera_metadata_section_name(entry.tag, metadata_);
      if (tag_section == nullptr) {
        tag_section = "unknownSection";
      }
      tag_name = get_local_camera_metadata_tag_name(entry.tag, metadata_);
      if (tag_name == nullptr) {
        tag_name = "unknownTag";
      }
      const char* type_name;
      if (entry.type >= NUM_TYPES) {
        type_name = "unknown";
      } else {
        type_name = camera_metadata_type_names[entry.type];
      }
      ALOGI("%s: (%d) %s.%s (%05x): %s[%zu] ", __FUNCTION__, i, tag_section,
            tag_name, entry.tag, type_name, entry.count);

      if (verbosity < MetadataDumpVerbosity::kTagEntryWith16Data) {
        continue;
      }

      if (entry.type >= NUM_TYPES) {
        continue;
      }
      const uint8_t* data = entry.data.u8;

      int32_t count = entry.count;
      if (verbosity < MetadataDumpVerbosity::kAllInformation && count > 16) {
        count = 16;
      }
      PrintData(data, entry.type, count, indentation);
    }
  }
}

status_t HalCameraMetadata::Append(
    std::unique_ptr<HalCameraMetadata> hal_metadata) {
  if (hal_metadata == nullptr) {
    ALOGE("%s: hal_metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  return Append(hal_metadata->ReleaseCameraMetadata());
}

status_t HalCameraMetadata::Append(camera_metadata_t* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }
  std::unique_lock<std::mutex> lock(metadata_lock_);
  size_t extra_entries = get_camera_metadata_entry_count(metadata);
  size_t extra_data = get_camera_metadata_data_count(metadata);
  status_t res = ResizeIfNeeded(extra_entries, extra_data);
  if (res != OK) {
    ALOGE("%s: Resize fail", __FUNCTION__);
    return res;
  }

  return append_camera_metadata(metadata_, metadata);
}

size_t HalCameraMetadata::GetEntryCount() const {
  std::unique_lock<std::mutex> lock(metadata_lock_);
  return (metadata_ == nullptr) ? 0 : get_camera_metadata_entry_count(metadata_);
}

status_t HalCameraMetadata::CopyEntry(const camera_metadata_t* src,
                                      camera_metadata_t* dest,
                                      size_t entry_index) const {
  if (src == nullptr || dest == nullptr) {
    ALOGE("%s: src (%p) or dest(%p) is nullptr", __FUNCTION__, src, dest);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res = get_camera_metadata_ro_entry(src, entry_index, &entry);
  if (res != OK) {
    ALOGE("%s: failed to get entry index %zu", __FUNCTION__, entry_index);
    return res;
  }

  res = add_camera_metadata_entry(dest, entry.tag, entry.data.u8, entry.count);
  if (res != OK) {
    ALOGE("%s: failed to add entry index %zu", __FUNCTION__, entry_index);
    return res;
  }

  return OK;
}
}  // namespace google_camera_hal
}  // namespace android