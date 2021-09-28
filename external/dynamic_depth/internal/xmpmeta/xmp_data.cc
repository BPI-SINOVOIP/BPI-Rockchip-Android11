#include "xmpmeta/xmp_data.h"

namespace dynamic_depth {
namespace xmpmeta {

XmpData::XmpData() : xmp_(nullptr), xmp_extended_(nullptr) {}

XmpData::~XmpData() { Reset(); }

void XmpData::Reset() {
  if (xmp_) {
    xmlFreeDoc(xmp_);
    xmp_ = nullptr;
  }
  if (xmp_extended_) {
    xmlFreeDoc(xmp_extended_);
    xmp_extended_ = nullptr;
  }
}

const xmlDocPtr XmpData::StandardSection() const { return xmp_; }

xmlDocPtr* XmpData::MutableStandardSection() { return &xmp_; }

const xmlDocPtr XmpData::ExtendedSection() const { return xmp_extended_; }

xmlDocPtr* XmpData::MutableExtendedSection() { return &xmp_extended_; }

}  // namespace xmpmeta
}  // namespace dynamic_depth
