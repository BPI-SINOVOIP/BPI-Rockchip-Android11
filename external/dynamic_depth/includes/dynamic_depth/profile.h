#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILE_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// Implements the Profile element in the Dynamic Depth specification, with
// serialization and deserialization.
class Profile : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates a Profile element from the given fields. Returns null if
  // the type is empty, or if the camera_indices are shorter than the
  // specified minimum length for types supported in the Dynamic Depth
  // specification. Type is case-sensitive. If wrong, this will be treated as an
  // unsupported type.
  static std::unique_ptr<Profile> FromData(
      const string& type, const std::vector<int>& camera_indices);

  // Returns the deserialized Profile, null if parsing fails.
  static std::unique_ptr<Profile> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Returns the Profile type.
  const string& GetType() const;

  // Returns the camera indices.
  const std::vector<int>& GetCameraIndices() const;

  // Disallow copying.
  Profile(const Profile&) = delete;
  void operator=(const Profile&) = delete;

 private:
  Profile(const string& type, const std::vector<int>& camera_indices);

  string type_;
  std::vector<int> camera_indices_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILE_H_  // NOLINT
