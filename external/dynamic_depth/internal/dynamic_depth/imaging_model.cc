#include "dynamic_depth/imaging_model.h"

#include <math.h>

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"

namespace dynamic_depth {
namespace {

using ::dynamic_depth::xmpmeta::EncodeFloatArrayBase64;
using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

constexpr char kFocalLengthX[] = "FocalLengthX";
constexpr char kFocalLengthY[] = "FocalLengthY";
constexpr char kImageWidth[] = "ImageWidth";
constexpr char kImageHeight[] = "ImageHeight";
constexpr char kPrincipalPointX[] = "PrincipalPointX";
constexpr char kPrincipalPointY[] = "PrincipalPointY";
constexpr char kSkew[] = "Skew";
constexpr char kPixelAspectRatio[] = "PixelAspectRatio";
constexpr char kDistortion[] = "Distortion";
constexpr char kDistortionCount[] = "DistortionCount";

constexpr char kNamespaceHref[] =
    "http://ns.google.com/photos/dd/1.0/imagingmodel/";

std::unique_ptr<ImagingModel> ParseFields(const Deserializer& deserializer) {
  Point<double> focal_length(0, 0);
  Dimension image_size(0, 0);
  Point<double> principal_point(0.5, 0.5);
  double skew = 0;
  double pixel_aspect_ratio = 1.0;
  const string& prefix = DynamicDepthConst::ImagingModel();

  if (!deserializer.ParseDouble(prefix, kFocalLengthX, &focal_length.x) ||
      !deserializer.ParseDouble(prefix, kFocalLengthY, &focal_length.y) ||
      !deserializer.ParseInt(prefix, kImageWidth, &image_size.width) ||
      !deserializer.ParseInt(prefix, kImageHeight, &image_size.height)) {
    return nullptr;
  }

  double temp1;
  double temp2;
  if (deserializer.ParseDouble(prefix, kPrincipalPointX, &temp1) &&
      deserializer.ParseDouble(prefix, kPrincipalPointY, &temp2)) {
    principal_point.x = temp1;
    principal_point.y = temp2;
  }

  if (deserializer.ParseDouble(prefix, kSkew, &temp1)) {
    skew = temp1;
  }

  if (deserializer.ParseDouble(prefix, kPixelAspectRatio, &temp1)) {
    pixel_aspect_ratio = temp1;
  }

  int distortion_count = 0;
  std::vector<float> distortion;
  if (deserializer.ParseInt(DynamicDepthConst::ImagingModel(), kDistortionCount,
                            &distortion_count)) {
    if (distortion_count % 2 != 0) {
      LOG(ERROR) << "Parsed DistortionCount = " << distortion_count
                 << " was expected to be even";
      return nullptr;
    }

    deserializer.ParseFloatArrayBase64(DynamicDepthConst::ImagingModel(),
                                       kDistortion, &distortion);
    if (distortion.size() != distortion_count * 2) {
      LOG(ERROR) << "Parsed DistortionCount of " << distortion_count
                 << " but should be " << distortion.size()
                 << " when multiplied by 2";
      return nullptr;
    }
  }

  ImagingModelParams params(focal_length, image_size);
  params.principal_point = principal_point;
  params.distortion = distortion;
  params.skew = skew;
  params.pixel_aspect_ratio = pixel_aspect_ratio;
  return ImagingModel::FromData(params);
}

}  // namespace

// Private constructor.
ImagingModel::ImagingModel(const ImagingModelParams& params)
    : params_(params) {}

// Public methods.
void ImagingModel::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }

  ns_name_href_map->emplace(DynamicDepthConst::ImagingModel(), kNamespaceHref);
}

std::unique_ptr<ImagingModel> ImagingModel::FromData(
    const ImagingModelParams& params) {
  if (!params.distortion.empty() && params.distortion.size() % 2 != 0) {
    LOG(ERROR) << "Distortion must be empty or contain pairs of values, but an "
               << " odd number (size=" << params.distortion.size()
               << ") was found";
    return nullptr;
  }

  return std::unique_ptr<ImagingModel>(new ImagingModel(params));  // NOLINT
}

std::unique_ptr<ImagingModel> ImagingModel::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::ImagingModel()),
          DynamicDepthConst::ImagingModel());
  if (deserializer == nullptr) {
    return nullptr;
  }

  return ParseFields(*deserializer);
}

Point<double> ImagingModel::GetFocalLength() const {
  return params_.focal_length;
}

Point<double> ImagingModel::GetPrincipalPoint() const {
  return params_.principal_point;
}

Dimension ImagingModel::GetImageSize() const { return params_.image_size; }

double ImagingModel::GetSkew() const { return params_.skew; }

double ImagingModel::GetPixelAspectRatio() const {
  return params_.pixel_aspect_ratio;
}

const std::vector<float>& ImagingModel::GetDistortion() const {
  return params_.distortion;
}

int ImagingModel::GetDistortionCount() const {
  return static_cast<int>(floor(params_.distortion.size() / 2));
}

bool ImagingModel::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  // Short-circuiting ensures unnecessary writes will not be performed.
  if (!serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kFocalLengthX,
                                 std::to_string(params_.focal_length.x)) ||
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kFocalLengthY,
                                 std::to_string(params_.focal_length.y)) ||
      // Image dimensions.
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(), kImageWidth,
                                 std::to_string(params_.image_size.width)) ||
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kImageHeight,
                                 std::to_string(params_.image_size.height)) ||
      // Principal point.
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kPrincipalPointX,
                                 std::to_string(params_.principal_point.x)) ||
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kPrincipalPointY,
                                 std::to_string(params_.principal_point.y)) ||
      // Skew, pixel aspect ratio.
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(), kSkew,
                                 std::to_string(params_.skew)) ||
      !serializer->WriteProperty(DynamicDepthConst::ImagingModel(),
                                 kPixelAspectRatio,
                                 std::to_string(params_.pixel_aspect_ratio))) {
    return false;
  }

  // Write distortion model only if needed.
  if (params_.distortion.empty()) {
    return true;
  }

  // No error-checking that there are an even number of values in
  // params_.distortion, because this is already done in the instantiator and
  // deserializer.
  string base64_encoded_distortion;
  if (!EncodeFloatArrayBase64(params_.distortion, &base64_encoded_distortion)) {
    LOG(ERROR) << "Distortion encoding failed";
    return false;
  }

  int distortion_count = static_cast<int>(floor(params_.distortion.size() / 2));
  if (!serializer->WriteProperty(
          DynamicDepthConst::ImagingModel(), kDistortionCount,
          ::dynamic_depth::strings::SimpleItoa(distortion_count))) {
    return false;
  }

  if (!serializer->WriteProperty(DynamicDepthConst::ImagingModel(), kDistortion,
                                 base64_encoded_distortion)) {
    return false;
  }

  return true;
}

}  // namespace dynamic_depth
