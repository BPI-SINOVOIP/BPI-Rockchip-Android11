#ifndef DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_JPEG_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_JPEG_H_  // NOLINT

#include <stddef.h>
#include <stdint.h>

namespace dynamic_depth {

// Android depth photo validation sequence
int32_t ValidateAndroidDynamicDepthBuffer(const char* buffer, size_t buffer_length);

}  // namespace dynamic_depth

#endif  // DYNAMIC_DEPTH_INCLUDES_DYNAMIC_DEPTH_JPEG_H_  // NOLINT
