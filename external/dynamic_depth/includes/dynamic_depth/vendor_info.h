#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VENDOR_INFO_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VENDOR_INFO_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

/**
 * A VendorInfo element for a Dynamic Depth device.
 */
class VendorInfo : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("VendorInfo", "http://ns.google.com/photos/dd/1.0/vendorinfo/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates an VendorInfo from the given fields. Returns null if
  // any of the required fields is empty (see fields below).
  // Manufacturer is the manufacturer of the element that created the content.
  // Model is the model of the element that created the content.
  // Notes is general comments.
  static std::unique_ptr<VendorInfo> FromData(const string& manufacturer,
                                              const string& model,
                                              const string& notes);

  // Returns the deserialized VendorInfo; null if parsing fails.
  static std::unique_ptr<VendorInfo> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer,
      const string& namespace_str);

  // Returns the Manufacturer.
  const string& GetManufacturer() const;

  // Returns the Model.
  const string& GetModel() const;

  // Returns the Notes.
  const string& GetNotes() const;

  // Disallow copying.
  VendorInfo(const VendorInfo&) = delete;
  void operator=(const VendorInfo&) = delete;

 private:
  VendorInfo();

  bool ParseFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  // Required field.
  string manufacturer_;  // The manufacturer.

  // Optional fields.
  string model_;  // The model.
  string notes_;  // The notes.
};

}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_VENDOR_INFO_H_  // NOLINT
