#ifndef DYNAMIC_DEPTH_INCLUDES_XMPMETA_MD5_H_  // NOLINT
#define DYNAMIC_DEPTH_INCLUDES_XMPMETA_MD5_H_  // NOLINT

#include <string>

#include "base/port.h"

namespace dynamic_depth {
namespace xmpmeta {

// Returns the MD5 hash of to_hash as a 32-character hex string.
// Wrapper around OpenSSL to avoid Gyp dependency problems.
string MD5Hash(const string& to_hash);

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INCLUDES_XMPMETA_MD5_H_  // NOLINT
