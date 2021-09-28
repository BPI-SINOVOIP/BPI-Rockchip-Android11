#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANE_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/pose.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// A Plane element for a Dynamic Depth device.
// Only horizontal planes are currently supported.
class Plane : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("Plane", "http://ns.google.com/photos/dd/1.0/plane/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Returns the deserialized Plane; null if parsing fails.
  static std::unique_ptr<Plane> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Creates a Plane from the given fields.
  // The Pose must be present.
  // boundary contains the bounding vertices: [x0, z0, x1, z1, ..., xn, zn].
  // extent_x is the length of the plane on the X axis.
  // extent_z is the length of the plane on the Z axis.
  static std::unique_ptr<Plane> FromData(std::unique_ptr<Pose> pose,
                                         const std::vector<float>& boundary,
                                         const double extent_x,
                                         const double extent_z);

  // Getters.
  const Pose* GetPose() const;

  // Returns the plane's bounding vertices on the XZ plane.
  const std::vector<float>& GetBoundary() const;

  // Returns the number of vertices in the boundary polygon.
  int GetBoundaryVertexCount() const;

  // Returns the plane's length along the X axis.
  const double GetExtentX() const;

  // Returns the plane's length along the Z axis.
  const double GetExtentZ() const;

  // Disallow copying.
  Plane(const Plane&) = delete;
  void operator=(const Plane&) = delete;

 private:
  Plane();

  // Extracts plane fields.
  bool ParsePlaneFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  // The pose of the center of this plane.
  std::unique_ptr<Pose> pose_;

  // Contains the plane's bounding vertices: [x0, z0, x1, z1, ..., xn, zn].
  // Optional field, set to an empty vector if not present.
  std::vector<float> boundary_;

  // Required field if "Boundary" is present. Optional otherwise and
  // expected to be zero; set to 0 if not present.
  int boundary_vertex_count_;

  // The length of the plane on the X axis. A value of -1 represents infinity.
  // Optional field, set to -1 (infinity) if not present.
  double extent_x_;

  // The length of the plane on the Z axis. A value of -1 represents infinity.
  // Optional field, set to -1 (infinity) if not present.
  double extent_z_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_PLANE_H_  // NOLINT
