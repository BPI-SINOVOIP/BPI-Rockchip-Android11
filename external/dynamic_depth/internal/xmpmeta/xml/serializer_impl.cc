#include "xmpmeta/xml/serializer_impl.h"

#include <libxml/tree.h>

#include "base/integral_types.h"
#include "android-base/logging.h"
#include "strings/numbers.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/utils.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Methods specific to SerializerImpl.
SerializerImpl::SerializerImpl(
    const std::unordered_map<string, xmlNsPtr>& namespaces, xmlNodePtr node)
    : node_(node), namespaces_(namespaces) {
  CHECK(node_ != nullptr) << "Node cannot be null";
  CHECK(node_->name != nullptr) << "Name in the XML node cannot be null";
}

bool SerializerImpl::SerializeNamespaces() {
  if (namespaces_.empty()) {
    return true;
  }
  if (node_->ns == nullptr && !namespaces_.empty()) {
    return false;
  }
  // Check that the namespaces all have hrefs and that there is a value
  // for the key node_name.
  // Set the namespaces in the root node.
  xmlNsPtr node_ns = node_->ns;
  for (const auto& entry : namespaces_) {
    CHECK(entry.second->href != nullptr) << "Namespace href cannot be null";
    if (node_ns != nullptr) {
      node_ns->next = entry.second;
    }
    node_ns = entry.second;
  }
  return true;
}

std::unique_ptr<SerializerImpl> SerializerImpl::FromDataAndSerializeNamespaces(
    const std::unordered_map<string, xmlNsPtr>& namespaces, xmlNodePtr node) {
  std::unique_ptr<SerializerImpl> serializer =
      std::unique_ptr<SerializerImpl>(            // NOLINT
          new SerializerImpl(namespaces, node));  // NOLINT
  if (!serializer->SerializeNamespaces()) {
    LOG(ERROR) << "Could not serialize namespaces";
    return nullptr;
  }
  return serializer;
}

// Implemented methods.
std::unique_ptr<Serializer> SerializerImpl::CreateSerializer(
    const string& node_ns_name, const string& node_name) const {
  if (node_name.empty()) {
    LOG(ERROR) << "Node name is empty";
    return nullptr;
  }

  if (namespaces_.count(node_ns_name) == 0 && !node_ns_name.empty()) {
    LOG(ERROR) << "Prefix " << node_ns_name << " not found in prefix list";
    return nullptr;
  }

  xmlNodePtr new_node =
      xmlNewNode(node_ns_name.empty() ? nullptr : namespaces_.at(node_ns_name),
                 ToXmlChar(node_name.data()));
  xmlAddChild(node_, new_node);
  return std::unique_ptr<Serializer>(
      new SerializerImpl(namespaces_, new_node));  // NOLINT
}

std::unique_ptr<Serializer> SerializerImpl::CreateItemSerializer(
    const string& prefix, const string& item_name) const {
  if (namespaces_.count(XmlConst::RdfPrefix()) == 0 ||
      namespaces_.at(XmlConst::RdfPrefix()) == nullptr) {
    LOG(ERROR) << "No RDF prefix namespace found";
    return nullptr;
  }
  if (!prefix.empty() && !namespaces_.count(prefix)) {
    LOG(ERROR) << "No namespace found for " << prefix;
    return nullptr;
  }
  if (strcmp(XmlConst::RdfSeq(), FromXmlChar(node_->name)) != 0) {
    LOG(ERROR) << "No rdf:Seq node for serializing this item";
    return nullptr;
  }

  xmlNsPtr rdf_prefix_ns = namespaces_.at(string(XmlConst::RdfPrefix()));
  xmlNodePtr li_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfLi()));
  xmlNodePtr new_node =
      xmlNewNode(prefix.empty() ? nullptr : namespaces_.at(prefix),
                 ToXmlChar(item_name.data()));
  xmlSetNs(li_node, rdf_prefix_ns);
  xmlAddChild(node_, li_node);
  xmlAddChild(li_node, new_node);
  return std::unique_ptr<Serializer>(
      new SerializerImpl(namespaces_, new_node));  // NOLINT
}

std::unique_ptr<Serializer> SerializerImpl::CreateListSerializer(
    const string& prefix, const string& list_name) const {
  if (namespaces_.count(XmlConst::RdfPrefix()) == 0 ||
      namespaces_.at(XmlConst::RdfPrefix()) == nullptr) {
    LOG(ERROR) << "No RDF prefix namespace found";
    return nullptr;
  }
  if (!prefix.empty() && !namespaces_.count(prefix)) {
    LOG(ERROR) << "No namespace found for " << prefix;
    return nullptr;
  }

  xmlNodePtr list_node =
      xmlNewNode(prefix.empty() ? nullptr : namespaces_.at(prefix),
                 ToXmlChar(list_name.data()));
  xmlNsPtr rdf_prefix_ns = namespaces_.at(string(XmlConst::RdfPrefix()));
  xmlNodePtr seq_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfSeq()));
  xmlSetNs(seq_node, rdf_prefix_ns);
  xmlAddChild(list_node, seq_node);
  xmlAddChild(node_, list_node);
  return std::unique_ptr<Serializer>(
      new SerializerImpl(namespaces_, seq_node));  // NOLINT
}

