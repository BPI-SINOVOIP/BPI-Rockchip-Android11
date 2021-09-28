#include "dynamic_depth/vendor_info.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "xmpmeta/base64.h"
#include "xmpmeta/xml/utils.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

const char kPropertyPrefix[] = "VendorInfo";
const char kModel[] = "Model";
const char kManufacturer[] = "Manufacturer";
const char kNotes[] = "Notes";

const char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/vendorinfo/";

}  // namespace

// Private constructor.
VendorInfo::VendorInfo() : manufacturer_(""), model_(""), notes_("") {}

// Public methods.
void VendorInfo::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->insert(
      std::pair<string, string>(kPropertyPrefix, kNamespaceHref));
}

std::unique_ptr<VendorInfo> VendorInfo::FromData(const string& manufacturer,
                                                 const string& model,
                                                 const string& notes) {
  if (manufacturer.empty()) {
    LOG(ERROR) << "No manufacturer data given";
    return nullptr;
  }
  std::unique_ptr<VendorInfo> vendor_info(new VendorInfo());
  vendor_info->model_ = model;
  vendor_info->manufacturer_ = manufacturer;
  vendor_info->notes_ = notes;
  return vendor_info;
}

std::unique_ptr<VendorInfo> VendorInfo::FromDeserializer(
    const Deserializer& parent_deserializer, const string& namespace_str) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(namespace_str, kPropertyPrefix);
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<VendorInfo> vendor_info(new VendorInfo());
  if (!vendor_info->ParseFields(*deserializer)) {
    return nullptr;
  }
  return vendor_info;
}

const string& VendorInfo::GetManufacturer() const { return manufacturer_; }
const string& VendorInfo::GetModel() const { return model_; }
const string& VendorInfo::GetNotes() const { return notes_; }

bool VendorInfo::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  // Write required field.
  if (!serializer->WriteProperty(DynamicDepthConst::VendorInfo(), kManufacturer,
                                 manufacturer_)) {
    return false;
  }

  // Write optional fields.
  if (!model_.empty()) {
    serializer->WriteProperty(DynamicDepthConst::VendorInfo(), kModel, model_);
  }
  if (!notes_.empty()) {
    serializer->WriteProperty(DynamicDepthConst::VendorInfo(), kNotes, notes_);
  }
  return true;
}

// Private methods.
bool VendorInfo::ParseFields(const Deserializer& deserializer) {
  // Required field.
  if (!deserializer.ParseString(DynamicDepthConst::VendorInfo(), kManufacturer,
                                &manufacturer_)) {
    return false;
  }

  // Optional fields.
  deserializer.ParseString(DynamicDepthConst::VendorInfo(), kModel, &model_);
  deserializer.ParseString(DynamicDepthConst::VendorInfo(), kNotes, &notes_);
  return true;
}

}  // namespace dynamic_depth
