#include "xmpmeta/file.h"

#include <cstdio>
#include "android-base/logging.h"

namespace dynamic_depth {
namespace xmpmeta {

using std::string;

void WriteStringToFileOrDie(const string& data, const string& filename) {
  FILE* file_descriptor = fopen(filename.c_str(), "wb");
  if (!file_descriptor) {
    LOG(FATAL) << "Couldn't write to file: " << filename;
  }
  fwrite(data.c_str(), 1, data.size(), file_descriptor);
  fclose(file_descriptor);
}

void ReadFileToStringOrDie(const string& filename, string* data) {
  FILE* file_descriptor = fopen(filename.c_str(), "r");

  if (!file_descriptor) {
    LOG(FATAL) << "Couldn't read file: " << filename;
  }

  // Resize the input buffer appropriately.
  fseek(file_descriptor, 0L, SEEK_END);
  int num_bytes = ftell(file_descriptor);
  data->resize(num_bytes);

  // Read the data.
  fseek(file_descriptor, 0L, SEEK_SET);
  int num_read =
      fread(&((*data)[0]), sizeof((*data)[0]), num_bytes, file_descriptor);
  if (num_read != num_bytes) {
    LOG(FATAL) << "Couldn't read all of " << filename
               << "expected bytes: " << num_bytes * sizeof((*data)[0])
               << "actual bytes: " << num_read;
  }
  fclose(file_descriptor);
}

string JoinPath(const string& dirname, const string& basename) {
#ifdef _WIN32
  static const char separator = '\\';
#else
  static const char separator = '/';
#endif  // _WIN32

  if ((!basename.empty() && basename[0] == separator) || dirname.empty()) {
    return basename;
  } else if (dirname[dirname.size() - 1] == separator) {
    return dirname + basename;
  } else {
    return dirname + string(&separator, 1) + basename;
  }
}

}  // namespace xmpmeta
}  // namespace dynamic_depth
