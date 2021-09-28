#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERA_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERA_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "dynamic_depth/app_info.h"
#include "dynamic_depth/depth_map.h"
#include "dynamic_depth/element.h"
#include "dynamic_depth/image.h"
#include "dynamic_depth/imaging_model.h"
#include "dynamic_depth/light_estimate.h"
#include "dynamic_depth/point_cloud.h"
#include "dynamic_depth/pose.h"
#include "dynamic_depth/vendor_info.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// The camera trait is serialized only if it is one of PHYSICAL or LOGICAL.
// NONE signifies an undefined trait.
enum CameraTrait { NONE = 0, PHYSICAL = 1, LOGICAL = 2 };

struct CameraParams {
  // The Image must be present.
  std::unique_ptr<Image> image;

  // Optional elements.
  std::unique_ptr<DepthMap> depth_map;
  std::unique_ptr<LightEstimate> light_estimate;
  std::unique_ptr<Pose> pose;
  std::unique_ptr<ImagingModel> imaging_model;
  std::unique_ptr<PointCloud> point_cloud;
  std::unique_ptr<VendorInfo> vendor_info;
  std::unique_ptr<AppInfo> app_info;
  CameraTrait trait;

  explicit CameraParams(std::unique_ptr<Image> img)
      : depth_map(nullptr),
        light_estimate(nullptr),
        pose(nullptr),
        imaging_model(nullptr),
        point_cloud(nullptr),
        vendor_info(nullptr),
        app_info(nullptr),
        trait(CameraTrait::PHYSICAL) {
    image = std::move(img);
  }

  inline bool operator==(const CameraParams& other) const {
    return image.get() == other.image.get() &&
           light_estimate.get() == other.light_estimate.get() &&
           pose.get() == other.pose.get() &&
           depth_map.get() == other.depth_map.get() &&
           imaging_model.get() == other.imaging_model.get() &&
           point_cloud.get() == other.point_cloud.get() &&
           vendor_info.get() == other.vendor_info.get() &&
           app_info.get() == other.app_info.get();
  }

  inline bool operator!=(const CameraParams& other) const {
    return !(*this == other);
  }
};

// Implements the Camera element from the Dynamic Depth specification, with
// serialization and deserialization.
class Camera : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates a Camera from the given objects in params.
  // Aside from the Image element, all other elements are optional and can be
  // null.
  static std::unique_ptr<Camera> FromData(std::unique_ptr<CameraParams> params);

  // Same as above, but allows the Image element to be null. This should be used
  // only for Camera 0, since the lack of an Image element indicates that it
  // refers to the JPEG container image. An Item element is generated for the
  // container image, and populated into items.
  // Currently supports only image/jpeg.
  // The items parameter may be null if params->image is not null.
  // TODO(miraleung): Add other mime types as args when needed.
  static std::unique_ptr<Camera> FromDataForCamera0(
      std::unique_ptr<CameraParams> params,
      std::vector<std::unique_ptr<Item>>* items);

  // Returns the deserialized Camera object, null if parsing fails.
  // Not sensitive to case when parsing a camera's trait.
  static std::unique_ptr<Camera> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Getters. Except for Imaeg (which should never be null), these will return
  // null if the corresponding fields are not present.
  // which should never be null.
  const Image* GetImage() const;
  const LightEstimate* GetLightEstimate() const;
  const Pose* GetPose() const;
  const DepthMap* GetDepthMap() const;
  const ImagingModel* GetImagingModel() const;
  const PointCloud* GetPointCloud() const;
  const VendorInfo* GetVendorInfo() const;
  const AppInfo* GetAppInfo() const;
  CameraTrait GetTrait() const;

  // Disallow copying.
  Camera(const Camera&) = delete;
  void operator=(const Camera&) = delete;

 private:
  explicit Camera(std::unique_ptr<CameraParams> params);

  std::unique_ptr<CameraParams> params_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CAMERA_H_  // NOLINT
