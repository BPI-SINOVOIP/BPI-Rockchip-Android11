#include "dynamic_depth/depth_map.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "dynamic_depth/item.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"

using ::dynamic_depth::Item;
using ::dynamic_depth::xmpmeta::EncodeFloatArrayBase64;
using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {
constexpr const char* kNamespaceHref =
    "http://ns.google.com/photos/dd/1.0/depthmap/";

constexpr const char* kFormat = "Format";
constexpr const char* kNear = "Near";
constexpr const char* kFar = "Far";
constexpr const char* kUnits = "Units";
constexpr const char* kDepthUri = "DepthURI";
constexpr const char* kItemSemantic = "ItemSemantic";
constexpr const char* kConfidenceUri = "ConfidenceURI";
constexpr const char* kMeasureType = "MeasureType";
constexpr const char* kSoftware = "Software";
constexpr const char* kFocalTable = "FocalTable";
constexpr const char* kFocalTableEntryCount = "FocalTableEntryCount";

constexpr const char* kFormatRangeInverse = "RangeInverse";
constexpr const char* kFormatRangeLinear = "RangeLinear";
constexpr const char* kFormatRangeInverseLower = "rangeinverse";
constexpr const char* kFormatRangeLinearLower = "rangelinear";

constexpr const char* kUnitsMeters = "Meters";
constexpr const char* kUnitsDiopters = "Diopters";
constexpr const char* kUnitsNone = "None";
constexpr const char* kUnitsMetersLower = "meters";
constexpr const char* kUnitsDioptersLower = "diopters";

constexpr const char* kMeasureTypeOpticalAxis = "OpticalAxis";
constexpr const char* kMeasureTypeOpticRay = "OpticRay";
constexpr const char* kMeasureTypeOpticRayLower = "opticray";

constexpr const char* kItemSemanticDepth = "Depth";
constexpr const char* kItemSemanticSegmentation = "Segmentation";
constexpr const char* kItemSemanticSegmentationLower = "segmentation";

string ItemSemanticToString(DepthItemSemantic item_semantic) {
  switch (item_semantic) {
    case DepthItemSemantic::kDepth:
      return kItemSemanticDepth;
    case DepthItemSemantic::kSegmentation:
      return kItemSemanticSegmentation;
  }
}

DepthItemSemantic StringToItemSemantic(const string& semantic_str) {
  string semantic_str_lower = semantic_str;
  std::transform(semantic_str_lower.begin(), semantic_str_lower.end(),
                 semantic_str_lower.begin(), ::tolower);
  if (kItemSemanticSegmentationLower == semantic_str_lower) {
    return DepthItemSemantic::kSegmentation;
  }

  return DepthItemSemantic::kDepth;
}

string FormatToString(DepthFormat format) {
  switch (format) {
    case DepthFormat::kRangeInverse:
      return kFormatRangeInverse;
    case DepthFormat::kRangeLinear:
      return kFormatRangeLinear;
    case DepthFormat::kFormatNone:
      return "";
  }
}

// Case insensitive.
DepthFormat StringToFormat(const string& format_str) {
  string format_str_lower = format_str;
  std::transform(format_str_lower.begin(), format_str_lower.end(),
                 format_str_lower.begin(), ::tolower);
  if (kFormatRangeInverseLower == format_str_lower) {
    return DepthFormat::kRangeInverse;
  }

  if (kFormatRangeLinearLower == format_str_lower) {
    return DepthFormat::kRangeLinear;
  }

  return DepthFormat::kFormatNone;
}

string UnitsToString(DepthUnits units) {
  switch (units) {
    case DepthUnits::kMeters:
      return kUnitsMeters;
    case DepthUnits::kDiopters:
      return kUnitsDiopters;
    case DepthUnits::kUnitsNone:
      return kUnitsNone;
  }
}

DepthUnits StringToUnits(const string& units_str) {
  string units_str_lower = units_str;
  std::transform(units_str_lower.begin(), units_str_lower.end(),
                 units_str_lower.begin(), ::tolower);
  if (kUnitsMetersLower == units_str_lower) {
    return DepthUnits::kMeters;
  }

  if (kUnitsDioptersLower == units_str_lower) {
    return DepthUnits::kDiopters;
  }

  return DepthUnits::kUnitsNone;
}

string MeasureTypeToString(DepthMeasureType measure_type) {
  switch (measure_type) {
    case DepthMeasureType::kOpticRay:
      return kMeasureTypeOpticRay;
    case DepthMeasureType::kOpticalAxis:
      return kMeasureTypeOpticalAxis;
  }
}

DepthMeasureType StringToMeasureType(const string& measure_type_str) {
  string measure_type_str_lower = measure_type_str;
  std::transform(measure_type_str_lower.begin(), measure_type_str_lower.end(),
                 measure_type_str_lower.begin(), ::tolower);
  if (kMeasureTypeOpticRayLower == measure_type_str_lower) {
    return DepthMeasureType::kOpticRay;
  }

  return DepthMeasureType::kOpticalAxis;
}

}  // namespace

