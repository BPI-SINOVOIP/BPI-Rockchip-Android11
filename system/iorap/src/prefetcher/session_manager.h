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

#ifndef PREFETCHER_SESSION_MANAGER_H_
#define PREFETCHER_SESSION_MANAGER_H_

#include <optional>
#include <ostream>
#include <memory>

namespace iorap {
namespace prefetcher {

class Session;

enum class SessionKind : uint32_t {
  kInProcessDirect,
  kOutOfProcessIpc,
  kOutOfProcessSocket,
};

inline std::ostream& operator<<(std::ostream& os, SessionKind kind) {
  if (kind == SessionKind::kInProcessDirect) {
    os << "kInProcessDirect";
  } else if (kind == SessionKind::kOutOfProcessIpc) {
    os << "kOutOfProcessIpc";
  } else if (kind == SessionKind::kOutOfProcessSocket) {
    os << "kOutOfProcessSocket";
  } else {
    os << "(invalid)";
  }
  return os;
}

class SessionManager {
 public:
  static std::unique_ptr<SessionManager> CreateManager(SessionKind kind);

  // Create a new session. The description is used by Dump.
  // Manager maintains a strong ref to this session, so DestroySession must also
  // be called prior to all refs dropping to 0.
  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description) = 0;

  // Create a new session. The description is used by Dump.
  // Manager maintains a strong ref to this session, so DestroySession must also
  // be called prior to all refs dropping to 0.
  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description,
                                                 std::optional<int> fd) {
    return CreateSession(session_id, description);
  }

  // Look up an existing session that was already created.
  // Returns null if there is no such session.
  virtual std::shared_ptr<Session> FindSession(size_t session_id) const = 0;

  // Drop all manager references to an existing session.
  // Returns false if the session does not exist already.
  virtual bool DestroySession(size_t session_id) = 0;

  // Multi-line detailed dump, e.g. for dumpsys.
  // Single-line summary dump, e.g. for logcat.
  virtual void Dump(std::ostream& os, bool multiline) const = 0;

  // Note: session lifetime is tied to manager. The manager has strong pointers to sessions.
  virtual ~SessionManager() {}

 protected:
  SessionManager();
};

// Single-line summary dump of Session.
std::ostream& operator<<(std::ostream&os, const SessionManager& session);

}  // namespace prefetcher
}  // namespace iorap

#endif

