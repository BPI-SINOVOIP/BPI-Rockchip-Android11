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

#define LOG_TAG "HalCameraMetadataTests"
#include <log/log.h>

#include <gtest/gtest.h>
#include <hal_camera_metadata.h>
#include <system/camera_metadata.h>

namespace android {
namespace google_camera_hal {

static constexpr uint32_t kDataBytes = 256;
static constexpr uint32_t kNumEntries = 10;
static constexpr uint32_t kDefaultDataBytes = 1;
static constexpr uint32_t kDefaultNumEntries = 1;
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

// Test creating HalCameraMetadata with sizes.
TEST(HalCameraMetadataTests, CreateWithSizes) {
  // Create HalCameraMetadata with sizes.
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";
}

// Test creating HalCameraMetadata with a camera_metadata.
TEST(HalCameraMetadataTests, CreateToOwn) {
  camera_metadata_t* metadata =
      allocate_camera_metadata(kNumEntries, kDataBytes);
  ASSERT_NE(metadata, nullptr) << "Creating metadata failed.";

  auto hal_metadata = HalCameraMetadata::Create(metadata);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";
}

TEST(HalCameraMetadataTests, Clone) {
  camera_metadata_t* m = allocate_camera_metadata(kNumEntries, kDataBytes);
  auto metadata = HalCameraMetadata::Clone(m);
  ASSERT_NE(metadata, nullptr) << "Cloning metadata failed.";

  free_camera_metadata(m);
}

TEST(HalCameraMetadataTests, GetCameraMetadataSize) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  // Verify the metadata size is >= what we asked for.
  ASSERT_GE(hal_metadata->GetCameraMetadataSize(), kDataBytes);
}

TEST(HalCameraMetadataTests, ReleaseCameraMetadata) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  camera_metadata_t* metadata = hal_metadata->ReleaseCameraMetadata();
  ASSERT_NE(metadata, nullptr) << "Releasing hal_metadata failed.";

  free_camera_metadata(metadata);
  metadata = nullptr;

  // Verify hal_metadata doesn't hold any metadata anymore.
  ASSERT_EQ(hal_metadata->ReleaseCameraMetadata(), nullptr);
  ASSERT_EQ(hal_metadata->GetCameraMetadataSize(), (uint32_t)0);
}

TEST(HalCameraMetadataTests, GetRawCameraMetadata) {
  camera_metadata_t* raw_metadata =
      allocate_camera_metadata(kNumEntries, kDataBytes);
  auto hal_metadata = HalCameraMetadata::Create(raw_metadata);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  const camera_metadata_t* metadata = hal_metadata->GetRawCameraMetadata();
  // GetRawCameraMetadata() must return the same pointer we used for Create()
  ASSERT_EQ(metadata, raw_metadata) << "Getting hal_metadata failed.";
  // Verify hal_metadata hasn't released the metadata
  ASSERT_GE(hal_metadata->GetCameraMetadataSize(), (uint32_t)kDataBytes);
}

