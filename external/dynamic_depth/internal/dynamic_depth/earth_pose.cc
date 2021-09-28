#include "dynamic_depth/earth_pose.h"

#include <math.h>

#include <cmath>

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kLatitude[] = "Latitude";
const char kLongitude[] = "Longitude";
const char kAltitude[] = "Altitude";
const char kRotationX[] = "RotationX";
const char kRotationY[] = "RotationY";
const char kRotationZ[] = "RotationZ";
const char kRotationW[] = "RotationW";
const char kTimestamp[] = "Timestamp";
const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/earthpose/";

const std::vector<float> NormalizeQuaternion(const std::vector<float>& quat) {
  if (quat.size() < 4) {
    return std::vector<float>();
  }
  float length = std::sqrt((quat[0] * quat[0]) + (quat[1] * quat[1]) +
                           (quat[2] * quat[2])) +
                 (quat[3] * quat[3]);
  const std::vector<float> normalized = {quat[0] / length, quat[1] / length,
                                         quat[2] / length, quat[3] / length};
  return normalized;
}

}  // namespace

// Private constructor.
EarthPose::EarthPose() : timestamp_(-1) {}

// Public methods.
void EarthPose::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::EarthPose(), kNamespaceHref);
}

std::unique_ptr<EarthPose> EarthPose::FromData(
    const std::vector<double>& position, const std::vector<float>& orientation,
    const int64 timestamp) {
  if (position.empty() && orientation.empty()) {
    LOG(ERROR) << "Either position or orientation must be provided";
    return nullptr;
  }

  std::unique_ptr<EarthPose> earth_pose(new EarthPose());
  if (position.size() >= 3) {
    earth_pose->position_ = position;
  }

  if (orientation.size() >= 4) {
    earth_pose->orientation_ = NormalizeQuaternion(orientation);
  }

  if (timestamp >= 0) {
    earth_pose->timestamp_ = timestamp;
  }

  return earth_pose;
}

std::unique_ptr<EarthPose> EarthPose::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::EarthPose()),
          DynamicDepthConst::EarthPose());
  if (deserializer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<EarthPose> earth_pose(new EarthPose());
  if (!earth_pose->ParseEarthPoseFields(*deserializer)) {
    return nullptr;
  }
  return earth_pose;
}

bool EarthPose::HasPosition() const { return position_.size() == 3; }
bool EarthPose::HasOrientation() const { return orientation_.size() == 4; }

const std::vector<double>& EarthPose::GetPosition() const { return position_; }

const std::vector<float>& EarthPose::GetOrientation() const {
  return orientation_;
}

int64 EarthPose::GetTimestamp() const { return timestamp_; }

bool EarthPose::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (!HasPosition() && !HasOrientation()) {
    LOG(ERROR) << "Device pose has neither position nor orientation";
    return false;
  }

  bool success = true;
  if (position_.size() == 3) {
    success &=
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kLatitude,
                                  std::to_string(position_[0])) &&
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kLongitude,
                                  std::to_string(position_[1])) &&
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kAltitude,
                                  std::to_string(position_[2]));
  }

  if (orientation_.size() == 4) {
    success &=
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kRotationX,
                                  std::to_string(orientation_[0])) &&
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kRotationY,
                                  std::to_string(orientation_[1])) &&
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kRotationZ,
                                  std::to_string(orientation_[2])) &&
        serializer->WriteProperty(DynamicDepthConst::EarthPose(), kRotationW,
                                  std::to_string(orientation_[3]));
  }

  if (timestamp_ >= 0) {
    serializer->WriteProperty(DynamicDepthConst::EarthPose(), kTimestamp,
                              std::to_string(timestamp_));
  }

  return success;
}

// Private methods.
bool EarthPose::ParseEarthPoseFields(const Deserializer& deserializer) {
  double lat, lon, alt;
  // If a position field is present, the rest must be as well.
  if (deserializer.ParseDouble(DynamicDepthConst::EarthPose(), kLatitude,
                               &lat)) {
    if (!deserializer.ParseDouble(DynamicDepthConst::EarthPose(), kLongitude,
                                  &lon)) {
      return false;
    }
    if (!deserializer.ParseDouble(DynamicDepthConst::EarthPose(), kAltitude,
                                  &alt)) {
      return false;
    }
    position_ = {lat, lon, alt};
  }

  // Same for orientation.
  float x, y, z, w;
  if (deserializer.ParseFloat(DynamicDepthConst::EarthPose(), kRotationX, &x)) {
    if (!deserializer.ParseFloat(DynamicDepthConst::EarthPose(), kRotationY,
                                 &y)) {
      return false;
    }
    if (!deserializer.ParseFloat(DynamicDepthConst::EarthPose(), kRotationZ,
                                 &z)) {
      return false;
    }
    if (!deserializer.ParseFloat(DynamicDepthConst::EarthPose(), kRotationW,
                                 &w)) {
      return false;
    }
    orientation_ = std::vector<float>({x, y, z, w});
  }

  if (position_.size() < 3 && orientation_.size() < 4) {
    return false;
  }

  deserializer.ParseLong(DynamicDepthConst::EarthPose(), kTimestamp,
                         &timestamp_);
  return true;
}

}  // namespace dynamic_depth
