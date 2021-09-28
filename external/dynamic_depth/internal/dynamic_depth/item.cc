#include "dynamic_depth/item.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

constexpr char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/item/";

constexpr char kMime[] = "Mime";
constexpr char kLength[] = "Length";
constexpr char kPadding[] = "Padding";
constexpr char kDataUri[] = "DataURI";

}  // namespace

// Private constructor.
Item::Item(const ItemParams& params) : params_(params) {}

// Private instantiator.

std::unique_ptr<Item> Item::FromDataInternal(const ItemParams& params,
                                             bool check_filepath) {
  if (check_filepath && params.length != params.payload_to_serialize.size()) {
    LOG(ERROR) << "Length does not match payload's size";
    return nullptr;
  }

  if (params.mime.empty()) {
    LOG(ERROR) << "Mime is empty";
    return nullptr;
  }

  if (params.length < 0) {
    LOG(ERROR) << "Item length must be non-negative";
    return nullptr;
  }

  if (params.padding > 0 &&
      static_cast<int>(params.length - params.padding) <= 0) {
    LOG(ERROR) << "Item length must be larger than padding; found padding="
               << params.padding << ", length=" << params.length;
    return nullptr;
  }

  // TODO(miraleung): Check for only supported mime types?

  return std::unique_ptr<Item>(new Item(params));  // NOLINT
}

void Item::GetNamespaces(std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }

  ns_name_href_map->emplace(DynamicDepthConst::Item(), kNamespaceHref);
}

std::unique_ptr<Item> Item::FromData(const ItemParams& params) {
  return FromDataInternal(params, true);
}

std::unique_ptr<Item> Item::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Item()),
          DynamicDepthConst::Item());
  if (deserializer == nullptr) {
    return nullptr;
  }

  string mime;
  int length;
  int padding = 0;
  string data_uri;

  if (!deserializer->ParseString(DynamicDepthConst::Item(), kMime, &mime) ||
      !deserializer->ParseInt(DynamicDepthConst::Item(), kLength, &length)) {
    return nullptr;
  }

  deserializer->ParseInt(DynamicDepthConst::Item(), kPadding, &padding);
  deserializer->ParseString(DynamicDepthConst::Item(), kDataUri, &data_uri);

  ItemParams params(mime, length);
  if (!data_uri.empty()) {
    params.data_uri = data_uri;
  }
  if (padding > 0) {
    params.padding = padding;
  }

  return Item::FromDataInternal(params, false);
}

// Getters.
const string& Item::GetMime() const { return params_.mime; }
unsigned int Item::GetLength() const { return params_.length; }
const string& Item::GetDataUri() const { return params_.data_uri; }
unsigned int Item::GetPadding() const { return params_.padding; }
const string& Item::GetPayloadToSerialize() const {
  return params_.payload_to_serialize;
}

bool Item::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  // No error-checking for the mime or length here, since it's assumed to be
  // taken care of in the instantiator.
  bool success = serializer->WriteProperty(DynamicDepthConst::Item(), kMime,
                                           params_.mime) &&
                 serializer->WriteProperty(DynamicDepthConst::Item(), kLength,
                                           std::to_string(params_.length));
  if (!params_.data_uri.empty()) {
    success &= serializer->WriteProperty(DynamicDepthConst::Item(), kDataUri,
                                         params_.data_uri);
  }

  if (params_.padding > 0) {
    success &= serializer->WriteProperty(DynamicDepthConst::Item(), kPadding,
                                         std::to_string(params_.padding));
  }

  return success;
}

}  // namespace dynamic_depth
