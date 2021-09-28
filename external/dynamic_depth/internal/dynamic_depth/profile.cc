#include "dynamic_depth/profile.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kType[] = "Type";
const char kCameraIndices[] = "CameraIndices";

const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/profile/";

// Profile type names.
constexpr char kArPhoto[] = "ARPhoto";
constexpr char kArPhotoLowercase[] = "arphoto";
constexpr char kDepthPhoto[] = "DepthPhoto";
constexpr char kDepthPhotoLowercase[] = "depthphoto";

// Profile camera indices' sizes.
constexpr size_t kArPhotoIndicesSize = 1;
constexpr size_t kDepthPhotoIndicesSize = 1;

// Returns true if the type is unsupported, or if the type is supported in the
// Dynamic Depth Profile element and the size of the camera indices matches that
// specified in the spec.
bool ValidateKnownTypeAndIndices(const string& type, size_t camera_indices_size,
                                 string* matched_type) {
  string type_lower = type;
  std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(),
                 ::tolower);
  bool isArPhoto = (kArPhotoLowercase == type_lower);
  bool isDepthPhoto = (kDepthPhotoLowercase == type_lower);
  if (!isArPhoto && !isDepthPhoto) {
    return true;
  }
  bool matches =
      (isArPhoto && camera_indices_size >= kArPhotoIndicesSize) ||
      (isDepthPhoto && camera_indices_size >= kDepthPhotoIndicesSize);
  if (!matches) {
    LOG(WARNING) << "Size of camera indices for "
                 << (isArPhoto ? kArPhoto : kDepthPhoto) << " must be at least "
                 << (isArPhoto ? kArPhotoIndicesSize : kDepthPhotoIndicesSize);
  } else {
    *matched_type = isArPhoto ? kArPhoto : kDepthPhoto;
  }

  return matches;
}

}  // namespace

// Private constructor.
Profile::Profile(const string& type, const std::vector<int>& camera_indices)
    : type_(type), camera_indices_(camera_indices) {}

// Public methods.
void Profile::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Profile(), kNamespaceHref);
}

std::unique_ptr<Profile> Profile::FromData(
    const string& type, const std::vector<int>& camera_indices) {
  if (type.empty()) {
    LOG(ERROR) << "Profile must have a type";
    return nullptr;
  }
  // Check that the camera indices' length is at least the size of that
  // specified for the type. This has no restrictions on unsupported profile
  // types.
  // Ensure also that a consistent case-sensitive profile is stored, even if the
  // caller provided a case-insensitive name.
  string matched_type;
  if (!ValidateKnownTypeAndIndices(type, camera_indices.size(),
                                   &matched_type)) {
    return nullptr;
  }

  return std::unique_ptr<Profile>(
      new Profile(matched_type.empty() ? type :
                  matched_type, camera_indices));  // NOLINT
}

std::unique_ptr<Profile> Profile::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Profile()),
          DynamicDepthConst::Profile());
  if (deserializer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<Profile> profile(new Profile("", {}));
  if (!deserializer->ParseString(DynamicDepthConst::Profile(), kType,
                                 &profile->type_)) {
    return nullptr;
  }
  deserializer->ParseIntArray(DynamicDepthConst::Profile(), kCameraIndices,
                              &profile->camera_indices_);
  if (!ValidateKnownTypeAndIndices(
          profile->type_, profile->camera_indices_.size(), &profile->type_)) {
    return nullptr;
  }
  return profile;
}

const string& Profile::GetType() const { return type_; }

const std::vector<int>& Profile::GetCameraIndices() const {
  return camera_indices_;
}

bool Profile::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }
  if (!serializer->WriteProperty(DynamicDepthConst::Profile(), kType, type_)) {
    return false;
  }
  if (camera_indices_.empty()) {
    return true;
  }
  return serializer->WriteIntArray(DynamicDepthConst::Profile(), kCameraIndices,
                                   camera_indices_);
}

}  // namespace dynamic_depth
