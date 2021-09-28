#include "xmpmeta/xml/utils.h"

#include "android-base/logging.h"
#include "base/port.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/search.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

xmlNodePtr GetFirstDescriptionElement(const xmlDocPtr parent) {
  return DepthFirstSearch(parent, XmlConst::RdfDescription());
}

xmlNodePtr GetFirstSeqElement(xmlDocPtr parent) {
  // DepthFirstSearch will perform the null check.
  return DepthFirstSearch(parent, XmlConst::RdfSeq());
}

// Returns the first rdf:Seq element found in the given node.
xmlNodePtr GetFirstSeqElement(xmlNodePtr parent) {
  // DepthFirstSearch will perform the null check.
  return DepthFirstSearch(parent, XmlConst::RdfSeq());
}

// Returns the ith (zero-indexed) element in the given node.
// {@code parent} is an rdf:Seq node.
xmlNodePtr GetElementAt(xmlNodePtr node, int index) {
  if (node == nullptr || index < 0) {
    LOG(ERROR) << "Node was null or index was negative";
    return nullptr;
  }
  const string node_name = FromXmlChar(node->name);
  if (strcmp(node_name.c_str(), XmlConst::RdfSeq())) {
    LOG(ERROR) << "Node is not an rdf:Seq node, was " << node_name;
    return nullptr;
  }
  int i = 0;
  for (xmlNodePtr child = node->children; child != nullptr && i <= index;
       child = child->next) {
    if (strcmp(FromXmlChar(child->name), XmlConst::RdfLi())) {
      // This is not an rdf:li node. This can occur because the node's content
      // is also treated as a node, and these should be ignored.
      continue;
    }
    if (i == index) {
      return child;
    }
    i++;
  }
  return nullptr;
}

const string GetLiNodeContent(xmlNodePtr node) {
  string value;
  if (node == nullptr || strcmp(FromXmlChar(node->name), XmlConst::RdfLi())) {
    LOG(ERROR) << "Node is null or is not an rdf:li node";
    return value;
  }
  xmlChar* node_content = xmlNodeGetContent(node);
  value = FromXmlChar(node_content);
  free(node_content);
  return value;
}

const string XmlDocToString(const xmlDocPtr doc) {
  xmlChar* xml_doc_contents;
  int doc_size = 0;
  xmlDocDumpFormatMemoryEnc(doc, &xml_doc_contents, &doc_size,
                            XmlConst::EncodingStr(), 1);
  const string xml_doc_string(FromXmlChar(xml_doc_contents));
  xmlFree(xml_doc_contents);
  return xml_doc_string;
}

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth
