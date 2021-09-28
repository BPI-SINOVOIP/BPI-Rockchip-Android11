#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERAS_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERAS_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/camera.h"
#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// Implements the Device:Cameras field from the Dynamic Depth specification,
// with serialization and deserialization for its child Camera elements.
class Cameras : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates this object from the given cameras. Returns null if the list is
  // empty.
  // If creation succeeds, ownership of the Camera objects are transferred to
  // the resulting Cameras object. The vector of Camera objects will be cleared.
  static std::unique_ptr<Cameras> FromCameraArray(
      std::vector<std::unique_ptr<Camera>>* camera_list);

  // Returns the deserialized cameras in a Cameras object, null if parsing
  // failed for all the cameras.
  static std::unique_ptr<Cameras> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Returns the list of cameras.
  const std::vector<const Camera*> GetCameras() const;

  // Disallow copying.
  Cameras(const Cameras&) = delete;
  void operator=(const Cameras&) = delete;

 private:
  Cameras();

  std::vector<std::unique_ptr<Camera>> camera_list_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERAS_H_  // NOLINT
