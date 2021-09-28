#ifndef DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_ELEMENT_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_ELEMENT_H_  // NOLINT

#include <unordered_map>

#include "xmpmeta/xml/deserializer.h"
#include "xmpmeta/xml/serializer.h"

namespace dynamic_depth {

/**
 * An interface for an element in the Dynamic Depth spec.
 */
class Element {
 public:
  virtual ~Element() {}

  // Appends child elements' namespaces' and their respective hrefs to the
  // given collection, and any parent nodes' names to prefix_names.
  // Key: Name of the namespace.
  // Value: Full namespace URL.
  // Example: ("Image", "http://ns.google.com/photos/dd/1.0/image/")
  virtual void GetNamespaces(
      std::unordered_map<string, string>* ns_name_href_map) = 0;

  // Serializes this element.
  virtual bool Serialize(
      ::dynamic_depth::xmpmeta::xml::Serializer* serializer) const = 0;
};

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INTERNAL_DYNAMIC_DEPTH_ELEMENT_H_  // NOLINT
