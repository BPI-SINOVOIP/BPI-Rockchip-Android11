#include "xmpmeta/jpeg_io.h"

#include <fstream>
#include <sstream>

#include "android-base/logging.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace {

// File markers.
// See: http://www.fileformat.info/format/jpeg/egff.htm or
// https://en.wikipedia.org/wiki/JPEG
const int kSoi = 0xd8;   // Start of image marker.
const int kApp1 = 0xe1;  // Start of EXIF section.
const int kSos = 0xda;   // Start of scan.

// Number of bytes used to store a section's length in a JPEG file.
const int kSectionLengthByteSize = 2;

// Returns the number of bytes available to be read. Sets the seek position
// to the place it was in before calling this function.
size_t GetBytesAvailable(std::istream* input_stream) {
  const std::streamoff pos = input_stream->tellg();
  if (pos == -1) {
    return 0;
  }

  input_stream->seekg(0, std::ios::end);
  if (!input_stream->good()) {
    return 0;
  }

  const std::streamoff end = input_stream->tellg();
  if (end == -1) {
    return 0;
  }
  input_stream->seekg(pos);

  if (end <= pos) {
    return 0;
  }
  return end - pos;
}

// Returns the first byte in the stream cast to an integer.
int ReadByteAsInt(std::istream* input_stream) {
  unsigned char byte;
  input_stream->read(reinterpret_cast<char*>(&byte), 1);
  if (!input_stream->good()) {
    // Return an invalid value - no byte can be read as -1.
    return -1;
  }
  return static_cast<int>(byte);
}

// Reads the length of a section from 2 bytes.
size_t Read2ByteLength(std::istream* input_stream, bool* error) {
  const int length_high = ReadByteAsInt(input_stream);
  const int length_low = ReadByteAsInt(input_stream);
  if (length_high == -1 || length_low == -1) {
    *error = true;
    return 0;
  }
  *error = false;
  return length_high << 8 | length_low;
}

bool HasPrefixString(const string& to_check, const string& prefix) {
  if (to_check.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), to_check.begin());
}

}  // namespace

Section::Section(const string& buffer) {
  marker = kApp1;
  is_image_section = false;
  data = buffer;
}

bool Section::IsMarkerApp1() { return marker == kApp1; }

std::vector<Section> Parse(const ParseOptions& options,
                           std::istream* input_stream) {
  std::vector<Section> sections;
  // Return early if this is not the start of a JPEG section.
  if (ReadByteAsInt(input_stream) != 0xff ||
      ReadByteAsInt(input_stream) != kSoi) {
    LOG(WARNING) << "File's first two bytes does not match the sequence \xff"
                 << kSoi;
    return std::vector<Section>();
  }

  int chr;  // Short for character.
  while ((chr = ReadByteAsInt(input_stream)) != -1) {
    if (chr != 0xff) {
      LOG(WARNING) << "Read non-padding byte: " << chr;
      return sections;
    }
    // Skip padding bytes.
    while ((chr = ReadByteAsInt(input_stream)) == 0xff) {
    }
    if (chr == -1) {
      LOG(WARNING) << "No more bytes in file available to be read.";
      return sections;
    }

    const int marker = chr;
    if (marker == kSos) {
      // kSos indicates the image data will follow and no metadata after that,
      // so read all data at one time.
      if (!options.read_meta_only) {
        Section section;
        section.marker = marker;
        section.is_image_section = true;
        const size_t bytes_available = GetBytesAvailable(input_stream);
        section.data.resize(bytes_available);
        input_stream->read(&section.data[0], bytes_available);
        if (input_stream->good()) {
          sections.push_back(section);
        }
      }
      // All sections have been read.
      return sections;
    }

    bool error;
    const size_t length = Read2ByteLength(input_stream, &error);
    if (error || length < kSectionLengthByteSize) {
      // No sections to read.
      LOG(WARNING) << "No sections to read; section length is " << length;
      return sections;
    }

    const size_t bytes_left = GetBytesAvailable(input_stream);
    if (length - kSectionLengthByteSize > bytes_left) {
      LOG(WARNING) << "Invalid section length = " << length
                   << " total bytes available = " << bytes_left;
      return sections;
    }

    if (!options.read_meta_only || marker == kApp1) {
      Section section;
      section.marker = marker;
      section.is_image_section = false;
      const size_t data_size = length - kSectionLengthByteSize;
      section.data.resize(data_size);
      if (section.data.size() != data_size) {
        LOG(WARNING) << "Discrepancy in section data size "
                     << section.data.size() << "and data size " << data_size;
        return sections;
      }
      input_stream->read(&section.data[0], section.data.size());
      if (input_stream->good() &&
          (options.section_header.empty() ||
           HasPrefixString(section.data, options.section_header))) {
        sections.push_back(section);
        // Return if we have specified to return the 1st section with
        // the given name.
        if (options.section_header_return_first) {
          return sections;
        }
      }
    } else {
      // Skip this section since all EXIF/XMP meta will be in kApp1 section.
      input_stream->ignore(length - kSectionLengthByteSize);
    }
  }
  return sections;
}

void WriteSections(const std::vector<Section>& sections,
                   std::ostream* output_stream) {
  output_stream->put(0xff);
  output_stream->put(static_cast<unsigned char>(kSoi));
  for (const Section& section : sections) {
    output_stream->put(0xff);
    output_stream->put(section.marker);
    if (!section.is_image_section) {
      const int section_length = static_cast<int>(section.data.length()) + 2;
      // It's not the image data.
      const int lh = section_length >> 8;
      const int ll = section_length & 0xff;
      output_stream->put(lh);
      output_stream->put(ll);
    }
    output_stream->write(section.data.c_str(), section.data.length());
  }
}

}  // namespace xmpmeta
}  // namespace dynamic_depth
