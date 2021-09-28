#include "dynamic_depth/planes.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

namespace dynamic_depth {

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

// Private constructor.
Planes::Planes() = default;

void Planes::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr || plane_list_.empty()) {
    LOG(ERROR) << "Namespace list is null or plane list is empty";
    return;
  }
  for (const auto& plane : plane_list_) {
    plane->GetNamespaces(ns_name_href_map);
  }
}

std::unique_ptr<Planes> Planes::FromPlaneArray(
    std::vector<std::unique_ptr<Plane>>* plane_list) {
  if (plane_list == nullptr || plane_list->empty()) {
    LOG(ERROR) << "Plane list is empty";
    return nullptr;
  }

  for (const auto& plane : *plane_list) {
    if (!plane) {
      LOG(ERROR) << "plane_list cannot contain null elements";
      return nullptr;
    }
  }

  // Using `new` to access a non-public constructor.
  std::unique_ptr<Planes> planes(new Planes());  // NOLINT
  std::swap(*plane_list, planes->plane_list_);
  return planes;
}

std::unique_ptr<Planes> Planes::FromDeserializer(
    const Deserializer& parent_deserializer) {
  // Using `new` to access a non-public constructor.
  std::unique_ptr<Planes> planes(new Planes());  // NOLINT
  int i = 0;
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializerFromListElementAt(
          DynamicDepthConst::Namespace(DynamicDepthConst::Planes()),
          DynamicDepthConst::Planes(), 0);
  while (deserializer) {
    std::unique_ptr<Plane> plane = Plane::FromDeserializer(*deserializer);
    if (plane == nullptr) {
      LOG(ERROR) << "Unable to deserialize a plane";
      return nullptr;
    }

    planes->plane_list_.push_back(std::move(plane));
    i++;
    deserializer = parent_deserializer.CreateDeserializerFromListElementAt(
        DynamicDepthConst::Namespace(DynamicDepthConst::Planes()),
        DynamicDepthConst::Planes(), i);
  }

  if (planes->plane_list_.empty()) {
    return nullptr;
  }
  return planes;
}

int Planes::GetPlaneCount() const { return plane_list_.size(); }

const Plane* Planes::GetPlaneAt(int i) const { return plane_list_[i].get(); }

bool Planes::Serialize(Serializer* serializer) const {
  if (plane_list_.empty()) {
    LOG(ERROR) << "Plane list is empty";
    return false;
  }
  std::unique_ptr<Serializer> planes_serializer =
      serializer->CreateListSerializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Planes()),
          DynamicDepthConst::Planes());
  if (planes_serializer == nullptr) {
    // Error is logged in Serializer.
    return false;
  }
  for (int i = 0; i < plane_list_.size(); i++) {
    std::unique_ptr<Serializer> plane_serializer =
        planes_serializer->CreateItemSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::Plane()),
            DynamicDepthConst::Plane());
    if (plane_serializer == nullptr) {
      LOG(ERROR) << "Could not create a list item serializer for Plane";
      return false;
    }

    if (plane_list_[i] == nullptr) {
      LOG(ERROR) << "Plane " << i << " is null, could not serialize";
      continue;
    }

    if (!plane_list_[i]->Serialize(plane_serializer.get())) {
      LOG(ERROR) << "Could not serialize plane " << i;
      continue;
    }
  }
  return true;
}

}  // namespace dynamic_depth
