#include "dynamic_depth/container.h"

#include "android-base/logging.h"
#include "dynamic_depth/const.h"
#include "xmpmeta/xml/const.h"

using ::dynamic_depth::xmpmeta::xml::Deserializer;
using ::dynamic_depth::xmpmeta::xml::Serializer;
using ::dynamic_depth::xmpmeta::xml::XmlConst;

namespace dynamic_depth {

constexpr char kNamespaceHref[] =
    "http://ns.google.com/photos/dd/1.0/container/";
constexpr char kDirectory[] = "Directory";
constexpr char kResourceType[] = "Resource";

// Private constructor.
Container::Container() {}

void Container::GetNamespaces(
    std::unordered_map<string, string>* ns_name_href_map) {
  if (ns_name_href_map == nullptr || items_.empty()) {
    LOG(ERROR) << "Namespace list is null or item list is empty";
    return;
  }
  ns_name_href_map->emplace(DynamicDepthConst::Container(), kNamespaceHref);
  items_[0]->GetNamespaces(ns_name_href_map);
}

std::unique_ptr<Container> Container::FromItems(
    std::vector<std::unique_ptr<Item>>* items) {
  if (items == nullptr || items->empty()) {
    LOG(ERROR) << "Item list is empty";
    return nullptr;
  }

  std::unique_ptr<Container> container(new Container());
  container->items_ = std::move(*items);
  // Purge item elements that are null.
  container->items_.erase(
      std::remove_if(
          container->items_.begin(), container->items_.end(),
          [](const std::unique_ptr<Item>& item) { return item == nullptr; }),
      container->items_.end());
  if (container->items_.empty()) {
    LOG(ERROR) << "No non-null elements in items";
    return nullptr;
  }

  return container;
}

std::unique_ptr<Container> Container::FromDeserializer(
    const Deserializer& parent_deserializer) {
  std::unique_ptr<Container> container(new Container());
  int i = 0;
  std::unique_ptr<Deserializer> deserializer =
      parent_deserializer.CreateDeserializerFromListElementAt(
          DynamicDepthConst::Namespace(DynamicDepthConst::Container()),
          DynamicDepthConst::Container(), 0);
  while (deserializer) {
    std::unique_ptr<Item> item = Item::FromDeserializer(*deserializer);
    if (item == nullptr) {
      LOG(ERROR) << "Unable to deserialize a item";
      return nullptr;
    }
    container->items_.emplace_back(std::move(item));
    deserializer = parent_deserializer.CreateDeserializerFromListElementAt(
        DynamicDepthConst::Namespace(DynamicDepthConst::Container()),
        DynamicDepthConst::Container(), ++i);
  }

  if (container->items_.empty()) {
    return nullptr;
  }
  return container;
}

const std::vector<const Item*> Container::GetItems() const {
  std::vector<const Item*> items;
  for (const auto& item : items_) {
    items.push_back(item.get());
  }
  return items;
}

bool Container::Serialize(Serializer* serializer) const {
  if (items_.empty()) {
    LOG(ERROR) << "Item list is empty";
    return false;
  }

  std::unique_ptr<Serializer> container_serializer =
      serializer->CreateSerializer(
          DynamicDepthConst::Namespace(DynamicDepthConst::Container()),
          DynamicDepthConst::Container());
  if (!container_serializer->WriteProperty(XmlConst::RdfPrefix(),
                                           XmlConst::RdfParseType(),
                                           kResourceType)) {
    return false;
  }

  std::unique_ptr<Serializer> directory_serializer =
      container_serializer->CreateListSerializer(DynamicDepthConst::Container(),
                                                 kDirectory);
  if (directory_serializer == nullptr) {
    // Error is logged in Serializer.
    return false;
  }

  for (int i = 0; i < items_.size(); i++) {
    std::unique_ptr<Serializer> item_serializer =
        directory_serializer->CreateItemSerializer(
            DynamicDepthConst::Namespace(DynamicDepthConst::Item()),
            DynamicDepthConst::Item());
    if (item_serializer == nullptr) {
      LOG(ERROR) << "Could not create a list item serializer for Item";
      return false;
    }
    if (!items_[i]->Serialize(item_serializer.get())) {
      LOG(ERROR) << "Could not serialize item " << i;
      return false;
    }
  }
  return true;
}

}  // namespace dynamic_depth
