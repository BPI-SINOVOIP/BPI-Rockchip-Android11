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

#include "read_ahead.h"

#include "common/trace.h"
#include "prefetcher/session_manager.h"
#include "prefetcher/session.h"
#include "prefetcher/task_id.h"
#include "serialize/arena_ptr.h"
#include "serialize/protobuf_io.h"

#include <android-base/chrono_utils.h>
#include <android-base/logging.h>
#include <android-base/scopeguard.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <cutils/trace.h>
#include <deque>
#include <fcntl.h>
#include <functional>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unordered_map>
#include <utils/Printer.h>

namespace iorap {
namespace prefetcher {

enum class PrefetchStrategy {
  kFadvise = 0,
  kMmapLocked = 1,
  kMlock = 2,
};

std::ostream& operator<<(std::ostream& os, PrefetchStrategy ps) {
  switch (ps) {
    case PrefetchStrategy::kFadvise:
      os << "fadvise";
      break;
    case PrefetchStrategy::kMmapLocked:
      os << "mmap";
      break;
    case PrefetchStrategy::kMlock:
      os << "mlock";
      break;
    default:
      os << "<invalid>";
  }
  return os;
}

static constexpr PrefetchStrategy kPrefetchStrategy = PrefetchStrategy::kFadvise;

static PrefetchStrategy GetPrefetchStrategy() {
  PrefetchStrategy strat = PrefetchStrategy::kFadvise;

  std::string prefetch_env =
      ::android::base::GetProperty("iorapd.readahead.strategy", /*default*/"");

  if (prefetch_env == "") {
    LOG(VERBOSE)
        << "ReadAhead strategy defaulted. Did you want to set iorapd.readahead.strategy ?";
  } else if (prefetch_env == "mmap") {
    strat = PrefetchStrategy::kMmapLocked;
    LOG(VERBOSE) << "ReadAhead strategy: kMmapLocked";
  } else if (prefetch_env == "mlock") {
    strat = PrefetchStrategy::kMlock;
    LOG(VERBOSE) << "ReadAhead strategy: kMlock";
  } else if (prefetch_env == "fadvise") {
    strat = PrefetchStrategy::kFadvise;
    LOG(VERBOSE) << "ReadAhead strategy: kFadvise";
  } else {
    LOG(WARNING) << "Unknown iorapd.readahead.strategy: " << prefetch_env << ", ignoring";
  }

  return strat;
}

struct TaskData {
  TaskId task_id;   // also the session ID.

  size_t SessionId() const {
    if (session != nullptr) {
      DCHECK_EQ(session->SessionId(), task_id.id);
    }

    // good enough to be used as the session ID. Task IDs are always monotonically increasing.
    return task_id.id;
  }

  std::shared_ptr<Session> session;
  int32_t trace_cookie;  // async trace cookie in BeginTask/FinishTask.
};

// Remember the last 5 files being prefetched.
static constexpr size_t kRecentDataCount = 5;

struct RecentData {
  TaskId task_id;
  size_t file_lengths_sum;
};

struct RecentDataKeeper {
  std::deque<RecentData> recents_;
  std::mutex mutex_;

  void RecordRecent(TaskId task_id, size_t file_lengths_sum) {
    std::lock_guard<std::mutex> guard{mutex_};

    while (recents_.size() > kRecentDataCount) {
      recents_.pop_front();
    }
    recents_.push_back(RecentData{std::move(task_id), file_lengths_sum});
  }

