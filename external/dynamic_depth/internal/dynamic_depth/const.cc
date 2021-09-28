#include "dynamic_depth/const.h"

#include "android-base/logging.h"
#include "base/port.h"

namespace dynamic_depth {
namespace {

// Element names.
constexpr char kAppInfo[] = "AppInfo";
constexpr char kCamera[] = "Camera";
constexpr char kDepthMap[] = "DepthMap";
constexpr char kDevice[] = "Device";
constexpr char kEarthPose[] = "EarthPose";
constexpr char kImagingModel[] = "ImagingModel";
constexpr char kImage[] = "Image";
constexpr char kItem[] = "Item";
constexpr char kLightEstimate[] = "LightEstimate";
constexpr char kPlane[] = "Plane";
constexpr char kPointCloud[] = "PointCloud";
constexpr char kPose[] = "Pose";
constexpr char kProfile[] = "Profile";
constexpr char kVendorInfo[] = "VendorInfo";

// Type names.
constexpr char kCameras[] = "Cameras";
constexpr char kContainer[] = "Container";
constexpr char kPlanes[] = "Planes";
constexpr char kProfiles[] = "Profiles";

}  // namespace

// Redeclare static constexpr variables.
// https://stackoverflow.com/questions/8016780/
//     undefined-reference-to-static-constexpr-char
constexpr std::array<const char*, DynamicDepthConst::kNumDistortionTypes>
    DynamicDepthConst::kDistortionTypeNames;

// Dynamic Depth element names.
const char* DynamicDepthConst::AppInfo() { return kAppInfo; }

const char* DynamicDepthConst::Camera() { return kCamera; }

const char* DynamicDepthConst::DepthMap() { return kDepthMap; }

const char* DynamicDepthConst::Device() { return kDevice; }

const char* DynamicDepthConst::EarthPose() { return kEarthPose; }

const char* DynamicDepthConst::ImagingModel() { return kImagingModel; }

const char* DynamicDepthConst::Image() { return kImage; }

const char* DynamicDepthConst::Item() { return kItem; }

const char* DynamicDepthConst::LightEstimate() { return kLightEstimate; }

const char* DynamicDepthConst::Plane() { return kPlane; }

const char* DynamicDepthConst::PointCloud() { return kPointCloud; }

const char* DynamicDepthConst::Pose() { return kPose; }

const char* DynamicDepthConst::Profile() { return kProfile; }

const char* DynamicDepthConst::VendorInfo() { return kVendorInfo; }

// Dynamic Depth type names.
const char* DynamicDepthConst::Cameras() { return kCameras; }

const char* DynamicDepthConst::Container() { return kContainer; }

const char* DynamicDepthConst::Planes() { return kPlanes; }

const char* DynamicDepthConst::Profiles() { return kProfiles; }

// Returns the namespace to which the given Dynamic Depth element or type
// belongs. AppInfo and VendorInfo are not included because they can belong to
// either the Device or Camera elements.
const std::string DynamicDepthConst::Namespace(const std::string& node_name) {
  if (node_name == kPose) {
    LOG(WARNING) << kPose << " maps to " << kDevice << ", " << kCamera
                 << ", and " << kPlane << "; should be manually chosen. "
                 << "Returning empty";
    return "";
  }

  // Elements.
  if (node_name == kImagingModel || node_name == kImage ||
      node_name == kDepthMap || node_name == kPointCloud ||
      node_name == kLightEstimate) {
    return kCamera;
  }

  if (node_name == kItem) {
    return kContainer;
  }

  if (node_name == kCamera || node_name == kEarthPose ||
      node_name == kProfile || node_name == kPlane) {
    return kDevice;
  }

  // Types.
  if (node_name == kCameras || node_name == kContainer ||
      node_name == kPlanes || node_name == kProfiles) {
    return kDevice;
  }

  return "";
}
}  // namespace dynamic_depth
