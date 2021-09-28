#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CONTAINER_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CONTAINER_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/item.h"

namespace dynamic_depth {

// A Container that holds a directory / array of file Item elementss. Files
// at the end of the image appear in the same sequential order as the Item
// elements held in an instance of this object.
class Container : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates this object from the given items. Returns null if the list is
  // empty. If creation succeeds, ownership of the Item objects are transferred
  // to the resulting Container object. The vector of Item objects will be
  // cleared.
  static std::unique_ptr<Container> FromItems(
      std::vector<std::unique_ptr<Item>>* items);

  // Returns the deserialized item elements, null if parsing failed for all
  // items.
  static std::unique_ptr<Container> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  // Returns the list of cameras.
  const std::vector<const Item*> GetItems() const;

  // Disallow copying.
  Container(const Container&) = delete;
  void operator=(const Container&) = delete;

 private:
  Container();

  std::vector<std::unique_ptr<Item>> items_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_CONTAINER_H_  // NOLINT
