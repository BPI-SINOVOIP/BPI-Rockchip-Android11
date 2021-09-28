#include "xmpmeta/xmp_writer.h"

#include <libxml/tree.h>
#include <libxml/xmlIO.h>
#include <libxml/xmlstring.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "android-base/logging.h"
#include "xmpmeta/jpeg_io.h"
#include "xmpmeta/md5.h"
#include "xmpmeta/xml/const.h"
#include "xmpmeta/xml/utils.h"
#include "xmpmeta/xmp_const.h"
#include "xmpmeta/xmp_data.h"
#include "xmpmeta/xmp_parser.h"

using ::dynamic_depth::xmpmeta::xml::FromXmlChar;
using ::dynamic_depth::xmpmeta::xml::GetFirstDescriptionElement;
using ::dynamic_depth::xmpmeta::xml::ToXmlChar;
using ::dynamic_depth::xmpmeta::xml::XmlConst;

namespace dynamic_depth {
namespace xmpmeta {
namespace {

const char kXmlStartTag = '<';

const char kCEmptyString[] = "\x00";
const int kXmlDumpFormat = 1;
const int kInvalidIndex = -1;

// True if 's' starts with substring 'x'.
bool StartsWith(const string& s, const string& x) {
  return s.size() >= x.size() && !s.compare(0, x.size(), x);
}
// True if 's' ends with substring 'x'.
bool EndsWith(const string& s, const string& x) {
  return s.size() >= x.size() && !s.compare(s.size() - x.size(), x.size(), x);
}

// Creates the outer rdf:RDF node for XMP.
xmlNodePtr CreateXmpRdfNode() {
  xmlNodePtr rdf_node = xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfNodeName()));
  xmlNsPtr rdf_ns = xmlNewNs(rdf_node, ToXmlChar(XmlConst::RdfNodeNs()),
                             ToXmlChar(XmlConst::RdfPrefix()));
  xmlSetNs(rdf_node, rdf_ns);
  return rdf_node;
}

// Creates the root node for XMP.
xmlNodePtr CreateXmpRootNode() {
  xmlNodePtr root_node = xmlNewNode(nullptr, ToXmlChar(XmpConst::NodeName()));
  xmlNsPtr root_ns = xmlNewNs(root_node, ToXmlChar(XmpConst::Namespace()),
                              ToXmlChar(XmpConst::NamespacePrefix()));
  xmlSetNs(root_node, root_ns);
  xmlSetNsProp(root_node, root_ns, ToXmlChar(XmpConst::AdobePropName()),
               ToXmlChar(XmpConst::AdobePropValue()));
  return root_node;
}

// Creates a new XMP metadata section, with an x:xmpmeta element wrapping
// rdf:RDF and rdf:Description child elements. This is the equivalent of
// createXMPMeta in geo/lightfield/metadata/XmpUtils.java
xmlDocPtr CreateXmpSection() {
  xmlDocPtr xmp_meta = xmlNewDoc(ToXmlChar(XmlConst::Version()));

  xmlNodePtr root_node = CreateXmpRootNode();
  xmlNodePtr rdf_node = CreateXmpRdfNode();
  xmlNodePtr description_node =
      xmlNewNode(nullptr, ToXmlChar(XmlConst::RdfDescription()));
  xmlNsPtr rdf_prefix_ns =
      xmlNewNs(description_node, nullptr, ToXmlChar(XmlConst::RdfPrefix()));
  xmlSetNs(description_node, rdf_prefix_ns);

  // rdf:about is mandatory.
  xmlSetNsProp(description_node, rdf_node->ns, ToXmlChar(XmlConst::RdfAbout()),
               ToXmlChar(""));

  // Align nodes into the proper hierarchy.
  xmlAddChild(rdf_node, description_node);
  xmlAddChild(root_node, rdf_node);
  xmlDocSetRootElement(xmp_meta, root_node);

  return xmp_meta;
}

void WriteIntTo4Bytes(int integer, std::ostream* output_stream) {
  output_stream->put((integer >> 24) & 0xff);
  output_stream->put((integer >> 16) & 0xff);
  output_stream->put((integer >> 8) & 0xff);
  output_stream->put(integer & 0xff);
}

// Serializes an XML document to a string.
void SerializeMeta(const xmlDocPtr parent, string* serialized_value) {
  if (parent == nullptr || parent->children == nullptr) {
    LOG(WARNING) << "Nothing to serialize, either XML doc is null or it has "
                 << "no elements";
    return;
  }

  std::ostringstream serialized_stream;
  xmlChar* xml_doc_contents;
  int doc_size = 0;
  xmlDocDumpFormatMemoryEnc(parent, &xml_doc_contents, &doc_size,
                            XmlConst::EncodingStr(), kXmlDumpFormat);
  const char* xml_doc_string = FromXmlChar(xml_doc_contents);

  // Find the index of the second "<" so we can discard the first element,
  // which is <?xml version...>, so start searching after the first "<". XMP
  // starts directly afterwards.
  const int xmp_start_idx =
      static_cast<int>(strchr(&xml_doc_string[2], kXmlStartTag) -
                       xml_doc_string) -
      1;
  serialized_stream.write(&xml_doc_string[xmp_start_idx],
                          doc_size - xmp_start_idx);
  xmlFree(xml_doc_contents);
  *serialized_value = serialized_stream.str();
}

// TODO(miraleung): Switch to different library for Android if needed.
const string GetGUID(const string& to_hash) { return MD5Hash(to_hash); }

// Creates the standard XMP section.
void CreateStandardSectionXmpString(const string& buffer, string* value) {
  std::ostringstream data_stream;
  data_stream.write(XmpConst::Header(), strlen(XmpConst::Header()));
  data_stream.write(kCEmptyString, 1);
  data_stream.write(buffer.c_str(), buffer.length());
  *value = data_stream.str();
}

// Creates the extended XMP section.
void CreateExtendedSections(const string& buffer,
                            std::vector<Section>* extended_sections) {
  string guid = GetGUID(buffer);
  // Increment by 1 for the null byte in the middle.
  const int header_length =
      static_cast<int>(strlen(XmpConst::ExtensionHeader()) + 1 + guid.length());
  const int buffer_length = static_cast<int>(buffer.length());
  const int overhead = header_length + XmpConst::ExtensionHeaderOffset();
  const int num_sections =
      buffer_length / (XmpConst::ExtendedMaxBufferSize() - overhead) + 1;
  for (int i = 0, position = 0; i < num_sections; ++i) {
    const int section_size =
        std::min(static_cast<int>(buffer_length - position + overhead),
                 XmpConst::ExtendedMaxBufferSize());
    const int bytes_from_buffer = section_size - overhead;

    // Header and GUID.
    std::ostringstream data_stream;
    data_stream.write(XmpConst::ExtensionHeader(),
                      strlen(XmpConst::ExtensionHeader()));
    data_stream.write(kCEmptyString, 1);
    data_stream.write(guid.c_str(), guid.length());

    // Total buffer length.
    WriteIntTo4Bytes(buffer_length, &data_stream);
    // Current position.
    WriteIntTo4Bytes(position, &data_stream);
    // Data
    data_stream.write(&buffer[position], bytes_from_buffer);
    position += bytes_from_buffer;

    extended_sections->push_back(Section(data_stream.str()));
  }
}

int InsertStandardXMPSection(const string& buffer,
                             std::vector<Section>* sections) {
  if (buffer.length() > XmpConst::MaxBufferSize()) {
    LOG(WARNING) << "The standard XMP section (at size " << buffer.length()
                 << ") cannot have a size larger than "
                 << XmpConst::MaxBufferSize() << " bytes";
    return kInvalidIndex;
  }
  string value;
  CreateStandardSectionXmpString(buffer, &value);
  Section xmp_section(value);
  // If we can find the old XMP section, replace it with the new one
  for (int index = 0; index < sections->size(); ++index) {
    if (sections->at(index).IsMarkerApp1() &&
        StartsWith(sections->at(index).data, XmpConst::Header())) {
      // Replace with the new XMP data.
      sections->at(index) = xmp_section;
      return index;
    }
  }
  // If the first section is EXIF, insert XMP data after it.
  // Otherwise, make XMP data the first section.
  const int position =
      (!sections->empty() && sections->at(0).IsMarkerApp1()) ? 1 : 0;
  sections->emplace(sections->begin() + position, xmp_section);
  return position;
}

// Position is the index in the Section vector where the extended sections
// will be inserted.
void InsertExtendedXMPSections(const string& buffer, int position,
                               std::vector<Section>* sections) {
  std::vector<Section> extended_sections;
  CreateExtendedSections(buffer, &extended_sections);
  sections->insert(sections->begin() + position, extended_sections.begin(),
                   extended_sections.end());
}

// Returns true if the respective sections in xmp_data and their serialized
// counterparts are (correspondingly) not null and not empty.
bool XmpSectionsAndSerializedDataValid(const XmpData& xmp_data,
                                       const string& main_buffer,
                                       const string& extended_buffer) {
  // Standard section and its serialized counterpart cannot be null/empty.
  // Extended section can be null XOR the extended buffer can be empty.
  const bool extended_is_consistent =
      ((xmp_data.ExtendedSection() == nullptr) == extended_buffer.empty());
  const bool is_valid = (xmp_data.StandardSection() != nullptr) &&
                        !main_buffer.empty() && extended_is_consistent;
  if (!is_valid) {
    LOG(ERROR) << "XMP sections Xor their serialized counterparts are empty";
  }
  return is_valid;
}

// Updates a list of JPEG sections with serialized XMP data.
bool UpdateSections(const string& main_buffer, const string& extended_buffer,
                    std::vector<Section>* sections) {
  if (main_buffer.empty()) {
    LOG(WARNING) << "Main section was empty";
    return false;
  }

  // Update the list of sections with the new standard XMP section.
  const int main_index = InsertStandardXMPSection(main_buffer, sections);
  if (main_index < 0) {
    LOG(WARNING) << "Could not find a valid index for inserting the "
                 << "standard sections";
    return false;
  }

  // Insert the extended section right after the main section.
  if (!extended_buffer.empty()) {
    InsertExtendedXMPSections(extended_buffer, main_index + 1, sections);
  }
  return true;
}

void LinkXmpStandardAndExtendedSections(const string& extended_buffer,
                                        xmlDocPtr standard_section) {
  xmlNodePtr description_node = GetFirstDescriptionElement(standard_section);
  xmlNsPtr xmp_note_ns_ptr =
      xmlNewNs(description_node, ToXmlChar(XmpConst::NoteNamespace()),
               ToXmlChar(XmpConst::HasExtensionPrefix()));
  const string extended_id = GetGUID(extended_buffer);
  xmlSetNsProp(description_node, xmp_note_ns_ptr,
               ToXmlChar(XmpConst::HasExtension()),
               ToXmlChar(extended_id.c_str()));
  xmlUnsetProp(description_node, ToXmlChar(XmpConst::HasExtension()));
}

}  // namespace

