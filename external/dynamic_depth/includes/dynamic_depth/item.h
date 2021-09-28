#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_ITEM_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_ITEM_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/dimension.h"
#include "dynamic_depth/element.h"
#include "dynamic_depth/point.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {
struct ItemParams {
  // Required fields.
  string mime;          // Must not be empty.
  unsigned int length;  // Must not be zero.

  // Optional.
  unsigned int padding;
  string data_uri;

  // Only for final file serialization - not used in XMP metadata I/O.
  // IMPORTANT: Callers should enforce that this file exists.
  // TODO(miraleung): This could be stored as char* to optimize performance.
  string payload_to_serialize;

  ItemParams(const string& in_mime, unsigned int len)
      : mime(in_mime),
        length(len),
        padding(0),
        data_uri(""),
        payload_to_serialize("") {}
  ItemParams(const string& in_mime, unsigned int len, const string& uri)
      : mime(in_mime),
        length(len),
        padding(0),
        data_uri(uri),
        payload_to_serialize("") {}

  inline bool operator==(const ItemParams& other) const {
    return mime == other.mime && length == other.length &&
           padding == other.padding && data_uri == other.data_uri &&
           payload_to_serialize == other.payload_to_serialize;
  }

  inline bool operator!=(const ItemParams& other) const {
    return !(*this == other);
  }
};

class Item : public Element {
 public:
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  static std::unique_ptr<Item> FromData(const ItemParams& params);

  // Returns the deserialized item elements, null if parsing failed for all
  // items.
  static std::unique_ptr<Item> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer);

  const string& GetMime() const;
  unsigned int GetLength() const;
  const string& GetDataUri() const;
  unsigned int GetPadding() const;
  const string& GetPayloadToSerialize() const;

  // Disallow copying.
  Item(const Item&) = delete;
  void operator=(const Item&) = delete;

 private:
  Item(const ItemParams& params);
  static std::unique_ptr<Item> FromDataInternal(const ItemParams& params,
                                                bool check_filepath);

  ItemParams params_;
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_ITEM_H_  // NOLINT
