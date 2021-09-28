#include "dynamic_depth/light_estimate.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "xmpmeta/base64.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {
constexpr int kColorCorrectionSize = 3;

constexpr char kPixelIntensity[] = "PixelIntensity";
constexpr char kColorCorrectionR[] = "ColorCorrectionR";
constexpr char kColorCorrectionG[] = "ColorCorrectionG";
constexpr char kColorCorrectionB[] = "ColorCorrectionB";

constexpr char kNamespaceHref[] =
    "http://ns.google.com/photos/dd/1.0/lightestimate/";

}  // namespace

// Private constructor.
LightEstimate::LightEstimate() {}

// Public methods.
void LightEstimate::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::LightEstimate(), kNamespaceHref);
}

std::unique_ptr<LightEstimate> LightEstimate::FromData(float pixel_intensity) {
  return LightEstimate::FromData(pixel_intensity, {1.0f, 1.0f, 1.0f});
}

std::unique_ptr<LightEstimate> LightEstimate::FromData(
    float pixel_intensity, const std::vector<float>& color_correction) {
  // Priate constructor.
  std::unique_ptr<LightEstimate> light_estimate(
      std::unique_ptr<LightEstimate>(new LightEstimate()));  // NOLINT
  light_estimate->pixel_intensity_ = pixel_intensity;

  if (color_correction.size() >= kColorCorrectionSize) {
    std::copy(color_correction.begin(),
              color_correction.begin() + kColorCorrectionSize,
              light_estimate->color_correction_.begin());
  } else {
    LOG(WARNING) << "Color correction had fewer than three values, "
                 << "reverting to default of 1.0 for all RGB values";
  }

  return light_estimate;
}

float LightEstimate::GetPixelIntensity() const { return pixel_intensity_; }

const std::vector<float>& LightEstimate::GetColorCorrection() const {
  return color_correction_;
}

std::unique_ptr<LightEstimate> LightEstimate::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::LightEstimate()),
          DynamicDepthConst::LightEstimate());
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<LightEstimate> light_estimate(
      std::unique_ptr<LightEstimate>(new LightEstimate()));  // NOLINT
  if (!deserializer->ParseFloat(DynamicDepthConst::LightEstimate(),
                                kPixelIntensity,
                                &light_estimate->pixel_intensity_)) {
    return nullptr;
  }

  float color_correction_r;
  float color_correction_g;
  float color_correction_b;
  if (deserializer->ParseFloat(DynamicDepthConst::LightEstimate(),
                               kColorCorrectionR, &color_correction_r) &&
      deserializer->ParseFloat(DynamicDepthConst::LightEstimate(),
                               kColorCorrectionG, &color_correction_g) &&
      deserializer->ParseFloat(DynamicDepthConst::LightEstimate(),
                               kColorCorrectionB, &color_correction_b)) {
    light_estimate->color_correction_[0] = color_correction_r;
    light_estimate->color_correction_[1] = color_correction_g;
    light_estimate->color_correction_[2] = color_correction_b;
  }

  return light_estimate;
}

bool LightEstimate::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (!serializer->WriteProperty(DynamicDepthConst::LightEstimate(),
                                 kPixelIntensity,
                                 std::to_string(pixel_intensity_))) {
    return false;
  }

  CHECK(color_correction_.size() >= 3)
      << "Color correction not initialized to a size-3 vector";
  return serializer->WriteProperty(DynamicDepthConst::LightEstimate(),
                                   kColorCorrectionR,
                                   std::to_string(color_correction_[0])) &&
         serializer->WriteProperty(DynamicDepthConst::LightEstimate(),
                                   kColorCorrectionG,
                                   std::to_string(color_correction_[1])) &&
         serializer->WriteProperty(DynamicDepthConst::LightEstimate(),
                                   kColorCorrectionB,
                                   std::to_string(color_correction_[2]));
}

}  // namespace dynamic_depth
