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

#define LOG_TAG "CameraDeviceTests"
#include <log/log.h>

#include <camera_device.h>
#include <gtest/gtest.h>
#include <system/camera_metadata.h>

#include "mock_device_hwl.h"

namespace android {
namespace google_camera_hal {

TEST(CameraDeviceTests, Create) {
  auto device = CameraDevice::Create(nullptr);
  EXPECT_EQ(device, nullptr);

  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  device = CameraDevice::Create(std::move(mock_device_hwl));
  EXPECT_NE(device, nullptr);
}

TEST(CameraDeviceTests, GetPublicCameraId) {
  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  uint32_t camera_id = 5;
  mock_device_hwl->camera_id_ = camera_id;

  auto device = CameraDevice::Create(std::move(mock_device_hwl));
  ASSERT_NE(device, nullptr);

  EXPECT_EQ(device->GetPublicCameraId(), camera_id);
}

void TestResourceCost(const CameraResourceCost& resource_cost) {
  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  mock_device_hwl->resource_cost_ = resource_cost;

  auto device = CameraDevice::Create(std::move(mock_device_hwl));
  ASSERT_NE(device, nullptr);

  EXPECT_EQ(device->GetResourceCost(nullptr), BAD_VALUE);

  CameraResourceCost result_cost = {};
  ASSERT_EQ(device->GetResourceCost(&result_cost), OK);
  EXPECT_EQ(result_cost.resource_cost, resource_cost.resource_cost);
  EXPECT_EQ(result_cost.conflicting_devices, resource_cost.conflicting_devices);
}

TEST(CameraDeviceTests, GetResourceCost) {
  TestResourceCost({
      .resource_cost = 50,
      .conflicting_devices = {1, 2, 3},
  });

  TestResourceCost({
      .resource_cost = 100,
      .conflicting_devices = {},
  });
}

// TODO(b/138960498): Test GetCameraCharacteristics and
// GetPhysicalCameraCharacteristicsafter HalCameraMetadata supports setting and
// getting metadata.

TEST(CameraDeviceTests, SetTorchMode) {
  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  auto device = CameraDevice::Create(std::move(mock_device_hwl));
  ASSERT_NE(device, nullptr);

  EXPECT_EQ(device->SetTorchMode(TorchMode::kOff), OK);
  EXPECT_EQ(device->SetTorchMode(TorchMode::kOn), OK);
}

TEST(CameraDeviceTests, DumpState) {
  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  std::string test_string = "CameraDeviceTests_DumpState";
  mock_device_hwl->dump_string_ = "\n" + test_string + "\n";

  auto device = CameraDevice::Create(std::move(mock_device_hwl));
  ASSERT_NE(device, nullptr);

  // Dump to an invalid file descriptor.
  EXPECT_EQ(device->DumpState(-1), BAD_VALUE);

  std::FILE* f = std::tmpfile();
  ASSERT_NE(f, nullptr);

  // Dump to a valid file descriptor.
  EXPECT_EQ(device->DumpState(fileno(f)), OK);

  // Verify test_string exist in the file.
  std::rewind(f);

  char line[512];
  bool found_test_string = false;

  while (fgets(line, sizeof(line), f) != nullptr) {
    if (std::string(line).find(test_string) != std::string::npos) {
      found_test_string = true;
    }
  }

  fclose(f);

  EXPECT_EQ(found_test_string, true);
}

TEST(CameraDeviceTests, CreateCameraDeviceSession) {
  auto mock_device_hwl = MockDeviceHwl::Create();
  ASSERT_NE(mock_device_hwl, nullptr);

  auto device = CameraDevice::Create(std::move(mock_device_hwl));
  ASSERT_NE(device, nullptr);

  std::unique_ptr<CameraDeviceSession> session[2];

  EXPECT_EQ(device->CreateCameraDeviceSession(nullptr), BAD_VALUE);

  EXPECT_EQ(device->CreateCameraDeviceSession(&session[0]), OK);
  EXPECT_NE(session[0], nullptr);

  // Verify the session is valid after destroying the device.
  device = nullptr;
  EXPECT_EQ(session[0]->Flush(), OK);

  // TODO(b/121145153): Creating a second session should fail.

  // TODO(b/121145153): Creating a second session should succeed after
  // destroying the first session.
}

}  // namespace google_camera_hal
}  // namespace android
