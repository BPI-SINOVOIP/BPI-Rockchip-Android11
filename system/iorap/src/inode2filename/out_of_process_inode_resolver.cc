// Copyright (C) 2020 The Android Open Source Project
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

#include "common/cmd_utils.h"
#include "inode2filename/inode_resolver.h"
#include "inode2filename/out_of_process_inode_resolver.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <sstream>
#include <stdio.h>
#include <unistd.h>

namespace rx = rxcpp;

namespace iorap::inode2filename {

using ::android::base::unique_fd;
using ::android::base::borrowed_fd;

static const char* GetCommandFileName() {
  // Avoid ENOENT by execve by specifying the absolute path of inode2filename.
#ifdef __ANDROID__
  return "/system/bin/iorap.inode2filename";
#else
  static const char* file_name = nullptr;

  if (file_name == nullptr) {
    char* out_dir = getenv("ANDROID_HOST_OUT");
    static std::string file_name_str = out_dir;
    if (out_dir != nullptr) {
      file_name_str += "/bin/";
    } else {
      // Assume it's in the same directory as the binary we are in.
      std::string self_path;
      CHECK(::android::base::Readlink("/proc/self/exe", /*out*/&self_path));

      std::string self_dir = ::android::base::Dirname(self_path);
      file_name_str = self_dir + "/";
    }
    file_name_str += "iorap.inode2filename";

    file_name = file_name_str.c_str();
  }

  return file_name;
#endif
}

std::error_code ErrorCodeFromErrno() {
  int err = errno;
  return std::error_code(err, std::system_category());
}

std::ios_base::failure IosBaseFailureWithErrno(const char* message) {
  std::error_code ec = ErrorCodeFromErrno();
  return std::ios_base::failure(message, ec);
}

static constexpr bool kDebugFgets = false;

int32_t ReadLineLength(FILE* stream, bool* eof) {
  char buf[sizeof(int32_t)];
  size_t count = fread(buf, 1, sizeof(int32_t), stream);
  if (feof(stream)) {
    // If reaching the end of the stream when trying to read the first int, just
    // return. This is legitimate, because after reading the last line, the next
    // iteration will reach this.
    *eof = true;
    return 0;
  }
  int32_t length;
  memcpy(&length, buf, sizeof(int32_t));
  return length;
}

// The steam is like [size1][file1][size2][file2]...[sizeN][fileN].
std::string ReadOneLine(FILE* stream, bool* eof) {
  DCHECK(stream != nullptr);
  DCHECK(eof != nullptr);

  int32_t length = ReadLineLength(stream, eof);
  if (length <= 0) {
    PLOG(ERROR) << "unexpected 0 length line.";
    *eof = true;
    return "";
  }

  std::string str(length, '\0');
  size_t count = fread(&str[0], sizeof(char), length, stream);
  if (feof(stream) || ferror(stream) || count != (uint32_t)length) {
    // error! :(
    PLOG(ERROR) << "unexpected end of the line during fread";
    *eof = true;
    return "";
  }
  return str;
}

static inline void LeftTrim(/*inout*/std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
    return !std::isspace(ch);
  }));
}

// Parses an --output-format=ipc kind of line into an InodeResult.
// Returns nullopt if parsing failed.
std::optional<InodeResult> ParseFromLine(const std::string& line) {
  // inode <- INT:INT:INT
  // line_ok <- 'K ' inode ' ' STRING
  // line_err <- 'E ' inode ' ' INT
  //
  // result <- line_ok | line_err

  std::stringstream ss{line};

  bool result_ok = false;

  std::string ok_or_error;
  ss >> ok_or_error;

  if (ss.fail()) {
    return std::nullopt;
  }

  if (ok_or_error == "K") {
    result_ok = true;
  } else if (ok_or_error == "E") {
    result_ok = false;
  } else {
    return std::nullopt;
  }

  std::string inode_s;
  ss >> inode_s;

  if (ss.fail()) {
    return std::nullopt;
  }

  Inode inode;

  std::string inode_parse_error_msg;
  if (!Inode::Parse(inode_s, /*out*/&inode, /*out*/&inode_parse_error_msg)) {
    return std::nullopt;
  }

  if (result_ok == false) {
    int error_code;
    ss >> error_code;

    if (ss.fail()) {
      return std::nullopt;
    }

    return InodeResult::makeFailure(inode, error_code);
  } else if (result_ok == true) {
    std::string rest_of_line;
    ss >> rest_of_line;

    // parse " string with potential spaces"
    // into "string with potential spaces"
    LeftTrim(/*inout*/rest_of_line);

    if (ss.fail()) {
      return std::nullopt;
    }

    const std::string& file_name = rest_of_line;

    return InodeResult::makeSuccess(inode, file_name);
  }

  return std::nullopt;
}

