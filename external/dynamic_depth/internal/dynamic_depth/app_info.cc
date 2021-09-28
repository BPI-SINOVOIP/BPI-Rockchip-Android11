#include "dynamic_depth/app_info.h"

#include <memory>

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"
#include "xmpmeta/xml/utils.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kPropertyPrefix[] = "AppInfo";
const char kVersion[] = "Version";
const char kApplication[] = "Application";
const char kItemUri[] = "ItemURI";

const char kTextMime[] = "text/plain";

const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/appinfo/";

}  // namespace

// Private constructor.
AppInfo::AppInfo() : application_(""), version_(""), item_uri_("") {}

// Public methods.
void AppInfo::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->insert(
      std::pair<string, string>(kPropertyPrefix, kNamespaceHref));
}

std::unique_ptr<AppInfo> AppInfo::FromData(
    const string& application, const string& version, const string& data,
    const string& item_uri, std::vector<std::unique_ptr<Item>>* items) {
  if (application.empty()) {
    LOG(ERROR) << "No application name given";
    return nullptr;
  }

  if (version.empty() && item_uri.empty() && items == nullptr) {
    LOG(ERROR) << "One of version or item_uri must be present, but neither was "
               << "found, or items is null while version is empty";
    return nullptr;
  }

  if (!item_uri.empty() && items == nullptr) {
    LOG(ERROR) << "Item URI given, but no place to store the generated item "
                  "element; returning null";
    return nullptr;
  }

  if (!item_uri.empty() && data.empty()) {
    LOG(ERROR) << "Data provided, but no item URI given";
    return nullptr;
  }

  // Store the data with a text/plain mimetype.
  if (!data.empty() && !item_uri.empty() && items != nullptr) {
    ItemParams item_params(kTextMime, data.size(), item_uri);
    item_params.payload_to_serialize = data;
    items->emplace_back(Item::FromData(item_params));
  }

  std::unique_ptr<AppInfo>
      vendor_info(std::unique_ptr<AppInfo>(new AppInfo()));  // NOLINT
  vendor_info->application_ = application;
  vendor_info->version_ = version;
  vendor_info->item_uri_ = item_uri;
  return vendor_info;
}

std::unique_ptr<AppInfo> AppInfo::FromDeserializer(
    const Deserializer& parent_deserializer, const string& namespace_str) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(namespace_str, kPropertyPrefix);
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<AppInfo>
      vendor_info(std::unique_ptr<AppInfo>(new AppInfo()));  // NOLINT
  if (!vendor_info->ParseFields(*deserializer)) {
    return nullptr;
  }
  return vendor_info;
}

const string& AppInfo::GetApplication() const { return application_; }
const string& AppInfo::GetVersion() const { return version_; }
const string& AppInfo::GetItemUri() const { return item_uri_; }

bool AppInfo::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  // Write required field.
  if (!serializer->WriteProperty(DynamicDepthConst::AppInfo(), kApplication,
                                 application_)) {
    return false;
  }

  // No error checking here, because we've already done that in the instantiator
  // and deserializer.
  if (!version_.empty()) {
    serializer->WriteProperty(DynamicDepthConst::AppInfo(), kVersion, version_);
  }

  if (!item_uri_.empty()) {
    serializer->WriteProperty(DynamicDepthConst::AppInfo(), kItemUri,
                              item_uri_);
  }
  return true;
}

// Private methods.
bool AppInfo::ParseFields(const Deserializer& deserializer) {
  // Required field.
  if (!deserializer.ParseString(DynamicDepthConst::AppInfo(), kApplication,
                                &application_)) {
    return false;
  }

  // One of the following fields must be present.
  bool success = deserializer.ParseString(DynamicDepthConst::AppInfo(),
                                          kVersion, &version_);
  success |= deserializer.ParseString(DynamicDepthConst::AppInfo(), kItemUri,
                                      &item_uri_);
  return success;
}

}  // namespace dynamic_depth
