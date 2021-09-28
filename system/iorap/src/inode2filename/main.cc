// Copyright (C) 2018 The Android Open Source Project
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

#include "common/debug.h"
#include "common/expected.h"
#include "inode2filename/inode_resolver.h"

#include <android-base/strings.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>

#if defined(IORAP_INODE2FILENAME_MAIN)

namespace iorap::inode2filename {

void Usage(char** argv) {
  std::cerr << "Usage: " << argv[0] << " <options> <<inode_syntax>> [inode_syntax1 inode_syntax2 ...]" << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "  Block until all inodes have been read in, then begin searching for filenames for those inodes." << std::endl;
  std::cerr << "  Results are written immediately as they are available, and once all inodes are found, " << std::endl;
  std::cerr << "  the program will terminate." << std::endl;
  std::cerr << "" << std::endl;
  std::cerr << "    Inode syntax:     ('dev_t@inode' | 'major:minor:inode')" << std::endl;
  std::cerr << "" << std::endl;       // CLI-only flags.
  std::cerr << "    --help,-h         Print this Usage." << std::endl;
  std::cerr << "    --verbose,-v      Set verbosity (default off)." << std::endl;
  std::cerr << "    --wait,-w         Wait for key stroke before continuing (default off)." << std::endl;
  std::cerr << "" << std::endl;       // General flags.
  std::cerr << "    --all,-a          Enumerate all inode->filename mappings in the dataset (default off)." << std::endl;
  std::cerr << "                      All <<inode_syntaxN>> arguments are ignored." << std::endl;
  std::cerr << "    --data-source=,   Choose a data source (default 'diskscan')." << std::endl;
  std::cerr << "    -ds " << std::endl;
  std::cerr << "        diskscan      Scan disk recursively using readdir." << std::endl;
  std::cerr << "        textcache     Use the file from the '--output-format=text'." << std::endl;
  std::cerr << "        bpf           Query kernel BPF maps (experimental)." << std::endl;
  std::cerr << "    --output=,-o      Choose an output file (default 'stdout')." << std::endl;
  std::cerr << "    --output-format=, Choose an output format (default 'log')." << std::endl;
  std::cerr << "    -of " << std::endl;
  std::cerr << "        log           Log human-readable, non-parsable format to stdout+logcat." << std::endl;
  std::cerr << "        textcache     Results are in the same format as system/extras/pagecache." << std::endl;
  std::cerr << "        ipc           Results are in a binary inter-process communications format" << std::endl;
  std::cerr << "    --process-mode=,  Choose a process mode (default 'in'). Test-oriented." << std::endl;
  std::cerr << "    -pm " << std::endl;
  std::cerr << "        in            Use a single process to do the work in." << std::endl;
  std::cerr << "        out           Out-of-process work (forks into a -pm=in)." << std::endl;
  std::cerr << "    --verify=,-vy     Verification modes for the data source (default 'stat')." << std::endl;
  std::cerr << "        stat          Use stat(2) call to validate data inodes are up-to-date. " << std::endl;
  std::cerr << "        none          Trust that the data-source is up-to-date without checking." << std::endl;
  std::cerr << "" << std::endl;       // --data-source=<?> specific flags.
  std::cerr << "    Data-source-specific commands:" << std::endl;
  std::cerr << "      --data-source=diskscan" << std::endl;
  std::cerr << "          --root=,-r        Add root directory (default '.'). Repeatable." << std::endl;
  std::cerr << "      --data-source=textcache" << std::endl;
  std::cerr << "          --textcache=,-tc  Name of file that contains the textcache." << std::endl;
  std::cerr << "" << std::endl;       // Programmatic flags.
  std::cerr << "    --in-fd=#         Input file descriptor. Default input is from argv." << std::endl;
  std::cerr << "    --out-fd=#        Output file descriptor. Default stdout." << std::endl;
  exit(1);
}

static fruit::Component<SystemCall> GetSystemCallComponent() {
    return fruit::createComponent().bind<SystemCall, SystemCallImpl>();
}

std::optional<DataSourceKind> ParseDataSourceKind(std::string_view str) {
  if (str == "diskscan") {
    return DataSourceKind::kDiskScan;
  } else if (str == "textcache") {
    return DataSourceKind::kTextCache;
  } else if (str == "bpf") {
    return DataSourceKind::kBpf;
  }
  return std::nullopt;
}

enum class OutputFormatKind {
  kLog,
  kTextCache,
  kIpc,
};

std::optional<OutputFormatKind> ParseOutputFormatKind(std::string_view str) {
  if (str == "log") {
    return OutputFormatKind::kLog;
  } else if (str == "textcache") {
    return OutputFormatKind::kTextCache;
  } else if (str == "ipc") {
    return OutputFormatKind::kIpc;
  }
  return std::nullopt;
}

std::optional<VerifyKind> ParseVerifyKind(std::string_view str) {
  if (str == "none") {
    return VerifyKind::kNone;
  } else if (str == "stat") {
    return VerifyKind::kStat;
  }
  return std::nullopt;
}

std::optional<ProcessMode> ParseProcessMode(std::string_view str) {
  if (str == "in") {
    return ProcessMode::kInProcessDirect;
  } else if (str == "out") {
    return ProcessMode::kOutOfProcessIpc;
  }
  return std::nullopt;
}

bool StartsWith(std::string_view haystack, std::string_view needle) {
  return haystack.size() >= needle.size()
      && haystack.compare(0, needle.size(), needle) == 0;
}

bool EndsWith(std::string_view haystack, std::string_view needle) {
  return haystack.size() >= needle.size()
      && haystack.compare(haystack.size() - needle.size(), haystack.npos, needle) == 0;
}

bool StartsWithOneOf(std::string_view haystack,
                     std::string_view needle,
                     std::string_view needle2) {
  return StartsWith(haystack, needle) || StartsWith(haystack, needle2);
}

enum ParseError {
  kParseSkip,
  kParseFailed,
};

std::optional<std::string> ParseNamedArgument(std::initializer_list<std::string> names,
                                              std::string argstr,
                                              std::optional<std::string> arg_next,
                                              /*inout*/
                                              int* arg_pos) {
  for (const std::string& name : names) {
    {
      // Try parsing only 'argstr' for '--foo=bar' type parameters.
      std::vector<std::string> split_str = ::android::base::Split(argstr, "=");
      if (split_str.size() >= 2) {
        /*
        std::cerr << "ParseNamedArgument(name=" << name << ", argstr='"
                  <<  argstr << "')" << std::endl;
        */

        if (split_str[0] + "=" == name) {
          return split_str[1];
        }
      }
    }
    //if (EndsWith(name, "=")) {
    //  continue;
    /*} else */ {
      // Try parsing 'argstr arg_next' for '-foo bar' type parameters.
      if (argstr == name) {
        ++(*arg_pos);

        if (arg_next) {
          return arg_next;
        } else {
          // Missing argument, e.g. '-foo' was the last token in the argv.
          std::cerr << "Missing " << name << " flag value." << std::endl;
          exit(1);
        }
      }
    }
  }

  return std::nullopt;
}

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  android::base::SetLogger(android::base::StderrLogger);

