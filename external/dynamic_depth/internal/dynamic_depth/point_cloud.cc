#include "dynamic_depth/point_cloud.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"
#include "xmpmeta/xml/utils.h"

using ::dynamic_depth::xmpmeta::EncodeFloatArrayBase64;
using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kPropertyPrefix[] = "PointCloud";
const char kPointCount[] = "PointCount";
const char kPoints[] = "Points";
const char kMetric[] = "Metric";

const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/pointcloud/";

}  // namespace

// Private constructor.
PointCloud::PointCloud() : metric_(false) {}

// Public methods.
void PointCloud::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->insert(
      std::pair<string, string>(kPropertyPrefix, kNamespaceHref));
}

std::unique_ptr<PointCloud> PointCloud::FromData(
    const std::vector<float>& points, bool metric) {
  if (points.empty()) {
    LOG(ERROR) << "No point data given";
    return nullptr;
  }

  if (points.size() % 4 != 0) {
    LOG(ERROR) << "Points must be (x, y, z, c) tuples, so the size must be "
               << "divisible by 4, got " << points.size();
    return nullptr;
  }

  std::unique_ptr<PointCloud> point_cloud(new PointCloud());
  point_cloud->points_ = points;
  point_cloud->metric_ = metric;
  return point_cloud;
}

std::unique_ptr<PointCloud> PointCloud::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(kPropertyPrefix), kPropertyPrefix);
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<PointCloud> point_cloud(new PointCloud());
  if (!point_cloud->ParseFields(*deserializer)) {
    return nullptr;
  }
  return point_cloud;
}

int PointCloud::GetPointCount() const {
  return static_cast<int>(points_.size() / 4);
}

const std::vector<float>& PointCloud::GetPoints() const { return points_; }
bool PointCloud::GetMetric() const { return metric_; }

bool PointCloud::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (points_.empty()) {
    LOG(ERROR) << "No points in the PointCloud to serialize";
    return false;
  }

  // No error checking (e.g. points_.size() % 4 == 0), because serialization
  // shouldn't be blocked by this.
  string base64_encoded_points;
  if (!EncodeFloatArrayBase64(points_, &base64_encoded_points)) {
    LOG(ERROR) << "Points encoding failed";
    return false;
  }

  // Write required fields.
  int point_count = static_cast<int>(points_.size() / 4);
  if (!serializer->WriteProperty(
          DynamicDepthConst::PointCloud(), kPointCount,
          ::dynamic_depth::strings::SimpleItoa(point_count))) {
    return false;
  }

  if (!serializer->WriteProperty(DynamicDepthConst::PointCloud(), kPoints,
                                 base64_encoded_points)) {
    return false;
  }

  // Write optional fields.
  serializer->WriteBoolProperty(DynamicDepthConst::PointCloud(), kMetric,
                                metric_);
  return true;
}

// Private methods.
bool PointCloud::ParseFields(const Deserializer& deserializer) {
  // Required fields.
  std::vector<float> points;
  if (!deserializer.ParseFloatArrayBase64(DynamicDepthConst::PointCloud(),
                                          kPoints, &points)) {
    return false;
  }

  int point_count;
  if (!deserializer.ParseInt(DynamicDepthConst::PointCloud(), kPointCount,
                             &point_count)) {
    return false;
  }

  if (points.size() % 4 != 0) {
    LOG(ERROR) << "Parsed " << points.size() << " values but expected the size "
               << "to be divisible by 4 for (x, y, z, c) tuple representation";
    return false;
  }

  int parsed_points_count = static_cast<int>(points.size() / 4);
  if (parsed_points_count != point_count) {
    LOG(ERROR) << "Parsed PointCount = " << point_count << " but "
               << parsed_points_count << " points were found";
    return false;
  }

  // We know that point_count == points.size() now.
  points_ = points;

  // Optional fields.
  bool metric;
  if (!deserializer.ParseBoolean(DynamicDepthConst::PointCloud(), kMetric,
                                 &metric)) {
    // Set it to the default value.
    metric_ = false;
  } else {
    metric_ = metric;
  }

  return true;
}

}  // namespace dynamic_depth
