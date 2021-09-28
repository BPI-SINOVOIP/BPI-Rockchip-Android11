#ifndef DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_DATA_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_DATA_H_  // NOLINT

#include <libxml/tree.h>

namespace dynamic_depth {
namespace xmpmeta {

// XmpData contains the standard, and optionally extended, XMP metadata from a
// JPEG file. See xmp_parser for reading XmpData from a JPEG or reading
// attributes from XmpData.
class XmpData {
 public:
  XmpData();
  ~XmpData();

  // Frees any allocated resources and resets the xmlDocPtrs to null.
  void Reset();

  // The standard XMP section.
  const xmlDocPtr StandardSection() const;
  xmlDocPtr* MutableStandardSection();

  // The extended XMP section.
  const xmlDocPtr ExtendedSection() const;
  xmlDocPtr* MutableExtendedSection();

 private:
  xmlDocPtr xmp_;
  xmlDocPtr xmp_extended_;
};

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_DATA_H_  // NOLINT
