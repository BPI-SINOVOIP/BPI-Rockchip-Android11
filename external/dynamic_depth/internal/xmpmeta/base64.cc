#include "xmpmeta/base64.h"

#include "android-base/logging.h"
#include "strings/escaping.h"

namespace dynamic_depth {
namespace xmpmeta {
namespace {

bool EncodeBase64RawData(const uint8* data, size_t data_size, string* output) {
  // Disable linting because string_view doesn't appear to support uint8_t.
  dynamic_depth::Base64Escape(data, data_size, output, false);  // NOLINT
  return output->length() > 0;
}

template <typename T>
bool InternalEncodeArrayBase64(const std::vector<T>& data, string* output) {
  size_t buffer_size = data.size() * sizeof(T);
  return EncodeBase64RawData(reinterpret_cast<const uint8_t*>(data.data()),
                             buffer_size, output);
}

template <typename T>
bool InternalDecodeArrayBase64(const string& data, std::vector<T>* output) {
  string bytes;
  if (!DecodeBase64(data, &bytes)) {
    return false;
  }

  const int count = bytes.size() / sizeof(T);
  output->clear();
  output->resize(count);
  memcpy(output->data(), bytes.data(), output->size() * sizeof(T));
  return !output->empty();
}

}  // namespace

// Decodes the base64-encoded input range.
bool DecodeBase64(const string& data, string* output) {
  // Support decoding of both web-safe and regular base64.
  // "Web-safe" base-64 replaces + with - and / with _, and omits
  // trailing = padding characters.
  if (dynamic_depth::Base64Unescape(data, output)) {
    return true;
  }
  return dynamic_depth::WebSafeBase64Unescape(data, output);
}

// Base64-encodes the given data.
bool EncodeBase64(const string& data, string* output) {
  return EncodeBase64RawData(reinterpret_cast<const uint8*>(data.c_str()),
                             data.length(), output);
}

// Base64-encodes the given int array.
bool EncodeIntArrayBase64(const std::vector<int32_t>& data, string* output) {
  return InternalEncodeArrayBase64<int32_t>(data, output);
}

// Base64-decodes the given base64-encoded string.
bool DecodeIntArrayBase64(const string& data, std::vector<int32_t>* output) {
  return InternalDecodeArrayBase64<int32_t>(data, output);
}

// Base64-encodes the given float array.
bool EncodeFloatArrayBase64(const std::vector<float>& data, string* output) {
  return InternalEncodeArrayBase64<float>(data, output);
}

// Base64-decodes the given base64-encoded string.
bool DecodeFloatArrayBase64(const string& data, std::vector<float>* output) {
  return InternalDecodeArrayBase64<float>(data, output);
}

// Base64-encodes the given double array.
bool EncodeDoubleArrayBase64(const std::vector<double>& data, string* output) {
  return InternalEncodeArrayBase64<double>(data, output);
}

// Base64-decodes the given base64-encoded string.
bool DecodeDoubleArrayBase64(const string& data, std::vector<double>* output) {
  return InternalDecodeArrayBase64<double>(data, output);
}
}  // namespace xmpmeta
}  // namespace dynamic_depth
