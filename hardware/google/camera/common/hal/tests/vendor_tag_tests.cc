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

#include <gtest/gtest.h>
#define LOG_TAG "CameraVendorTagTests"
#include <log/log.h>

#include <vector>

#include "system/camera_metadata.h"
#include "vendor_tag_defs.h"
#include "vendor_tag_utils.h"
#include "vendor_tags.h"

namespace android {
namespace google_camera_hal {

static constexpr uint32_t kDataBytes = 256;
static constexpr uint32_t kNumEntries = 10;

TEST(CameraVendorTagTest, TestCharacteristics) {
  auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

  std::vector<uint32_t> dummy_keys = {
      VendorTagIds::kLogicalCamDefaultPhysicalId};
  status_t res = hal_metadata->Set(
      ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
      reinterpret_cast<int32_t*>(dummy_keys.data()), dummy_keys.size());
  ASSERT_EQ(res, OK);

  res = hal_metadata->Set(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                          reinterpret_cast<int32_t*>(dummy_keys.data()),
                          dummy_keys.size());
  ASSERT_EQ(res, OK);

  res = hal_metadata->Set(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS,
                          reinterpret_cast<int32_t*>(dummy_keys.data()),
                          dummy_keys.size());
  ASSERT_EQ(res, OK);

  res = hal_metadata->Set(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                          reinterpret_cast<int32_t*>(dummy_keys.data()),
                          dummy_keys.size());
  ASSERT_EQ(res, OK);

  res = hal_vendor_tag_utils::ModifyCharacteristicsKeys(hal_metadata.get());
  EXPECT_EQ(res, OK);

  res = hal_vendor_tag_utils::ModifyCharacteristicsKeys(nullptr);
  EXPECT_NE(res, OK) << "ModifyCameraCharacteristics() should have failed with "
                        "a nullptr metadata";
}

TEST(CameraVendorTagTest, TestDefaultRequest) {
  std::vector<RequestTemplate> request_templates = {
      RequestTemplate::kPreview, RequestTemplate::kStillCapture,
      RequestTemplate::kVideoRecord, RequestTemplate::kVideoSnapshot,
      RequestTemplate::kZeroShutterLag};

  status_t res;
  for (auto& request_template : request_templates) {
    auto hal_metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
    ASSERT_NE(hal_metadata, nullptr) << "Creating hal_metadata failed.";

    res = hal_vendor_tag_utils::ModifyDefaultRequestSettings(
        request_template, hal_metadata.get());
    EXPECT_EQ(res, OK) << "ModifyDefaultRequestSettings() failed for request "
                          "template (%u)"
                       << static_cast<uint32_t>(request_template);
  }
}

TEST(CameraVendorTagTest, TestValidVendorTags) {
  uint32_t hwl_tag_id = VENDOR_SECTION_START;
  uint32_t hal_tag_id = kHalVendorTagSectionStart;

  ASSERT_LT(hwl_tag_id, hal_tag_id) << "Error! HAL vendor tag section start "
                                    << "must be greater than "
                                    << "VENDOR_SECTION_START";

  std::vector<VendorTagSection> hwl_sections = {
      {.section_name = "com.google.hwl.internal",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "magic",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "wand",
                 .tag_type = CameraMetadataType::kFloat}}},
      {.section_name = "com.google.3a",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "aec",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "awb",
                 .tag_type = CameraMetadataType::kInt32}}}};

  std::vector<VendorTagSection> hal_sections = {
      {.section_name = "com.pixel.experimental",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "hybrid_ae.enabled",
                 .tag_type = CameraMetadataType::kByte}}},
      /* Overlaps with HWL vendor section above; this is allowed */
      {.section_name = "com.google.3a",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "af",
                 .tag_type = CameraMetadataType::kFloat}}}};

  std::vector<VendorTagSection> combined_sections;
  status_t ret = vendor_tag_utils::CombineVendorTags(hwl_sections, hal_sections,
                                                     &combined_sections);

  EXPECT_EQ(ret, OK) << "CombineVendorTags() failed for valid tags!";

  ret = vendor_tag_utils::CombineVendorTags(hwl_sections, hal_sections,
                                            /*destination=*/nullptr);
  EXPECT_NE(ret, OK) << "CombineVendorTags() should have failed with nullptr";

  // Test metadata operations on above tags. They should fail since the camera
  // framework hasn't been initialized via VendorTagManager utility class
  auto metadata = HalCameraMetadata::Create(kNumEntries, kDataBytes);
  ASSERT_NE(metadata, nullptr) << "Creating metadata failed.";
  // Hard coded vendor tag ID for 'com.google.hwl.internal.magic' from above
  uint32_t magic_tag_id = VENDOR_SECTION_START;
  float good_magic = 42.1337f;
  // Try to set a random value to above vendor tag
  ret = metadata->Set(magic_tag_id, &good_magic, /*count=*/1);
  EXPECT_NE(ret, OK) << "Error! Setting metadata should have failed before "
                     << "initializing VendorTagManager";

  // Framework camera_metadata operations relying on 'vendor_tag_ops_t' should
  // fail before VendorTagManager is initialized with the vendor tags
  int count = VendorTagManager::GetInstance().GetCount();
  EXPECT_EQ(0, count) << "Error! VendorTagManager should return a count of 0"
                      << " before being initialized";

  // Now initialize VendorTagManager, and expect metadata operations same
  // as previously done above to succeed
  ret = VendorTagManager::GetInstance().AddTags(combined_sections);
  ASSERT_EQ(ret, OK);
  count = VendorTagManager::GetInstance().GetCount();
  int expected_count = 0;
  for (auto& section : hal_sections) {
    expected_count += section.tags.size();
  }
  for (auto& section : hwl_sections) {
    expected_count += section.tags.size();
  }
  EXPECT_EQ(count, expected_count);

  ret = metadata->Set(magic_tag_id, &good_magic, /*count=*/1);
  EXPECT_EQ(ret, OK) << "Error! Setting metadata should have succeeded after "
                     << "initializing VendorTagManager";

  // Try setting metadata with an invalid type (expected is float)
  int32_t dark_magic = 13;
  ret = metadata->Set(magic_tag_id, &dark_magic, /*count=*/1);
  EXPECT_NE(ret, OK) << "Error! Setting metadata with an incorrect payload "
                     << "type should have failed";

  // For debugging fun - print the combined list of vendor tags
  ALOGI("Vendor tag list START");
  ALOGI("---------------------");
  if (count > 0) {
    uint32_t* tag_id_list = new uint32_t[count];
    VendorTagManager::GetInstance().GetAllTags(tag_id_list);
    for (int i = 0; i < count; i++) {
      uint32_t tag_id = tag_id_list[i];
      const char* section_name =
          VendorTagManager::GetInstance().GetSectionName(tag_id);
      const char* tag_name = VendorTagManager::GetInstance().GetTagName(tag_id);
      int tag_type = VendorTagManager::GetInstance().GetTagType(tag_id);
      ALOGI("ID: 0x%x (%u)\tType: %d\t%s.%s", tag_id, tag_id, tag_type,
            section_name, tag_name);
    }

    delete[] tag_id_list;
    ALOGI("Vendor tag list END");
    ALOGI("-------------------");
  }

  ret = VendorTagManager::GetInstance().AddTags(combined_sections);
  EXPECT_NE(ret, OK) << "Calling AddTags with the same tags should fail.";

  std::vector<VendorTagSection> extra_sections = {
      {.section_name = "extra_section",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "extra_tag",
                 .tag_type = CameraMetadataType::kByte}}}};

  ret = VendorTagManager::GetInstance().AddTags(extra_sections);
  EXPECT_EQ(ret, OK) << "Adding extra tag sections should succeed";
  expected_count += 1;
  count = VendorTagManager::GetInstance().GetCount();
  EXPECT_EQ(count, expected_count);

  VendorTagManager::GetInstance().Reset();
  count = VendorTagManager::GetInstance().GetCount();
  EXPECT_EQ(0, count) << "Error! VendorTagManager should return a count of 0"
                      << " after being reset";
}

