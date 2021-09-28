#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_IMPL_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_IMPL_H_  // NOLINT

#include <libxml/tree.h>

#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <string>

#include "base/port.h"
#include "xmpmeta/xml/deserializer.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Deserializes an XML node.
// Example:
//   xmlNodePtr device_node =
//       DepthFirstSearch(xmp.ExtendedSection(), "Device", "Description");
//   DeserializerImpl deserializer(device_node);
//   string revision;
//   deserializer.ParseString("Device", "Revision", &revision);
// TODO(miraleung): Add example for list node deserializer.
class DeserializerImpl : public Deserializer {
 public:
  // Creates a deserializer with a null rdf:Seq node.
  explicit DeserializerImpl(const xmlNodePtr node);

  // Returns a Deserializer.
  // If prefix is empty, the deserializer will be created on the first node
  // found with a name that matches child_name.
  // child_name is the name of the next node to deserialize.
  std::unique_ptr<Deserializer> CreateDeserializer(
      const string& prefix, const string& child_name) const override;

  // Returns a Deserializer from a list element node, if one is available as
  // a descendant of node_.
  // If prefix is empty, the deserializer will be created on the first node
  // found with a name that matches child_name.
  // Returns null if seq_node_ is null or if the index is out of range.
  std::unique_ptr<Deserializer> CreateDeserializerFromListElementAt(
      const string& prefix, const string& list_name, int index) const override;

  // Parsers for XML properties.
  // If prefix is empty, the node's namespace may be null. Otherwise, it must
  // not be null.
  bool ParseBase64(const string& prefix, const string& name,
                   string* value) const override;
  bool ParseIntArrayBase64(const string& prefix, const string& name,
                           std::vector<int>* values) const override;
  bool ParseFloatArrayBase64(const string& prefix, const string& name,
                             std::vector<float>* values) const override;
  bool ParseDoubleArrayBase64(const string& prefix, const string& name,
                              std::vector<double>* values) const override;
  bool ParseBoolean(const string& prefix, const string& name,
                    bool* value) const override;
  bool ParseDouble(const string& prefix, const string& name,
                   double* value) const override;
  bool ParseInt(const string& prefix, const string& name,
                int* value) const override;
  bool ParseFloat(const string& prefix, const string& name,
                  float* value) const override;
  bool ParseLong(const string& prefix, const string& name,
                 int64* value) const override;
  bool ParseString(const string& prefix, const string& name,
                   string* value) const override;

  // Parses the numbers in an rdf:Seq list into the values collection.
  // The given collection is cleared of any existing values, and the
  // parsed numbers are written to it.
  bool ParseIntArray(const string& prefix, const string& list_name,
                     std::vector<int>* values) const override;
  bool ParseDoubleArray(const string& prefix, const string& list_name,
                        std::vector<double>* values) const override;

  // Disallow copying.
  DeserializerImpl(const DeserializerImpl&) = delete;
  void operator=(const DeserializerImpl&) = delete;

 private:
  xmlNodePtr node_;
  // Remembers the parent node of the last deserializer created on the rdf:Seq
  // node. For performance reasons only, to avoid unnessarily traversing
  // the XML document tree.
  mutable xmlNodePtr list_node_;
  // Lock modifications of list_node_ in const functions to make it thread-safe.
  mutable std::mutex mtx_;
};

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_IMPL_H_  // NOLINT
