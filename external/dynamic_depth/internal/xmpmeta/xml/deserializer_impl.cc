#include "xmpmeta/xml/deserializer_impl.h"

#include <algorithm>

#include "base/integral_types.h"
#include "android-base/logging.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/search.h"
#include "xmpmeta/xml/utils.h"
#include "xmpmeta/xmp_parser.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {
namespace {

// Converts a string to a boolean value if bool_str is one of "false" or "true",
// regardless of letter casing.
bool BoolStringToBool(const string& bool_str, bool* value) {
  string bool_str_lower = bool_str;
  std::transform(bool_str_lower.begin(), bool_str_lower.end(),
                 bool_str_lower.begin(), ::tolower);
  if (bool_str_lower == "true") {
    *value = true;
    return true;
  }
  if (bool_str_lower == "false") {
    *value = false;
    return true;
  }
  return false;
}

// Search for an rdf:Seq node, if it hasn't already been set.
// parent_name is the name of the rdf:Seq node's parent.
xmlNodePtr FindSeqNode(const xmlNodePtr node, const string& prefix,
                       const string& parent_name) {
  xmlNodePtr parent_node =
      DepthFirstSearch(node, prefix.data(), parent_name.data());
  if (parent_node == nullptr) {
    LOG(WARNING) << "Node " << parent_name << " not found";
    return nullptr;
  }
  return GetFirstSeqElement(parent_node);
}

// Extracts the specified string attribute.
bool GetStringProperty(const xmlNodePtr node, const string& prefix,
                       const string& property, string* value) {
  const xmlDocPtr doc = node->doc;
  for (const _xmlAttr* attribute = node->properties; attribute != nullptr;
       attribute = attribute->next) {
    // If prefix is not empty, then the attribute's namespace must not be null.
    if (((attribute->ns && !prefix.empty() &&
          strcmp(FromXmlChar(attribute->ns->prefix), prefix.data()) == 0) ||
         prefix.empty()) &&
        strcmp(FromXmlChar(attribute->name), property.data()) == 0) {
      xmlChar* attribute_string =
          xmlNodeListGetString(doc, attribute->children, 1);
      *value = FromXmlChar(attribute_string);
      xmlFree(attribute_string);
      return true;
    }
  }
  return false;
}

// Reads the contents of a node.
// E.g. <prefix:node_name>Contents Here</prefix:node_name>
bool ReadNodeContent(const xmlNodePtr node, const string& prefix,
                     const string& node_name, string* value) {
  auto* element = DepthFirstSearch(node, prefix.data(), node_name.data());
  if (element == nullptr) {
    return false;
  }
  if (!prefix.empty() &&
      (element->ns == nullptr || element->ns->prefix == nullptr ||
       strcmp(FromXmlChar(element->ns->prefix), prefix.data()) != 0)) {
    return false;
  }
  xmlChar* node_content = xmlNodeGetContent(element);
  *value = FromXmlChar(node_content);
  free(node_content);
  return true;
}

// Reads the string value of a property from the given XML node.
bool ReadStringProperty(const xmlNodePtr node, const string& prefix,
                        const string& property, string* value) {
  if (node == nullptr) {
    return false;
  }
  if (property.empty()) {
    LOG(ERROR) << "Property not given";
    return false;
  }

  // Try parsing in the format <Node ... Prefix:Property="Value"/>
  bool success = GetStringProperty(node, prefix, property, value);
  if (!success) {
    // Try parsing in the format <Prefix:Property>Value</Prefix:Property>
    success = ReadNodeContent(node, prefix, property, value);
  }
  return success;
}

// Same as ReadStringProperty, but applies base-64 decoding to the output.
bool ReadBase64Property(const xmlNodePtr node, const string& prefix,
                        const string& property, string* value) {
  string base64_data;
  if (!ReadStringProperty(node, prefix, property, &base64_data)) {
    return false;
  }
  return DecodeBase64(base64_data, value);
}

}  // namespace

DeserializerImpl::DeserializerImpl(const xmlNodePtr node)
    : node_(node), list_node_(nullptr) {}

