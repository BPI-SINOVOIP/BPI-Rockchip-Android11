#ifndef DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_WRITER_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_WRITER_H_  // NOLINT

#include <iostream>
#include <memory>
#include <string>

#include "base/port.h"
#include "xmpmeta/xmp_data.h"

namespace dynamic_depth {
namespace xmpmeta {

// Creates a new XmpData object and initializes the boilerplate for the
// standard XMP section.
// The extended section is initialized only if create_extended is true.
std::unique_ptr<XmpData> CreateXmpData(bool create_extended);

// Writes  XMP data to an existing JPEG image file.
// This is equivalent to writeXMPMeta in geo/lightfield/metadata/XmpUtil.java.
// If the extended section is not null, this will modify the given XmpData by
// setting a property in the standard section that links it with the
// extended section.
bool WriteLeftEyeAndXmpMeta(const string& left_data, const string& filename,
                            const XmpData& xmp_data);

// Same as above, but allows the caller to manage their own istream and ostream.
// filename is written to only if metadata serialization is successful.
// Assumes the caller will take care of opening and closing the
// output_jpeg_stream (if it is associated with a file), as well as
// initialization of the input_jpeg_stream. This is nearly equivalent to
// writeXMPMeta in kgeo/lightfield/metadata/XmpUtil.java.
bool WriteLeftEyeAndXmpMeta(const XmpData& xmp_data,
                            std::istream* input_jpeg_stream,
                            std::ostream* output_jpeg_stream);

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_XMPMETA_XMP_WRITER_H_  // NOLINT
