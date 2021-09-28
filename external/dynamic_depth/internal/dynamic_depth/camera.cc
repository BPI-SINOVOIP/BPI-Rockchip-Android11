
#include "dynamic_depth/camera.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/camera/";

constexpr const char* kTrait = "Trait";
constexpr const char* kTraitPhysical = "Physical";
constexpr const char* kTraitPhysicalLower = "physical";
constexpr const char* kTraitLogical = "Logical";
constexpr const char* kTraitLogicalLower = "logical";

constexpr const char* kImageJpegMime = "image/jpeg";

string TraitToString(CameraTrait trait) {
  switch (trait) {
    case PHYSICAL:
      return kTraitPhysical;
    case LOGICAL:
      return kTraitLogical;
    case NONE:  // Fallthrough.
    default:
      return "";
  }
}

CameraTrait StringToTrait(const string& trait_name) {
  string trait_lower = trait_name;
  std::transform(trait_lower.begin(), trait_lower.end(), trait_lower.begin(),
                 ::tolower);
  if (kTraitPhysicalLower == trait_lower) {
    return CameraTrait::PHYSICAL;
  }

  if (kTraitLogicalLower == trait_lower) {
    return CameraTrait::LOGICAL;
  }

  return CameraTrait::NONE;
}

std::unique_ptr<Camera> ParseFields(const Deserializer& deserializer) {
  string trait_str;
  deserializer.ParseString(DynamicDepthConst::Camera(), kTrait, &trait_str);
  CameraTrait trait = StringToTrait(trait_str);

  std::unique_ptr<Image> image = Image::FromDeserializer(deserializer);
  if (image == nullptr) {
    LOG(ERROR) << "An image must be present in a Camera, but none was found";
    return nullptr;
  }

  std::unique_ptr<LightEstimate> light_estimate =
      LightEstimate::FromDeserializer(deserializer);

  std::unique_ptr<Pose> pose =
      Pose::FromDeserializer(deserializer, DynamicDepthConst::Camera());

  std::unique_ptr<DepthMap> depth_map =
      DepthMap::FromDeserializer(deserializer);

  std::unique_ptr<ImagingModel> imaging_model =
      ImagingModel::FromDeserializer(deserializer);

  std::unique_ptr<PointCloud> point_cloud =
      PointCloud::FromDeserializer(deserializer);

  std::unique_ptr<VendorInfo> vendor_info =
      VendorInfo::FromDeserializer(deserializer, DynamicDepthConst::Camera());

  std::unique_ptr<AppInfo> app_info =
      AppInfo::FromDeserializer(deserializer, DynamicDepthConst::Camera());

  std::unique_ptr<CameraParams> params(new CameraParams(std::move(image)));
  params->depth_map = std::move(depth_map);
  params->light_estimate = std::move(light_estimate);
  params->pose = std::move(pose);
  params->imaging_model = std::move(imaging_model);
  params->point_cloud = std::move(point_cloud);
  params->vendor_info = std::move(vendor_info);
  params->app_info = std::move(app_info);
  params->trait = trait;
  return Camera::FromData(std::move(params));
}

}  // namespace

// Private constructor.
Camera::Camera(std::unique_ptr<CameraParams> params) {
  params_ = std::move(params);
}

// Public methods.
void Camera::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Camera(), kNamespaceHref);
  if (params_->image) {
    params_->image->GetNamespaces(ns_name_href_map);
  }
  if (params_->light_estimate) {
    params_->light_estimate->GetNamespaces(ns_name_href_map);
  }
  if (params_->pose) {
    params_->pose->GetNamespaces(ns_name_href_map);
  }
  if (params_->depth_map) {
    params_->depth_map->GetNamespaces(ns_name_href_map);
  }
  if (params_->imaging_model) {
    params_->imaging_model->GetNamespaces(ns_name_href_map);
  }
  if (params_->point_cloud) {
    params_->point_cloud->GetNamespaces(ns_name_href_map);
  }
  if (params_->vendor_info) {
    params_->vendor_info->GetNamespaces(ns_name_href_map);
  }
  if (params_->app_info) {
    params_->app_info->GetNamespaces(ns_name_href_map);
  }
}

std::unique_ptr<Camera> Camera::FromDataForCamera0(
    std::unique_ptr<CameraParams> params,
    std::vector<std::unique_ptr<Item>>* items) {
  if (params->image == nullptr) {
    params->image = Image::FromDataForPrimaryImage(kImageJpegMime, items);
  }
  return std::unique_ptr<Camera>(new Camera(std::move(params)));  // NOLINT
}

