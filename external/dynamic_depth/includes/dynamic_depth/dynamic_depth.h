#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DYNAMIC_DEPTH_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DYNAMIC_DEPTH_H_  // NOLINT

#include <iostream>

#include "dynamic_depth/device.h"
#include "xmpmeta/xmp_writer.h"

namespace dynamic_depth {

// Serialize a JPEG image, its Dynamic Depth metadata, and GContainer files
// if applicable.
bool WriteImageAndMetadataAndContainer(const string& out_filename,
                                       const uint8_t* primary_image_bytes,
                                       size_t primary_image_bytes_count,
                                       Device* device);

// Same as WriteImageAndMetadataAndContainer, but on istream and ostream.
bool WriteImageAndMetadataAndContainer(std::istream* input_jpeg_stream,
                                       Device* device,
                                       std::ostream* output_jpeg_stream);

// Retrieves the contents of a Container:Item's associated file. The contents
// are populated into out_payload.
// As per the Dynamic Depth spec, file contents are base64-encoded if they're
// of an image/ mime type, and not if they're of a text/ mime type. Dynamic
// Depth elements that use Container:Item will haev handled this appropriately
// on item construction.
bool GetItemPayload(const string& input_image_filename,
                    const Container* container, const string& item_uri,
                    string* out_payload);

// Convenience method for the aboove.
bool GetItemPayload(const string& input_image_filename, const Device* device,
                    const string& item_uri, string* out_payload);

// Used by AOSP.
// Same as the above, but for an istream.
bool GetItemPayload(const Container* container, const string& item_uri,
                    std::istream& input_jpeg_stream, string* out_payload);

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_DYNAMIC_DEPTH_H_  // NOLINT
