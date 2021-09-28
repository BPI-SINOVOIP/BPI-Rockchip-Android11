#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGING_MODEL_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGING_MODEL_H_  // NOLINT

#include <memory>
#include <unordered_map>

#include "dynamic_depth/dimension.h"
#include "dynamic_depth/element.h"
#include "dynamic_depth/point.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {
struct ImagingModelParams {
  // Required. The order of numbers is (x, y), in pixels.
  Point<double> focal_length;

  // Required. The order of numbers is (width, height), in pixels.
  Dimension image_size;

  // Optional. Set to (0.5, 0.5) if not present.
  // The order of numbers is (x, y).
  Point<double> principal_point;

  // Optional.
  std::vector<float> distortion;  // Distortion parameters.
  double skew;
  double pixel_aspect_ratio;

  ImagingModelParams(const Point<double>& focal_len,
                     const Dimension& image_size)
      : focal_length(focal_len),
        image_size(image_size),
        principal_point(Point<double>(0.5, 0.5)),
        distortion(std::vector<float>()),
        skew(0),
        pixel_aspect_ratio(1) {}

  inline bool operator==(const ImagingModelParams& other) const {
    return focal_length == other.focal_length &&
           image_size == other.image_size &&
           principal_point == other.principal_point &&
           distortion == other.distortion && skew == other.skew &&
           pixel_aspect_ratio == other.pixel_aspect_ratio;
  }

  inline bool operator!=(const ImagingModelParams& other) const {
    return !(*this == other);
  }
};

class ImagingModel : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates an ImagingModel from the given params.
  static std::unique_ptr<ImagingModel> FromData(
      const ImagingModelParams& params);

  // Returns the deserialized equirect model, null if parsing fails.
  static std::unique_ptr<ImagingModel> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Getters.
  Point<double> GetFocalLength() const;
  Point<double> GetPrincipalPoint() const;
  Dimension GetImageSize() const;
  double GetSkew() const;
  double GetPixelAspectRatio() const;
  const std::vector<float>& GetDistortion() const;
  int GetDistortionCount() const;

  // Disallow copying.
  ImagingModel(const ImagingModel&) = delete;
  void operator=(const ImagingModel&) = delete;

 private:
  ImagingModel(const ImagingModelParams& params);

  ImagingModelParams params_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGING_MODEL_H_  // NOLINT
