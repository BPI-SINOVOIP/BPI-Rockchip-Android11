#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILES_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILES_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/profile.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// Implements the Device:Profiles field from the Dynamic Depth specification,
// with serialization and deserialization for its child Profile elements.
class Profiles : public Element {
 public:
  // Interface methods.
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Static methods.

  // Creates this object from the given profiles. If the list is empty, returns
  // a unique_ptr owning nothing.
  static std::unique_ptr<Profiles> FromProfileArray(
      std::vector<std::unique_ptr<Profile>>* profile_list);

  // Returns the deserialized profiles in a Profiles object, a unique_ptr owning
  // nothing if parsing failed for all the profiles.
  static std::unique_ptr<Profiles> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Non-static methods.

  // Returns the list of cameras.
  const std::vector<const Profile*> GetProfiles() const;

  // Disallow copying
  Profiles(const Profiles&) = delete;
  void operator=(const Profiles&) = delete;

 private:
  Profiles() = default;

  std::vector<std::unique_ptr<Profile>> profile_list_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PROFILES_H_  // NOLINT