  bool all = false;
  bool wait_for_keystroke = false;
  bool enable_verbose = false;
  std::vector<std::string> root_directories;
  std::vector<Inode> inode_list;
  int recording_time_sec = 0;

  DataSourceKind data_source = DataSourceKind::kDiskScan;
  OutputFormatKind output_format = OutputFormatKind::kLog;
  VerifyKind verify = VerifyKind::kStat;
  ProcessMode process_mode = ProcessMode::kInProcessDirect;

  std::optional<std::string> output_filename;
  std::optional<int /*fd*/> in_fd, out_fd;  // input-output file descriptors [for fork+exec].
  std::optional<std::string> text_cache_filename;

  if (argc == 1) {
    Usage(argv);
  }

  for (int arg = 1; arg < argc; ++arg) {
    std::string argstr = argv[arg];
    std::optional<std::string> arg_next;
    if ((arg + 1) < argc) {
      arg_next = argv[arg+1];
    }

    if (argstr == "--help" || argstr == "-h") {
      Usage(argv);
    } else if (auto val = ParseNamedArgument({"--root=", "-r"}, argstr, arg_next, /*inout*/&arg);
               val) {
      root_directories.push_back(*val);
    } else if (argstr == "--verbose" || argstr == "-v") {
      enable_verbose = true;
    } else if (argstr == "--wait" || argstr == "-w") {
      wait_for_keystroke = true;
    }
     else if (argstr == "--all" || argstr == "-a") {
      all = true;
    } else if (auto val = ParseNamedArgument({"--data-source=", "-ds"},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      auto ds = ParseDataSourceKind(*val);
      if (!ds) {
        std::cerr << "Invalid --data-source=<value>" << std::endl;
        return 1;
      }
      data_source = *ds;
    } else if (auto val = ParseNamedArgument({"--output=", "-o"},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      output_filename = *val;
    } else if (auto val = ParseNamedArgument({"--process-mode=", "-pm"},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      auto pm = ParseProcessMode(*val);
      if (!pm) {
        std::cerr << "Invalid --process-mode=<value>" << std::endl;
        return 1;
      }
      process_mode = *pm;
    }
    else if (auto val = ParseNamedArgument({"--output-format=", "-of"},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      auto of = ParseOutputFormatKind(*val);
      if (!of) {
        std::cerr << "Missing --output-format=<value>" << std::endl;
        return 1;
      }
      output_format = *of;
    } else if (auto val = ParseNamedArgument({"--verify=", "-vy="},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      auto vy = ParseVerifyKind(*val);
      if (!vy) {
        std::cerr << "Invalid --verify=<value>" << std::endl;
        return 1;
      }
      verify = *vy;
    } else if (auto val = ParseNamedArgument({"--textcache=", "-tc"},
                                             argstr,
                                             arg_next,
                                             /*inout*/&arg);
               val) {
      text_cache_filename = *val;
    } else {
      Inode maybe_inode{};

      std::string error_msg;
      if (Inode::Parse(argstr, /*out*/&maybe_inode, /*out*/&error_msg)) {
        inode_list.push_back(maybe_inode);
      } else {
        if (argstr.size() >= 1) {
          if (argstr[0] == '-') {
            std::cerr << "Unrecognized flag: " << argstr << std::endl;
            return 1;
          }
        }

        std::cerr << "Failed to parse inode (" << argstr << ") because: " << error_msg << std::endl;
        return 1;
      }
    }
  }

  if (root_directories.size() == 0) {
    root_directories.push_back(".");
  }

  if (inode_list.size() == 0 && !all) {
    std::cerr << "Provide at least one inode. Or use --all to dump everything." << std::endl;
    return 1;
  } else if (all && inode_list.size() > 0) {
    std::cerr << "[WARNING]: --all flag ignores all inodes passed on command line." << std::endl;
  }

  std::ofstream fout;
  if (output_filename) {
    fout.open(*output_filename);
    if (!fout) {
      std::cerr << "Failed to open output file for writing: \"" << *output_filename << "\"";
      return 1;
    }
  } else {
    fout.open("/dev/null");  // have to open *something* otherwise rdbuf fails.
    fout.basic_ios<char>::rdbuf(std::cout.rdbuf());
  }

  if (enable_verbose) {
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);

    LOG(VERBOSE) << "Verbose check";
    LOG(VERBOSE) << "Debug check: " << ::iorap::kIsDebugBuild;

    for (auto& inode_num : inode_list) {
      LOG(VERBOSE) << "Searching for inode " << inode_num;
    }

    LOG(VERBOSE) << "Dumping all inodes? " << all;
  }
  // else use
  // $> ANDROID_LOG_TAGS='*:d' iorap.inode2filename <args>
  // which will enable arbitrary log levels.

  // Useful to attach a debugger...
  // 1) $> inode2filename -w <args>
  // 2) $> gdbclient <pid>
  if (wait_for_keystroke) {
    LOG(INFO) << "Self pid: " << getpid();
    LOG(INFO) << "Press any key to continue...";
    std::cin >> wait_for_keystroke;
  }

  fruit::Injector<SystemCall> injector(GetSystemCallComponent);

  InodeResolverDependencies ir_dependencies;
  // Passed from command-line.
  ir_dependencies.data_source = data_source;
  ir_dependencies.process_mode = process_mode;
  ir_dependencies.root_directories = root_directories;
  ir_dependencies.text_cache_filename = text_cache_filename;
  ir_dependencies.verify = verify;
  // Hardcoded.
  ir_dependencies.system_call = injector.get<SystemCall*>();

  std::shared_ptr<InodeResolver> inode_resolver =
      InodeResolver::Create(ir_dependencies);

  inode_resolver->StartRecording();
  sleep(recording_time_sec);  // TODO: add cli flag for this when we add something that needs it.
  inode_resolver->StopRecording();

  auto/*observable<InodeResult>*/ inode_results = all
      ? inode_resolver->EmitAll()
      : inode_resolver->FindFilenamesFromInodes(std::move(inode_list));

  int return_code = 2;
  inode_results.subscribe(
      /*on_next*/[&return_code, output_format, &fout](const InodeResult& result) {
    if (result) {
      LOG(DEBUG) << "Inode match: " << result;
      if (output_format == OutputFormatKind::kLog) {
        fout << "\033[1;32m[OK]\033[0m  "
             << result.inode
             << " \"" << result.data.value() << "\"" << std::endl;
      } else if (output_format == OutputFormatKind::kIpc) {
        std::stringstream stream;
        stream << "K " << result.inode << " " << result.data.value();
        std::string line = stream.str();

        // Convert the size to 4 bytes.
        int32_t size = line.size();
        char buf[sizeof(int32_t)];
        memcpy(buf, &size, sizeof(int32_t));
        fout.write(buf, sizeof(int32_t));
        fout.write(line.c_str(), size);
      } else if (output_format == OutputFormatKind::kTextCache) {
        // Same format as TextCacheDataSource (system/extras/pagecache/pagecache.py -d)
        //   "$device_number $inode $filesize $filename..."
        const Inode& inode = result.inode;
        fout << inode.GetDevice() << " "
             << inode.GetInode()
             << " -1 "  // always -1 for filesize, since we don't track what it is.
             << result.data.value() << "\n";  // don't use endl which flushes, for faster writes.
      } else {
        LOG(FATAL) << "Not implemented this kind of --output-format";
      }

      return_code = 0;
    } else {
      LOG(DEBUG) << "Failed to match inode: " << result;
      if (output_format == OutputFormatKind::kLog) {
        fout << "\033[1;31m[ERR]\033[0m "
             << result.inode
             << " '" << *result.ErrorMessage() << "'" << std::endl;
      } else if (output_format == OutputFormatKind::kIpc) {
        std::stringstream stream;
        stream << "E " << result.inode << " " << result.data.error() << std::endl;
        std::string line = stream.str();

        // Convert the size to 4 bytes.
        int32_t size = line.size();
        char buf[sizeof(int32_t)];
        memcpy(buf, &size, sizeof(int32_t));
        fout.write(buf, sizeof(int32_t));
        fout.write(line.c_str(), size);
      }
      else if (output_format == OutputFormatKind::kTextCache) {
        // Don't add bad results to the textcache. They are dropped.
      } else {
        LOG(FATAL) << "Not implemented this kind of --output-format";
      }
    }
  }, /*on_error*/[&return_code](rxcpp::util::error_ptr error) {
    // Usually occurs very early on before we see the first result.
    // In this case the error is terminal so we just end up exiting out soon.
    return_code = 3;
    LOG(ERROR) << "Critical error: " << rxcpp::util::what(error);
  });

  // 0 -> found at least a single match,
  // 1 -> bad parameters,
  // 2 -> could not find any matches,
  // 3 -> rxcpp on_error.
  return return_code;
}

}  // namespace iorap::inode2filename

int main(int argc, char** argv) {
  return ::iorap::inode2filename::main(argc, argv);
}

#endif