TEST(CameraVendorTagTest, TestVendorTagsOverlappingIds) {
  // Make the HAL and HWL tag IDs overlap
  uint32_t hwl_tag_id = kHalVendorTagSectionStart;
  uint32_t hal_tag_id = kHalVendorTagSectionStart;

  std::vector<VendorTagSection> hwl_sections = {
      {.section_name = "com.google.hwl.internal",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "magic",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "wand",
                 .tag_type = CameraMetadataType::kFloat}}},
      {.section_name = "com.google.hwl.3a",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "aec",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "awb",
                 .tag_type = CameraMetadataType::kInt32}}}};

  std::vector<VendorTagSection> hal_sections = {
      {.section_name = "com.pixel.experimental",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "hybrid_ae.enabled",
                 .tag_type = CameraMetadataType::kByte}}},
      {.section_name = "com.google.hwl.3a",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "af",
                 .tag_type = CameraMetadataType::kFloat}}}};

  std::vector<VendorTagSection> combined_sections;
  status_t ret = vendor_tag_utils::CombineVendorTags(hwl_sections, hal_sections,
                                                     &combined_sections);

  EXPECT_NE(ret, OK) << "CombineVendorTags() succeeded for invalid tags!";
}

TEST(CameraVendorTagTest, TestVendorTagsOverlappingNames) {
  uint32_t hwl_tag_id = VENDOR_SECTION_START;
  uint32_t hal_tag_id = kHalVendorTagSectionStart;

  // Define duplicate tag name in both sections: com.google.hwl.3a.aec
  std::vector<VendorTagSection> hwl_sections = {
      {.section_name = "com.google.hwl.internal",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "magic",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "wand",
                 .tag_type = CameraMetadataType::kFloat}}},
      {.section_name = "com.google.hwl.3a",
       .tags = {{.tag_id = hwl_tag_id++,
                 .tag_name = "aec",
                 .tag_type = CameraMetadataType::kFloat},
                {.tag_id = hwl_tag_id++,
                 .tag_name = "awb",
                 .tag_type = CameraMetadataType::kInt32}}}};

  std::vector<VendorTagSection> hal_sections = {
      {.section_name = "com.pixel.experimental",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "hybrid_ae.enabled",
                 .tag_type = CameraMetadataType::kByte}}},
      {.section_name = "com.google.hwl.3a",
       .tags = {{.tag_id = hal_tag_id++,
                 .tag_name = "aec",
                 .tag_type = CameraMetadataType::kFloat}}}};

  std::vector<VendorTagSection> combined_sections;
  status_t ret = vendor_tag_utils::CombineVendorTags(hwl_sections, hal_sections,
                                                     &combined_sections);

  EXPECT_NE(ret, OK) << "CombineVendorTags() succeeded for invalid tags";
}
}  // namespace google_camera_hal
}  // namespace android
