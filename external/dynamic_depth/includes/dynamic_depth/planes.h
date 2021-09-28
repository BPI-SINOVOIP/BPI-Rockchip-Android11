#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANES_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANES_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/plane.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// THe list of planes in a Dynamic Depth Device t ype.
class Planes : public Element {
 public:
  // Creates this object from the given planes. Returns null if the list is
  // empty, null, or contains null elements.
  // If creation succeeds, ownership of the Plane objects are transferred to
  // the resulting Planes object. The pointer to the vector of Plane objects
  // will be replaced with the plane_list_, whose contents are not necessarily
  // defined.
  static std::unique_ptr<Planes> FromPlaneArray(
      std::vector<std::unique_ptr<Plane>>* plane_list);

  // Returns the deserialized cameras in a Planes object, null if parsing
  // failed for all the planes, one of the planes is null,  or the list of
  // planes was empty.
  static std::unique_ptr<Planes> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Disallow copying.
  Planes(const Planes&) = delete;
  Plane& operator=(const Planes&) = delete;

  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Returns false if the list of planes is empty, or serialization fails.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Returns the number of plane elements in this Plane object.
  int GetPlaneCount() const;

  // Returns the ith plane, which is guaranteed not to be null because
  // FromPlaneArray or FromDeserializer would have failed to construct a valid
  // Planes object.
  const Plane* GetPlaneAt(int i) const;

 private:
  Planes();

  std::vector<std::unique_ptr<Plane>> plane_list_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANES_H_  // NOLINT
