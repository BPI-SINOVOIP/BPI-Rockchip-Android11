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

#define LOG_TAG "CameraIDManagerTests"
#include <log/log.h>

#include <camera_id_manager.h>
#include <gtest/gtest.h>

namespace android {
namespace google_camera_hal {

TEST(CameraIdMangerTest, TestNoCameras) {
  auto id_manager = CameraIdManager::Create({});

  EXPECT_NE(id_manager, nullptr)
      << "Creating CameraIDManager with an empty list of cameras failed";
}

TEST(CameraIdMangerTest, InvalidLogicalCameras) {
  std::vector<CameraIdMap> cameras;
  cameras.push_back(CameraIdMap({0, false, {}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({2, false, {0, 1}}));

  // Create with a logical camera that is not visible to the framework
  auto id_manager = CameraIdManager::Create(cameras);

  EXPECT_EQ(id_manager, nullptr)
      << "Creating CameraIDManager with a logical camera that is not visible to"
         "the framework succeeded";

  cameras.clear();
  cameras.push_back(CameraIdMap({0, true, {0, 1}}));
  cameras.push_back(CameraIdMap({1, false, {}}));

  // Test for logical cameras that list logical cameras in their physical
  // camera ID list
  id_manager = CameraIdManager::Create(cameras);

  EXPECT_EQ(id_manager, nullptr)
      << "Creating CameraIDManager with a logical camera which lists a "
         "logical camera in its physical ID list succeeded";

  // Same test as above, different variation
  cameras.clear();
  cameras.push_back(CameraIdMap({0, true, {1, 2}}));
  cameras.push_back(CameraIdMap({1, true, {}}));
  cameras.push_back(CameraIdMap({2, true, {}}));
  cameras.push_back(CameraIdMap({3, true, {0, 1}}));

  id_manager = CameraIdManager::Create(cameras);

  EXPECT_EQ(id_manager, nullptr)
      << "Creating CameraIDManager with a logical camera which lists a "
         "logical camera in its physical ID list succeeded";
}

TEST(CameraIdMangerTest, NoVisibleCameras) {
  std::vector<CameraIdMap> cameras;
  cameras.push_back(CameraIdMap({0, false, {}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({2, false, {0, 1}}));

  // Create with a list of cameras none of which are visible to the framework
  auto id_manager = CameraIdManager::Create(cameras);

  EXPECT_EQ(id_manager, nullptr)
      << "Creating CameraIDManager with no visible cameras succeeded";
}

TEST(CameraIdMangerTest, InvalidParameters) {
  std::vector<CameraIdMap> cameras;
  cameras.push_back(CameraIdMap({0, true, {1, 2}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({2, false, {}}));
  cameras.push_back(CameraIdMap({3, false, {}}));
  cameras.push_back(CameraIdMap({4, true, {1, 3}}));

  auto id_manager = CameraIdManager::Create(cameras);
  ASSERT_NE(id_manager, nullptr);

  uint32_t dummy = 0;
  status_t res;
  // Test for invalid IDs or bad parameters
  res = id_manager->GetInternalCameraId(cameras.size(), &dummy);
  EXPECT_NE(res, OK) << "GetInternalCameraId() succeeded with an invalid ID";
  res = id_manager->GetInternalCameraId(cameras.size(), nullptr);
  EXPECT_NE(res, OK) << "GetInternalCameraId() succeeded with a null parameter";
  res = id_manager->GetPublicCameraId(cameras.size(), &dummy);
  EXPECT_NE(res, OK) << "GetPublicCameraId() succeeded with an invalid ID";
  res = id_manager->GetPublicCameraId(cameras.size(), nullptr);
  EXPECT_NE(res, OK) << "GetPublicCameraId() succeeded with a null parameter";

  // Test for failure when IDs are not unique
  cameras.clear();
  cameras.push_back(CameraIdMap({0, true, {}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({1, true, {}}));

  id_manager = CameraIdManager::Create(cameras);
  EXPECT_EQ(id_manager, nullptr) << "Creating camera manager with duplicate"
                                    "camera IDs succeeded";
}

TEST(CameraIdMangerTest, GetCameraIds) {
  std::vector<CameraIdMap> cameras;
  cameras.push_back(CameraIdMap({0, true, {1, 2}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({2, false, {}}));
  cameras.push_back(CameraIdMap({3, false, {}}));
  cameras.push_back(CameraIdMap({4, true, {1, 3}}));

  auto id_manager = CameraIdManager::Create(cameras);
  ASSERT_NE(id_manager, nullptr);

  status_t res;
  uint32_t internal_id = 0;
  uint32_t public_id = 0;

  // Loop over public IDs and map public -> internal -> public to ensure
  // we get the same ID back
  for (uint32_t i = 0; i < cameras.size(); i++) {
    res = id_manager->GetInternalCameraId(i, &internal_id);
    EXPECT_EQ(res, OK);

    res = id_manager->GetPublicCameraId(internal_id, &public_id);
    EXPECT_EQ(res, OK);

    EXPECT_EQ(public_id, i);
  }
}

// Helper function to verify that the given CameraIdManager maps to the
// expected output.
// 'expected_internal_ids' is the expected mapping, using index as public ID
// and value as expected internal ID
void ValidateCameraIds(CameraIdManager* id_manager,
                       std::vector<CameraIdMap>& cameras,
                       std::vector<uint32_t>& expected_internal_ids) {
  status_t res;
  uint32_t internal_id;
  uint32_t public_id;

  ASSERT_EQ(expected_internal_ids.size(), cameras.size())
      << "Mismatching test vector, did you forget to update the test?";

  // Test GetInternalCameraId()
  for (uint32_t i = 0; i < cameras.size(); i++) {
    public_id = i;
    res = id_manager->GetInternalCameraId(public_id, &internal_id);

    EXPECT_EQ(res, OK) << "GetInternalCameraId() failed!";

    if (res == OK) {
      EXPECT_EQ(internal_id, expected_internal_ids[public_id])
          << "Expected public ID " << public_id << " to map to internal ID "
          << expected_internal_ids[public_id] << " but instead got "
          << internal_id;
    }
  }

  // Test GetPublicCameraId()
  for (uint32_t i = 0; i < cameras.size(); i++) {
    internal_id = expected_internal_ids[i];
    res = id_manager->GetPublicCameraId(internal_id, &public_id);

    EXPECT_EQ(res, OK) << "GetInternalCameraId() failed!";

    if (res == OK) {
      ASSERT_EQ(public_id, i)
          << "Expected internal ID " << internal_id << " to map to public ID "
          << i << " but instead got " << public_id;
    }
  }

  // Test GetPhysicalCameraIds()
  for (uint32_t public_id : id_manager->GetCameraIds()) {
    res = id_manager->GetInternalCameraId(public_id, &internal_id);
    EXPECT_EQ(res, OK);
    ASSERT_LE(internal_id, cameras.size());

    auto physical_camera_ids = id_manager->GetPhysicalCameraIds(public_id);
    // Test to see if the physical IDs of logical cameras are mapped correctly
    for (uint32_t i = 0; i < physical_camera_ids.size(); i++) {
      // Public ID of the physical camera
      uint32_t phys_public_id = physical_camera_ids[i];
      uint32_t phys_internal_id = 0;
      uint32_t phys_internal_id_expected =
          cameras[internal_id].physical_camera_ids[i];

      res = id_manager->GetInternalCameraId(phys_public_id, &phys_internal_id);
      EXPECT_EQ(res, OK);
      if (res == OK) {
        EXPECT_EQ(phys_internal_id_expected, phys_internal_id);
      }
    }
  }

  // Test GetVisibleCameraIds()
  std::vector<uint32_t> public_ids = id_manager->GetVisibleCameraIds();
  size_t visible_camera_count = 0;
  for (auto camera : cameras) {
    if (camera.visible_to_framework) {
      visible_camera_count++;
    }
  }
  ASSERT_EQ(visible_camera_count, public_ids.size());

  // Ensure that all cameras marked as visible appear in the public ID list
  for (uint32_t public_id = 0; public_id < visible_camera_count; public_id++) {
    auto it = std::find(public_ids.begin(), public_ids.end(), public_id);
    EXPECT_NE(it, public_ids.end());
  }
}

TEST(CameraIdMangerTest, LogicalCamera) {
  std::vector<CameraIdMap> cameras;

  /* Create the following list of cameras:
   *
   * ID  Visible  Physical IDs
   *--------------------------
   * 0   N
   * 1   N
   * 2   N
   * 3   N
   * 4   N
   * 5   Y        0, 1
   * 6   Y        2, 3, 4
   * 7   Y        3, 4
   *
   *
   * Expected public camera list:
   * Public ID  Internal ID  Physical IDs  Visible
   *----------------------------------------------
   * 0          5            3, 4          Y
   * 1          6            5, 6, 7       Y
   * 2          7            6, 7          Y
   * 3          0                          N
   * 4          1                          N
   * 5          2                          N
   * 6          3                          N
   * 7          4                          N
   */
  cameras.push_back(CameraIdMap({0, false, {}}));
  cameras.push_back(CameraIdMap({1, false, {}}));
  cameras.push_back(CameraIdMap({2, false, {}}));
  cameras.push_back(CameraIdMap({3, false, {}}));
  cameras.push_back(CameraIdMap({4, false, {}}));
  cameras.push_back(CameraIdMap({5, true, {0, 1}}));
  cameras.push_back(CameraIdMap({6, true, {2, 3, 4}}));
  cameras.push_back(CameraIdMap({7, true, {3, 4}}));

  // Expected mapping, using index as public ID and value as internal ID
  std::vector<uint32_t> expected_internal_ids = {5, 6, 7, 0, 1, 2, 3, 4};

  // Create with a list of cameras none of which are visible to the framework
  auto id_manager = CameraIdManager::Create(cameras);
  ASSERT_NE(id_manager, nullptr) << "Creating CameraIDManager failed!";

  ValidateCameraIds(id_manager.get(), cameras, expected_internal_ids);

  // Now mark all cameras as visible and repeat the test:
  expected_internal_ids.clear();
  for (auto& camera : cameras) {
    camera.visible_to_framework = true;
    // IDs should now be identical to internal ones since all are visible
    expected_internal_ids.push_back(camera.id);
  }

  id_manager = CameraIdManager::Create(cameras);
  ASSERT_NE(id_manager, nullptr) << "Creating CameraIDManager failed!";

  ValidateCameraIds(id_manager.get(), cameras, expected_internal_ids);

  /* Create the following list of cameras:
   *
   * ID  Visible  Physical IDs
   *--------------------------
   * 0   N
   * 1   Y        0, 2
   * 2   Y
   * 3   Y
   *
   *
   * Expected public camera list:
   * Public ID  Internal ID  Physical IDs  Visible
   *----------------------------------------------
   * 0          1            3, 1          Y
   * 1          2                          Y
   * 2          3                          Y
   * 3          0                          N
   */
  cameras.clear();
  cameras.push_back(CameraIdMap({0, false, {}}));
  cameras.push_back(CameraIdMap({1, true, {0, 2}}));
  cameras.push_back(CameraIdMap({2, true, {}}));
  cameras.push_back(CameraIdMap({3, true, {}}));

  // Expected mapping, using index as public ID and value as internal ID
  expected_internal_ids = {1, 2, 3, 0};

  // Create with a list of cameras none of which are visible to the framework
  id_manager = CameraIdManager::Create(cameras);
  ASSERT_NE(id_manager, nullptr) << "Creating CameraIDManager failed!";

  ValidateCameraIds(id_manager.get(), cameras, expected_internal_ids);
}

}  // namespace google_camera_hal
}  // namespace android
