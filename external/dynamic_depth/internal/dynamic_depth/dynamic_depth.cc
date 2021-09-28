#include "dynamic_depth/dynamic_depth.h"

#include <fstream>
#include <sstream>

#include "android-base/logging.h"
#include "dynamic_depth/container.h"
#include "dynamic_depth/item.h"
#include "image_io/gcontainer/gcontainer.h"
#include "xmpmeta/xmp_data.h"
#include "xmpmeta/xmp_writer.h"

namespace dynamic_depth {
namespace {

using ::dynamic_depth::xmpmeta::CreateXmpData;
using ::dynamic_depth::xmpmeta::XmpData;

constexpr char kImageMimePrefix[] = "image";

bool IsMimeTypeImage(const string& mime) {
  string mime_lower = mime;
  std::transform(mime_lower.begin(), mime_lower.end(), mime_lower.begin(),
                 ::tolower);
  return strncmp(mime_lower.c_str(), kImageMimePrefix, mime_lower.find("/")) ==
         0;
}

}  // namespace

bool WriteImageAndMetadataAndContainer(std::istream* input_jpeg_stream,
                                       Device* device,
                                       std::ostream* output_jpeg_stream) {
  const std::unique_ptr<XmpData> xmp_data = CreateXmpData(true);
  device->SerializeToXmp(xmp_data.get());
  bool success =
      WriteLeftEyeAndXmpMeta(*xmp_data, input_jpeg_stream, output_jpeg_stream);

  if (device->GetContainer() == nullptr) {
    return success;
  }

  // Append Container:Item elements' payloads.
  for (auto item : device->GetContainer()->GetItems()) {
    const string& payload = item->GetPayloadToSerialize();
    const unsigned int payload_size = item->GetLength();
    if (payload_size <= 0 || payload.empty()) {
      continue;
    }
    output_jpeg_stream->write(payload.c_str(), payload_size);
  }

  return success;
}

bool WriteImageAndMetadataAndContainer(const string& out_filename,
                                       const uint8_t* primary_image_bytes,
                                       size_t primary_image_bytes_count,
                                       Device* device) {
  std::istringstream input_jpeg_stream(
      std::string(reinterpret_cast<const char*>(primary_image_bytes),
                  primary_image_bytes_count));
  std::ofstream output_jpeg_stream;
  output_jpeg_stream.open(out_filename, std::ostream::out);
  bool success = WriteImageAndMetadataAndContainer(&input_jpeg_stream, device,
                                                   &output_jpeg_stream);
  output_jpeg_stream.close();
  return success;
}

bool GetItemPayload(const string& input_image_filename, const Device* device,
                    const string& item_uri, string* out_payload) {
  if (device == nullptr || device->GetContainer() == nullptr) {
    LOG(ERROR) << "No Container element to parse";
    return false;
  }

  return GetItemPayload(input_image_filename, device->GetContainer(), item_uri,
                        out_payload);
}

bool GetItemPayload(const string& input_image_filename,
                    const Container* container, const string& item_uri,
                    string* out_payload) {
  std::ifstream input_stream(input_image_filename);
  return GetItemPayload(container, item_uri, input_stream, out_payload);
}

bool GetItemPayload(const Container* container, const string& item_uri,
                    std::istream& input_jpeg_stream, string* out_payload) {
  if (container == nullptr) {
    LOG(ERROR) << "Container cannot be null";
    return false;
  }

  size_t file_offset = 0;
  size_t file_length = 0;
  int index = 0;
  bool is_mime_type_image = false;
  for (const auto& item : container->GetItems()) {
    is_mime_type_image = IsMimeTypeImage(item->GetMime());

    if (item_uri.compare(item->GetDataUri()) == 0) {
      // Found a matching item.
      file_length = item->GetLength();
      break;
    }

    file_offset += item->GetLength();
    index++;
  }

  if (file_length == 0) {
    if (index == 0 && is_mime_type_image) {
      LOG(INFO) << "Item references the primary image, Not populating data";
      return true;
    }

    // File length can be zero to indicate the primary image (checked above) or
    // use the last file in the list. If this check fails, it's not in this
    // state.
    if (file_offset == 0) {
      LOG(ERROR) << "Not using the primary image, or not image mime, or not "
                    "the first item, but has file offset of 0";
      return false;
    }
  }

  std::string std_payload;
  bool success = ::photos_editing_formats::image_io::gcontainer::
      ParseFileAfterImageFromStream(file_offset, file_length, input_jpeg_stream,
                                    &std_payload);
  *out_payload = std_payload;
  return success;
}

}  // namespace dynamic_depth
