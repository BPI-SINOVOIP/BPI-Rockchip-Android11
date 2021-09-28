#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_POINT_CLOUD_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_POINT_CLOUD_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

// Implements the Point Cloud element from the Dynamic Depth specification, with
// serialization and deserialization.
namespace dynamic_depth {

class PointCloud : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates a Point Cloud from the given fields. Returns null if position is
  // empty or points.size() is not divisible by 4.
  // The first two arguments are required fields, the rest are optional.
  // points is a list of (x, y, z, c) tuples, so it must have a size that is
  // evenly divisible by 4.
  // The first three values are the point's XYZ coordinates, and the fourth
  // is the confidence value. More details are available in the specification.
  static std::unique_ptr<PointCloud> FromData(const std::vector<float>& points,
                                              bool metric);

  // Returns the deserialized PointCloud; null if parsing fails.
  // The returned pointer is owned by the caller.
  static std::unique_ptr<PointCloud> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Getters.
  // Returns the number of (x, y, z, c) tuples, *not* the length of points_.
  int GetPointCount() const;
  const std::vector<float>& GetPoints() const;
  bool GetMetric() const;

  PointCloud(const PointCloud&) = delete;
  void operator=(const PointCloud&) = delete;

 private:
  PointCloud();

  bool ParseFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  // Required fields.
  std::vector<float> points_;

  // Optional fields.
  bool metric_;
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_POINT_CLOUD_H_  // NOLINT
