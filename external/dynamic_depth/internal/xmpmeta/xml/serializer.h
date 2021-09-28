#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_H_  // NOLINT

#include <memory>
#include <string>
#include <vector>

#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Serializes properties for a hierarchy of objects.
// Example:
//  BookSerializer serializer();
//  // Serialize a list of objects.
//  std::unique_ptr<Serializer> book_list_serializer =
//      serializer->CreateListSerializer("Books");
//  for (Book *book : book_list) {
//    std::unique_ptr<Serializer> book_serializer =
//        cameras_serializer->CreateItemSerializer("Book");
//    success &= book->Serialize(book_serializer.get());
//
//    // Write properties in an object.
//    // This would be called from the Book class.
//    string book_name("Book");
//    std::unique_ptr<Serializer> book_info_serializer =
//        book_serializer->CreateSerializer("Info");
//    book_info_serializer->WriteProperty("Author", "Cereal Eyser");
//    book_info_serializer->WriteProperty("ISBN", "314159265359");
//    std::unique_ptr<Serializer> genre_serializer =
//        book_serializer->CreateSeralizer("Genre", true);
//    std::unique_ptr<Serializer> fantasy_serializer =
//        genre_serializer->CreateSerialzer("Fantasy");
//    // Serialize genre properties here.
//  }

class Serializer {
 public:
  virtual ~Serializer() {}

  // Returns a Serializer for an object that is an item in a list.
  virtual std::unique_ptr<Serializer> CreateItemSerializer(
      const string& prefix, const string& item_name) const = 0;

  // Returns a Serializer for a list of objects.
  virtual std::unique_ptr<Serializer> CreateListSerializer(
      const string& prefix, const string& list_name) const = 0;

  // Creates a serializer from the current serializer.
  // node_ns_name is the XML namespace to which the newly created node belongs.
  // If this parameter is an empty string, the new node will not belong to a
  // namespace.
  // node_name is the name of the new node. This parameter cannot be an empty
  // string.
  virtual std::unique_ptr<Serializer> CreateSerializer(
      const string& node_ns_name, const string& node_name) const = 0;

  // Serializes a property with the given prefix.
  // Example: <NodeName PropertyPrefix:PropertyName="PropertyValue" />
  virtual bool WriteBoolProperty(const string& prefix, const string& name,
                                 bool value) const = 0;
  virtual bool WriteProperty(const string& prefix, const string& name,
                             const string& value) const = 0;

  // Serializes the collection of values.
  virtual bool WriteIntArray(const string& prefix, const string& array_name,
                             const std::vector<int>& values) const = 0;
  virtual bool WriteDoubleArray(const string& prefix, const string& array_name,
                                const std::vector<double>& values) const = 0;
};

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SERIALIZER_H_  // NOLINT
