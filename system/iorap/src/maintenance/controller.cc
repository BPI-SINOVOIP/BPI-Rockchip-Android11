// Copyright (C) 2019 The Android Open Source Project
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

#include "compiler/compiler.h"
#include "maintenance/controller.h"

#include "common/cmd_utils.h"
#include "common/debug.h"
#include "common/expected.h"
#include "common/trace.h"

#include "db/models.h"
#include "inode2filename/inode.h"
#include "inode2filename/search_directories.h"
#include "prefetcher/read_ahead.h"

#include <android-base/file.h>
#include <utils/Printer.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <sys/wait.h>

namespace iorap::maintenance {

const constexpr int64_t kCompilerCheckIntervalMs = 10;
static constexpr size_t kMinTracesForCompilation = 1;

struct LastJobInfo {
  time_t last_run_ns_{0};
  size_t activities_last_compiled_{0};
};

LastJobInfo last_job_info_;
std::mutex last_job_info_mutex_;

// Gets the path of output compiled trace.
db::CompiledTraceFileModel CalculateNewestFilePath(
    const std::string& package_name,
    const std::string& activity_name,
    int version) {
   db::VersionedComponentName versioned_component_name{
     package_name, activity_name, version};

   db::CompiledTraceFileModel output_file =
       db::CompiledTraceFileModel::CalculateNewestFilePath(versioned_component_name);

   return output_file;
}

using ArgString = const char*;

static constexpr const char kCommandFileName[] = "/system/bin/iorap.cmd.compiler";

int Exec::Execve(const std::string& pathname,
                 std::vector<std::string>& argv_vec,
                 char *const envp[]) {
  std::unique_ptr<ArgString[]> argv_ptr =
      common::VecToArgv(kCommandFileName, argv_vec);

  return execve(pathname.c_str(), (char**)argv_ptr.get(), envp);
}

pid_t Exec::Fork() {
  return fork();
}

// Represents the parameters used when fork+exec compiler.
struct CompilerForkParameters {
  std::vector<std::string> input_pbs;
  std::vector<uint64_t> timestamp_limit_ns;
  std::string output_proto;
  ControllerParameters controller_params;

