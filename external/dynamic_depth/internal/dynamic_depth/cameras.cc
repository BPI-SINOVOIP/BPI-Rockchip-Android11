#include "dynamic_depth/cameras.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {

const char kNodeName[] = "Cameras";
const char kCameraName[] = "Camera";

// Private constructor.
Cameras::Cameras() {}

// Public methods.
void Cameras::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr || camera_list_.empty()) {
    LOG(ERROR) << "Namespace list is null or camera list is empty";
    return;
  }
  for (const auto& camera : camera_list_) {
    camera->GetNamespaces(ns_name_href_map);
  }
}

std::unique_ptr<Cameras> Cameras::FromCameraArray(
    std::vector<std::unique_ptr<Camera>>* camera_list) {
  if (camera_list == nullptr || camera_list->empty()) {
    LOG(ERROR) << "Camera list is empty";
    return nullptr;
  }
  std::unique_ptr<Cameras> cameras(new Cameras());
  cameras->camera_list_ = std::move(*camera_list);
  return cameras;
}

std::unique_ptr<Cameras> Cameras::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Cameras> cameras(new Cameras());
  int i = 0;
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializerFromListElementAt(
          DynamicDepthConst::Namespace(kNodeName), kNodeName, 0);
  while (deserializer) {
    std::unique_ptr<Camera> camera = Camera::FromDeserializer(*deserializer);
    if (camera == nullptr) {
      LOG(ERROR) << "Unable to deserialize a camera";
      return nullptr;
    }
    cameras->camera_list_.emplace_back(std::move(camera));
    deserializer = parent_deserializer.CreateDeserializerFromListElementAt(
        DynamicDepthConst::Namespace(kNodeName), kNodeName, ++i);
  }

  if (cameras->camera_list_.empty()) {
    return nullptr;
  }
  return cameras;
}

const std::vector<const Camera*> Cameras::GetCameras() const {
  std::vector<const Camera*> camera_list;
  for (const auto& camera : camera_list_) {
    camera_list.push_back(camera.get());
  }
  return camera_list;
}

bool Cameras::Serialize(Serializer* serializer) const {
  if (camera_list_.empty()) {
    LOG(ERROR) << "Camera list is empty";
    return false;
  }
  std::unique_ptr<Serializer> cameras_serializer =
      serializer->CreateListSerializer(DynamicDepthConst::Namespace(kNodeName),
                                       kNodeName);
  if (cameras_serializer == nullptr) {
    // Error is logged in Serializer.
    return false;
  }
  for (int i = 0; i < camera_list_.size(); i++) {
    std::unique_ptr<Serializer> camera_serializer =
        cameras_serializer->CreateItemSerializer(
            DynamicDepthConst::Namespace(kCameraName), kCameraName);
    if (camera_serializer == nullptr) {
      LOG(ERROR) << "Could not create a list item serializer for Camera";
      return false;
    }
    if (!camera_list_[i]->Serialize(camera_serializer.get())) {
      LOG(ERROR) << "Could not serialize camera " << i;
      return false;
    }
  }
  return true;
}

}  // namespace dynamic_depth
