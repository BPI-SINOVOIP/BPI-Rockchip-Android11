#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_UTILS_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_UTILS_H_  // NOLINT

#include <libxml/tree.h>

#include <string>

#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Convenience function to convert an xmlChar* to a char*
inline const char* FromXmlChar(const xmlChar* in) {
  return reinterpret_cast<const char*>(in);
}

// Convenience function to convert a char* to an xmlChar*.
inline const xmlChar* ToXmlChar(const char* in) {
  return reinterpret_cast<const xmlChar*>(in);
}

// Returns the first rdf:Description node; null if not found.
xmlNodePtr GetFirstDescriptionElement(xmlDocPtr parent);

// Returns the first rdf:Seq element found in the XML document.
xmlNodePtr GetFirstSeqElement(xmlDocPtr parent);

// Returns the first rdf:Seq element found in the given node.
// Returns {@code parent} if that is itself an rdf:Seq node.
xmlNodePtr GetFirstSeqElement(xmlNodePtr parent);

// Returns the ith (zero-indexed) rdf:li node in the given rdf:Seq node.
// Returns null if either of {@code index} < 0, {@code node} is null, or is
// not an rdf:Seq node.
xmlNodePtr GetElementAt(xmlNodePtr node, int index);

// Returns the value in an rdf:li node. This is for a node whose value
// does not have a name, e.g. <rdf:li>value</rdf:li>.
// If the given rdf:li node has a nested node, it returns the string
// representation of the contents of those nodes, which replaces the XML
// tags with one whitespace character for each tag character.
// This is treated as undefined behavior; it is the caller's responsibility
// to remove any whitespace and newlines.
const string GetLiNodeContent(xmlNodePtr node);

// Returns the given XML doc serialized to a string.
// For debugging purposes.
const string XmlDocToString(const xmlDocPtr doc);

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_UTILS_H_  // NOLINT