// Private constructor.
DepthMap::DepthMap(const DepthMapParams& params) : params_(params) {}

// Private parser.
std::unique_ptr<DepthMap> DepthMap::ParseFields(
    const Deserializer& deserializer) {
  const string& prefix = DynamicDepthConst::DepthMap();
  string format_str;
  float near;
  float far;
  string units_str;
  string depth_uri;
  string item_semantic_str;

  if (!deserializer.ParseString(prefix, kItemSemantic, &item_semantic_str) ||
      !deserializer.ParseString(prefix, kFormat, &format_str) ||
      !deserializer.ParseFloat(prefix, kNear, &near) ||
      !deserializer.ParseFloat(prefix, kFar, &far) ||
      !deserializer.ParseString(prefix, kUnits, &units_str) ||
      !deserializer.ParseString(prefix, kDepthUri, &depth_uri)) {
    return nullptr;
  }

  DepthMapParams params(StringToFormat(format_str), near, far,
                        StringToUnits(units_str), depth_uri);
  params.item_semantic = StringToItemSemantic(item_semantic_str);

  string confidence_uri;
  if (deserializer.ParseString(prefix, kConfidenceUri, &confidence_uri)) {
    params.confidence_uri = confidence_uri;
  }

  string measure_type_str;
  if (deserializer.ParseString(prefix, kMeasureType, &measure_type_str)) {
    params.measure_type = StringToMeasureType(measure_type_str);
  }

  string software;
  if (deserializer.ParseString(prefix, kSoftware, &software)) {
    params.software = software;
  }

  std::vector<float> focal_table;
  int focal_table_entry_count;
  if (deserializer.ParseFloatArrayBase64(prefix, kFocalTable, &focal_table) &&
      (!deserializer.ParseInt(prefix, kFocalTableEntryCount,
                              &focal_table_entry_count) &&
       focal_table.size() / 2 != focal_table_entry_count)) {
    return nullptr;
  }
  params.focal_table = focal_table;

  return std::unique_ptr<DepthMap>(new DepthMap(params));  // NOLINT
}

// Public methods.
void DepthMap::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::DepthMap(), kNamespaceHref);
}

std::unique_ptr<DepthMap> DepthMap::FromData(
    const DepthMapParams& params, std::vector<std::unique_ptr<Item>>* items) {
  if (params.format == DepthFormat::kFormatNone) {
    LOG(ERROR)
        << "Format must be specified, cannot be of type DepthFormat::NONE";
    return nullptr;
  }

  if (params.depth_uri.empty() || params.depth_image_data.empty()) {
    LOG(ERROR) << "Depth image data and URI must be provided";
    return nullptr;
  }

  if (!params.focal_table.empty() && params.focal_table.size() % 2 != 0) {
    LOG(ERROR) << "Focal table entries must consist of pairs";
    return nullptr;
  }

  if (items == nullptr) {
    LOG(ERROR) << "List of items is null";
    return nullptr;
  }

  if (params.mime.empty()) {
    LOG(ERROR) << "Depth image mime must be provided to DepthMapParams";
    return nullptr;
  }

  ItemParams depth_item_params(params.mime, params.depth_image_data.size(),
                               params.depth_uri);
  depth_item_params.payload_to_serialize = params.depth_image_data;
  items->emplace_back(Item::FromData(depth_item_params));

  bool available_confidence_uri_and_data = true;
  if (!params.confidence_uri.empty() && !params.confidence_data.empty()) {
    // Assumes that the confidence mime is the same as that of the depth map.
    ItemParams confidence_item_params(
        params.mime, params.confidence_data.size(), params.confidence_uri);
    confidence_item_params.payload_to_serialize = params.confidence_data;
    items->emplace_back(Item::FromData(confidence_item_params));
  } else if (!params.confidence_uri.empty() && params.confidence_data.empty()) {
    LOG(ERROR) << "No confidence data provided, the URI will be set to empty "
                  "and not serialized";
    available_confidence_uri_and_data = false;
  }

  auto depth_map = std::unique_ptr<DepthMap>(new DepthMap(params));  // NOLINT
  if (!available_confidence_uri_and_data) {
    // Ensure we don't serialize the confidence URI if no data has been
    // provided.
    depth_map->params_.confidence_uri = "";
  }

  return depth_map;
}

std::unique_ptr<DepthMap> DepthMap::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::DepthMap()),
          DynamicDepthConst::DepthMap());
  if (deserializer == nullptr) {
    return nullptr;
  }

  return ParseFields(*deserializer);
}

DepthFormat DepthMap::GetFormat() const { return params_.format; }
float DepthMap::GetNear() const { return params_.near; }
float DepthMap::GetFar() const { return params_.far; }
DepthUnits DepthMap::GetUnits() const { return params_.units; }
const string DepthMap::GetDepthUri() const { return params_.depth_uri; }
DepthItemSemantic DepthMap::GetItemSemantic() const {
  return params_.item_semantic;
}
const string DepthMap::GetConfidenceUri() const {
  return params_.confidence_uri;
}

DepthMeasureType DepthMap::GetMeasureType() const {
  return params_.measure_type;
}

const string DepthMap::GetSoftware() const { return params_.software; }
const std::vector<float>& DepthMap::GetFocalTable() const {
  return params_.focal_table;
}

size_t DepthMap::GetFocalTableEntryCount() const {
  return params_.focal_table.size() / 2;
}

bool DepthMap::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }
  if (params_.depth_uri.empty()) {
    LOG(ERROR) << "Depth image URI is empty";
    return false;
  }

  const string& prefix = DynamicDepthConst::DepthMap();
  // Error checking is already done in FromData.
  if (!serializer->WriteProperty(prefix, kItemSemantic,
                                 ItemSemanticToString(params_.item_semantic)) ||
      !serializer->WriteProperty(prefix, kFormat,
                                 FormatToString(params_.format)) ||
      !serializer->WriteProperty(prefix, kUnits,
                                 UnitsToString(params_.units)) ||
      !serializer->WriteProperty(prefix, kNear, std::to_string(params_.near)) ||
      !serializer->WriteProperty(prefix, kFar, std::to_string(params_.far)) ||
      !serializer->WriteProperty(prefix, kDepthUri, params_.depth_uri)) {
    return false;
  }

  serializer->WriteProperty(prefix, kMeasureType,
                            MeasureTypeToString(params_.measure_type));

  if (!params_.confidence_uri.empty()) {
    serializer->WriteProperty(prefix, kConfidenceUri, params_.confidence_uri);
  }

  if (!params_.software.empty()) {
    serializer->WriteProperty(prefix, kSoftware, params_.software);
  }

  if (!params_.focal_table.empty()) {
    string base64_encoded_focal_table;
    if (!EncodeFloatArrayBase64(params_.focal_table,
                                &base64_encoded_focal_table)) {
      LOG(ERROR) << "Focal table encoding failed";
    } else {
      int focal_table_entry_count =
          static_cast<int>(params_.focal_table.size() / 2);
      if (!serializer->WriteProperty(
              prefix, kFocalTableEntryCount,
              ::dynamic_depth::strings::SimpleItoa(focal_table_entry_count)) ||
          !serializer->WriteProperty(prefix, kFocalTable,
                                     base64_encoded_focal_table)) {
        LOG(ERROR) << "Focal table or entry count could not be serialized";
      }
    }
  }

  return true;
}

}  // namespace dynamic_depth
