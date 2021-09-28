#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SEARCH_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SEARCH_H_  // NOLINT

#include <libxml/tree.h>

// Performs searches an XML tree.
namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

// Depth-first search on the nodes in this XML doc.
// Performs Depth first search on the child XML elements in order.
// Returns the first child element with a matching node name. If not found,
// returns a null pointer.
xmlNodePtr DepthFirstSearch(const xmlDocPtr parent, const char* name);

// Returns the first child element with a matching prefix and name.
// If prefix is null or empty, this has the same effect as the method abouve.
// Otherwise, the resulting node's namespace and its name must not be null.
xmlNodePtr DepthFirstSearch(const xmlDocPtr parent, const char* prefix,
                            const char* name);

// Depth-first search on the parent, for a child element with the given name.
// The element name excludes its prefix.
// Returns a null pointer if no matching element is found.
xmlNodePtr DepthFirstSearch(const xmlNodePtr parent, const char* name);

// Returns the first child element with a matching prefix and name.
// If prefix is null or empty, this has the same effect as the method abouve.
// Otherwise, the resulting node's namespace and its name must not be null.
xmlNodePtr DepthFirstSearch(const xmlNodePtr parent, const char* prefix,
                            const char* name);

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_SEARCH_H_  // NOLINT