std::unique_ptr<XmpData> CreateXmpData(bool create_extended) {
  std::unique_ptr<XmpData> xmp_data(new XmpData());
  *xmp_data->MutableStandardSection() = CreateXmpSection();
  if (create_extended) {
    *xmp_data->MutableExtendedSection() = CreateXmpSection();
  }
  return xmp_data;
}

bool WriteLeftEyeAndXmpMeta(const string& left_data, const string& filename,
                            const XmpData& xmp_data) {
  std::istringstream input_jpeg_stream(left_data);
  std::ofstream output_jpeg_stream;
  output_jpeg_stream.open(filename, std::ostream::out);
  bool success =
      WriteLeftEyeAndXmpMeta(xmp_data, &input_jpeg_stream, &output_jpeg_stream);
  output_jpeg_stream.close();
  return success;
}

bool WriteLeftEyeAndXmpMeta(const XmpData& xmp_data,
                            std::istream* input_jpeg_stream,
                            std::ostream* output_jpeg_stream) {
  if (input_jpeg_stream == nullptr || output_jpeg_stream == nullptr) {
    LOG(ERROR) << "Input and output streams must both be non-null";
    return false;
  }

  // Get a list of sections from the input stream.
  ParseOptions parse_options;
  std::vector<Section> sections = Parse(parse_options, input_jpeg_stream);

  string extended_buffer;
  if (xmp_data.ExtendedSection() != nullptr) {
    SerializeMeta(xmp_data.ExtendedSection(), &extended_buffer);
    LinkXmpStandardAndExtendedSections(extended_buffer,
                                       xmp_data.StandardSection());
  }
  string main_buffer;
  SerializeMeta(xmp_data.StandardSection(), &main_buffer);

  // Update the input sections with the XMP data.
  if (!XmpSectionsAndSerializedDataValid(xmp_data, main_buffer,
                                         extended_buffer) ||
      !UpdateSections(main_buffer, extended_buffer, &sections)) {
    return false;
  }

  WriteSections(sections, output_jpeg_stream);
  return true;
}

}  // namespace xmpmeta
}  // namespace dynamic_depth
