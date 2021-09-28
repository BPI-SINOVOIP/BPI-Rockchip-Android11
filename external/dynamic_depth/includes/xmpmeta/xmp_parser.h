#ifndef DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_PARSER_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_PARSER_H_  // NOLINT

#include <fstream>
#include <string>

#include "base/port.h"
#include "xmpmeta/xmp_data.h"

namespace dynamic_depth {
namespace xmpmeta {

// Populates a XmpData from the header of the JPEG file.
bool ReadXmpHeader(const string& filename, bool skip_extended,
                   XmpData* xmp_data);

// Populates a XmpData from the header of JPEG file that has already been read
// into memory.
bool ReadXmpFromMemory(const string& jpeg_contents, bool skip_extended,
                       XmpData* xmp_data);

// Populates a XmpData from the header of the given stream (stream data is
// in JPEG format).
bool ReadXmpHeader(std::istream* input_stream, bool skip_extended,
                   XmpData* xmp_data);
}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_PARSER_H_  // NOLINT