void SetGetMetadata(std::unique_ptr<HalCameraMetadata> hal_metadata, bool dump) {
  // int64 case
  int64_t exposure_time_ns = 1000000000;
  status_t res =
      hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_EQ(res, OK) << "Set int64 failed";

  camera_metadata_ro_entry entry;
  res = hal_metadata->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_EXPOSURE_TIME failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get int64_t count failed.";
  ASSERT_NE(entry.data.i64, nullptr) << "Get int64_t data nullptr.";
  ASSERT_EQ(exposure_time_ns, *entry.data.i64) << "Get int64_t data failed.";

  // int32 case
  int32_t sensitivity = 200;
  res = hal_metadata->Set(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);
  ASSERT_EQ(res, OK) << "Set int32 failed";

  res = hal_metadata->Get(ANDROID_SENSOR_SENSITIVITY, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_SENSITIVITY failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get int32_t count failed.";
  ASSERT_NE(entry.data.i32, nullptr) << "Get int32_t data nullptr.";
  ASSERT_EQ(sensitivity, *entry.data.i32) << "Get int32_t data failed.";

  // float case
  float focus_distance = 0.5f;
  res = hal_metadata->Set(ANDROID_LENS_FOCUS_DISTANCE, &focus_distance, 1);
  ASSERT_EQ(res, OK) << "Set float failed";

  res = hal_metadata->Get(ANDROID_LENS_FOCUS_DISTANCE, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_LENS_FOCUS_DISTANCE failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get float count failed.";
  ASSERT_NE(entry.data.f, nullptr) << "Get float data nullptr.";
  ASSERT_EQ(focus_distance, *entry.data.f) << "Get float data failed.";

  // rational case
  camera_metadata_rational_t ae_compensation_step[] = {{0, 1}};
  res =
      hal_metadata->Set(ANDROID_CONTROL_AE_COMPENSATION_STEP,
                        ae_compensation_step, ARRAY_SIZE(ae_compensation_step));
  ASSERT_EQ(res, OK) << "Set rational failed";

  res = hal_metadata->Get(ANDROID_CONTROL_AE_COMPENSATION_STEP, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_CONTROL_AE_COMPENSATION_STEP failed";
  ASSERT_EQ(entry.count, ARRAY_SIZE(ae_compensation_step))
      << "Get rational count failed.";
  for (uint32_t i = 0; i < entry.count; i++) {
    ASSERT_EQ(ae_compensation_step[i].numerator, entry.data.r[i].numerator)
        << "Get rational numerator failed at" << i;
    ASSERT_EQ(ae_compensation_step[i].denominator, entry.data.r[i].denominator)
        << "Get rational denominator failed at" << i;
  }

  // uint8 case
  uint8_t mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  res = hal_metadata->Set(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &mode, 1);
  ASSERT_EQ(res, OK) << "Set uint8 failed";

  res = hal_metadata->Get(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_CONTROL_AVAILABLE_SCENE_MODES failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get uint8 count failed.";
  ASSERT_NE(entry.data.u8, nullptr) << "Get uint8 data nullptr.";
  ASSERT_EQ(mode, *entry.data.u8) << "Get uint8 data failed.";

  // double case
  double noise[] = {1.234, 2.345};
  res =
      hal_metadata->Set(ANDROID_SENSOR_NOISE_PROFILE, noise, ARRAY_SIZE(noise));
  ASSERT_EQ(res, OK) << "Set double failed";

  res = hal_metadata->Get(ANDROID_SENSOR_NOISE_PROFILE, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_NOISE_PROFILE failed";
  ASSERT_EQ(entry.count, ARRAY_SIZE(noise)) << "Get double count failed.";
  for (uint32_t i = 0; i < entry.count; i++) {
    ASSERT_EQ(noise[i], entry.data.d[i])
        << "Get double numerator failed at" << i;
  }

  // string case
  std::string string("1234");
  res = hal_metadata->Set(ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS, string);
  ASSERT_EQ(res, OK) << "Set string failed";

  res = hal_metadata->Get(ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS failed";
  ASSERT_EQ(entry.count, string.length() + 1) << "Get string count failed.";
  ASSERT_NE(entry.data.u8, nullptr) << "Get string data nullptr.";
  ASSERT_STREQ(string.c_str(), reinterpret_cast<const char*>(entry.data.u8))
      << "Get string data failed.";

  // Check the dump metadata to file
  if (dump) {
    std::FILE* f = std::tmpfile();
    ASSERT_NE(f, nullptr);

    std::string test_string_key = "sensitivity";
    std::string test_string_value = std::to_string(sensitivity);
    hal_metadata->Dump(fileno(f), MetadataDumpVerbosity::kAllInformation,
                       /*indentation*/ 0);

    char line[512];
    bool found_test_string = false;

    // find key
    std::rewind(f);
    while (fgets(line, sizeof(line), f) != nullptr) {
      if (std::string(line).find(test_string_key) != std::string::npos) {
        found_test_string = true;
      }
    }
    EXPECT_EQ(found_test_string, true) << "find sensitivity key failed";

    // find value
    found_test_string = false;
    std::rewind(f);
    while (fgets(line, sizeof(line), f) != nullptr) {
      if (std::string(line).find(test_string_value) != std::string::npos) {
        found_test_string = true;
      }
    }
    fclose(f);
    EXPECT_EQ(found_test_string, true) << "find sensitivity value failed";

    // Dump file to logcat when fd < 0
    hal_metadata->Dump(/*fd*/ -1, MetadataDumpVerbosity::kAllInformation,
                       /*indentation*/ 0);
  }
}

TEST(HalCameraMetadataTests, SetGetMetadataWithoutResize) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  SetGetMetadata(std::move(hal_metadata), false);
}

TEST(HalCameraMetadataTests, SetGetMetadataWithResize) {
  // only create 1 entry and 1 data byte metadata.
  // It needs resize when set more metadata.
  auto hal_metadata =
      HalCameraMetadata::Create(kDefaultNumEntries, kDefaultDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  SetGetMetadata(std::move(hal_metadata), false);
}

TEST(HalCameraMetadataTests, MetadataWithInvalidType) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  // ANDROID_SENSOR_EXPOSURE_TIME is int64_t type
  // And will set metadata fail if type is int32_t
  int32_t exposure_time_ns = 100;
  status_t res =
      hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_NE(res, OK) << "Set invalid type failed";
}

TEST(HalCameraMetadataTests, EraseMetadata) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  // int64 case
  int64_t exposure_time_ns = 1000000000;
  status_t res =
      hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_EQ(res, OK) << "Set int64 failed";

  camera_metadata_ro_entry entry;
  res = hal_metadata->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_EXPOSURE_TIME failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get int64_t count failed.";
  ASSERT_NE(entry.data.i64, nullptr) << "Get int64_t data nullptr.";
  ASSERT_EQ(exposure_time_ns, *entry.data.i64) << "Get int64_t data failed.";

  res = hal_metadata->Erase(ANDROID_SENSOR_EXPOSURE_TIME);
  ASSERT_EQ(res, OK) << "Erase failed";

  res = hal_metadata->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  ASSERT_EQ(res, NAME_NOT_FOUND) << "Erase and check tag failed";

  res = hal_metadata->Get(ANDROID_SENSOR_EXPOSURE_TIME, nullptr);
  ASSERT_EQ(res, BAD_VALUE) << "Get with nullptr did not return BAD_VALUE";
}

TEST(HalCameraMetadataTests, Dump) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  SetGetMetadata(std::move(hal_metadata), true);
}

TEST(HalCameraMetadataTests, AppendMetadata) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  int64_t exposure_time_ns = 1000000000;
  status_t res =
      hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_EQ(res, OK) << "Set int64 failed";

  camera_metadata_ro_entry entry;
  res = hal_metadata->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_EXPOSURE_TIME failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get int64_t count failed.";
  ASSERT_NE(entry.data.i64, nullptr) << "Get int64_t data nullptr.";
  ASSERT_EQ(exposure_time_ns, *entry.data.i64) << "Get int64_t data failed.";

  auto hal_metadata_dst =
      HalCameraMetadata::Create(kDefaultNumEntries, kDefaultDataBytes);
  ASSERT_NE(hal_metadata_dst, nullptr) << "Creating hal_metadata_dst failed.";

  res = hal_metadata_dst->Append(nullptr);
  ASSERT_EQ(res, BAD_VALUE) << "Append nullptr failed";

  res = hal_metadata_dst->Append(std::move(hal_metadata));
  ASSERT_EQ(res, OK) << "Append failed";

  res = hal_metadata_dst->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_EXPOSURE_TIME failed after append";
}

TEST(HalCameraMetadataTests, GetEntryCount) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  auto size = hal_metadata->GetEntryCount();
  ASSERT_EQ(size, (uint32_t)0) << "Get empty entry count failed.";

  int64_t exposure_time_ns = 1000000000;
  status_t res =
      hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_EQ(res, OK) << "Set int64 failed";

  size = hal_metadata->GetEntryCount();
  ASSERT_EQ(size, (uint32_t)1) << "Get entry count failed.";
}

TEST(HalCameraMetadataTests, GetByEntryIndex) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  camera_metadata_ro_entry entry;
  status_t res = hal_metadata->GetByIndex(&entry, /*entry_index*/ 0);
  ASSERT_NE(res, OK) << "Get invalid index 0 failed";

  int64_t exposure_time_ns = 1000000000;
  res = hal_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time_ns, 1);
  ASSERT_EQ(res, OK) << "Set int64 failed";

  auto size = hal_metadata->GetEntryCount();
  ASSERT_EQ(size, (uint32_t)1) << "Get entry count failed.";

  res = hal_metadata->GetByIndex(&entry, /*entry_index*/ 0);
  ASSERT_EQ(res, OK) << "Get ANDROID_SENSOR_EXPOSURE_TIME failed";
  ASSERT_EQ(entry.count, (uint32_t)1) << "Get int64_t count failed.";
  ASSERT_NE(entry.data.i64, nullptr) << "Get int64_t data nullptr.";
  ASSERT_EQ(exposure_time_ns, *entry.data.i64) << "Get int64_t data failed.";

  res = hal_metadata->GetByIndex(&entry, /*entry_index*/ 1);
  ASSERT_NE(res, OK) << "Get invalid index 1 failed";
}

}  // namespace google_camera_hal
}  // namespace android
