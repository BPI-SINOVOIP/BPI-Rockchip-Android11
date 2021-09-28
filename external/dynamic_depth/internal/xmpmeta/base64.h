#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_BASE64_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_BASE64_H_  // NOLINT

#include <iostream>
#include <string>
#include <vector>

#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {
// Decodes the base64-encoded input range. Supports decoding of both web-safe
// and regular base64."Web-safe" base-64 replaces + with - and / with _, and
// omits trailing = padding characters.
bool DecodeBase64(const string& data, string* output);

// Base64-encodes the given string.
bool EncodeBase64(const string& data, string* output);

// Base64-encodes the given int array.
bool EncodeIntArrayBase64(const std::vector<int32_t>& data, string* output);

// Base64-decodes the given int array.
bool DecodeIntArrayBase64(const string& data, std::vector<int32_t>* output);

// Base64-encodes the given float array.
bool EncodeFloatArrayBase64(const std::vector<float>& data, string* output);

// Base64-decodes the given float array.
bool DecodeFloatArrayBase64(const string& data, std::vector<float>* output);

// Base64-encodes the given double array.
bool EncodeDoubleArrayBase64(const std::vector<double>& data, string* output);

// Base64-decodes the given double array.
bool DecodeDoubleArrayBase64(const string& data, std::vector<double>* output);

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_BASE64_H_  // NOLINT
