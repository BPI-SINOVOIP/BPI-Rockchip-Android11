#include "dynamic_depth/plane.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"

using ::dynamic_depth::xmpmeta::EncodeFloatArrayBase64;
using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

constexpr char kBoundary[] = "Boundary";
constexpr char kBoundaryVertexCount[] = "BoundaryVertexCount";
constexpr char kExtentX[] = "ExtentX";
constexpr char kExtentZ[] = "ExtentZ";

constexpr char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/plane/";

}  // namespace

// Private constructor.
Plane::Plane() {}

// Public methods.
void Plane::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Plane(), kNamespaceHref);

  if (pose_ != nullptr) {
    pose_->GetNamespaces(ns_name_href_map);
  }
}

std::unique_ptr<Plane> Plane::FromData(std::unique_ptr<Pose> pose,
                                       const std::vector<float>& boundary,
                                       const double extent_x,
                                       const double extent_z) {
  if (pose == nullptr) {
    LOG(ERROR) << "The Plane's pose must be provided";
    return nullptr;
  }

  if (boundary.size() % 2 != 0) {
    LOG(ERROR) << "Number of vertices in the boundary polygon must be 2-tuples";
    return nullptr;
  }

  std::unique_ptr<Plane> plane(std::unique_ptr<Plane>(new Plane()));  // NOLINT
  plane->pose_ = std::move(pose);
  plane->boundary_vertex_count_ = boundary.size() / 2;
  if (!boundary.empty()) {
    plane->boundary_ = boundary;
  }

  plane->extent_x_ = extent_x;
  plane->extent_z_ = extent_z;
  return plane;
}

std::unique_ptr<Plane> Plane::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Plane()),
          DynamicDepthConst::Plane());
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<Plane> plane(std::unique_ptr<Plane>(new Plane()));  // NOLINT
  if (!plane->ParsePlaneFields(*deserializer)) {
    return nullptr;
  }

  return plane;
}

const Pose* Plane::GetPose() const { return pose_.get(); }

const std::vector<float>& Plane::GetBoundary() const { return boundary_; }

int Plane::GetBoundaryVertexCount() const { return boundary_vertex_count_; }

const double Plane::GetExtentX() const { return extent_x_; }

const double Plane::GetExtentZ() const { return extent_z_; }

bool Plane::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (pose_ == nullptr) {
    LOG(ERROR) << "Plane's pose must be present, not serializing";
    return false;
  }

  if (!serializer->WriteProperty(
          DynamicDepthConst::Plane(), kBoundaryVertexCount,
          ::dynamic_depth::strings::SimpleItoa(boundary_vertex_count_))) {
    return false;
  }
  if (!boundary_.empty()) {
    string base64_encoded_boundary;
    if (!EncodeFloatArrayBase64(boundary_, &base64_encoded_boundary)) {
      LOG(ERROR) << "Boundary polygon encoding failed.";
      return false;
    }

    if (!serializer->WriteProperty(DynamicDepthConst::Plane(), kBoundary,
                                   base64_encoded_boundary)) {
      return false;
    }
  }

  if (!serializer->WriteProperty(DynamicDepthConst::Plane(), kExtentX,
                                 std::to_string(extent_x_))) {
    return false;
  }

  if (!serializer->WriteProperty(DynamicDepthConst::Plane(), kExtentZ,
                                 std::to_string(extent_z_))) {
    return false;
  }

  std::unique_ptr<Serializer> pose_serializer = serializer->CreateSerializer(
      DynamicDepthConst::Plane(), DynamicDepthConst::Pose());
  return pose_->Serialize(pose_serializer.get());
}

// Private methods.
bool Plane::ParsePlaneFields(const Deserializer& deserializer) {
  std::unique_ptr<Pose> pose =
      Pose::FromDeserializer(deserializer, DynamicDepthConst::Plane());
  if (pose == nullptr) {
    LOG(ERROR) << "Plane's pose could not be parsed, stopping deserialization";
    return false;
  }

  // The BoundaryVertexCount field is required only if the Boundary field is
  // populated.
  std::vector<float> boundary;
  int boundary_vertex_count = 0;
  if (deserializer.ParseFloatArrayBase64(DynamicDepthConst::Plane(), kBoundary,
                                         &boundary)) {
    if (!deserializer.ParseInt(DynamicDepthConst::Plane(), kBoundaryVertexCount,
                               &boundary_vertex_count)) {
      return false;
    }
  }

  double extent_x = -1;
  deserializer.ParseDouble(DynamicDepthConst::Plane(), kExtentX, &extent_x);

  double extent_z = -1;
  deserializer.ParseDouble(DynamicDepthConst::Plane(), kExtentZ, &extent_z);

  pose_ = std::move(pose);
  boundary_ = boundary;
  boundary_vertex_count_ = boundary_vertex_count;
  extent_x_ = extent_x;
  extent_z_ = extent_z;

  return true;
}

}  // namespace dynamic_depth
