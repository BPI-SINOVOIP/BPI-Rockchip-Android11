#include "dynamic_depth/profiles.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {

void Profiles::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr || profile_list_.empty()) {
    LOG(ERROR) << "Namespace list is null or profile list is empty";
    return;
  }
  for (const auto& profile : profile_list_) {
    profile->GetNamespaces(ns_name_href_map);
  }
}

std::unique_ptr<Profiles> Profiles::FromProfileArray(
    std::vector<std::unique_ptr<Profile>>* profile_list) {
  if (profile_list->empty()) {
    LOG(ERROR) << "Profile list is empty";
    return nullptr;
  }
  std::unique_ptr<Profiles> profiles(new Profiles());
  profiles->profile_list_ = std::move(*profile_list);
  return profiles;
}

std::unique_ptr<Profiles> Profiles::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Profiles> profiles(new Profiles());
  int i = 0;
  for (std::unique_ptr<Deserializer> deserializer =
           parent_deserializer.CreateDeserializerFromListElementAt(
               DynamicDepthConst::Namespace(DynamicDepthConst::Profiles()),
               DynamicDepthConst::Profiles(), i);
       deserializer != nullptr;
       deserializer = parent_deserializer.CreateDeserializerFromListElementAt(
           DynamicDepthConst::Namespace(DynamicDepthConst::Profiles()),
           DynamicDepthConst::Profiles(), ++i)) {
    std::unique_ptr<Profile> profile = Profile::FromDeserializer(*deserializer);
    if (profile != nullptr) {
      profiles->profile_list_.emplace_back(std::move(profile));
    }
  }

  if (profiles->profile_list_.empty()) {
    return nullptr;
  }
  return profiles;
}

const std::vector<const Profile*> Profiles::GetProfiles() const {
  std::vector<const Profile*> profile_list;
  for (const auto& profile : profile_list_) {
    profile_list.push_back(profile.get());
  }
  return profile_list;
}

bool Profiles::Serialize(Serializer* serializer) const {
  if (profile_list_.empty()) {
    LOG(ERROR) << "Profile list is empty";
    return false;
  }
  bool success = true;
  int i = 0;
  std::unique_ptr<Serializer> profiles_serializer =
      serializer->CreateListSerializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Profiles()),
          DynamicDepthConst::Profiles());
  if (profiles_serializer == nullptr) {
    // Error is logged in Serializer.
    return false;
  }
  for (const auto& profile : profile_list_) {
    std::unique_ptr<Serializer> profile_serializer =
        profiles_serializer->CreateItemSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::Profile()),
            DynamicDepthConst::Profile());
    if (profile_serializer == nullptr) {
      continue;
    }
    success &= profile->Serialize(profile_serializer.get());
    if (!success) {
      LOG(ERROR) << "Could not serialize profile " << i;
    }
    ++i;
  }
  return success;
}

}  // namespace dynamic_depth
