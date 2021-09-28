#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEPTH_MAP_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEPTH_MAP_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "dynamic_depth/element.h"
#include "dynamic_depth/item.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// The depth conversion format. Please see the Depth Map element in the
// Dynamnic Depth specification for more details.
enum class DepthFormat { kFormatNone = 0, kRangeInverse = 1, kRangeLinear = 2 };

//  The Units of the depth map. Please see the Depth Map element in the Dynamic
//  Depth specification.
enum class DepthUnits { kUnitsNone = 0, kMeters = 1, kDiopters = 2 };

// The type of depth measurement. Please see the Depth Map element in the
// Dynamic Depth specification.
enum class DepthMeasureType { kOpticalAxis = 1, kOpticRay = 2 };

// The semantics of this depth map.
enum class DepthItemSemantic { kDepth = 1, kSegmentation = 2 };

struct DepthMapParams {
  // Mandatory values.
  DepthFormat format;
  float near;
  float far;
  DepthUnits units;
  string depth_uri;
  string mime;
  DepthItemSemantic item_semantic = DepthItemSemantic::kDepth;

  // The bytes of the depth image. Must be non-empty at write-time (i.e.
  // programmatic construction).
  string depth_image_data = "";

  // Optional values.
  DepthMeasureType measure_type = DepthMeasureType::kOpticalAxis;
  string confidence_uri = "";
  // The bytes of the confidence map. If confidence_uri is not empty, the
  // confidence data must be non-empty at write-time (i.e. programmatic
  // construction).
  string confidence_data = "";
  string software = "";

  // A list of (distance, radius) pairs. This should generally have a short
  // length, so copying is expected to be inexpensive.
  std::vector<float> focal_table;

  explicit DepthMapParams(DepthFormat in_format, float in_near, float in_far,
                          DepthUnits in_units, string in_depth_uri)
      : format(in_format),
        near(in_near),
        far(in_far),
        units(in_units),
        depth_uri(in_depth_uri) {}

  inline bool operator==(const DepthMapParams& other) const {
    return format == other.format && near == other.near && far == other.far &&
           units == other.units && depth_uri == other.depth_uri &&
           depth_image_data == other.depth_image_data &&
           measure_type == other.measure_type &&
           confidence_uri == other.confidence_uri &&
           confidence_data == other.confidence_data &&
           software == other.software && focal_table == other.focal_table;
  }

  inline bool operator!=(const DepthMapParams& other) const {
    return !(*this == other);
  }
};

// Implements the Depth Map element from the Dynamic Depth specification, with
// serialization and deserialization.
class DepthMap : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates a DepthMap from the given objects in params.
  static std::unique_ptr<DepthMap> FromData(
      const DepthMapParams& params, std::vector<std::unique_ptr<Item>>* items);

  // Returns the deserialized DepthMap object, null if parsing fails.
  // Not sensitive to case when parsing the Format, Units, or MeasureType
  // fields.
  static std::unique_ptr<DepthMap> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  DepthFormat GetFormat() const;
  float GetNear() const;
  float GetFar() const;
  DepthUnits GetUnits() const;
  const string GetDepthUri() const;
  DepthItemSemantic GetItemSemantic() const;
  const string GetConfidenceUri() const;
  DepthMeasureType GetMeasureType() const;
  const string GetSoftware() const;
  const std::vector<float>& GetFocalTable() const;
  size_t GetFocalTableEntryCount() const;

  // Disallow copying
  DepthMap(const DepthMap&) = delete;
  void operator=(const DepthMap&) = delete;

 private:
  explicit DepthMap(const DepthMapParams& params);
  static std::unique_ptr<DepthMap> ParseFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  DepthMapParams params_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DEPTH_MAP_H_  // NOLINT
