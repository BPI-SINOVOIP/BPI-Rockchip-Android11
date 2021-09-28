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

#include "session.h"

#include "prefetcher/prefetcher_daemon.h"
#include "prefetcher/task_id.h"
#include "serialize/arena_ptr.h"
#include "serialize/protobuf_io.h"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <functional>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unordered_map>

namespace iorap {
namespace prefetcher {

// Print per-entry details even if successful. Default-off, too spammy.
static constexpr bool kLogVerboseReadAhead = false;

std::ostream& operator<<(std::ostream& os, const Session& session) {
  session.Dump(os, /*multiline*/false);
  return os;
}

Session::Session() {
}

void SessionBase::Dump(std::ostream& os, bool multiline) const {
  if (!multiline) {
    os << "Session{";
    os << "session_id=" << SessionId();
    os << "}";
    return;
  } else {
    os << "Session (id=" << SessionId() << ")" << std::endl;
    return;
  }
}

SessionBase::SessionBase(size_t session_id, std::string description)
  : session_id_{session_id}, description_{description} {
}

std::optional<std::string_view> SessionBase::GetFilePath(size_t path_id) const {
  auto it = path_map_.find(path_id);
  if (it != path_map_.end()) {
    return {it->second};
  } else {
    return std::nullopt;
  }
}

bool SessionBase::RemoveFilePath(size_t path_id) {
  auto it = path_map_.find(path_id);
  if (it != path_map_.end()) {
    path_map_.erase(it);
    return true;
  } else {
    return false;
  }
}

bool SessionBase::InsertFilePath(size_t path_id, std::string file_path) {
  path_map_.insert({path_id, std::move(file_path)});
  return true;
}

bool SessionBase::ProcessFd(int fd) {
  // Only SessionDirect has an implementation of this.
  // TODO: Maybe add a CommandChoice::kProcessFd ? instead of kCreateFdSession?
  LOG(FATAL) << "SessionBase::ProcessFd is not implemented";

  return false;
}

//
// Direct
//

std::ostream& operator<<(std::ostream& os, const SessionDirect::Entry& entry) {
  os << "Entry{";
  os << "path_id=" << entry.path_id << ",";
  os << "kind=" << static_cast<int>(entry.kind) << ",";
  os << "length=" << entry.length << ",";
  os << "offset=" << entry.offset << ",";
  os << "}";

  return os;
}

// Just in case the failures are slowing down performance, turn them off.
// static constexpr bool kLogFailures = true;
static constexpr bool kLogFailures = false;

bool SessionDirect::RegisterFilePath(size_t path_id, std::string_view file_path) {
  std::string file_path_str{file_path};  // no c_str for string_view.

  auto fd = TEMP_FAILURE_RETRY(open(file_path_str.c_str(), O_RDONLY));
  if (fd < 0) {
    if (kLogFailures) {
      PLOG(ERROR) << "Failed to register file path: " << file_path << ", id=" << path_id
                  << ", open(2) failed: ";
    }
    fd = android::base::unique_fd{};  // mark as 'bad' descriptor.
  }

  LOG(VERBOSE) << "RegisterFilePath path_id=" << path_id << ", file_path=" << file_path_str;

  if (!InsertFilePath(path_id, std::move(file_path_str))) {
    return false;
  }

  path_fd_map_.insert(std::make_pair(path_id, std::move(fd)));
  DCHECK(entry_list_map_[path_id].empty());

  return true;
}


bool SessionDirect::UnregisterFilePath(size_t path_id) {
  if (!RemoveFilePath(path_id)) {
    return false;
  }

  {  // Scoped FD reference lifetime.
    auto maybe_fd = GetFdForPath(path_id);

    DCHECK(*maybe_fd != nullptr);
    const android::base::unique_fd& entry_fd = **maybe_fd;

    auto list = entry_list_map_[path_id];

    for (const EntryMapping& entry_mapping : list) {
      ReadAheadKind kind = entry_mapping.entry.kind;

      switch (kind) {
        case ReadAheadKind::kFadvise:
          // Nothing to do.
          break;
        case ReadAheadKind::kMmapLocked:
        FALLTHROUGH_INTENDED;
        case ReadAheadKind::kMlock:
          // Don't do any erases in the unregister file path to avoid paying O(n^2) erase cost.
          UnmapWithoutErase(entry_mapping);
          break;
      }
    }
  }

  auto it = entry_list_map_.find(path_id);
  auto end = entry_list_map_.end();
  DCHECK(it != end);
  entry_list_map_.erase(it);

  // Close the FD for this file path.
  auto fd_it = path_fd_map_.find(path_id);
  DCHECK(fd_it != path_fd_map_.end());
  path_fd_map_.erase(fd_it);

  return true;
}

// Note: return a pointer because optional doesn't hold references directly.
std::optional<android::base::unique_fd*> SessionDirect::GetFdForPath(size_t path_id) {
  auto it = path_fd_map_.find(path_id);
  if (it == path_fd_map_.end()) {
    return std::nullopt;
  } else {
    return &it->second;
  }
}

bool SessionDirect::ReadAhead(size_t path_id,
                              ReadAheadKind kind,
                              size_t length,
                              size_t offset) {
  // Take by-reference so we can mutate list at the end.
  auto& list = entry_list_map_[path_id];

  Entry entry{path_id, kind, length, offset};
  EntryMapping entry_mapping{entry, /*address*/nullptr, /*success*/false};

  bool success = true;

  auto maybe_fd = GetFdForPath(path_id);
  if (!maybe_fd) {
    LOG(ERROR) << "SessionDirect: Failed to find FD for path_id=" << path_id;
    return false;
  }

  DCHECK(*maybe_fd != nullptr);
  const android::base::unique_fd& entry_fd = **maybe_fd;

  std::optional<std::string_view> file_name_opt = GetFilePath(path_id);
  DCHECK(file_name_opt.has_value());  // if one map has it, all maps have it.
  std::string_view file_name = *file_name_opt;

  if (!entry_fd.ok()) {
    LOG(VERBOSE) << "SessionDirect: No file descriptor for (path_id=" << path_id << ") "
                 << "path '" << file_name << "', failed to readahead entry.";
    // Even failures get kept with success=false.
    list.push_back(entry_mapping);
    return false;
  }

  switch (kind) {
    case ReadAheadKind::kFadvise:
      if (posix_fadvise(entry_fd, offset, length, POSIX_FADV_WILLNEED) < 0) {
        PLOG(ERROR) << "SessionDirect: Failed to fadvise entry " << file_name
                    << ", offset=" << offset << ", length=" << length;
        success = false;
      }
      break;
    case ReadAheadKind::kMmapLocked:
    FALLTHROUGH_INTENDED;
    case ReadAheadKind::kMlock: {
      const bool need_mlock = kind == ReadAheadKind::kMlock;

      int flags = MAP_SHARED;
      if (!need_mlock) {
        // MAP_LOCKED is a best-effort to lock the page. it could still be
        // paged in later at a fault.
        flags |= MAP_LOCKED;
      }

      entry_mapping.address =
        mmap(/*addr*/nullptr, length, PROT_READ, flags, entry_fd, offset);

      if (entry_mapping.address == nullptr) {
        PLOG(ERROR) << "SessionDirect: Failed to mmap entry " << file_name
                    << ", offset=" << offset << ", length=" << length;
        success = false;
        break;
      }

      // Strong guarantee that page will be locked if mlock returns successfully.
      if (need_mlock && mlock(entry_mapping.address, length) < 0) {
        PLOG(ERROR) << "SessionDirect: Failed to mlock entry " << file_name
                    << ", offset=" << offset << ", length=" << length;
        // We already have a mapping address, so we should add it to the list.
        // However this didn't succeed 100% because the lock failed, so return false later.
        success = false;
      }
    }
  }

  // Keep track of success so we know in Dump() what the number of failed entry mappings were.
  entry_mapping.success = success;

  // Keep track of this so that we can clean it up later in UnreadAhead.
  list.push_back(entry_mapping);

  if (entry_mapping.success) {
    if (kLogVerboseReadAhead) {
      LOG(VERBOSE) << "SessionDirect: ReadAhead for " << entry_mapping.entry;
    }
  }  // else one of the errors above already did print.

  return success;
}

bool SessionDirect::UnreadAhead(size_t path_id,
                                ReadAheadKind kind,
                                size_t length,
                                size_t offset) {
  Entry entry{path_id, kind, length, offset};

  auto list = entry_list_map_[path_id];
  if (list.empty()) {
    return false;
  }

  std::optional<EntryMapping> entry_mapping;
  size_t idx = 0;

  for (size_t i = 0; i < list.size(); ++i) {
    if (entry == list[i].entry) {
      entry_mapping = list[i];
      idx = 0;
      break;
    }
  }

  if (!entry_mapping) {
    return false;
  }

  switch (kind) {
    case ReadAheadKind::kFadvise:
      // Nothing to do.
      // TODO: maybe fadvise(RANDOM)?
      return true;
    case ReadAheadKind::kMmapLocked:
    FALLTHROUGH_INTENDED;
    case ReadAheadKind::kMlock:
      UnmapWithoutErase(*entry_mapping);
      return true;
  }

  list.erase(list.begin() + idx);

  // FDs close only with UnregisterFilePath.
  return true;
}

void SessionDirect::UnmapWithoutErase(const EntryMapping& entry_mapping) {
  void* address = entry_mapping.address;
  size_t length = entry_mapping.entry.length;

  // munmap also unlocks. Do not need explicit munlock.
  if (munmap(address, length) < 0) {
    PLOG(WARNING) << "ReadAhead (Finish): Failed to munmap address: "
                  << address << ", length: " << length;
  }

}

bool SessionDirect::ProcessFd(int fd) {
  // TODO: the path is advisory, but it would still be cleaner to pass it separately
  const char* fd_path = SessionDescription().c_str();

  android::base::Timer open_timer{};
  android::base::Timer total_timer{};

  serialize::ArenaPtr<serialize::proto::TraceFile> trace_file_ptr =
      serialize::ProtobufIO::Open(fd, fd_path);

  if (trace_file_ptr == nullptr) {
    LOG(ERROR) << "SessionDirect::ProcessFd failed, corrupted protobuf format? " << fd_path;
    return false;
  }

  // TODO: maybe make it part of a kProcessFd type of command?
  ReadAheadKind kind = ReadAheadKind::kFadvise;

  // TODO: The "Task[Id]" should probably be the one owning the trace file.
  // When the task is fully complete, the task can be deleted and the
  // associated arenas can go with them.

  // TODO: we should probably have the file entries all be relative
  // to the package path?

  // Open every file in the trace index.
  const serialize::proto::TraceFileIndex& index = trace_file_ptr->index();

  size_t count_entries = 0;
  for (const serialize::proto::TraceFileIndexEntry& index_entry : index.entries()) {
    LOG(VERBOSE) << "ReadAhead: found file entry: " << index_entry.file_name();

    if (index_entry.id() < 0) {
      LOG(WARNING) << "ReadAhead: Skip bad TraceFileIndexEntry, negative ID not allowed: "
                   << index_entry.id();
      continue;
    }

    size_t path_id = index_entry.id();
    const auto& path_file_name = index_entry.file_name();

    if (!this->RegisterFilePath(path_id, path_file_name)) {
      LOG(WARNING) << "ReadAhead: Failed to register file path: " << path_file_name;
      ++count_entries;
    }
  }
  LOG(VERBOSE) << "ReadAhead: Registered " << count_entries << " file paths";
  std::chrono::milliseconds open_duration_ms = open_timer.duration();

  LOG(DEBUG) << "ProcessFd: open+parsed headers in " << open_duration_ms.count() << "ms";

  // Go through every trace entry and readahead every (file,offset,len) tuple.
  size_t entry_offset = 0;
  const serialize::proto::TraceFileList& file_list = trace_file_ptr->list();
  for (const serialize::proto::TraceFileEntry& file_entry : file_list.entries()) {
    ++entry_offset;

    if (file_entry.file_length() < 0 || file_entry.file_offset() < 0) {
      LOG(WARNING) << "ProcessFd entry negative file length or offset, illegal: "
                   << "index_id=" << file_entry.index_id() << ", skipping";
      continue;
    }

    // Attempt to perform readahead. This can generate more warnings dynamically.
    if (!this->ReadAhead(file_entry.index_id(),
                         kind,
                         file_entry.file_length(),
                         file_entry.file_offset())) {
      if (kLogFailures) {
        LOG(WARNING) << "Failed readahead, bad file length/offset in entry @ "
                     << (entry_offset - 1);
      }
    }
  }

  std::chrono::milliseconds total_duration_ms = total_timer.duration();
  LOG(DEBUG) << "ProcessFd: total duration " << total_duration_ms.count() << "ms";

  {
    struct timeval now;
    gettimeofday(&now, nullptr);

    uint64_t now_usec = (now.tv_sec * 1000000LL + now.tv_usec);
    LOG(DEBUG) << "ProcessFd: finishing usec: " << now_usec;
  }

  return true;
}


static bool IsDumpEveryEntry() {
  // Set to 'true' to dump every single entry for debugging (multiline).
  // Otherwise it only prints per-file-path summaries.
  return ::android::base::GetBoolProperty("iorapd.readahead.dump_all", /*default*/false);
}

static bool IsDumpEveryPath() {
  // Dump per-file-path (entry) stats. Defaults to on if the above property is on.
  return ::android::base::GetBoolProperty("iorapd.readahead.dump_paths", /*default*/false);
}

void SessionDirect::Dump(std::ostream& os, bool multiline) const {
  {
    struct timeval now;
    gettimeofday(&now, nullptr);

    uint64_t now_usec = (now.tv_sec * 1000000LL + now.tv_usec);
    LOG(DEBUG) << "SessionDirect::Dump: beginning usec: " << now_usec;
  }

  size_t path_count = entry_list_map_.size();

  size_t read_ahead_entries = 0;
  size_t read_ahead_bytes = 0;

  size_t overall_entry_count = 0;
  size_t overall_byte_count = 0;
  for (auto it = entry_list_map_.begin(); it != entry_list_map_.end(); ++it) {
    const auto& entry_mapping_list = it->second;

    for (size_t j = 0; j < entry_mapping_list.size(); ++j) {
      const EntryMapping& entry_mapping = entry_mapping_list[j];
      const Entry& entry = entry_mapping.entry;

      ++overall_entry_count;
      overall_byte_count += entry.length;

      if (entry_mapping.success) {
        ++read_ahead_entries;
        read_ahead_bytes += entry.length;
      }
    }
  }

  double overall_success_entry_rate =
      read_ahead_entries * 100.0 / overall_entry_count;
  double overall_success_byte_rate =
      read_ahead_bytes * 100.0 / overall_byte_count;

  size_t fd_count = path_fd_map_.size();
  size_t good_fd_count = 0;
  for (auto it = path_fd_map_.begin(); it != path_fd_map_.end(); ++it) {
    if (it->second.ok()) {
      ++good_fd_count;
    }
  }
  double good_fd_rate = good_fd_count * 100.0 / fd_count;
  // double bad_fd_rate = (fd_count - good_fd_count) * 1.0 / fd_count;

  if (!multiline) {
    os << "SessionDirect{";
    os << "session_id=" << SessionId() << ",";

    os << "file_paths=" << path_count << " (good: " << good_fd_rate << "),";
    os << "read_ahead_entries=" << read_ahead_entries;
    os << "(" << overall_success_entry_rate << "%),";
    os << "read_ahead_bytes=" << read_ahead_bytes << "";
    os << "(" << overall_success_byte_rate << "%),";
    os << "timer=" << timer_.duration().count() << ",";

    os << "}";
    return;
  } else {
    // Always try to pay attention to these stats below.
    // They can be signs of potential performance problems.
    os << "Session Direct (id=" << SessionId() << ")" << std::endl;

    os << "  Summary: " << std::endl;
    os << "    Description = " << SessionDescription() << std::endl;
    os << "    Duration = " << timer_.duration().count() << "ms" << std::endl;
    os << "    Total File Paths=" << path_count << " (good: " << good_fd_rate << "%)" << std::endl;
    os << "    Total Entries=" << overall_entry_count;
    os << " (good: " << overall_success_entry_rate << "%)" << std::endl;
    os << "    Total Bytes=" << overall_byte_count << "";
    os << " (good: " << overall_success_byte_rate << "%)" << std::endl;
    os << std::endl;

    // Probably too spammy, but they could narrow down the issue for a problem in above stats.
    if (!IsDumpEveryPath() && !IsDumpEveryEntry()) {
      return;
    }

    for (auto it = entry_list_map_.begin(); it != entry_list_map_.end(); ++it) {
      size_t path_id = it->first;
      const auto& entry_mapping_list = it->second;

      std::optional<std::string_view> file_path = GetFilePath(path_id);
      os << "  File Path (id=" << path_id << "): ";
      if (file_path.has_value()) {
        os << "'" << *file_path << "'";
      } else {
        os << "(nullopt)";
      }

      auto fd_it = path_fd_map_.find(path_id);
      os << ", FD=";
      if (fd_it != path_fd_map_.end()) {
        const android::base::unique_fd& fd = fd_it->second;
        os << fd.get();  // -1 for failed fd.
      } else {
        os << "(none)";
      }
      os << std::endl;

      size_t total_entries = entry_mapping_list.size();
      size_t total_bytes = 0;

      size_t local_read_ahead_entries = 0;
      size_t local_read_ahead_bytes = 0;
      for (size_t j = 0; j < entry_mapping_list.size(); ++j) {
        const EntryMapping& entry_mapping = entry_mapping_list[j];
        const Entry& entry = entry_mapping.entry;

        total_bytes += entry.length;

        // Sidenote: Bad FDs will have 100% failed mappings.
        // Good FDs may sometimes have failed mappings.
        if (entry_mapping.success) {
          ++local_read_ahead_entries;
          local_read_ahead_bytes += entry.length;
        }

        if (IsDumpEveryEntry()) {
          os << "    Entry " << j << " details:" << std::endl;
          os << "      " << entry << std::endl;
          os << "      Mapping " << (entry_mapping.success ? "Succeeded" :  "Failed")
             << ", Address " << entry_mapping.address << std::endl;
        }
      }

      double entry_success_rate = local_read_ahead_entries * 100.0 / total_entries;
      double bytes_success_rate = local_read_ahead_bytes * 100.0 / total_bytes;

      double entry_failure_rate = (total_entries - local_read_ahead_entries) * 100.0 / total_entries;
      double bytes_failure_rate = (total_bytes - local_read_ahead_bytes) * 100.0 / total_bytes;

      os << "    Successful: Entries=" << local_read_ahead_entries
         << " (" << entry_success_rate << "%)"
         << ", Bytes=" << local_read_ahead_bytes
         << " (" << bytes_success_rate << "%)"
         << std::endl;
      os << "    Failed: Entries=" << (total_entries - local_read_ahead_entries)
         << " (" << entry_failure_rate << "%)"
         << ", Bytes=" << (total_bytes - local_read_ahead_bytes)
         << " (" << bytes_failure_rate << "%)"
         << std::endl;
      os << "    Total: Entries=" << total_entries
         << ", Bytes=" << total_bytes
         << std::endl;
    }

    return;
  }
}

SessionDirect::~SessionDirect() {
  for (auto it = entry_list_map_.begin(); it != entry_list_map_.end();) {
    size_t path_id = it->first;

    ++it; // the iterator is removed in the following Unregister method.
    UnregisterFilePath(path_id);
  }
}

//
// Indirect
//

SessionIndirect::SessionIndirect(size_t session_id,
                                 std::string description,
                                 std::shared_ptr<PrefetcherDaemon> daemon,
                                 bool send_command)
    : SessionBase{session_id, description},
      daemon_{daemon} {
  // Don't do anything in e.g. subclasses.
  if (!send_command) {
    return;
  }

  Command cmd{};
  cmd.choice = CommandChoice::kCreateSession;
  cmd.session_id = session_id;
  cmd.file_path = description;

  LOG(VERBOSE) << "SessionIndirect: " << cmd;

  if (!daemon_->SendCommand(cmd)) {
    LOG(FATAL) << "SessionIndirect: Failure to create session " << session_id
               << ", description: " << description;
  }
}

SessionIndirect::~SessionIndirect() {
  Command cmd{};
  cmd.choice = CommandChoice::kDestroySession;
  cmd.session_id = SessionId();

  if (!daemon_->SendCommand(cmd)) {
    LOG(WARNING) << "SessionIndirect: Failure to destroy session " << SessionId()
                 << ", description: " << SessionDescription();
  }
}

void SessionIndirect::Dump(std::ostream& os, bool multiline) const {
  // SessionBase::Dump(os, multiline);
  // TODO: does having the local dump do anything for us?

  Command cmd{};
  cmd.choice = CommandChoice::kDumpSession;
  cmd.session_id = SessionId();

  daemon_->SendCommand(cmd);
}

bool SessionIndirect::RegisterFilePath(size_t path_id, std::string_view file_path) {
  Command cmd{};
  cmd.choice = CommandChoice::kRegisterFilePath;
  cmd.session_id = SessionId();
  cmd.id = path_id;
  cmd.file_path = file_path;

  return daemon_->SendCommand(cmd);
}

bool SessionIndirect::UnregisterFilePath(size_t path_id) {
  Command cmd{};
  cmd.choice = CommandChoice::kUnregisterFilePath;
  cmd.session_id = SessionId();
  cmd.id = path_id;

  return daemon_->SendCommand(cmd);
}
bool SessionIndirect::ReadAhead(size_t path_id,
                                ReadAheadKind kind,
                                size_t length,
                                size_t offset) {
  Command cmd{};
  cmd.choice = CommandChoice::kReadAhead;
  cmd.session_id = SessionId();
  cmd.id = path_id;
  cmd.read_ahead_kind = kind;
  cmd.length = length;
  cmd.offset = offset;

  return daemon_->SendCommand(cmd);
}

bool SessionIndirect::UnreadAhead(size_t path_id,
                                  ReadAheadKind kind,
                                  size_t length,
                                  size_t offset) {
  LOG(WARNING) << "UnreadAhead: command not implemented yet";
  return true;
}

//
// IndirectSocket
//

SessionIndirectSocket::SessionIndirectSocket(size_t session_id,
                                             int fd,
                                             std::string description,
                                             std::shared_ptr<PrefetcherDaemon> daemon)
    : SessionIndirect{session_id, description, daemon, /*send_command*/false} {
  // TODO: all of the WriteCommand etc in the daemon.
  Command cmd{};
  cmd.choice = CommandChoice::kCreateFdSession;
  cmd.fd = fd;
  cmd.session_id = session_id;
  cmd.file_path = description;

  LOG(VERBOSE) << "SessionIndirectSocket: " << cmd;

  if (!daemon_->SendCommand(cmd)) {
    LOG(FATAL) << "SessionIndirectSocket: Failure to create session " << session_id
               << ", description: " << description;
  }

  // This goes into the SessionDirect ctor + SessionDirect::ProcessFd
  // as implemented in PrefetcherDaemon::ReceiveCommand
}

SessionIndirectSocket::~SessionIndirectSocket() {
}

}  // namespace prefetcher
}  // namespace iorap
