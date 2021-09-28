#include "dynamic_depth/pose.h"

#include <math.h>

#include <cmath>

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kPositionX[] = "PositionX";
const char kPositionY[] = "PositionY";
const char kPositionZ[] = "PositionZ";
const char kRotationX[] = "RotationX";
const char kRotationY[] = "RotationY";
const char kRotationZ[] = "RotationZ";
const char kRotationW[] = "RotationW";
const char kTimestamp[] = "Timestamp";
const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/pose/";

const std::vector<float> NormalizeQuaternion(const std::vector<float>& quat) {
  if (quat.size() < 4) {
    return std::vector<float>();
  }
  float length = std::sqrt((quat[0] * quat[0]) + (quat[1] * quat[1]) +
                           (quat[2] * quat[2]) + (quat[3] * quat[3]));
  const std::vector<float> normalized = {quat[0] / length, quat[1] / length,
                                         quat[2] / length, quat[3] / length};
  return normalized;
}

}  // namespace

// Private constructor.
Pose::Pose() : timestamp_(-1) {}

// Public methods.
void Pose::GetNamespaces(std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Pose(), kNamespaceHref);
}

std::unique_ptr<Pose> Pose::FromData(const std::vector<float>& position,
                                     const std::vector<float>& orientation,
                                     const int64 timestamp) {
  if (position.empty() && orientation.empty()) {
    LOG(ERROR) << "Either position or orientation must be provided";
    return nullptr;
  }

  std::unique_ptr<Pose> pose(new Pose());
  if (position.size() >= 3) {
    pose->position_ = position;
  }

  if (orientation.size() >= 4) {
    pose->orientation_ = NormalizeQuaternion(orientation);
  }

  if (timestamp >= 0) {
    pose->timestamp_ = timestamp;
  }

  return pose;
}

std::unique_ptr<Pose> Pose::FromDeserializer(
    const Deserializer& parent_deserializer, const char* parent_namespace) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(parent_namespace,
                                             DynamicDepthConst::Pose());
  if (deserializer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<Pose> pose(new Pose());
  if (!pose->ParsePoseFields(*deserializer)) {
    return nullptr;
  }
  return pose;
}

bool Pose::HasPosition() const { return position_.size() == 3; }
bool Pose::HasOrientation() const { return orientation_.size() == 4; }

const std::vector<float>& Pose::GetPosition() const { return position_; }

const std::vector<float>& Pose::GetOrientation() const { return orientation_; }

int64 Pose::GetTimestamp() const { return timestamp_; }

bool Pose::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (!HasPosition() && !HasOrientation()) {
    LOG(ERROR) << "Camera pose has neither position nor orientation";
    return false;
  }

  bool success = true;
  if (position_.size() == 3) {
    success &= serializer->WriteProperty(DynamicDepthConst::Pose(), kPositionX,
                                         std::to_string(position_[0])) &&
               serializer->WriteProperty(DynamicDepthConst::Pose(), kPositionY,
                                         std::to_string(position_[1])) &&
               serializer->WriteProperty(DynamicDepthConst::Pose(), kPositionZ,
                                         std::to_string(position_[2]));
  }

  if (orientation_.size() == 4) {
    success &= serializer->WriteProperty(DynamicDepthConst::Pose(), kRotationX,
                                         std::to_string(orientation_[0])) &&
               serializer->WriteProperty(DynamicDepthConst::Pose(), kRotationY,
                                         std::to_string(orientation_[1])) &&
               serializer->WriteProperty(DynamicDepthConst::Pose(), kRotationZ,
                                         std::to_string(orientation_[2])) &&
               serializer->WriteProperty(DynamicDepthConst::Pose(), kRotationW,
                                         std::to_string(orientation_[3]));
  }

  if (timestamp_ >= 0) {
    serializer->WriteProperty(DynamicDepthConst::Pose(), kTimestamp,
                              std::to_string(timestamp_));
  }

  return success;
}

// Private methods.
bool Pose::ParsePoseFields(const Deserializer& deserializer) {
  float x, y, z, w;
  const string& prefix = DynamicDepthConst::Pose();
  // If a position field is present, the rest must be as well.
  if (deserializer.ParseFloat(prefix, kPositionX, &x)) {
    if (!deserializer.ParseFloat(prefix, kPositionY, &y)) {
      return false;
    }
    if (!deserializer.ParseFloat(prefix, kPositionZ, &z)) {
      return false;
    }
    position_ = {x, y, z};
  }

  // Same for orientation.
  if (deserializer.ParseFloat(prefix, kRotationX, &x)) {
    if (!deserializer.ParseFloat(prefix, kRotationY, &y)) {
      return false;
    }
    if (!deserializer.ParseFloat(prefix, kRotationZ, &z)) {
      return false;
    }
    if (!deserializer.ParseFloat(prefix, kRotationW, &w)) {
      return false;
    }
    orientation_ = std::vector<float>({x, y, z, w});
  }

  if (position_.size() < 3 && orientation_.size() < 4) {
    return false;
  }

  deserializer.ParseLong(prefix, kTimestamp, &timestamp_);
  return true;
}

}  // namespace dynamic_depth
