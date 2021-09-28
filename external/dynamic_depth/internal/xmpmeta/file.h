#ifndef DYNAMIC_DEPTH_INTERNAL_XMPMETA_FILE_H_  // NOLINT
#define DYNAMIC_DEPTH_INTERNAL_XMPMETA_FILE_H_  // NOLINT

#include <string>

namespace dynamic_depth {
namespace xmpmeta {

void WriteStringToFileOrDie(const std::string &data,
                            const std::string &filename);
void ReadFileToStringOrDie(const std::string &filename, std::string *data);

// Join two path components, adding a slash if necessary.  If basename is an
// absolute path then JoinPath ignores dirname and simply returns basename.
std::string JoinPath(const std::string &dirname, const std::string &basename);

}  // namespace xmpmeta
}  // namespace dynamic_depth

#endif // DYNAMIC_DEPTH_INTERNAL_XMPMETA_FILE_H_  // NOLINT
