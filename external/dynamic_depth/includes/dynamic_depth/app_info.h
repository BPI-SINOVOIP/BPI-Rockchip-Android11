#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_APP_INFO_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_APP_INFO_H_  // NOLINT

#include <memory>
#include <string>
#include <unordered_map>

#include "dynamic_depth/element.h"
#include "dynamic_depth/item.h"
#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

/**
 * A AppInfo element for a Dynamic Depth device.
 */
class AppInfo : public Element {
 public:
  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("AppInfo", "http://ns.google.com/photos/dd/1.0/appinfo/")
  void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) override;

  // Serializes this object.
  bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const override;

  // Creates an AppInfo from the given fields. Returns null if the version
  //   field is empty and [item_uri is empty and items is null].
  // Params' descriptions:
  // application is the name of the application that created the content.
  // version is the application's version for the content.
  // data is the optional payload associated with the given app. If this field
  //     is not empty but item_uri is empty or items is null, then the data will
  //     not be serialized.
  // item_uri is the Container URI of the file that contains the content.
  //     application, and at least one of version or item_uri, must not be
  //     empty.
  // items is the list of items where the serialized data is stored. It is the
  //     caller's responsibility to use items to construct a Container, and
  //     ensure that it is serialized along with this Image element. Data will
  //     not be serialized if this field is null.
  static std::unique_ptr<AppInfo> FromData(
      const string& application, const string& version, const string& data,
      const string& item_uri, std::vector<std::unique_ptr<Item>>* items);

  // Returns the deserialized AppInfo; null if parsing fails.
  static std::unique_ptr<AppInfo> FromDeserializer(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& parent_deserializer,
      const string& namespace_str);

  // Getters.
  const string& GetApplication() const;
  const string& GetVersion() const;
  const string& GetItemUri() const;

  // Disallow copying.
  AppInfo(const AppInfo&) = delete;
  void operator=(const AppInfo&) = delete;

 private:
  AppInfo();

  bool ParseFields(
      const ::dynamic_depth::xmpmeta::xml::Deserializer& deserializer);

  // Required.
  string application_;

  // At least one of version or item_uri must be present.
  string version_;
  string item_uri_;
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_APP_INFO_H_  // NOLINT
