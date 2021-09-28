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

#ifndef PREFETCHER_SESSION_H_
#define PREFETCHER_SESSION_H_

#include <android-base/chrono_utils.h>
#include <android-base/unique_fd.h>

#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace iorap {
namespace prefetcher {

#ifndef READ_AHEAD_KIND
#define READ_AHEAD_KIND 1
enum class ReadAheadKind : uint32_t {
  kFadvise = 0,
  kMmapLocked = 1,
  kMlock = 2,
};
#endif

class Session {
 public:
  virtual bool RegisterFilePath(size_t path_id, std::string_view file_path) = 0;
  virtual bool UnregisterFilePath(size_t path_id) = 0;

  // Immediately perform a readahead now.
  // Fadvise: the readahead will have been queued by the kernel.
  // MmapLocked/Mlock: the memory is pinned by the requested process.
  virtual bool ReadAhead(size_t path_id, ReadAheadKind kind, size_t length, size_t offset) = 0;

  // Cancels a readahead previously done.
  // The length/offset should match the call of ReadAhead.
  virtual bool UnreadAhead(size_t path_id, ReadAheadKind kind, size_t length, size_t offset) = 0;

  // Multi-line detailed dump, e.g. for dumpsys.
  // Single-line summary dump, e.g. for logcat.
  virtual void Dump(std::ostream& os, bool multiline) const = 0;

  // Process the FD for kCreateFdSession.
  // Assumes there's a compiled_trace.pb at the fd, calling this function
  // will immediately process it and execute any read-aheads.
  //
  // FD is borrowed only for the duration of the function call.
  virtual bool ProcessFd(int fd) = 0;

  // Get the session ID associated with this session.
  // Session IDs are distinct, they are not used for new sessions.
  virtual size_t SessionId() const = 0;

  // Get this session's description.
  // Only useful for logging/dumping.
  virtual const std::string& SessionDescription() const = 0;

  // Implicitly unregister any remaining file paths.
  // All read-aheads are also cancelled.
  virtual ~Session() {}

 protected:
  Session();
};

// Single-line summary dump of Session.
std::ostream& operator<<(std::ostream&os, const Session& session);

class SessionBase : public Session {
 public:
  virtual void Dump(std::ostream& os, bool multiline) const override;
  virtual ~SessionBase() {}

  virtual size_t SessionId() const override {
    return session_id_;
  }

  virtual const std::string& SessionDescription() const override {
    return description_;
  }

  virtual bool ProcessFd(int fd) override;

 protected:
  SessionBase(size_t session_id, std::string description);
  std::optional<std::string_view> GetFilePath(size_t path_id) const;
  bool RemoveFilePath(size_t path_id);
  bool InsertFilePath(size_t path_id, std::string file_path);

  android::base::Timer timer_{};
 private:
  // Note: Store filename for easier debugging and for dumping.
  std::unordered_map</*path_id*/size_t, std::string> path_map_;
  size_t session_id_;
  std::string description_;
};

// In-process session.
class SessionDirect : public SessionBase {
 public:
  virtual bool RegisterFilePath(size_t path_id, std::string_view file_path) override;

  virtual bool UnregisterFilePath(size_t path_id) override;
  virtual bool ReadAhead(size_t path_id,
                         ReadAheadKind kind,
                         size_t length,
                         size_t offset);

  virtual bool UnreadAhead(size_t path_id,
                           ReadAheadKind kind,
                           size_t length,
                           size_t offset) override;

  virtual bool ProcessFd(int fd) override;

  virtual void Dump(std::ostream& os, bool multiline) const override;

  virtual ~SessionDirect();

  SessionDirect(size_t session_id, std::string description)
    : SessionBase{session_id, std::move(description)} {
  }
 protected:
  struct Entry {
    size_t path_id;
    ReadAheadKind kind;
    size_t length;
    size_t offset;

    constexpr bool operator==(const Entry& other) const {
      return path_id == other.path_id &&
        kind == other.kind &&
        length == other.length &&
        offset == other.offset;
    }
    constexpr bool operator!=(const Entry& other) const {
      return !(*this == other);
    }

    friend std::ostream& operator<<(std::ostream& os, const Entry& entry);
  };

  struct EntryMapping {
    Entry entry;
    void* address;
    bool success;
  };

  // bool EntryReadAhead(const Entry& entry) const;
  // bool EntryUnReadAhead(const Entry& entry) const;

  void UnmapWithoutErase(const EntryMapping& entry_mapping);
  std::optional<android::base::unique_fd*> GetFdForPath(size_t path_id);

 private:
  std::unordered_map</*path_id*/size_t, std::vector<EntryMapping>> entry_list_map_;
  std::unordered_map</*path_id*/size_t, android::base::unique_fd> path_fd_map_;

 public:
  friend std::ostream& operator<<(std::ostream& os, const SessionDirect::Entry& entry);
};

std::ostream& operator<<(std::ostream& os, const SessionDirect::Entry& entry);

class PrefetcherDaemon;

// Out-of-process session. Requires prefetcher daemon.
class SessionIndirect : public SessionBase {
 public:
  virtual bool RegisterFilePath(size_t path_id, std::string_view file_path) override;

  virtual bool UnregisterFilePath(size_t path_id) override;
  virtual bool ReadAhead(size_t path_id,
                         ReadAheadKind kind,
                         size_t length,
                         size_t offset) override;

  virtual bool UnreadAhead(size_t path_id,
                           ReadAheadKind kind,
                           size_t length,
                           size_t offset) override;

  virtual void Dump(std::ostream& os, bool multiline) const override;

  // Creates a new session indirectly.
  // Writes to daemon the new session command.
  SessionIndirect(size_t session_id,
                  std::string description,
                  std::shared_ptr<PrefetcherDaemon> daemon,
                  bool send_command = true);

  // Destroys the current session.
  // Writes to daemon that the session is to be destroyed.
  virtual ~SessionIndirect();

 protected:
  std::shared_ptr<PrefetcherDaemon> daemon_;
};

// Out-of-process session. Requires prefetcher daemon.
class SessionIndirectSocket : public SessionIndirect {
 public:
  // Creates a new session indirectly.
  // Writes to daemon the new session command.
  SessionIndirectSocket(size_t session_id,
                        int fd,
                        std::string description,
                        std::shared_ptr<PrefetcherDaemon> daemon);
  // Destroys the current session.
  // Writes to daemon that the session is to be destroyed.
  virtual ~SessionIndirectSocket();

 private:
};


}  // namespace prefetcher
}  // namespace iorap

#endif

