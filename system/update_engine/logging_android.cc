//
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
//

#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <base/files/dir_reader_posix.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <log/log.h>

#include "update_engine/common/utils.h"

using std::string;

#ifdef _UE_SIDELOAD
constexpr bool kSideload = true;
#else
constexpr bool kSideload = false;
#endif

namespace chromeos_update_engine {
namespace {

constexpr char kSystemLogsRoot[] = "/data/misc/update_engine_log";
constexpr size_t kLogCount = 5;

// Keep the most recent |kLogCount| logs but remove the old ones in
// "/data/misc/update_engine_log/".
void DeleteOldLogs(const string& kLogsRoot) {
  base::DirReaderPosix reader(kLogsRoot.c_str());
  if (!reader.IsValid()) {
    LOG(ERROR) << "Failed to read " << kLogsRoot;
    return;
  }

  std::vector<string> old_logs;
  while (reader.Next()) {
    if (reader.name()[0] == '.')
      continue;

    // Log files are in format "update_engine.%Y%m%d-%H%M%S",
    // e.g. update_engine.20090103-231425
    uint64_t date;
    uint64_t local_time;
    if (sscanf(reader.name(),
               "update_engine.%" PRIu64 "-%" PRIu64 "",
               &date,
               &local_time) == 2) {
      old_logs.push_back(reader.name());
    } else {
      LOG(WARNING) << "Unrecognized log file " << reader.name();
    }
  }

  std::sort(old_logs.begin(), old_logs.end(), std::greater<string>());
  for (size_t i = kLogCount; i < old_logs.size(); i++) {
    string log_path = kLogsRoot + "/" + old_logs[i];
    if (unlink(log_path.c_str()) == -1) {
      PLOG(WARNING) << "Failed to unlink " << log_path;
    }
  }
}

string SetupLogFile(const string& kLogsRoot) {
  DeleteOldLogs(kLogsRoot);

  return base::StringPrintf("%s/update_engine.%s",
                            kLogsRoot.c_str(),
                            utils::GetTimeAsString(::time(nullptr)).c_str());
}

const char* LogPriorityToCString(int priority) {
  switch (priority) {
    case ANDROID_LOG_VERBOSE:
      return "VERBOSE";
    case ANDROID_LOG_DEBUG:
      return "DEBUG";
    case ANDROID_LOG_INFO:
      return "INFO";
    case ANDROID_LOG_WARN:
      return "WARN";
    case ANDROID_LOG_ERROR:
      return "ERROR";
    case ANDROID_LOG_FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

using LoggerFunction = std::function<void(const struct __android_log_message*)>;

class FileLogger {
 public:
  explicit FileLogger(const string& path) {
    fd_.reset(TEMP_FAILURE_RETRY(
        open(path.c_str(),
             O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW | O_SYNC,
             0644)));
    if (fd_ == -1) {
      // Use ALOGE that logs to logd before __android_log_set_logger.
      ALOGE("Cannot open persistent log %s: %s", path.c_str(), strerror(errno));
      return;
    }
    // The log file will have AID_LOG as group ID; this GID is inherited from
    // the parent directory "/data/misc/update_engine_log" which sets the SGID
    // bit.
    if (fchmod(fd_.get(), 0640) == -1) {
      // Use ALOGE that logs to logd before __android_log_set_logger.
      ALOGE("Cannot chmod 0640 persistent log %s: %s",
            path.c_str(),
            strerror(errno));
      return;
    }
  }
  // Copy-constructor needed to be converted to std::function.
  FileLogger(const FileLogger& other) { fd_.reset(dup(other.fd_)); }
  void operator()(const struct __android_log_message* log_message) {
    if (fd_ == -1) {
      return;
    }

    std::string_view message_str =
        log_message->message != nullptr ? log_message->message : "";

    WriteToFd(GetPrefix(log_message));
    WriteToFd(message_str);
    WriteToFd("\n");
  }

 private:
  android::base::unique_fd fd_;
  void WriteToFd(std::string_view message) {
    ignore_result(
        android::base::WriteFully(fd_, message.data(), message.size()));
  }

  string GetPrefix(const struct __android_log_message* log_message) {
    std::stringstream ss;
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;
    ss << "[" << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
       << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
       << tm_time->tm_hour << std::setw(2) << tm_time->tm_min << std::setw(2)
       << tm_time->tm_sec << '.' << std::setw(6) << tv.tv_usec << "] ";
    // libchrome logs prepends |message| with severity, file and line, but
    // leave logger_data->file as nullptr.
    // libbase / liblog logs doesn't. Hence, add them to match the style.
    // For liblog logs that doesn't set logger_data->file, not printing the
    // priority is acceptable.
    if (log_message->file) {
      ss << "[" << LogPriorityToCString(log_message->priority) << ':'
         << log_message->file << '(' << log_message->line << ")] ";
    }
    return ss.str();
  }
};

class CombinedLogger {
 public:
  CombinedLogger(bool log_to_system, bool log_to_file) {
    if (log_to_system) {
      if (kSideload) {
        // No logd in sideload. Use stdout.
        // recovery has already redirected stdio properly.
        loggers_.push_back(__android_log_stderr_logger);
      } else {
        loggers_.push_back(__android_log_logd_logger);
      }
    }
    if (log_to_file) {
      loggers_.push_back(std::move(FileLogger(SetupLogFile(kSystemLogsRoot))));
    }
  }
  void operator()(const struct __android_log_message* log_message) {
    for (auto&& logger : loggers_) {
      logger(log_message);
    }
  }

 private:
  std::vector<LoggerFunction> loggers_;
};

// Redirect all libchrome logs to liblog using our custom handler that does
// not call __android_log_write and explicitly write to stderr at the same
// time. The preset CombinedLogger already writes to stderr properly.
bool RedirectToLiblog(int severity,
                      const char* file,
                      int line,
                      size_t message_start,
                      const std::string& str_newline) {
  android_LogPriority priority =
      (severity < 0) ? ANDROID_LOG_VERBOSE : ANDROID_LOG_UNKNOWN;
  switch (severity) {
    case logging::LOG_INFO:
      priority = ANDROID_LOG_INFO;
      break;
    case logging::LOG_WARNING:
      priority = ANDROID_LOG_WARN;
      break;
    case logging::LOG_ERROR:
      priority = ANDROID_LOG_ERROR;
      break;
    case logging::LOG_FATAL:
      priority = ANDROID_LOG_FATAL;
      break;
  }
  std::string_view sv = str_newline;
  ignore_result(android::base::ConsumeSuffix(&sv, "\n"));
  std::string str(sv.data(), sv.size());
  // This will eventually be redirected to CombinedLogger.
  // Use nullptr as tag so that liblog infers log tag from getprogname().
  __android_log_write(priority, nullptr /* tag */, str.c_str());
  return true;
}

}  // namespace

void SetupLogging(bool log_to_system, bool log_to_file) {
  // Note that libchrome logging uses liblog.
  // By calling liblog's __android_log_set_logger function, all of libchrome
  // (used by update_engine) / libbase / liblog (used by depended modules)
  // logging eventually redirects to CombinedLogger.
  static auto g_logger =
      std::make_unique<CombinedLogger>(log_to_system, log_to_file);
  __android_log_set_logger([](const struct __android_log_message* log_message) {
    (*g_logger)(log_message);
  });

  // libchrome logging should not log to file.
  logging::LoggingSettings log_settings;
  log_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  log_settings.logging_dest =
      static_cast<logging::LoggingDestination>(logging::LOG_NONE);
  log_settings.log_file = nullptr;
  logging::InitLogging(log_settings);
  logging::SetLogItems(false /* enable_process_id */,
                       false /* enable_thread_id */,
                       false /* enable_timestamp */,
                       false /* enable_tickcount */);
  logging::SetLogMessageHandler(&RedirectToLiblog);
}

}  // namespace chromeos_update_engine