  CompilerForkParameters(const std::vector<compiler::CompilationInput>& perfetto_traces,
                         const std::string& output_proto,
                         ControllerParameters controller_params) :
    output_proto(output_proto), controller_params(controller_params) {
        for (compiler::CompilationInput perfetto_trace : perfetto_traces) {
          input_pbs.push_back(perfetto_trace.filename);
          timestamp_limit_ns.push_back(perfetto_trace.timestamp_limit_ns);
        }
  }
};

std::vector<std::string> MakeCompilerParams(const CompilerForkParameters& params) {
    std::vector<std::string> argv;
    ControllerParameters controller_params = params.controller_params;

    common::AppendArgsRepeatedly(argv, params.input_pbs);
    common::AppendArgsRepeatedly(argv, "--timestamp_limit_ns", params.timestamp_limit_ns);

    if (controller_params.output_text) {
      argv.push_back("--output-text");
    }

    common::AppendArgs(argv, "--output-proto", params.output_proto);

    if (controller_params.inode_textcache) {
      common::AppendArgs(argv, "--inode-textcache", *controller_params.inode_textcache);
    }

    if (controller_params.verbose) {
      argv.push_back("--verbose");
    }

    return argv;
}

// Sets a watch dog for the given pid and kill it if timeout.
std::thread SetTimeoutWatchDog(pid_t pid, int64_t timeout_ms, std::atomic<bool>& cancel_watchdog) {
  std::thread watchdog_thread{[pid, timeout_ms, &cancel_watchdog]() {
    std::chrono::time_point start = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    while (!cancel_watchdog) {
      int status = kill(pid, 0);
      if (status != 0) {
        LOG(DEBUG) << "Process (" << pid << ") doesn't exist now.";
        break;
      }
      std::chrono::time_point cur = std::chrono::system_clock::now();
      if (cur - start > timeout) {
        LOG(INFO) << "Process (" << pid << ") is timeout!";
        LOG(INFO) << "start time: "
                   << std::chrono::system_clock::to_time_t(start)
                   << " end time: "
                   << std::chrono::system_clock::to_time_t(cur)
                   << " timeout: "
                   << timeout_ms;
        kill(pid, SIGKILL);
        break;
      }
      usleep(kCompilerCheckIntervalMs * 1000);
    }
  }};

  return watchdog_thread;
}

bool StartViaFork(const CompilerForkParameters& params) {
  const ControllerParameters& controller_params = params.controller_params;
  pid_t child = controller_params.exec->Fork();

  if (child == -1) {
    LOG(FATAL) << "Failed to fork a process for compilation";
  } else if (child > 0) {  // we are the caller of this function
    LOG(DEBUG) << "forked into a process for compilation , pid = " << child;

    int64_t compiler_timeout_ms =
        android::base::GetIntProperty("iorapd.maintenance.compiler_timeout_ms",
                                       /*default*/ 10 * 60 * 1000); // 10 min
    std::atomic<bool> cancel_watchdog(false);
    std::thread watchdog_thread = SetTimeoutWatchDog(child, compiler_timeout_ms, cancel_watchdog);
    int wstatus;
    waitpid(child, /*out*/&wstatus, /*options*/0);

    // Terminate the thread after the compiler process is killed or done.
    LOG(DEBUG) << "Terminate the watch dog thread.";
    cancel_watchdog = true;
    watchdog_thread.join();

    if (!WIFEXITED(wstatus)) {
      LOG(ERROR) << "Child terminated abnormally, status: " << WEXITSTATUS(wstatus);
      return false;
    }

    int status = WEXITSTATUS(wstatus);
    LOG(DEBUG) << "Child terminated, status: " << status;
    if (status == 0) {
      LOG(DEBUG) << "Iorap compilation succeeded";
      return true;
    } else {
      LOG(ERROR) << "Iorap compilation failed";
      return false;
    }
  } else {
    // we are the child that was forked.
    std::vector<std::string> argv_vec = MakeCompilerParams(params);
    std::unique_ptr<ArgString[]> argv_ptr =
        common::VecToArgv(kCommandFileName, argv_vec);

    std::stringstream argv; // for debugging.
    for (std::string arg : argv_vec) {
      argv  << arg << ' ';
    }
    LOG(DEBUG) << "fork+exec: " << kCommandFileName << " " << argv.str();

    controller_params.exec->Execve(kCommandFileName,
                                          argv_vec,
                                         /*envp*/nullptr);
    // This should never return.
  }
  return false;
}

// Gets the perfetto trace infos in the histories.
std::vector<compiler::CompilationInput> GetPerfettoTraceInfo(
    const db::DbHandle& db,
    const std::vector<db::AppLaunchHistoryModel>& histories) {
  std::vector<compiler::CompilationInput> perfetto_traces;

  for(db::AppLaunchHistoryModel history : histories) {
    // Get perfetto trace.
    std::optional<db::RawTraceModel> raw_trace =
        db::RawTraceModel::SelectByHistoryId(db, history.id);
    if (!raw_trace) {
      // This is normal: non-cold launches do not have traces.
      continue;
    }

    uint64_t timestamp_limit = std::numeric_limits<uint64_t>::max();
    // Get corresponding timestamp limit.
    if (history.report_fully_drawn_ns) {
      timestamp_limit = *history.report_fully_drawn_ns;
    } else if (history.total_time_ns) {
      timestamp_limit = *history.total_time_ns;
    } else {
      LOG(DEBUG) << " No timestamp exists. Using the max value.";
    }
    perfetto_traces.push_back({raw_trace->file_path, timestamp_limit});
  }
  return perfetto_traces;
}

// Helper struct for printing vector.
template <class T>
struct VectorPrinter {
  std::vector<T>& values;
};

std::ostream& operator<<(std::ostream& os,
                      const struct compiler::CompilationInput& perfetto_trace) {
  os << "file_path: " << perfetto_trace.filename << " "
     << "timestamp_limit: " << perfetto_trace.timestamp_limit_ns;
  return os;
}

template <class T>
std::ostream& operator<<(std::ostream& os, const struct VectorPrinter<T>& printer) {
  os << "[\n";
  for (T i : printer.values) {
    os << i << ",\n";
  }
  os << "]\n";
  return os;
}

// Compiled the perfetto traces for an activity.
bool CompileActivity(const db::DbHandle& db,
                     int package_id,
                     const std::string& package_name,
                     const std::string& activity_name,
                     int version,
                     const ControllerParameters& params) {
  ScopedFormatTrace atrace_compile_package(ATRACE_TAG_PACKAGE_MANAGER,
                                           "Compile activity %s",
                                           activity_name.c_str());

  LOG(DEBUG) << "CompileActivity: " << package_name << "/" << activity_name << "@" << version;

  db::CompiledTraceFileModel output_file =
      CalculateNewestFilePath(package_name, activity_name, version);

  std::string file_path = output_file.FilePath();

  if (!params.recompile) {
    if (std::filesystem::exists(file_path)) {
      LOG(DEBUG) << "compiled trace exists in " << file_path;

      db::VersionedComponentName vcn{package_name, activity_name, version};
      std::optional<db::PrefetchFileModel> prefetch_file =
          db::PrefetchFileModel::SelectByVersionedComponentName(db, vcn);
      if (prefetch_file) {
        return true;
      } else {
        LOG(WARNING) << "Missing corresponding prefetch_file db row for " << vcn;
        // let it go and compile again. we'll insert the prefetch_file at the bottom.
      }
    }
  }

  std::optional<db::ActivityModel> activity =
      db::ActivityModel::SelectByNameAndPackageId(db, activity_name.c_str(), package_id);
  if (!activity) {
    LOG(ERROR) << "Cannot find activity for package_id: " << package_id
               <<" activity_name: " <<activity_name;
    return false;
  }

  int activity_id = activity->id;

  std::vector<db::AppLaunchHistoryModel> histories =
      db::AppLaunchHistoryModel::SelectActivityHistoryForCompile(db, activity_id);

  {
    std::vector<compiler::CompilationInput> perfetto_traces =
        GetPerfettoTraceInfo(db, histories);

    if (perfetto_traces.size() < params.min_traces) {
      LOG(DEBUG) << "The number of perfetto traces is " << perfetto_traces.size()
                 <<", which is less than " << params.min_traces;
      return false;
    }

    {
      std::lock_guard<std::mutex> last_job_info_guard{last_job_info_mutex_};
      last_job_info_.activities_last_compiled_++;
    }

    // Show the compilation config.
    LOG(DEBUG) << "Try to compiled package_id: " << package_id
               << " package_name: " << package_name
               << " activity_name: " << activity_name
               << " version: " << version
               << " file_path: " << file_path
               << " verbose: " << params.verbose
               << " perfetto_traces: "
               << VectorPrinter<compiler::CompilationInput>{perfetto_traces};
    if (params.inode_textcache) {
      LOG(DEBUG) << "inode_textcache: " << *params.inode_textcache;
    }

    CompilerForkParameters compiler_params{perfetto_traces, file_path, params};

    if (!output_file.MkdirWithParents()) {
      LOG(ERROR) << "Compile activity failed. Failed to mkdirs " << file_path;
      return false;
    }

    ScopedFormatTrace atrace_compile_fork(ATRACE_TAG_PACKAGE_MANAGER,
                                          "Fork+exec iorap.cmd.compiler",
                                          activity_name.c_str());
    if (!StartViaFork(compiler_params)) {
      LOG(ERROR) << "Compilation failed for package_id:" << package_id
                 << " activity_name: " << activity_name;
      return false;
    }
  }

  std::optional<db::PrefetchFileModel> compiled_trace =
      db::PrefetchFileModel::Insert(db, activity_id, file_path);
  if (!compiled_trace) {
    LOG(ERROR) << "Cannot insert compiled trace activity_id: " << activity_id
               << " file_path: " << file_path;
    return false;
  }
  return true;
}

// Compiled the perfetto traces for activities in an package.
bool CompilePackage(const db::DbHandle& db,
                    const std::string& package_name,
                    int version,
                    const ControllerParameters& params) {
  ScopedFormatTrace atrace_compile_package(ATRACE_TAG_PACKAGE_MANAGER,
                                           "Compile package %s",
                                           package_name.c_str());

  std::optional<db::PackageModel> package =
        db::PackageModel::SelectByNameAndVersion(db, package_name.c_str(), version);

  if (!package) {
    LOG(ERROR) << "Cannot find package for package_name: "
               << package_name
               << " and version "
               << version;
    return false;
  }

  std::vector<db::ActivityModel> activities =
      db::ActivityModel::SelectByPackageId(db, package->id);

  bool ret = true;
  for (db::ActivityModel activity : activities) {
    if (!CompileActivity(db, package->id, package->name, activity.name, version, params)) {
      ret = false;
    }
  }
  return ret;
}

// Compiled the perfetto traces for packages in a device.
bool CompileAppsOnDevice(const db::DbHandle& db, const ControllerParameters& params) {
  {
    std::lock_guard<std::mutex> last_job_info_guard{last_job_info_mutex_};
    last_job_info_.activities_last_compiled_ = 0;
  }

  std::vector<db::PackageModel> packages = db::PackageModel::SelectAll(db);
  bool ret = true;
  for (db::PackageModel package : packages) {
    if (!CompilePackage(db, package.name, package.version, params)) {
      ret = false;
    }
  }

  {
    std::lock_guard<std::mutex> last_job_info_guard{last_job_info_mutex_};
    last_job_info_.last_run_ns_ = time(nullptr);
  }

  return ret;
}

// Compiled the perfetto traces for a single package in a device.
bool CompileSingleAppOnDevice(const db::DbHandle& db,
                              const ControllerParameters& params,
                              const std::string& package_name) {
  std::vector<db::PackageModel> packages = db::PackageModel::SelectByName(db, package_name.c_str());
  bool ret = true;
  for (db::PackageModel package : packages) {
    if (!CompilePackage(db, package.name, package.version, params)) {
      ret = false;
    }
  }

  return ret;
}

bool Compile(const std::string& db_path, const ControllerParameters& params) {
  iorap::db::SchemaModel db_schema = db::SchemaModel::GetOrCreate(db_path);
  db::DbHandle db{db_schema.db()};
  return CompileAppsOnDevice(db, params);
}

bool Compile(const std::string& db_path,
             const std::string& package_name,
             int version,
             const ControllerParameters& params) {
  iorap::db::SchemaModel db_schema = db::SchemaModel::GetOrCreate(db_path);
  db::DbHandle db{db_schema.db()};
  return CompilePackage(db, package_name, version, params);
}

bool Compile(const std::string& db_path,
             const std::string& package_name,
             const std::string& activity_name,
             int version,
             const ControllerParameters& params) {
  iorap::db::SchemaModel db_schema = db::SchemaModel::GetOrCreate(db_path);
  db::DbHandle db{db_schema.db()};

  std::optional<db::PackageModel> package =
      db::PackageModel::SelectByNameAndVersion(db, package_name.c_str(), version);

  if (!package) {
    LOG(ERROR) << "Cannot find package with name "
               << package_name
               << " and version "
               << version;
    return false;
  }
  return CompileActivity(db, package->id, package_name, activity_name, version, params);
}

static std::string TimeToString(time_t the_time) {
  tm tm_buf{};
  tm* tm_ptr = localtime_r(&the_time, &tm_buf);

  if (tm_ptr != nullptr) {
    char time_buffer[256];
    strftime(time_buffer, sizeof(time_buffer), "%a %b %d %H:%M:%S %Y", tm_ptr);
    return std::string{time_buffer};
  } else {
    return std::string{"(nullptr)"};
  }
}

static std::string GetTimestampForPrefetchFile(const db::PrefetchFileModel& prefetch_file) {
  std::filesystem::path path{prefetch_file.file_path};

  std::error_code ec{};
  auto last_write_time = std::filesystem::last_write_time(path, /*out*/ec);
  if (ec) {
    return std::string("Failed to get last write time: ") + ec.message();
  }

  time_t time = decltype(last_write_time)::clock::to_time_t(last_write_time);

  std::string time_str = TimeToString(time);
  return time_str;
}

void DumpPackageActivity(const db::DbHandle& db,
                         ::android::Printer& printer,
                         const db::PackageModel& package,
                         const db::ActivityModel& activity) {
  int package_id = package.id;
  const std::string& package_name = package.name;
  int package_version = package.version;
  const std::string& activity_name = activity.name;
  db::VersionedComponentName vcn{package_name, activity_name, package_version};

  // com.google.Settings/com.google.Settings.ActivityMain@1234567890
  printer.printFormatLine("  %s/%s@%d",
                          package_name.c_str(),
                          activity_name.c_str(),
                          package_version);

  std::optional<db::PrefetchFileModel> prefetch_file =
      db::PrefetchFileModel::SelectByVersionedComponentName(db, vcn);

  std::vector<db::AppLaunchHistoryModel> histories =
      db::AppLaunchHistoryModel::SelectActivityHistoryForCompile(db, activity.id);
  std::vector<compiler::CompilationInput> perfetto_traces =
        GetPerfettoTraceInfo(db, histories);

  if (prefetch_file) {
    bool exists_on_disk = std::filesystem::exists(prefetch_file->file_path);

    std::optional<size_t> prefetch_byte_sum =
        prefetcher::ReadAhead::PrefetchSizeInBytes(prefetch_file->file_path);

    if (exists_on_disk) {
      printer.printFormatLine("    Compiled Status: Usable compiled trace");
    } else {
      printer.printFormatLine("    Compiled Status: Prefetch file deleted from disk.");
    }

    if (prefetch_byte_sum) {
      printer.printFormatLine("      Bytes to be prefetched: %zu", *prefetch_byte_sum);
    } else {
      printer.printFormatLine("      Bytes to be prefetched: (bad file path)" );
    }

    printer.printFormatLine("      Time compiled: %s",
                            GetTimestampForPrefetchFile(*prefetch_file).c_str());
    printer.printFormatLine("      %s", prefetch_file->file_path.c_str());
  } else {
    size_t size = perfetto_traces.size();

    if (size >= kMinTracesForCompilation) {
      printer.printFormatLine("    Compiled Status: Raw traces pending compilation (%zu)",
                              perfetto_traces.size());
    } else {
      size_t remaining = kMinTracesForCompilation - size;
      printer.printFormatLine("    Compiled Status: Need %zu more traces for compilation",
                              remaining);
    }
  }

  printer.printFormatLine("    Raw traces:");
  printer.printFormatLine("      Trace count: %zu", perfetto_traces.size());

  for (compiler::CompilationInput& compilation_input : perfetto_traces) {
    std::string& raw_trace_file_name = compilation_input.filename;

    printer.printFormatLine("      %s", raw_trace_file_name.c_str());
  }
}

void DumpPackage(const db::DbHandle& db,
                 ::android::Printer& printer,
                 db::PackageModel package) {
  std::vector<db::ActivityModel> activities =
      db::ActivityModel::SelectByPackageId(db, package.id);

  for (db::ActivityModel& activity : activities) {
    DumpPackageActivity(db, printer, package, activity);
  }
}

void DumpAllPackages(const db::DbHandle& db, ::android::Printer& printer) {
  printer.printLine("Package history in database:");

  std::vector<db::PackageModel> packages = db::PackageModel::SelectAll(db);
  for (db::PackageModel package : packages) {
    DumpPackage(db, printer, package);
  }

  printer.printLine("");
}

void Dump(const db::DbHandle& db, ::android::Printer& printer) {
  bool locked = last_job_info_mutex_.try_lock();

  LastJobInfo info = last_job_info_;

  printer.printFormatLine("Background job:");
  if (!locked) {
    printer.printLine("""""  (possible deadlock)");
  }
  if (info.last_run_ns_ != time_t{0}) {
    std::string time_str = TimeToString(info.last_run_ns_);

    printer.printFormatLine("  Last run at: %s", time_str.c_str());
  } else {
    printer.printFormatLine("  Last run at: (None)");
  }
  printer.printFormatLine("  Activities last compiled: %zu", info.activities_last_compiled_);

  printer.printLine("");

  if (locked) {
    last_job_info_mutex_.unlock();
  }

  DumpAllPackages(db, printer);
}

}  // namespace iorap::maintenance