// Public methods.
std::unique_ptr<Deserializer> DeserializerImpl::CreateDeserializer(
    const string& prefix, const string& child_name) const {
  if (child_name.empty()) {
    LOG(ERROR) << "Child name is empty";
    return nullptr;
  }
  xmlNodePtr child_node =
      DepthFirstSearch(node_, prefix.data(), child_name.data());
  if (child_node == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<Deserializer>(
      new DeserializerImpl(child_node));  // NOLINT
}

std::unique_ptr<Deserializer>
DeserializerImpl::CreateDeserializerFromListElementAt(const string& prefix,
                                                      const string& list_name,
                                                      int index) const {
  if (index < 0) {
    LOG(ERROR) << "Index must be greater than or equal to zero";
    return nullptr;
  }

  if (list_name.empty()) {
    LOG(ERROR) << "Parent name cannot be empty";
    return nullptr;
  }
  // Search for an rdf:Seq node, if the name of list_node_ doesn't match
  // the given parent name.
  // Ensures thread safety.
  const xmlNodePtr list_node = [&] {
    std::lock_guard<std::mutex> lock(mtx_);
    if (list_node_ == nullptr ||
        string(FromXmlChar(list_node_->name)) != list_name) {
      list_node_ = DepthFirstSearch(node_, prefix.data(), list_name.data());
    }
    return list_node_;
  }();
  if (list_node == nullptr) {
    return nullptr;
  }

  xmlNodePtr seq_node = GetFirstSeqElement(list_node);
  if (seq_node == nullptr) {
    LOG(ERROR) << "No rdf:Seq node found on " << list_name;
    return nullptr;
  }
  xmlNodePtr li_node = GetElementAt(seq_node, index);
  if (li_node == nullptr) {
    return nullptr;
  }
  // Return a new Deserializer with the current rdf:li node and the current
  // node name.
  return std::unique_ptr<Deserializer>(
      new DeserializerImpl(li_node));  // NOLINT
}

bool DeserializerImpl::ParseBase64(const string& prefix, const string& name,
                                   string* value) const {
  return ReadBase64Property(node_, prefix, name, value);
}

bool DeserializerImpl::ParseIntArrayBase64(const string& prefix,
                                           const string& name,
                                           std::vector<int>* values) const {
  string base64_data;
  if (!ReadStringProperty(node_, prefix, name, &base64_data)) {
    return false;
  }
  return DecodeIntArrayBase64(base64_data, values);
}

bool DeserializerImpl::ParseFloatArrayBase64(const string& prefix,
                                             const string& name,
                                             std::vector<float>* values) const {
  string base64_data;
  if (!ReadStringProperty(node_, prefix, name, &base64_data)) {
    return false;
  }
  return DecodeFloatArrayBase64(base64_data, values);
}

bool DeserializerImpl::ParseDoubleArrayBase64(
    const string& prefix, const string& name,
    std::vector<double>* values) const {
  string base64_data;
  if (!ReadStringProperty(node_, prefix, name, &base64_data)) {
    return false;
  }
  return DecodeDoubleArrayBase64(base64_data, values);
}

bool DeserializerImpl::ParseBoolean(const string& prefix, const string& name,
                                    bool* value) const {
  string string_value;
  if (!ReadStringProperty(node_, prefix, name, &string_value)) {
    return false;
  }
  return BoolStringToBool(string_value, value);
}

bool DeserializerImpl::ParseDouble(const string& prefix, const string& name,
                                   double* value) const {
  string string_value;
  if (!ReadStringProperty(node_, prefix, name, &string_value)) {
    return false;
  }
  *value = std::stod(string_value);
  return true;
}

bool DeserializerImpl::ParseInt(const string& prefix, const string& name,
                                int* value) const {
  string string_value;
  if (!ReadStringProperty(node_, prefix, name, &string_value)) {
    return false;
  }
  *value = std::stoi(string_value);  // NOLINT
  return true;
}

bool DeserializerImpl::ParseFloat(const string& prefix, const string& name,
                                  float* value) const {
  string string_value;
  if (!ReadStringProperty(node_, prefix, name, &string_value)) {
    return false;
  }
  *value = std::stof(string_value);
  return true;
}

bool DeserializerImpl::ParseLong(const string& prefix, const string& name,
                                 int64* value) const {
  string string_value;
  if (!ReadStringProperty(node_, prefix, name, &string_value)) {
    return false;
  }
  *value = std::stol(string_value);
  return true;
}

bool DeserializerImpl::ParseString(const string& prefix, const string& name,
                                   string* value) const {
  return ReadStringProperty(node_, prefix, name, value);
}

bool DeserializerImpl::ParseIntArray(const string& prefix,
                                     const string& list_name,
                                     std::vector<int>* values) const {
  xmlNodePtr seq_node = FindSeqNode(node_, prefix, list_name);
  if (seq_node == nullptr) {
    return false;
  }
  values->clear();
  int i = 0;
  for (xmlNodePtr li_node = GetElementAt(seq_node, 0); li_node != nullptr;
       li_node = GetElementAt(seq_node, ++i)) {
    string value = GetLiNodeContent(li_node);
    for (int i = 0; i < value.size(); ++i) {
      if (!isdigit(value[i])) {
        LOG(ERROR) << "Could not parse rdf:li node value to an integer";
        return false;
      }
    }
    int int_value = std::atoi(value.c_str());  // NOLINT
    values->push_back(int_value);
  }

  return true;
}

bool DeserializerImpl::ParseDoubleArray(const string& prefix,
                                        const string& list_name,
                                        std::vector<double>* values) const {
  xmlNodePtr seq_node = FindSeqNode(node_, prefix, list_name);
  if (seq_node == nullptr) {
    return false;
  }
  values->clear();
  int i = 0;
  for (xmlNodePtr li_node = GetElementAt(seq_node, 0); li_node != nullptr;
       li_node = GetElementAt(seq_node, ++i)) {
    double double_value;
    if (!dynamic_depth::strings::safe_strtod(GetLiNodeContent(li_node),
                                             &double_value)) {
      LOG(ERROR) << "Could not parse rdf:li node value to a double";
      return false;
    }
    values->push_back(double_value);
  }

  return true;
}

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth
