#include "xmpmeta/xmp_parser.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stack>

#include "android-base/logging.h"
#include "strings/case.h"
#include "strings/numbers.h"
#include "xmpmeta/base64.h"
#include "xmpmeta/jpeg_io.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/deserializer_impl.h"
#include "xmpmeta/xml/search.h"
#include "xmpmeta/xml/utils.h"
#include "xmpmeta/xmp_const.h"

using ::dynamic_depth::xmpmeta::xml::DepthFirstSearch;
using ::dynamic_depth::xmpmeta::xml::DeserializerImpl;
using ::dynamic_depth::xmpmeta::xml::FromXmlChar;
using ::dynamic_depth::xmpmeta::xml::GetFirstDescriptionElement;

namespace dynamic_depth {
namespace xmpmeta {
namespace {

const char kJpgExtension[] = "jpg";
const char kJpegExtension[] = "jpeg";

bool BoolStringToBool(const string& bool_str, bool* value) {
  if (dynamic_depth::StringCaseEqual(bool_str, "true")) {
    *value = true;
    return true;
  }
  if (dynamic_depth::StringCaseEqual(bool_str, "false")) {
    *value = false;
    return true;
  }
  return false;
}

// Converts string_property to the type T.
template <typename T>
bool ConvertStringPropertyToType(const string& string_property, T* value);

// Gets the end of the XMP meta content. If there is no packet wrapper, returns
// data.length, otherwise returns 1 + the position of last '>' without '?'
// before it. Usually the packet wrapper end is "<?xpacket end="w"?>.
size_t GetXmpContentEnd(const string& data) {
  if (data.empty()) {
    return 0;
  }
  for (size_t i = data.size() - 1; i >= 1; --i) {
    if (data[i] == '>') {
      if (data[i - 1] != '?') {
        return i + 1;
      }
    }
  }
  // It should not reach here for a valid XMP meta.
  LOG(WARNING) << "Failed to find the end of the XMP meta content.";
  return data.size();
}

// True if 's' starts with substring 'x'.
bool StartsWith(const string& s, const string& x) {
  return s.size() >= x.size() && !s.compare(0, x.size(), x);
}
// True if 's' ends with substring 'x'.
bool EndsWith(const string& s, const string& x) {
  return s.size() >= x.size() && !s.compare(s.size() - x.size(), x.size(), x);
}

// Parses the first valid XMP section. Any other valid XMP section will be
// ignored.
bool ParseFirstValidXMPSection(const std::vector<Section>& sections,
                               XmpData* xmp) {
  for (const Section& section : sections) {
    if (StartsWith(section.data, XmpConst::Header())) {
      const size_t end = GetXmpContentEnd(section.data);
      // Increment header length by 1 for the null termination.
      const size_t header_length = strlen(XmpConst::Header()) + 1;
      // Check for integer underflow before subtracting.
      if (header_length >= end) {
        LOG(ERROR) << "Invalid content length: "
                   << static_cast<int>(end - header_length);
        return false;
      }
      const size_t content_length = end - header_length;
      // header_length is guaranteed to be <= data.size due to the if condition
      // above. If this contract changes we must add an additonal check.
      const char* content_start = &section.data[header_length];
      // xmlReadMemory requires an int. Before casting size_t to int we must
      // check for integer overflow.
      if (content_length > INT_MAX) {
        LOG(ERROR) << "First XMP section too large, size: " << content_length;
        return false;
      }
      *xmp->MutableStandardSection() = xmlReadMemory(
          content_start, static_cast<int>(content_length), nullptr, nullptr, 0);
      if (xmp->StandardSection() == nullptr) {
        LOG(WARNING) << "Failed to parse standard section.";
        return false;
      }
      return true;
    }
  }
  return false;
}

// Collects the extended XMP sections with the given name into a string. Other
// sections will be ignored.
string GetExtendedXmpSections(const std::vector<Section>& sections,
                              const string& section_name) {
  string extended_header = XmpConst::ExtensionHeader();
  extended_header += '\0' + section_name;
  // section_name is dynamically extracted from the xml file and can have an
  // arbitrary size. Check for integer overflow before addition.
  if (extended_header.size() > SIZE_MAX - XmpConst::ExtensionHeaderOffset()) {
    return "";
  }
  const size_t section_start_offset =
      extended_header.size() + XmpConst::ExtensionHeaderOffset();

  // Compute the size of the buffer to parse the extended sections.
  std::vector<const Section*> xmp_sections;
  std::vector<size_t> xmp_end_offsets;
  size_t buffer_size = 0;
  for (const Section& section : sections) {
    if (extended_header.empty() || StartsWith(section.data, extended_header)) {
      const size_t end_offset = section.data.size();
      const size_t section_size = end_offset - section_start_offset;
      if (end_offset < section_start_offset ||
          section_size > SIZE_MAX - buffer_size) {
        return "";
      }
      buffer_size += section_size;
      xmp_sections.push_back(&section);
      xmp_end_offsets.push_back(end_offset);
    }
  }

  // Copy all the relevant sections' data into a buffer.
  string buffer(buffer_size, '\0');
  if (buffer.size() != buffer_size) {
    return "";
  }
  size_t offset = 0;
  for (int i = 0; i < xmp_sections.size(); ++i) {
    const Section* section = xmp_sections[i];
    const size_t length = xmp_end_offsets[i] - section_start_offset;
    std::copy_n(&section->data[section_start_offset], length, &buffer[offset]);
    offset += length;
  }
  return buffer;
}

// Parses the extended XMP sections with the given name. All other sections
// will be ignored.
bool ParseExtendedXmpSections(const std::vector<Section>& sections,
                              const string& section_name, XmpData* xmp_data) {
  const string extended_sections =
      GetExtendedXmpSections(sections, section_name);
  // xmlReadMemory requires an int. Before casting size_t to int we must check
  // for integer overflow.
  if (extended_sections.size() > INT_MAX) {
    LOG(WARNING) << "Extended sections too large, size: "
                 << extended_sections.size();
    return false;
  }
  *xmp_data->MutableExtendedSection() = xmlReadMemory(
      extended_sections.data(), static_cast<int>(extended_sections.size()),
      nullptr, nullptr, XML_PARSE_HUGE);
  if (xmp_data->ExtendedSection() == nullptr) {
    LOG(WARNING) << "Failed to parse extended sections.";
    return false;
  }
  return true;
}

// Extracts a XmpData from a JPEG image stream.
bool ExtractXmpMeta(const bool skip_extended, std::istream* file,
                    XmpData* xmp_data) {
  // We cannot use CHECK because this is ported to AOSP.
  assert(xmp_data != nullptr);  // NOLINT
  xmp_data->Reset();

  ParseOptions parse_options;
  parse_options.read_meta_only = true;
  if (skip_extended) {
    parse_options.section_header = XmpConst::Header();
    parse_options.section_header_return_first = true;
  }
  const std::vector<Section> sections = Parse(parse_options, file);
  if (sections.empty()) {
    LOG(WARNING) << "No sections found.";
    return false;
  }

  if (!ParseFirstValidXMPSection(sections, xmp_data)) {
    LOG(WARNING) << "Could not parse first section.";
    return false;
  }
  if (skip_extended) {
    return true;
  }
  string extension_name;
  DeserializerImpl deserializer(
      GetFirstDescriptionElement(xmp_data->StandardSection()));
  if (!deserializer.ParseString(XmpConst::HasExtensionPrefix(),
                                XmpConst::HasExtension(), &extension_name)) {
    // No extended sections present, so nothing to parse.
    return true;
  }
  if (!ParseExtendedXmpSections(sections, extension_name, xmp_data)) {
    LOG(WARNING) << "Extended sections present, but could not be parsed.";
    return false;
  }
  return true;
}

// Extracts the specified string attribute.
bool GetStringProperty(const xmlNodePtr node, const char* prefix,
                       const char* property, string* value) {
  const xmlDocPtr doc = node->doc;
  for (const _xmlAttr* attribute = node->properties; attribute != nullptr;
       attribute = attribute->next) {
    if (attribute->ns &&
        strcmp(FromXmlChar(attribute->ns->prefix), prefix) == 0 &&
        strcmp(FromXmlChar(attribute->name), property) == 0) {
      xmlChar* attribute_string =
          xmlNodeListGetString(doc, attribute->children, 1);
      *value = FromXmlChar(attribute_string);
      xmlFree(attribute_string);
      return true;
    }
  }
  return false;
}

// Reads the contents of a node.
// E.g. <prefix:node_name>Contents Here</prefix:node_name>
bool ReadNodeContent(const xmlNodePtr node, const char* prefix,
                     const char* node_name, string* value) {
  auto* element = DepthFirstSearch(node, node_name);
  if (element == nullptr) {
    return false;
  }
  if (prefix != nullptr &&
      (element->ns == nullptr || element->ns->prefix == nullptr ||
       strcmp(FromXmlChar(element->ns->prefix), prefix) != 0)) {
    return false;
  }
  xmlChar* node_content = xmlNodeGetContent(element);
  *value = FromXmlChar(node_content);
  free(node_content);
  return true;
}

template <typename T>
bool ConvertStringPropertyToType(const string& string_property, T* value) {
  QCHECK(value) << "Cannot call this method on a generic type";
  return false;
}

template <>
bool ConvertStringPropertyToType<bool>(const string& string_property,
                                       bool* value) {
  return BoolStringToBool(string_property, value);
}

template <>
bool ConvertStringPropertyToType<double>(const string& string_property,
                                         double* value) {
  *value = std::stod(string_property);
  return true;
}

template <>
bool ConvertStringPropertyToType<int>(const string& string_property,
                                      int* value) {
  *value = 0;
  for (int i = 0; i < string_property.size(); ++i) {
    if (!isdigit(string_property[i])) {
      return false;
    }
  }

  *value = std::atoi(string_property.c_str());  // NOLINT
  return true;
}

template <>
bool ConvertStringPropertyToType<int64>(const string& string_property,
                                        int64* value) {
  *value = std::stol(string_property);
  return true;
}

}  // namespace

bool ReadXmpHeader(const string& filename, const bool skip_extended,
                   XmpData* xmp_data) {
  string filename_lower = filename;
  std::transform(filename_lower.begin(), filename_lower.end(),
                 filename_lower.begin(), ::tolower);
  if (!EndsWith(filename_lower, kJpgExtension) &&
      !EndsWith(filename_lower, kJpegExtension)) {
    LOG(WARNING) << "XMP parse: only JPEG file is supported";
    return false;
  }

  std::ifstream file(filename.c_str(), std::ios::binary);
  if (!file.is_open()) {
    LOG(WARNING) << " Could not read file: " << filename;
    return false;
  }
  return ExtractXmpMeta(skip_extended, &file, xmp_data);
}

bool ReadXmpFromMemory(const string& jpeg_contents, const bool skip_extended,
                       XmpData* xmp_data) {
  std::istringstream stream(jpeg_contents);
  return ExtractXmpMeta(skip_extended, &stream, xmp_data);
}

bool ReadXmpHeader(std::istream* input_stream, bool skip_extended,
                   XmpData* xmp_data) {
  return ExtractXmpMeta(skip_extended, input_stream, xmp_data);
}

}  // namespace xmpmeta
}  // namespace dynamic_depth
