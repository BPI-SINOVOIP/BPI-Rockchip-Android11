#include "dynamic_depth/depth_jpeg.h"

#include <fstream>
#include <sstream>

#include "android-base/logging.h"
#include "dynamic_depth/container.h"
#include "dynamic_depth/device.h"
#include "dynamic_depth/dynamic_depth.h"
#include "dynamic_depth/item.h"
#include "image_io/gcontainer/gcontainer.h"
#include "xmpmeta/xmp_data.h"
#include "xmpmeta/xmp_parser.h"
#include "xmpmeta/xmp_writer.h"

using ::dynamic_depth::xmpmeta::XmpData;

namespace dynamic_depth {

int32_t ValidateAndroidDynamicDepthBuffer(const char* buffer, size_t buffer_length) {
  XmpData xmp_data;
  std::string itemMime("image/jpeg");
  const string image_data(buffer, buffer_length);
  ReadXmpFromMemory(image_data, /*XmpSkipExtended*/ false, &xmp_data);

  // Check device presence
  std::unique_ptr<Device> device = Device::FromXmp(xmp_data);
  if (device == nullptr) {
    LOG(ERROR) << "Dynamic depth device element not present!";
    return -1;
  }

  // Check the container items mime type
  if ((device->GetContainer() == nullptr) || (device->GetContainer()->GetItems().empty())) {
    LOG(ERROR) << "No container or container items found!";
    return -1;
  }
  auto items = device->GetContainer()->GetItems();
  for (const auto& item : items) {
    if (item->GetMime() != itemMime) {
      LOG(ERROR) << "Item MIME type doesn't match the expected value: " << itemMime;
      return -1;
    }
  }
  // Check profiles
  const Profiles* profiles = device->GetProfiles();
  if (profiles == nullptr) {
    LOG(ERROR) << "No Profile found in the dynamic depth metadata";
    return -1;
  }

  const std::vector<const Profile*> profile_list = profiles->GetProfiles();
  // Stop at the first depth photo profile found.
  bool depth_photo_profile_found = false;
  int camera_index = 0;
  for (auto profile : profile_list) {
    depth_photo_profile_found = !profile->GetType().compare("DepthPhoto");
    if (depth_photo_profile_found) {
      // Use the first one if available.
      auto indices = profile->GetCameraIndices();
      if (!indices.empty()) {
        camera_index = indices[0];
      } else {
        camera_index = -1;
      }
      break;
    }
  }

  if (!depth_photo_profile_found || camera_index < 0) {
    LOG(ERROR) << "No dynamic depth profile found";
    return -1;
  }

  auto cameras = device->GetCameras();
  if (cameras == nullptr || camera_index > cameras->GetCameras().size() ||
      cameras->GetCameras()[camera_index] == nullptr) {
    LOG(ERROR) << "No camera or depth photo data found";
    return -1;
  }

  auto camera = cameras->GetCameras()[camera_index];
  auto depth_map = camera->GetDepthMap();
  if (depth_map == nullptr) {
    LOG(ERROR) << "No depth map found";
    return -1;
  }

  auto depth_uri = depth_map->GetDepthUri();
  if (depth_uri.empty()) {
    LOG(ERROR) << "Invalid depth map URI";
    return -1;
  }

  auto depth_units = depth_map->GetUnits();
  if (depth_units != dynamic_depth::DepthUnits::kMeters) {
    LOG(ERROR) << "Unexpected depth map units";
    return -1;
  }

  auto depth_format = depth_map->GetFormat();
  if (depth_format != dynamic_depth::DepthFormat::kRangeInverse) {
    LOG(ERROR) << "Unexpected depth map format";
    return -1;
  }

  auto near = depth_map->GetNear();
  auto far = depth_map->GetFar();
  if ((near < 0.f) || (far < 0.f) || (near > far) || (near == far)) {
    LOG(ERROR) << "Unexpected depth map near and far values";
    return -1;
  }

  auto confidence_uri = depth_map->GetConfidenceUri();
  if (confidence_uri.empty()) {
    LOG(ERROR) << "No confidence URI";
    return -1;
  }

  std::istringstream input_jpeg_stream(std::string(buffer, buffer_length));
  std::string depth_payload;
  if (!GetItemPayload(device->GetContainer(), depth_uri, input_jpeg_stream, &depth_payload)) {
    LOG(ERROR) << "Unable to retrieve depth map";
    return -1;
  }

  if (depth_payload.empty()) {
    LOG(ERROR) << "Invalid depth map";
    return -1;
  }

  return 0;
}

}  // namespace dynamic_depth
