#ifndef DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_CONST_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_CONST_H_  // NOLINT

#include <array>
#include <map>
#include <string>
#include <vector>

namespace dynamic_depth {

struct DynamicDepthConst {
  // Dynamic Depth element names.
  static const char* AppInfo();
  static const char* Camera();
  static const char* DepthMap();
  static const char* Device();
  static const char* EarthPose();
  static const char* ImagingModel();
  static const char* LightEstimate();
  static const char* Image();
  static const char* Item();
  static const char* Plane();
  static const char* PointCloud();
  static const char* Pose();
  static const char* Profile();
  static const char* VendorInfo();

  // Dynamic Depth type names (not shared with elements).
  static const char* Cameras();
  static const char* Container();
  static const char* Planes();
  static const char* Profiles();

  // Maps elements to the names of their XML namespaces.
  static const std::string Namespace(const std::string& node_name);

  // Distortion type names.
  // LINT.IfChange
  static constexpr int kNumDistortionTypes = 4;
  static constexpr std::array<const char*, kNumDistortionTypes>
      kDistortionTypeNames = {
          {"None", "BrownsTwoParams", "BrownsThreeParams", "BrownsFiveParams"}};
  // LINT.ThenChange(//depot/google3/photos/editing/formats/dynamic_depth/\
  //                 internal/dynamic_depth/distortion_type.h)
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_CONST_H_  // NOLINT
