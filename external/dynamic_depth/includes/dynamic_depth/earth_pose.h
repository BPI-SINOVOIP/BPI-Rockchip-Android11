#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_EARTH_POSE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_EARTH_POSE_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// Implements the EarthPose element in the Dynamic Depth specification, with
// serialization and deserialization.
class EarthPose : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("EarthPose", "http://ns.google.com/photos/dd/1.0/earthpose/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object. Returns true on success.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates a EarthPose from the given data.
  // The order of values in position is latitude, longitude, altitude..
  // The order of values in orientation is the quaternion x, y, z, w fields.
  // rotation angle in quaternion format.
  // Position and orientation are in raw coordinates, and will be stored as
  // normalied values. Please refer to the Dynamic Depth specification for the
  // Realm coordinate system.
  // At least one valid position or orientation must be provided. These
  // arguments will be ignored if the vector is of the wrong size.
  static std::unique_ptr<EarthPose> FromData(
      const std::vector<double>& position,
      const std::vector<float>& orientation, const int64 timestamp = -1);

  // Returns the deserialized XdmAudio; null if parsing fails.
  // The returned pointer is owned by the caller.
  static std::unique_ptr<EarthPose> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Returns true if the device's position is provided.
  bool HasPosition() const;

  // Returns true if the device's orientation is provided.
  bool HasOrientation() const;

  // Returns the device's position fields, or an empty vector if they are
  // not present.
  const std::vector<double>& GetPosition() const;

  // Returns the device's orientation fields, or an empty vector if they are
  // not present.
  const std::vector<float>& GetOrientation() const;

  // Timestamp.
  int64 GetTimestamp() const;

  // Disallow copying.
  EarthPose(const EarthPose&) = delete;
  void operator=(const EarthPose&) = delete;

 private:
  EarthPose();

  // Extracts device pose fields.
  bool ParseEarthPoseFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  // Position variables, in meters relative to camera 0.
  // If providing position data, all three fields must be set.
  // Stored in normalized form.
  // TODO(miraleung): Cleanup: consider std::optional for this and orientation_.
  std::vector<double> position_;

  // Orientation variables.
  // If providing orientation data, all four fields must be set.
  // Stored in normalized form.
  std::vector<float> orientation_;

  // Timestamp is Epoch time in milliseconds.
  int64 timestamp_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_EARTH_POSE_H_  // NOLINT
