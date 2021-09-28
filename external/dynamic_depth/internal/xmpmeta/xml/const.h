#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_CONST_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_CONST_H_  // NOLINT

namespace dynamic_depth {
namespace xmpmeta {
namespace xml {

struct XmlConst {
  // Encoding type.
  static const char* EncodingStr();

  // RDF metadata.
  static const char* RdfAbout();
  static const char* RdfDescription();
  static const char* RdfNodeName();
  static const char* RdfNodeNs();
  static const char* RdfPrefix();
  static const char* RdfSeq();
  static const char* RdfLi();
  static const char* RdfParseType();

  // XML metadata.
  static const char* NsAttrName();
  static const char* Separator();
  static const char* Version();
};

}  // namespace xml
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_XML_CONST_H_  // NOLINT