  void Dump(/*borrow*/::android::Printer& printer) {
    bool locked = mutex_.try_lock();

    printer.printFormatLine("Recent prefetches:");
    if (!locked) {
      printer.printLine("""""  (possible deadlock)");
    }

    for (const RecentData& data : recents_) {
      printer.printFormatLine("  %s", data.task_id.path.c_str());
      printer.printFormatLine("    Task ID: %zu", data.task_id.id);
      printer.printFormatLine("    Bytes count: %zu", data.file_lengths_sum);
    }

    if (recents_.empty()) {
      printer.printFormatLine("  (None)");
    }

    printer.printLine("");

    if (locked) {
      mutex_.unlock();
    }
  }
};

struct ReadAhead::Impl {
  Impl(bool use_sockets) {
    // Flip this property to test in-process vs out-of-process for the prefetcher code.
    bool out_of_process =
        ::android::base::GetBoolProperty("iorapd.readahead.out_of_process", /*default*/true);

    SessionKind session_kind =
        out_of_process ? SessionKind::kOutOfProcessIpc : SessionKind::kInProcessDirect;

    if (use_sockets) {
      session_kind = SessionKind::kOutOfProcessSocket;
    }

    session_manager_ = SessionManager::CreateManager(session_kind);
    session_kind_ = session_kind;
  }

  std::unique_ptr<SessionManager> session_manager_;
  SessionKind session_kind_;
  std::unordered_map<size_t /*task index*/, TaskData> read_ahead_file_map_;
  static RecentDataKeeper recent_data_keeper_;
  int32_t trace_cookie_{0};

  bool UseSockets() const {
    return session_kind_ == SessionKind::kOutOfProcessSocket;
  }
};

RecentDataKeeper ReadAhead::Impl::recent_data_keeper_{};

ReadAhead::ReadAhead() : ReadAhead(/*use_sockets*/false) {
}

ReadAhead::ReadAhead(bool use_sockets) : impl_(new Impl(use_sockets)) {
}

ReadAhead::~ReadAhead() {}

static bool PerformReadAhead(std::shared_ptr<Session> session, size_t path_id, ReadAheadKind kind, size_t length, size_t offset) {
  return session->ReadAhead(path_id, kind, length, offset);
}

void ReadAhead::FinishTask(const TaskId& id) {
  auto it = impl_->read_ahead_file_map_.find(id.id);
  if (it == impl_->read_ahead_file_map_.end()) {
    LOG(DEBUG) << "Could not find any TaskData for " << id;
    return;
  }

  TaskData& task_data = it->second;
  atrace_async_end(ATRACE_TAG_ACTIVITY_MANAGER,
                   "ReadAhead Task Scope (for File Descriptors)",
                   task_data.trace_cookie);

  auto deleter = [&]() {
    impl_->read_ahead_file_map_.erase(it);
  };
  auto scope_guard = android::base::make_scope_guard(deleter);
  (void)scope_guard;

  LOG(VERBOSE) << "ReadAhead (Finish)";

  if (!impl_->session_manager_->DestroySession(task_data.SessionId())) {
    LOG(WARNING) << "ReadAhead: Failed to destroy Session " << task_data.SessionId();
  }
}

void ReadAhead::BeginTaskForSockets(const TaskId& id, int32_t trace_cookie) {
  LOG(VERBOSE) << "BeginTaskForSockets: " << id;

  // TODO: atrace.
  android::base::Timer timer{};
  android::base::Timer open_timer{};

  int trace_file_fd_raw =
      TEMP_FAILURE_RETRY(open(id.path.c_str(), /*flags*/O_RDONLY));

  android::base::unique_fd trace_file_fd{trace_file_fd_raw};

  if (!trace_file_fd.ok()) {
    PLOG(ERROR) << "ReadAhead failed to open trace file: " << id.path;
    return;
  }

  TaskData task_data;
  task_data.task_id = id;
  task_data.trace_cookie = trace_cookie;

  std::shared_ptr<Session> session =
      impl_->session_manager_->CreateSession(task_data.SessionId(),
                                             /*description*/id.path,
                                             trace_file_fd.get());
  task_data.session = session;
  CHECK(session != nullptr);

  task_data.trace_cookie = ++trace_cookie;

  // TODO: maybe getprop and a single line by default?
  session->Dump(LOG_STREAM(INFO), /*multiline*/true);

  impl_->read_ahead_file_map_[id.id] = std::move(task_data);
  // FinishTask is identical, as it just destroys the session.
}

void ReadAhead::BeginTask(const TaskId& id) {
  {
    struct timeval now;
    gettimeofday(&now, nullptr);

    uint64_t now_usec = (now.tv_sec * 1000000LL + now.tv_usec);
    LOG(DEBUG) << "BeginTask: beginning usec: " << now_usec;
  }

  int32_t trace_cookie = ++impl_->trace_cookie_;
  atrace_async_begin(ATRACE_TAG_ACTIVITY_MANAGER,
                     "ReadAhead Task Scope (for File Descriptors)",
                     trace_cookie);

  if (impl_->UseSockets()) {
    BeginTaskForSockets(id, trace_cookie);
    return;
  }

  LOG(VERBOSE) << "BeginTask: " << id;

  // TODO: atrace.
  android::base::Timer timer{};

  // TODO: refactor this code with SessionDirect::ProcessFd ?
  TaskData task_data;
  task_data.task_id = id;
  task_data.trace_cookie = trace_cookie;

  ScopedFormatTrace atrace_begin_task(ATRACE_TAG_ACTIVITY_MANAGER,
                                      "ReadAhead::BeginTask %s",
                                      id.path.c_str());

  // Include CreateSession above the Protobuf deserialization so that we can include
  // the 'total_duration' as part of the Session dump (relevant when we use IPC mode only).
  std::shared_ptr<Session> session =
      impl_->session_manager_->CreateSession(task_data.SessionId(),
                                             /*description*/id.path);

  android::base::Timer open_timer{};

  // XX: Should we rename all the 'Create' to 'Make', or rename the 'Make' to 'Create' ?
  // Unfortunately make_unique, make_shared, etc is the standard C++ terminology.
  serialize::ArenaPtr<serialize::proto::TraceFile> trace_file_ptr =
      serialize::ProtobufIO::Open(id.path);

  if (trace_file_ptr == nullptr) {
    // TODO: distinguish between missing trace (this is OK, most apps wont have one)
    // and a bad error.
    LOG(DEBUG) << "ReadAhead could not start, missing trace file? " << id.path;
    return;
  }

  task_data.session = session;
  CHECK(session != nullptr);

  ReadAheadKind kind = static_cast<ReadAheadKind>(GetPrefetchStrategy());

  // TODO: The "Task[Id]" should probably be the one owning the trace file.
  // When the task is fully complete, the task can be deleted and the
  // associated arenas can go with them.

  // TODO: we should probably have the file entries all be relative
  // to the package path?

  // Open every file in the trace index.
  const serialize::proto::TraceFileIndex& index = trace_file_ptr->index();

  size_t count_entries = 0;
  {
    ScopedFormatTrace atrace_register_file_paths(ATRACE_TAG_ACTIVITY_MANAGER,
                                                 "ReadAhead::RegisterFilePaths %s",
                                                 id.path.c_str());
    for (const serialize::proto::TraceFileIndexEntry& index_entry : index.entries()) {
      LOG(VERBOSE) << "ReadAhead: found file entry: " << index_entry.file_name();

      if (index_entry.id() < 0) {
        LOG(WARNING) << "ReadAhead: Skip bad TraceFileIndexEntry, negative ID not allowed: "
                     << index_entry.id();
        continue;
      }

      size_t path_id = index_entry.id();
      const auto& path_file_name = index_entry.file_name();

      if (!session->RegisterFilePath(path_id, path_file_name)) {
        LOG(WARNING) << "ReadAhead: Failed to register file path: " << path_file_name;
      } else {
        ++count_entries;
      }
    }
  }
  LOG(VERBOSE) << "ReadAhead: Registered " << count_entries << " file paths";
  std::chrono::milliseconds open_duration_ms = open_timer.duration();

  LOG(DEBUG) << "ReadAhead: Opened file&headers in " << open_duration_ms.count() << "ms";

  size_t length_sum = 0;
  size_t entry_offset = 0;
  {
    ScopedFormatTrace atrace_perform_read_ahead(ATRACE_TAG_ACTIVITY_MANAGER,
                                                "ReadAhead::PerformReadAhead entries=%zu, path=%s",
                                                count_entries,
                                                id.path.c_str());

    // Go through every trace entry and readahead every (file,offset,len) tuple.
    const serialize::proto::TraceFileList& file_list = trace_file_ptr->list();
    for (const serialize::proto::TraceFileEntry& file_entry : file_list.entries()) {
      ++entry_offset;

      if (file_entry.file_length() < 0 || file_entry.file_offset() < 0) {
        LOG(WARNING) << "ReadAhead entry negative file length or offset, illegal: "
                     << "index_id=" << file_entry.index_id() << ", skipping";
        continue;
      }

      // Attempt to perform readahead. This can generate more warnings dynamically.
      if (!PerformReadAhead(session, file_entry.index_id(), kind, file_entry.file_length(), file_entry.file_offset())) {
        // TODO: Do we need below at all? The always-on Dump already prints a % of failed entries.
        // LOG(WARNING) << "Failed readahead, bad file length/offset in entry @ " << (entry_offset - 1);
      }

      length_sum += static_cast<size_t>(file_entry.file_length());
    }
  }

  {
    ScopedFormatTrace atrace_session_dump(ATRACE_TAG_ACTIVITY_MANAGER,
                                          "ReadAhead Session Dump entries=%zu",
                                          entry_offset);
    // TODO: maybe getprop and a single line by default?
    session->Dump(LOG_STREAM(INFO), /*multiline*/true);
  }

  atrace_int(ATRACE_TAG_ACTIVITY_MANAGER,
             "ReadAhead Bytes Length",
             static_cast<int32_t>(length_sum));

  impl_->read_ahead_file_map_[id.id] = std::move(task_data);

  ReadAhead::Impl::recent_data_keeper_.RecordRecent(id, length_sum);
}

void ReadAhead::Dump(::android::Printer& printer) {
  ReadAhead::Impl::recent_data_keeper_.Dump(printer);
}

std::optional<size_t> ReadAhead::PrefetchSizeInBytes(const std::string& file_path) {
  serialize::ArenaPtr<serialize::proto::TraceFile> trace_file_ptr =
      serialize::ProtobufIO::Open(file_path);

  if (trace_file_ptr == nullptr) {
    LOG(WARNING) << "PrefetchSizeInBytes: bad file at " << file_path;
    return std::nullopt;
  }

  size_t length_sum = 0;
  const serialize::proto::TraceFileList& file_list = trace_file_ptr->list();
  for (const serialize::proto::TraceFileEntry& file_entry : file_list.entries()) {

    if (file_entry.file_length() < 0 || file_entry.file_offset() < 0) {
      LOG(WARNING) << "ReadAhead entry negative file length or offset, illegal: "
                   << "index_id=" << file_entry.index_id() << ", skipping";
      continue;
    }

    length_sum += static_cast<size_t>(file_entry.file_length());
  }

  return length_sum;
}

}  // namespace prefetcher
}  // namespace iorap