bool SerializerImpl::WriteBoolProperty(const string& prefix, const string& name,
                                       bool value) const {
  const string& bool_str = (value ? "true" : "false");
  return WriteProperty(prefix, name, bool_str);
}

bool SerializerImpl::WriteProperty(const string& prefix, const string& name,
                                   const string& value) const {
  if (!strcmp(XmlConst::RdfSeq(), FromXmlChar(node_->name))) {
    LOG(ERROR) << "Cannot write a property on an rdf:Seq node";
    return false;
  }
  if (name.empty()) {
    LOG(ERROR) << "Property name is empty";
    return false;
  }

  // Check that prefix has a corresponding namespace href.
  if (!prefix.empty() && namespaces_.count(prefix) == 0) {
    LOG(ERROR) << "No namespace found for prefix " << prefix;
    return false;
  }

  // Serialize the property in the format Prefix:Name="Value".
  xmlSetNsProp(node_, prefix.empty() ? nullptr : namespaces_.at(prefix),
               ToXmlChar(name.data()), ToXmlChar(value.data()));
  return true;
}

bool SerializerImpl::WriteIntArray(const string& prefix,
                                   const string& array_name,
                                   const std::vector<int>& values) const {
  if (!strcmp(XmlConst::RdfSeq(), FromXmlChar(node_->name))) {
    LOG(ERROR) << "Cannot write a property on an rdf:Seq node";
    return false;
  }
  if (values.empty()) {
    LOG(WARNING) << "No values to write";
    return false;
  }
  if (namespaces_.count(XmlConst::RdfPrefix()) == 0 ||
      namespaces_.at(XmlConst::RdfPrefix()) == nullptr) {
    LOG(ERROR) << "No RDF prefix found";
    return false;
  }
  if (!prefix.empty() && !namespaces_.count(prefix)) {
    LOG(ERROR) << "No namespace found for " << prefix;
    return false;
  }
  if (array_name.empty()) {
    LOG(ERROR) << "Parent name cannot be empty";
    return false;
  }

  xmlNodePtr array_parent_node =
      xmlNewNode(prefix.empty() ? nullptr : namespaces_.at(prefix),
                 ToXmlChar(array_name.data()));
  xmlAddChild(node_, array_parent_node);

  xmlNsPtr rdf_prefix_ns = namespaces_.at(XmlConst::RdfPrefix());
  xmlNodePtr seq_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfSeq()));
  xmlSetNs(seq_node, rdf_prefix_ns);
  xmlAddChild(array_parent_node, seq_node);
  for (int value : values) {
    xmlNodePtr li_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfLi()));
    xmlSetNs(li_node, rdf_prefix_ns);
    xmlAddChild(seq_node, li_node);
    xmlNodeSetContent(li_node, ToXmlChar(std::to_string(value).c_str()));
  }

  return true;
}

bool SerializerImpl::WriteDoubleArray(const string& prefix,
                                      const string& array_name,
                                      const std::vector<double>& values) const {
  if (!strcmp(XmlConst::RdfSeq(), FromXmlChar(node_->name))) {
    LOG(ERROR) << "Cannot write a property on an rdf:Seq node";
    return false;
  }
  if (values.empty()) {
    LOG(WARNING) << "No values to write";
    return false;
  }
  if (namespaces_.count(XmlConst::RdfPrefix()) == 0 ||
      namespaces_.at(XmlConst::RdfPrefix()) == nullptr) {
    LOG(ERROR) << "No RDF prefix found";
    return false;
  }
  if (!prefix.empty() && !namespaces_.count(prefix)) {
    LOG(ERROR) << "No namespace found for " << prefix;
    return false;
  }
  if (array_name.empty()) {
    LOG(ERROR) << "Parent name cannot be empty";
    return false;
  }

  xmlNodePtr array_parent_node =
      xmlNewNode(prefix.empty() ? nullptr : namespaces_.at(prefix),
                 ToXmlChar(array_name.data()));
  xmlAddChild(node_, array_parent_node);

  xmlNsPtr rdf_prefix_ns = namespaces_.at(XmlConst::RdfPrefix());
  xmlNodePtr seq_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfSeq()));
  xmlSetNs(seq_node, rdf_prefix_ns);
  xmlAddChild(array_parent_node, seq_node);
  for (float value : values) {
    xmlNodePtr li_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfLi()));
    xmlSetNs(li_node, rdf_prefix_ns);
    xmlAddChild(seq_node, li_node);
    xmlNodeSetContent(
        li_node, ToXmlChar(dynamic_depth::strings::SimpleFtoa(value).c_str()));
  }

  return true;
}

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth
