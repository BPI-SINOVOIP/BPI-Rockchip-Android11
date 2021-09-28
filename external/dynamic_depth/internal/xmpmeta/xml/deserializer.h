#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_H_  // NOLINT

#include <memory>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Performs deserialization.
// Example:
//   Deserializer deserializer();
//   string revision;
//   deserializer.ParseString("Revision", &revision);
class Deserializer {
 public:
  virtual ~Deserializer() {}

  // Returns a Deserializer.
  // child_name is the name of the next node to deserialize.
  virtual std::unique_ptr<Deserializer> CreateDeserializer(
      const string& prefix, const string& child_name) const = 0;

  // Returns a Deserializer from a list element node.
  virtual std::unique_ptr<Deserializer> CreateDeserializerFromListElementAt(
      const string& prefix, const string& list_name, int index) const = 0;

  // Parsers for properties with the given prefix.
  // Parses a node such as <NodeName Prefix:Name="Value" />
  virtual bool ParseBase64(const string& prefix, const string& name,
                           string* value) const = 0;
  virtual bool ParseIntArrayBase64(const string& prefix, const string& name,
                                   std::vector<int>* values) const = 0;
  virtual bool ParseFloatArrayBase64(const string& prefix, const string& name,
                                     std::vector<float>* values) const = 0;
  virtual bool ParseDoubleArrayBase64(const string& prefix, const string& name,
                                      std::vector<double>* values) const = 0;
  virtual bool ParseBoolean(const string& prefix, const string& name,
                            bool* value) const = 0;
  virtual bool ParseInt(const string& prefix, const string& name,
                        int* value) const = 0;
  virtual bool ParseFloat(const string& prefix, const string& name,
                          float* value) const = 0;
  virtual bool ParseDouble(const string& prefix, const string& name,
                           double* value) const = 0;
  virtual bool ParseLong(const string& prefix, const string& name,
                         int64* value) const = 0;
  virtual bool ParseString(const string& prefix, const string& name,
                           string* value) const = 0;

  // Parsers for arrays.
  virtual bool ParseIntArray(const string& prefix, const string& list_name,
                             std::vector<int>* values) const = 0;
  virtual bool ParseDoubleArray(const string& prefix, const string& list_name,
                                std::vector<double>* values) const = 0;
};

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_DESERIALIZER_H_  // NOLINT
