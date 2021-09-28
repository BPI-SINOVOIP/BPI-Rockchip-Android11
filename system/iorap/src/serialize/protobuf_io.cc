// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "protobuf_io.h"

#include "common/trace.h"
#include "serialize/arena_ptr.h"

#include <android-base/chrono_utils.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utils/Trace.h>

#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "system/iorap/src/serialize/TraceFile.pb.h"

namespace iorap {
namespace serialize {

ArenaPtr<proto::TraceFile> ProtobufIO::Open(std::string file_path) {
  // TODO: file a bug about this.
  // Note: can't use {} here, clang think it's narrowing from long->int.
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(::open(file_path.c_str(), O_RDONLY)));
  if (fd.get() < 0) {
    PLOG(DEBUG) << "ProtobufIO: open failed: " << file_path;
    return nullptr;
  }

  return Open(fd.get(), file_path.c_str());
}

ArenaPtr<proto::TraceFile> ProtobufIO::Open(int fd, const char* file_path) {

  ScopedFormatTrace atrace_protobuf_io_open(ATRACE_TAG_ACTIVITY_MANAGER,
                                            "ProtobufIO::Open %s",
                                            file_path);
  android::base::Timer timer{};

  struct stat buf;
  if (fstat(fd, /*out*/&buf) < 0) {
    PLOG(ERROR) << "ProtobufIO: open error, fstat failed: " << file_path;
    return nullptr;
  }
  // XX: off64_t for stat::st_size ?

  // Using the mmap appears to be the only way to do zero-copy with protobuf lite.
  void* data = mmap(/*addr*/nullptr,
                    buf.st_size,
                    PROT_READ, MAP_SHARED | MAP_POPULATE,
                    fd,
                    /*offset*/0);
  if (data == nullptr) {
    PLOG(ERROR) << "ProtobufIO: open error, mmap failed: " << file_path;
    return nullptr;
  }

  ArenaPtr<proto::TraceFile> protobuf_trace_file = ArenaPtr<proto::TraceFile>::Make();
  if (protobuf_trace_file == nullptr) {
    LOG(ERROR) << "ProtobufIO: open error, failed to create arena: " << file_path;
    return nullptr;
  }

  google::protobuf::io::ArrayInputStream protobuf_input_stream{data, static_cast<int>(buf.st_size)};
  if (!protobuf_trace_file->ParseFromZeroCopyStream(/*in*/&protobuf_input_stream)) {
    // XX: Does protobuf on android already have the right LogHandler ?
    LOG(ERROR) << "ProtobufIO: open error, protobuf parsing failed: " << file_path;
    return nullptr;
  }

  if (munmap(data, buf.st_size) < 0) {
    PLOG(WARNING) << "ProtobufIO: open problem, munmap failed, possibly memory leak? "
                  << file_path;
  }

  LOG(VERBOSE) << "ProtobufIO: open succeeded: " << file_path << ", duration: " << timer;
  return protobuf_trace_file;
}

iorap::expected<size_t /*bytes written*/, int /*errno*/> ProtobufIO::WriteFully(
    const ::google::protobuf::MessageLite& message,
    std::string_view file_path) {

  std::string str{file_path};
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(
      ::open(str.c_str(),
             O_CREAT | O_TRUNC | O_RDWR,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)));  // ugo: rw-rw----
  if (fd.get() < 0) {
    int err = errno;
    PLOG(ERROR) << "ProtobufIO: open failed: " << file_path;
    return unexpected{err};
  }

  return WriteFully(message, fd.get(), file_path);
}

iorap::expected<size_t /*bytes written*/, int /*errno*/> ProtobufIO::WriteFully(
    const ::google::protobuf::MessageLite& message,
    int fd,
    std::string_view file_path) {

  int byte_size = message.ByteSize();
  if (byte_size < 0) {
    DCHECK(false) << "Invalid protobuf size: " << byte_size;
    LOG(ERROR) << "ProtobufIO: Invalid protobuf size: " << byte_size;
    return unexpected{EDOM};
  }
  size_t serialized_size = static_cast<size_t>(byte_size);

  // Change the file to be exactly the length of the protobuf.
  if (ftruncate(fd, static_cast<off_t>(serialized_size)) < 0) {
    int err = errno;
    PLOG(ERROR) << "ProtobufIO: ftruncate (size=" << serialized_size << ") failed";
    return unexpected{err};
  }

  // Using the mmap appears to be the only way to do zero-copy with protobuf lite.
  void* data = mmap(/*addr*/nullptr,
                    serialized_size,
                    PROT_WRITE,
                    MAP_SHARED,
                    fd,
                    /*offset*/0);
  if (data == nullptr) {
    int err = errno;
    PLOG(ERROR) << "ProtobufIO: mmap failed: " << file_path;
    return unexpected{err};
  }

  // Zero-copy write from protobuf to file via memory-map.
  ::google::protobuf::io::ArrayOutputStream output_stream{data, byte_size};
  if (!message.SerializeToZeroCopyStream(/*inout*/&output_stream)) {
    // This should never happen since we pre-allocated the file and memory map to be large
    // enough to store the full protobuf.
    DCHECK(false) << "ProtobufIO:: SerializeToZeroCopyStream failed despite precalculating size";
    LOG(ERROR) << "ProtobufIO: SerializeToZeroCopyStream failed";
    return unexpected{EXFULL};
  }

  // Guarantee that changes are written back prior to munmap.
  if (msync(data, static_cast<size_t>(serialized_size), MS_SYNC) < 0) {
    int err = errno;
    PLOG(ERROR) << "ProtobufIO: msync failed";
    return unexpected{err};
  }

  if (munmap(data, serialized_size) < 0) {
    PLOG(WARNING) << "ProtobufIO: munmap failed, possibly memory leak? "
                  << file_path;
  }

  return serialized_size;
}

}  // namespace serialize
}  // namespace iorap