struct OutOfProcessInodeResolver::Impl {
  Impl() {
  }

 private:
  // Create the argv we will pass to the forked inode2filename, corresponds to #EmitAll.
  std::vector<std::string> CreateArgvAll(const InodeResolverDependencies& deps) const {
    std::vector<std::string> argv;
    argv.push_back("--all");

    return CreateArgv(deps, std::move(argv));
  }

  // Create the argv we will pass to the forked inode2filename, corresponds to
  // #FindFilenamesFromInodes.
  std::vector<std::string> CreateArgvFind(const InodeResolverDependencies& deps,
                                          const std::vector<Inode>& inodes) const {
    std::vector<std::string> argv;
    iorap::common::AppendArgsRepeatedly(argv, inodes);

    return CreateArgv(deps, std::move(argv));
  }

  std::vector<std::string> CreateArgv(const InodeResolverDependencies& deps,
                                      std::vector<std::string> append_argv) const {
    InodeResolverDependencies deps_oop = deps;
    deps_oop.process_mode = ProcessMode::kInProcessDirect;

    std::vector<std::string> argv = ToArgs(deps_oop);

    argv.push_back("--output-format=ipc");

    if (iorap::common::GetBoolEnvOrProperty("iorap.inode2filename.log.verbose", false)) {
      argv.push_back("--verbose");
    }

    iorap::common::AppendArgsRepeatedly(argv, std::move(append_argv));

    return argv;
  }

 public:
  // fork+exec into inode2filename with 'inodes' as the search list.
  // Each result is parsed into a dest#on_next(result).
  // If a fatal error occurs, dest#on_error is called once and no other callbacks are called.
  void EmitFromCommandFind(rxcpp::subscriber<InodeResult>& dest,
                           const InodeResolverDependencies& deps,
                           const std::vector<Inode>& inodes) {
    // Trivial case: complete immediately.
    // Executing inode2filename with empty search list will just print the --help menu.
    if (inodes.empty()) {
      dest.on_completed();
    }

    std::vector<std::string> argv = CreateArgvFind(deps, inodes);
    EmitFromCommandWithArgv(/*inout*/dest, std::move(argv), inodes.size());
  }

  // fork+exec into inode2filename with --all (listing *all* inodes).
  // Each result is parsed into a dest#on_next(result).
  // If a fatal error occurs, dest#on_error is called once and no other callbacks are called.
  void EmitFromCommandAll(rxcpp::subscriber<InodeResult>& dest,
                           const InodeResolverDependencies& deps) {
    std::vector<std::string> argv = CreateArgvAll(deps);
    EmitFromCommandWithArgv(/*inout*/dest, std::move(argv), /*result_count*/std::nullopt);
  }

