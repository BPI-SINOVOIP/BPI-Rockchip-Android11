#include "dynamic_depth/image.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "dynamic_depth/item.h"

using ::dynamic_depth::Item;
using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;

namespace dynamic_depth {
namespace {

constexpr char kItemUri[] = "ItemURI";
constexpr char kItemSemantic[] = "ItemSemantic";

constexpr char kNamespaceHref[] = "http://ns.google.com/photos/dd/1.0/image/";
constexpr char kPrimaryImagePlaceholderItemUri[] = "primary_image";

constexpr char kItemSemanticPrimary[] = "Primary";
constexpr char kItemSemanticOriginal[] = "Original";
constexpr char kItemSemanticPrimaryLower[] = "primary";

string ItemSemanticToString(ImageItemSemantic item_semantic) {
  switch (item_semantic) {
    case ImageItemSemantic::kPrimary:
      return kItemSemanticPrimary;
    case ImageItemSemantic::kOriginal:
      return kItemSemanticOriginal;
  }
}

ImageItemSemantic StringToItemSemantic(const string& item_semantic_str) {
  string item_semantic_str_lower = item_semantic_str;
  std::transform(item_semantic_str_lower.begin(), item_semantic_str_lower.end(),
                 item_semantic_str_lower.begin(), ::tolower);
  if (kItemSemanticPrimaryLower == item_semantic_str_lower) {
    return ImageItemSemantic::kPrimary;
  }

  // Don't fail, default to Original.
  return ImageItemSemantic::kOriginal;
}

}  // namespace

// Private constructor.
Image::Image() {}

// Public methods.
void Image::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr) {
    LOG(ERROR) << "Namespace list or own namespace is null";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Image(), kNamespaceHref);
}

std::unique_ptr<Image> Image::FromData(
    const string& data, const string& mime, const string& item_uri,
    std::vector<std::unique_ptr<Item>>* items) {
  if (data.empty() || mime.empty()) {
    LOG(ERROR) << "No image data or mimetype given";
    return nullptr;
  }

  if (item_uri.empty()) {
    LOG(ERROR) << "Item URI must be provided";
    return nullptr;
  }

  if (items == nullptr) {
    LOG(ERROR) << "List of items is null";
    return nullptr;
  }

  ItemParams item_params(mime, data.size(), item_uri);
  item_params.payload_to_serialize = data;
  items->emplace_back(Item::FromData(item_params));

  std::unique_ptr<Image> image(std::unique_ptr<Image>(new Image()));  // NOLINT
  image->item_uri_ = item_uri;
  image->item_semantic_ = ImageItemSemantic::kOriginal;
  return image;
}

std::unique_ptr<Image> Image::FromDataForPrimaryImage(
    const string& mime, std::vector<std::unique_ptr<Item>>* items) {
  if (mime.empty()) {
    LOG(ERROR) << "No mimetype given";
    return nullptr;
  }

  if (items == nullptr) {
    LOG(ERROR) << "List of items is null";
    return nullptr;
  }

  ItemParams item_params(mime, 0, kPrimaryImagePlaceholderItemUri);
  items->emplace_back(Item::FromData(item_params));

  std::unique_ptr<Image> image(std::unique_ptr<Image>(new Image()));  // NOLINT
  image->item_uri_ = kPrimaryImagePlaceholderItemUri;
  image->item_semantic_ = ImageItemSemantic::kPrimary;
  return image;
}

std::unique_ptr<Image> Image::FromData(
    const uint8_t* data, size_t data_size, const string& mime,
    const string& item_uri, std::vector<std::unique_ptr<Item>>* items) {
  if ((data == nullptr || data_size == 0) || mime.empty()) {
    LOG(ERROR) << "No image data or mimetype given";
    return nullptr;
  }

  if (item_uri.empty()) {
    LOG(ERROR) << "Item URI must be provided";
    return nullptr;
  }

  if (items == nullptr) {
    LOG(ERROR) << "List of items is null";
    return nullptr;
  }

  ItemParams item_params(mime, data_size, item_uri);
  item_params.payload_to_serialize =
      std::string(reinterpret_cast<const char*>(data), data_size);
  items->emplace_back(Item::FromData(item_params));

  std::unique_ptr<Image> image(std::unique_ptr<Image>(new Image()));  // NOLINT
  image->item_uri_ = item_uri;
  image->item_semantic_ = ImageItemSemantic::kOriginal;
  return image;
}

const string& Image::GetItemUri() const { return item_uri_; }
ImageItemSemantic Image::GetItemSemantic() const { return item_semantic_; }

std::unique_ptr<Image> Image::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Image()),
          DynamicDepthConst::Image());
  if (deserializer == nullptr) {
    return nullptr;
  }

  std::unique_ptr<Image> image(new Image());
  if (!image->ParseImageFields(*deserializer)) {
    return nullptr;
  }
  return image;
}

bool Image::Serialize(Serializer* serializer) const {
  if (serializer == nullptr) {
    LOG(ERROR) << "Serializer is null";
    return false;
  }

  if (item_uri_.empty()) {
    LOG(ERROR) << "Item URI is empty";
    return false;
  }

  return serializer->WriteProperty(DynamicDepthConst::Image(), kItemSemantic,
                                   ItemSemanticToString(item_semantic_)) &&
         serializer->WriteProperty(DynamicDepthConst::Image(), kItemUri,
                                   item_uri_);
}

// Private methods.
bool Image::ParseImageFields(const Deserializer& deserializer) {
  string item_uri;
  string item_semantic_str;
  if (!deserializer.ParseString(DynamicDepthConst::Image(), kItemSemantic,
                                &item_semantic_str) ||
      !deserializer.ParseString(DynamicDepthConst::Image(), kItemUri,
                                &item_uri)) {
    return false;
  }

  item_uri_ = item_uri;
  item_semantic_ = StringToItemSemantic(item_semantic_str);
  return true;
}

}  // namespace dynamic_depth
