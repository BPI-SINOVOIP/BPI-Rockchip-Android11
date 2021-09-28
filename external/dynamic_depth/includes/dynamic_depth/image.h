#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGE_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGE_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/item.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

// The ItemSemantic of this Image.
enum class ImageItemSemantic { kPrimary = 1, kOriginal = 2 };

/**
 * An Image element for a Dynamic Depth device.
 */
class Image : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("Image", "http://ns.google.com/photos/dd/1.0/image/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates an original (non-primary) Image from the given fields. Returns null
  // if one of the following is true:
  //   - Mime field is empty.
  //   - Data field is null or empty.
  //   - Uri field is empty.
  //   - The items field is null.
  // Param description:
  //   - Data is NOT base64-encoded. This method takes care of encoding when it
  //     is passed to the generated Item element.
  //  - Mime is the mimetype of the image data.
  //  - Uri is the case-sensitive Item:DataUri of this Image, and must be
  //    unique amongst all Dynamic Depth elements. This will be the only way
  //    that metadata parsers will be able to retrieve the image.
  // Both data and mime are used to construct a new Container:Item, which will
  // be the last one appended to items.
  // It is the caller's responsibility to use items to construct a Container,
  // and ensure that it is serialized along with this Image element.
  static std::unique_ptr<Image> FromData(
      const string& data, const string& mime, const string& item_uri,
      std::vector<std::unique_ptr<Item>>* items);

  // Same as above, but more performant because it avoids an extra string copy.
  static std::unique_ptr<Image> FromData(
      const uint8_t* data, size_t data_size, const string& mime,
      const string& item_uri, std::vector<std::unique_ptr<Item>>* items);

  // Image instantiator for the primary (container) image.
  static std::unique_ptr<Image> FromDataForPrimaryImage(
      const string& mime, std::vector<std::unique_ptr<Item>>* items);

  // Returns the deserialized Image; null if parsing fails.
  static std::unique_ptr<Image> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  const string& GetItemUri() const;
  ImageItemSemantic GetItemSemantic() const;

  // Disallow copying.
  Image(const Image&) = delete;
  void operator=(const Image&) = delete;

 private:
  Image();

  // Extracts image fields.
  bool ParseImageFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  string item_uri_;
  ImageItemSemantic item_semantic_;
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_IMAGE_H_  // NOLINT