 private:
  void EmitFromCommandWithArgv(rxcpp::subscriber<InodeResult>& dest,
                               std::vector<std::string> argv_vec,
                               std::optional<size_t> result_count) {
    unique_fd pipe_reader, pipe_writer;
    if (!::android::base::Pipe(/*out*/&pipe_reader, /*out*/&pipe_writer)) {
      dest.on_error(
          rxcpp::util::make_error_ptr(
              IosBaseFailureWithErrno("Failed to create out-going pipe for inode2filename")));
      return;
    }

    pid_t child = fork();
    if (child == -1) {
      dest.on_error(
          rxcpp::util::make_error_ptr(
              IosBaseFailureWithErrno("Failed to fork process for inode2filename")));
      return;
    } else if (child > 0) {  // we are the caller of this function
      LOG(DEBUG) << "forked into a process for inode2filename , pid = " << child;
    } else {
      // we are the child that was forked

      const char* kCommandFileName = GetCommandFileName();

      std::stringstream argv; // for debugging.
      for (std::string arg : argv_vec) {
        argv  << arg << ' ';
      }
      LOG(DEBUG) << "fork+exec: " << kCommandFileName << " " << argv.str();

      // Redirect only stdout. stdin is unused, stderr is same as parent.
      if (dup2(pipe_writer.get(), STDOUT_FILENO) == -1) {
        // Trying to call #on_error does not make sense here because we are in a forked process,
        // the only thing we can do is crash definitively.
        PLOG(FATAL) << "Failed to dup2 for inode2filename";
      }

      std::unique_ptr<const char *[]> argv_ptr =
                common::VecToArgv(kCommandFileName, argv_vec);

      if (execve(kCommandFileName,
                 (char **)argv_ptr.get(),
                 /*envp*/nullptr) == -1) {
        // Trying to call #on_error does not make sense here because we are in a forked process,
        // the only thing we can do is crash definitively.
        PLOG(FATAL) << "Failed to execve process for inode2filename";
      }
      // This should never return.
    }

    // Immediately close the writer end of the pipe because we never use it.
    pipe_writer.reset();

    // Convert pipe(reader) file descriptor into FILE*.
    std::unique_ptr<FILE, int(*)(FILE*)> file_reader{
        ::android::base::Fdopen(std::move(pipe_reader), /*mode*/"r"), fclose};
    if (!file_reader) {
      dest.on_error(
          rxcpp::util::make_error_ptr(
              IosBaseFailureWithErrno("Failed to fdopen for inode2filename")));
      return;
    }

    size_t actual_result_count = 0;

    bool file_eof = false;
    while (!file_eof) {
      std::string inode2filename_line = ReadOneLine(file_reader.get(), /*out*/&file_eof);

      if (inode2filename_line.empty()) {
        if (!file_eof) {
          // Ignore empty lines.
          LOG(WARNING) << "inode2filename: got empty line";
        }
        continue;
      }

      LOG(DEBUG) << "inode2filename output-line: " << inode2filename_line;

      std::optional<InodeResult> res = ParseFromLine(inode2filename_line);
      if (!res) {
        std::string error_msg = "Invalid output: ";
        error_msg += inode2filename_line;
        dest.on_error(
            rxcpp::util::make_error_ptr(std::ios_base::failure(error_msg)));
        return;
      }
      dest.on_next(*res);

      ++actual_result_count;
    }

    LOG(DEBUG) << "inode2filename output-eof";

    // Ensure that the # of inputs into the rx stream match the # of outputs.
    // This is validating the post-condition of FindFilenamesFromInodes.
    if (result_count && actual_result_count != *result_count) {
      std::stringstream ss;
      ss << "Invalid number of results, expected: " << *result_count;
      ss << ", actual: " << actual_result_count;

      dest.on_error(
          rxcpp::util::make_error_ptr(std::ios_base::failure(ss.str())));
      return;
    }

    CHECK(child > 0);  // we are in the parent process, parse the IPC output of inode2filename
    dest.on_completed();
  }
};

rxcpp::observable<InodeResult>
    OutOfProcessInodeResolver::FindFilenamesFromInodes(std::vector<Inode> inodes) const {
  return rxcpp::observable<>::create<InodeResult>(
    [self=std::static_pointer_cast<const OutOfProcessInodeResolver>(shared_from_this()),
     inodes=std::move(inodes)](
        rxcpp::subscriber<InodeResult> s) {
      self->impl_->EmitFromCommandFind(s, self->GetDependencies(), inodes);
  });
}

rxcpp::observable<InodeResult>
    OutOfProcessInodeResolver::EmitAll() const {
  auto self = std::static_pointer_cast<const OutOfProcessInodeResolver>(shared_from_this());
  CHECK(self != nullptr);
  CHECK(self->impl_ != nullptr);

  return rxcpp::observable<>::create<InodeResult>(
    [self](rxcpp::subscriber<InodeResult> s) {
      CHECK(self != nullptr);
      CHECK(self->impl_ != nullptr);
      self->impl_->EmitFromCommandAll(s, self->GetDependencies());
  });
}

OutOfProcessInodeResolver::OutOfProcessInodeResolver(InodeResolverDependencies dependencies)
  : InodeResolver{std::move(dependencies)}, impl_{new Impl{}} {
}

OutOfProcessInodeResolver::~OutOfProcessInodeResolver() {
  // std::unique_ptr requires complete types, but we hide the definition in the header.
  delete impl_;
}

}  // namespace iorap::inode2filename
