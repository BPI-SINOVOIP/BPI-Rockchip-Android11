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

#define LOG_TAG "CameraProviderTests"
#include <log/log.h>

#include <camera_provider.h>
#include <gtest/gtest.h>

#include "mock_provider_hwl.h"

namespace android {
namespace google_camera_hal {

class CameraProviderTest : public ::testing::Test {
 protected:
  void CreateCameraProviderAndCheck(
      std::unique_ptr<MockProviderHwl> mock_provider_hwl = nullptr) {
    if (mock_provider_hwl == nullptr) {
      mock_provider_hwl = MockProviderHwl::Create();
    }

    DestroyCameraProvider();
    provider_ = CameraProvider::Create(std::move(mock_provider_hwl));
    ASSERT_NE(provider_, nullptr) << "Creating a CameraProvider failed.";
  }

  void DestroyCameraProvider() {
    provider_ = nullptr;
  }

  std::unique_ptr<CameraProvider> provider_;
};

TEST_F(CameraProviderTest, Create) {
  CreateCameraProviderAndCheck();
}

TEST_F(CameraProviderTest, SetCallback) {
  CameraDeviceStatus device_status = CameraDeviceStatus::kNotPresent;
  TorchModeStatus torch_status = TorchModeStatus::kAvailableOff;
  CameraProviderCallback callback = {
      .camera_device_status_change =
          [&device_status](std::string /*camera_id*/,
                           CameraDeviceStatus new_status) {
            device_status = new_status;
          },
      .torch_mode_status_change =
          [&torch_status](std::string /*camera_id*/, TorchModeStatus new_status) {
            torch_status = new_status;
          },
  };

  static const std::vector<CameraDeviceStatus> kMockCameraDeviceStatus = {
      CameraDeviceStatus::kNotPresent, CameraDeviceStatus::kPresent,
      CameraDeviceStatus::kEnumerating};

  // Verify the mock device status is same as set from callback.
  for (auto mock_device_status : kMockCameraDeviceStatus) {
    auto mock_provider_hwl = MockProviderHwl::Create();
    mock_provider_hwl->camera_device_status_ = mock_device_status;

    CreateCameraProviderAndCheck(std::move(mock_provider_hwl));

    status_t res = provider_->SetCallback(&callback);
    ASSERT_EQ(res, OK) << "Setting callback failed: " << strerror(res);

    EXPECT_EQ(device_status, mock_device_status);
  }

  static const std::vector<TorchModeStatus> kMockTorchModeStatus = {
      TorchModeStatus::kNotAvailable, TorchModeStatus::kAvailableOff,
      TorchModeStatus::kAvailableOn};

  // Verify the mock torch mode status is same as set from callback.
  for (auto mock_torch_status : kMockTorchModeStatus) {
    auto mock_provider_hwl = MockProviderHwl::Create();
    mock_provider_hwl->torch_status_ = mock_torch_status;

    CreateCameraProviderAndCheck(std::move(mock_provider_hwl));

    status_t res = provider_->SetCallback(&callback);
    ASSERT_EQ(res, OK) << "Setting callback failed: " << strerror(res);

    EXPECT_EQ(torch_status, mock_torch_status);
  }
}

// Test if camera provider passes all vendor tags specified in HWL.
TEST_F(CameraProviderTest, GetVendorTags) {
  static const uint32_t kMockTagIdOffset = 0x80000000;
  static const std::vector<VendorTagSection> kMockVendorTagSections = {
      {
          .section_name = "vendor.section_0",
          .tags =
              {
                  {
                      .tag_id = kMockTagIdOffset + 0,
                      .tag_name = "tag0",
                      .tag_type = CameraMetadataType::kByte,
                  },
                  {
                      .tag_id = kMockTagIdOffset + 1,
                      .tag_name = "tag1",
                      .tag_type = CameraMetadataType::kInt32,
                  },
              },
      },
      {
          .section_name = "vendor.section_1",
          .tags =
              {
                  {
                      .tag_id = kMockTagIdOffset + 2,
                      .tag_name = "tag2",
                      .tag_type = CameraMetadataType::kFloat,
                  },
                  {
                      .tag_id = kMockTagIdOffset + 3,
                      .tag_name = "tag3",
                      .tag_type = CameraMetadataType::kInt64,
                  },
                  {
                      .tag_id = kMockTagIdOffset + 4,
                      .tag_name = "tag4",
                      .tag_type = CameraMetadataType::kRational,
                  },
              },
      },
  };

  auto mock_provider_hwl = MockProviderHwl::Create();
  mock_provider_hwl->vendor_tag_sections_ = kMockVendorTagSections;

  CreateCameraProviderAndCheck(std::move(mock_provider_hwl));

  // Verify GetVendorTags(nullptr) returns an error.
  EXPECT_NE(provider_->GetVendorTags(nullptr), OK);

  std::vector<VendorTagSection> sections;
  status_t res = provider_->GetVendorTags(&sections);
  ASSERT_EQ(res, OK) << "Getting vendor tags failed: " << strerror(res);

  // Verify the mock sections are included in the returned sections.
  for (auto mock_section : kMockVendorTagSections) {
    bool found_section = false;

    for (auto returned_section : sections) {
      if (returned_section.section_name == mock_section.section_name) {
        found_section = true;

        // Verify the mock tags are included in the returned tags.
        for (auto mock_tag : mock_section.tags) {
          bool found_tag = false;
          for (auto returned_tag : returned_section.tags) {
            if (returned_tag.tag_id == mock_tag.tag_id) {
              found_tag = true;
              EXPECT_EQ(returned_tag.tag_name, mock_tag.tag_name);
              EXPECT_EQ(returned_tag.tag_type, mock_tag.tag_type);
              break;
            }
          }
          EXPECT_EQ(found_tag, true)
              << "Mock tag " << std::to_string(mock_tag.tag_id)
              << " in section " << mock_section.section_name << " is not found";
        }
        break;
      }
    }

    EXPECT_EQ(found_section, true)
        << "Mock section " << mock_section.section_name << " is not found";
  }
}

// Test if GetCameraIdList() returns the right number of camera IDs visible
// to framework.
TEST_F(CameraProviderTest, GetCameraIdList) {
  static const std::vector<CameraIdMap> id_maps = {
      {
          .id = 0,
          .visible_to_framework = true,
      },
      {
          .id = 1,
          .visible_to_framework = false,
      },
      {
          .id = 2,
          .visible_to_framework = true,
      },
  };

  // Calculate the number of public cameras.
  uint32_t num_public_cameras = 0;
  for (auto id_map : id_maps) {
    if (id_map.visible_to_framework) {
      num_public_cameras++;
    }
  }

  auto mock_provider_hwl = MockProviderHwl::Create();
  mock_provider_hwl->cameras_ = id_maps;

  CreateCameraProviderAndCheck(std::move(mock_provider_hwl));

  // Verify GetCameraIdList(nullptr) returns an error.
  EXPECT_NE(provider_->GetCameraIdList(nullptr), OK);

  std::vector<uint32_t> camera_ids;
  status_t res = provider_->GetCameraIdList(&camera_ids);
  ASSERT_EQ(res, OK) << "Getting camera IDs failed: " << strerror(res);

  // Verify the returned number of public cameras.
  EXPECT_EQ(camera_ids.size(), num_public_cameras);
}

// Test if IsSetTorchModeSupported() returns the same value that mock HWL
// returns.
TEST_F(CameraProviderTest, IsSetTorchModeSupported) {
  static const bool kIsTorchSupported[] = {true, false};

  for (auto is_torch_supported : kIsTorchSupported) {
    auto mock_provider_hwl = MockProviderHwl::Create();
    mock_provider_hwl->is_torch_supported_ = is_torch_supported;
    CreateCameraProviderAndCheck(std::move(mock_provider_hwl));

    EXPECT_EQ(provider_->IsSetTorchModeSupported(), is_torch_supported);
    DestroyCameraProvider();
  }
}

TEST_F(CameraProviderTest, CreateCameraDevice) {
  CreateCameraProviderAndCheck();

  std::vector<uint32_t> camera_ids;
  status_t res = provider_->GetCameraIdList(&camera_ids);
  ASSERT_EQ(res, OK) << "Getting camera IDs failed: " << strerror(res);

  for (auto camera_id : camera_ids) {
    std::unique_ptr<CameraDevice> device;
    provider_->CreateCameraDevice(camera_id, &device);
    EXPECT_NE(device, nullptr)
        << "Creating a CameraDevice for ID " << camera_id << " failed.";
  }
}

}  // namespace google_camera_hal
}  // namespace android
