#include "xmpmeta/xmp_const.h"

namespace dynamic_depth {
namespace xmpmeta {

// XMP namespace constants.
const char* XmpConst::Namespace() { return "adobe:ns:meta/"; }

const char* XmpConst::NamespacePrefix() { return "x"; }

const char* XmpConst::NodeName() { return "xmpmeta"; }

const char* XmpConst::AdobePropName() { return "xmptk"; }

const char* XmpConst::AdobePropValue() { return "Adobe XMP"; }

const char* XmpConst::NoteNamespace() {
  return "http://ns.adobe.com/xmp/note/";
}

// XMP headers.
const char* XmpConst::Header() { return "http://ns.adobe.com/xap/1.0/"; }

const char* XmpConst::ExtensionHeader() {
  return "http://ns.adobe.com/xmp/extension/";
}

const char* XmpConst::HasExtensionPrefix() { return "xmpNote"; }

const char* XmpConst::HasExtension() { return "HasExtendedXMP"; }

// Sizes.
const int XmpConst::ExtensionHeaderOffset() { return 8; }

const int XmpConst::MaxBufferSize() { return 65502; }

const int XmpConst::ExtendedMaxBufferSize() { return 65458; }

}  // namespace xmpmeta
}  // namespace dynamic_depth
