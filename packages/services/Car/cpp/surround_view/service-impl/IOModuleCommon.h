/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SURROUND_VIEW_SERVICE_IMPL_IOMODULECOMMON_H_
#define SURROUND_VIEW_SERVICE_IMPL_IOMODULECOMMON_H_

#include <string>

#include "core_lib.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Struct for camera related configurations.
// Note: Does not include camera intrinsics and extrinsics, these are specified in EVS metadata.
struct CameraConfig {
    // Id of logical group containing surronnd view cameras.
    std::string evsGroupId;

    // List of evs camera Ids  in order: front, right, rear, left.
    std::vector<std::string> evsCameraIds;

    // In order: front, right, rear, left.
    std::vector<std::string> maskFilenames;
};

struct SvConfig2d {
    // Bool flag for surround view 2d.
    bool sv2dEnabled;

    // Surround view 2d params.
    android_auto::surround_view::SurroundView2dParams sv2dParams;

    // Car model bounding box for 2d surround view.
    // To be moved into sv 2d params.
    android_auto::surround_view::BoundingBox carBoundingBox;
};

struct SvConfig3d {
    // Bool flag for enabling/disabling surround view 3d.
    bool sv3dEnabled;

    // Bool flag for enabling/disabling animations.
    bool sv3dAnimationsEnabled;

    // Car model config file.
    std::string carModelConfigFile;

    // Car model obj file.
    std::string carModelObjFile;

    // Surround view 3d params.
    android_auto::surround_view::SurroundView3dParams sv3dParams;
};

// Main struct in which surround view config is parsed into.
struct SurroundViewConfig {
    // Version info.
    std::string version;

    // Camera config.
    CameraConfig cameraConfig;

    // Surround view 2d config.
    SvConfig2d sv2dConfig;

    // Surround view 3d config.
    SvConfig3d sv3dConfig;
};

struct Range {
    // Range start.
    // Start value may be greater than end value.
    float start;

    // Range end.
    float end;
};

// Rotation axis
struct RotationAxis {
    // Unit axis direction vector.
    std::array<float, 3> axisVector;

    // Rotate about this point.
    std::array<float, 3> rotationPoint;
};

enum AnimationType {
    // Rotate a part about an axis from a start to end angle.
    ROTATION_ANGLE = 0,

    // Continuously rotate a part about an axis by a specified angular speed.
    ROTATION_SPEED = 1,

    // Linearly translates a part from one point to another.
    TRANSLATION = 2,

    // Switch to another texture once.
    SWITCH_TEXTURE_ONCE = 3,

    // Adjust the brightness of the texture once.
    ADJUST_GAMMA_ONCE = 4,

    // Repeatedly toggle between two textures.
    SWITCH_TEXTURE_REPEAT = 5,

    // Repeatedly toggle between two gamma values.
    ADJUST_GAMMA_REPEAT = 6,
};

// Rotation operation
struct RotationOp {
    // VHAL signal to trigger operation.
    uint64_t vhalProperty;

    // Rotation operation type.
    AnimationType type;

    // Rotation axis.
    RotationAxis axis;

    // Default rotation (angle/speed) value.
    // It is used for default rotation when the signal is on while vhal_range is
    // not provided.
    float defaultRotationValue;

    // Default animation time elapsed to finish the rotation operation.
    // It is ignored if VHAL provides continuous signal value.
    float animationTime;

    // physical rotation range with start mapped to vhal_range start and
    // end mapped to vhal_range end.
    Range rotationRange;

    // VHAL signal range.
    // Un-supported types: STRING, BYTES and VEC
    // Refer:  hardware/interfaces/automotive/vehicle/2.0/types.hal
    // VehiclePropertyType
    Range vhalRange;
};

// Translation operation.
struct TranslationOp {
    // VHAL signal to trigger operation.
    uint64_t vhalProperty;

    // Translation operation type.
    AnimationType type;

    // Unit direction vector.
    std::array<float, 3> direction;

    // Default translation value.
    // It is used for default translation when the signal is on while vhal_range
    // is not provided.
    float defaultTranslationValue;

    // Default animation time elapsed to finish the texture operation.
    // It is ignored if VHAL provides continuous signal value.
    float animationTime;

    // Physical translation range with start mapped to vhal_range start and
    // end mapped to vhal_range end.
    Range translationRange;

    // VHAL signal range.
    // Un-supported types: STRING, BYTES and VEC
    // Refer:  hardware/interfaces/automotive/vehicle/2.0/types.hal
    // VehiclePropertyType
    Range vhalRange;
};

// Texture operation.
struct TextureOp {
    // VHAL signal to trigger operation.
    uint64_t vhalProperty;

