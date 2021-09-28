#ifndef DYNAMIC_DEPTH_INCLUDES_XMPMETA_JPEG_IO_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_XMPMETA_JPEG_IO_H_  // NOLINT

#include <string>
#include <vector>

#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {

// Contains the data for a section in a JPEG file.
// A JPEG file contains many sections in addition to image data.
struct Section {
  // Constructors.
  Section() = default;
  explicit Section(const string& buffer);

  // Returns true if the section's marker matches an APP1 marker.
  bool IsMarkerApp1();

  int marker;
  bool is_image_section;
  string data;
};

struct ParseOptions {
  // If set to true, keeps only the EXIF and XMP sections (with
  // marker kApp1) and ignores others. Otherwise, keeps everything including
  // image data.
  bool read_meta_only = false;

  // If section_header is set, this boolean controls whether only the 1st
  // section matching the section_header will be returned. If not set
  // (the default), all the sections that math the section header will be
  // returned.
  bool section_header_return_first = false;

  // A filter that keeps all the sections whose data starts with the
  // given string. Ignored if empty.
  string section_header;
};

// Parses the JPEG image file.
std::vector<Section> Parse(const ParseOptions& options,
                           std::istream* input_stream);

// Writes JPEG data sections to a file.
void WriteSections(const std::vector<Section>& sections,
                   std::ostream* output_stream);

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_XMPMETA_JPEG_IO_H_  // NOLINT
