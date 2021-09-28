#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_LIGHT_ESTIMATE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_LIGHT_ESTIMATE_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// Light estimation parameters for a camera.
// This is stored as a sibling element to LightEstimate because it may apply to
// the container image, so the decoupling is required.
class LightEstimate : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("LightEstimate", "http://ns.google.com/photos/dd/1.0/image/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates an LightEstimate from the given field.
  static std::unique_ptr<LightEstimate> FromData(float pixel_intensity);

  // Takes the first three values from color_correction if the vector length is
  // greater than 3.
  // Color correction values should be between 0 and 1 (plus or minus 0.2).
  static std::unique_ptr<LightEstimate> FromData(
      float pixel_intensity, const std::vector<float>& color_correction);

  // Returns the deserialized LightEstimate; null if parsing fails.
  static std::unique_ptr<LightEstimate> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Returns the average pixel internsity.
  float GetPixelIntensity() const;
  const std::vector<float>& GetColorCorrection() const;

  // Disallow copying.
  LightEstimate(const LightEstimate&) = delete;
  void operator=(const LightEstimate&) = delete;

 private:
  LightEstimate();

  float pixel_intensity_ = 1.0f;

  // Optional, either all three together or none at all.
  // A size-3 vector of color correction values, in the order R, G, B.
  // Values can be approximately between 0 and 1 (plus or minus 0.2).
  // On reading back image metadata, if only one or two of the values are
  // present, then the defaults below are used.
  std::vector<float> color_correction_ = {1.0f, 1.0f, 1.0f};
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_LIGHT_ESTIMATE_H_  // NOLINT