std::unique_ptr<Camera> Camera::FromData(std::unique_ptr<CameraParams> params) {
  if (params->image == nullptr) {
    LOG(ERROR) << "Camera must have an image eleemnt";
    return nullptr;
  }

  return std::unique_ptr<Camera>(new Camera(std::move(params)));  // NOLINT
}

std::unique_ptr<Camera> Camera::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Camera()),
          DynamicDepthConst::Camera());
  if (deserializer == nullptr) {
    return nullptr;
  }

  return ParseFields(*deserializer);
}

const Image* Camera::GetImage() const { return params_->image.get(); }

const LightEstimate* Camera::GetLightEstimate() const {
  return params_->light_estimate.get();
}

const Pose* Camera::GetPose() const { return params_->pose.get(); }

const DepthMap* Camera::GetDepthMap() const { return params_->depth_map.get(); }

const ImagingModel* Camera::GetImagingModel() const {
  return params_->imaging_model.get();
}

const PointCloud* Camera::GetPointCloud() const {
  return params_->point_cloud.get();
}

const VendorInfo* Camera::GetVendorInfo() const {
  return params_->vendor_info.get();
}

const AppInfo* Camera::GetAppInfo() const { return params_->app_info.get(); }

CameraTrait Camera::GetTrait() const { return params_->trait; }

bool Camera::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (params_->trait != CameraTrait::NONE) {
    string trait_name = TraitToString(params_->trait);
    serializer->WriteProperty(DynamicDepthConst::Camera(), kTrait, trait_name);
  }

  // Error checking has already been done at instantiation time.
  if (params_->image != nullptr) {
    std::unique_ptr<Serializer> image_serializer = serializer->CreateSerializer(
        DynamicDepthConst::Namespace(DynamicDepthConst::Image()),
        DynamicDepthConst::Image());
    if (!params_->image->Serialize(image_serializer.get())) {
      LOG(WARNING) << "Could not serialize Image";
    }
  }

  if (params_->depth_map != nullptr) {
    std::unique_ptr<Serializer> depth_map_serializer =
        serializer->CreateSerializer(DynamicDepthConst::Camera(),
                                     DynamicDepthConst::DepthMap());
    if (!params_->depth_map->Serialize(depth_map_serializer.get())) {
      LOG(WARNING) << "Could not serializer Depth Map";
    }
  }

  if (params_->light_estimate != nullptr) {
    std::unique_ptr<Serializer> light_estimate_serializer =
        serializer->CreateSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::LightEstimate()),
            DynamicDepthConst::LightEstimate());
    if (!params_->light_estimate->Serialize(light_estimate_serializer.get())) {
      LOG(WARNING) << "Could not serialize LightEstimate";
    }
  }

  if (params_->pose != nullptr) {
    std::unique_ptr<Serializer> pose_serializer = serializer->CreateSerializer(
        DynamicDepthConst::Camera(), DynamicDepthConst::Pose());
    if (!params_->pose->Serialize(pose_serializer.get())) {
      LOG(WARNING) << "Could not serialize Pose";
    }
  }

  if (params_->imaging_model != nullptr) {
    std::unique_ptr<Serializer> imaging_model_serializer =
        serializer->CreateSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::ImagingModel()),
            DynamicDepthConst::ImagingModel());
    if (!params_->imaging_model->Serialize(imaging_model_serializer.get())) {
      LOG(WARNING) << "Could not serialize ImagingModel";
    }
  }

  if (params_->point_cloud != nullptr) {
    std::unique_ptr<Serializer> point_cloud_serializer =
        serializer->CreateSerializer(DynamicDepthConst::Camera(),
                                     DynamicDepthConst::PointCloud());
    if (!params_->point_cloud->Serialize(point_cloud_serializer.get())) {
      LOG(WARNING) << "Could not serialize PointCloud";
    }
  }

  if (params_->vendor_info != nullptr) {
    std::unique_ptr<Serializer> vendor_info_serializer =
        serializer->CreateSerializer(DynamicDepthConst::Camera(),
                                     DynamicDepthConst::VendorInfo());
    if (!params_->vendor_info->Serialize(vendor_info_serializer.get())) {
      LOG(WARNING) << "Could not serialize VendorInfo";
    }
  }

  if (params_->app_info != nullptr) {
    std::unique_ptr<Serializer> app_info_serializer =
        serializer->CreateSerializer(DynamicDepthConst::Camera(),
                                     DynamicDepthConst::AppInfo());
    if (!params_->app_info->Serialize(app_info_serializer.get())) {
      LOG(WARNING) << "Could not serialize AppInfo";
    }
  }

  return true;
}

}  // namespace dynamic_depth