    // Texture operation type.
    AnimationType type;

    // Default texture id.
    // It is used as default texture when the signal is on while vhal_range is
    // not provided.
    std::string defaultTexture;

    // Default animation time elapsed to finish the texture operation.
    // Unit is milliseconds.
    // If the animation time is specified, the vhal_property is assumed to be
    // on/off type.
    // It is ignored if it is equal or less than zero and vhal_property is
    // assumed to provide continuous value.
    int animationTime;

    // texture range mapped to texture_ids[i].first.
    Range textureRange;

    // VHAL signal range.
    // Un-supported types: STRING, BYTES and VEC
    // Refer:  hardware/interfaces/automotive/vehicle/2.0/types.hal
    // VehiclePropertyType
    Range vhalRange;

    // Texture ids for switching textures.
    // Applicable for animation types: kSwitchTextureOnce and
    // kSwitchTextureRepeated
    // 0 - n-1
    std::vector<std::pair<float, std::string>> textureIds;
};

// Gamma operation.
struct GammaOp {
    // VHAL signal to trigger operation.
    uint64_t vhalProperty;

    // Texture operation type.
    // Applicable for animation types: kAdjustGammaOnce and kAdjustGammaRepeat.
    AnimationType type;

    // Default animation time elapsed to finish the gamma operation.
    // Unit is milliseconds.
    // If the animation time is specified, the vhal_property is assumed to be
    // on/off type.
    // It is ignored if it is equal or less than zero and vhal_property is
    // assumed to provide continuous value.
    int animationTime;

    // Gamma range with start mapped to vhal_range start and
    // end mapped to vhal_range end.
    Range gammaRange;

    // VHAL signal range.
    // Un-supported types: STRING, BYTES and VEC
    // Refer:  hardware/interfaces/automotive/vehicle/2.0/types.hal
    // VehiclePropertyType
    Range vhalRange;
};

// Animation info of a car part
struct AnimationInfo {
    // Car animation part id(name). It is a unique id.
    std::string partId;

    // Car part parent name.
    std::string parentId;

    // List of child Ids.
    std::vector<std::string> childIds;

    // Car part pose w.r.t parent's coordinate.
    android_auto::surround_view::Mat4x4 pose;

    // VHAL priority from high [0] to low [n-1]. Only VHALs specified in the
    // vector have priority.
    std::vector<uint64_t> vhalPriority;

    // TODO(b/158245554): simplify xxOpsMap data structs.
    // Map of gamma operations. Key value is VHAL property.
    std::map<uint64_t, std::vector<GammaOp>> gammaOpsMap;

    // Map of texture operations. Key value is VHAL property.
    std::map<uint64_t, std::vector<TextureOp>> textureOpsMap;

    // Map of rotation operations. Key value is VHAL property.
    // Multiple rotation ops are supported and will be simultaneously animated in
    // order if their rotation axis are different and rotation points are the
    // same.
    std::map<uint64_t, std::vector<RotationOp>> rotationOpsMap;

    // Map of translation operations. Key value is VHAL property.
    std::map<uint64_t, std::vector<TranslationOp>> translationOpsMap;
};

// Main struct in which surround view car model config is parsed into.
struct AnimationConfig {
    std::string version;

    std::vector<AnimationInfo> animations;
};

// Car model.
struct CarModel {
    // Car model parts map.
    std::map<std::string, android_auto::surround_view::CarPart> partsMap;

    // Car testures map.
    std::map<std::string, android_auto::surround_view::CarTexture> texturesMap;
};

struct CarModelConfig {
    CarModel carModel;

    AnimationConfig animationConfig;
};

struct IOModuleConfig {
    // Camera config.
    CameraConfig cameraConfig;

    // Surround view 2d config.
    SvConfig2d sv2dConfig;

    // Surround view 3d config.
    SvConfig3d sv3dConfig;

    // Car model config.
    CarModelConfig carModelConfig;
};

enum IOStatus : uint8_t {
    // OK ststus. ALL fields read and parsed.
    OK = 0,

    // Error status. Cannot read the config file (config file missing or not
    // accessible)
    ERROR_READ_CONFIG_FILE = 1,

    // Error ststus. Config file format doesn't match.
    ERROR_CONFIG_FILE_FORMAT = 2,

    // Warning status. Read car model (obj, mtl) error. Either the files are
    // missing or wrong format.
    ERROR_READ_CAR_MODEL = 3,

    // Warning status. Read animation config file error. Either the file is
    // missing or wrong format.
    ERROR_READ_ANIMATION = 4,
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_IOMODULECOMMON_H_
