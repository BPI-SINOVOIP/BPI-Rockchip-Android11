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

#include "session_manager.h"

#include "prefetcher/prefetcher_daemon.h"
#include "prefetcher/session.h"
#include "prefetcher/task_id.h"
#include "serialize/arena_ptr.h"
#include "serialize/protobuf_io.h"

#include <android-base/logging.h>
#include <android-base/chrono_utils.h>
#include <android-base/unique_fd.h>
#include <fcntl.h>
#include <functional>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unordered_map>

namespace iorap {
namespace prefetcher {

std::ostream& operator<<(std::ostream& os, const SessionManager& manager) {
  manager.Dump(os, /*multiline*/false);
  return os;
}

SessionManager::SessionManager() {
}

class SessionManagerBase  : public SessionManager {
 public:
  virtual void Dump(std::ostream& os, bool multiline) const {
    if (!multiline) {
      os << "SessionManager{";

      os << "sessions=[";
      for (auto it = sessions_map_.begin();
           it != sessions_map_.end();
           ++it) {
        os << "(" << it->second.description << ") ";
        it->second.session->Dump(os, /*multiline*/false);
      }
      os << "]";
      return;
    }

    os << "SessionManager (session count = " << sessions_map_.size() << "):" << std::endl;
    os << std::endl;

    for (auto it = sessions_map_.begin();
         it != sessions_map_.end();
         ++it) {
      os << "Description: " << it->second.description << std::endl;
      it->second.session->Dump(os, /*multiline*/true);
    }

    // TODO: indentations? Use this pseudo line break for the time being.
    os << "--------------------------------" << std::endl;
  }

  virtual ~SessionManagerBase() {}

  virtual std::shared_ptr<Session> FindSession(size_t session_id) const override {
    auto it = sessions_map_.find(session_id);
    if (it != sessions_map_.end()) {
      DCHECK_EQ(session_id, it->second.SessionId());
      return it->second.session;
    } else {
      return nullptr;
    }
  }

  virtual bool DestroySession(size_t session_id) override {
    auto it = sessions_map_.find(session_id);
    if (it != sessions_map_.end()) {
      sessions_map_.erase(it);
      return true;
    } else {
      return false;
    }
  }

 protected:
  void InsertNewSession(std::shared_ptr<Session> session, std::string description) {
    DCHECK(!FindSession(session->SessionId())) << "session cannot already exist";

    size_t session_id = session->SessionId();

    SessionData data;
    data.session = std::move(session);
    data.description = std::move(description);

    sessions_map_.insert({session_id, std::move(data)});
  }

 private:
  struct SessionData {
    std::shared_ptr<Session> session;
    std::string description;

    size_t SessionId() const {
      return session->SessionId();
    }
  };

  std::unordered_map</*session_id*/size_t, SessionData> sessions_map_;
};

class SessionManagerDirect : public SessionManagerBase {
 public:
  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description) override {
    LOG(VERBOSE) << "CreateSessionDirect id=" << session_id << ", description=" << description;

    std::shared_ptr<Session> session =
      std::static_pointer_cast<Session>(std::make_shared<SessionDirect>(session_id,
                                                                        description));
    DCHECK(FindSession(session_id) == nullptr);
    InsertNewSession(session, std::move(description));
    return session;
  }

  SessionManagerDirect() {
    // Intentionally left empty.
  }

 private:
};


class SessionManagerIndirect : public SessionManagerBase {
 public:
  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description) override {
    LOG(VERBOSE) << "CreateSessionIndirect id=" << session_id << ", description=" << description;

    std::shared_ptr<Session> session =
        std::static_pointer_cast<Session>(std::make_shared<SessionIndirect>(session_id,
                                                                            description,
                                                                            daemon_));
    InsertNewSession(session, description);
    return session;
  }

  SessionManagerIndirect() : daemon_{std::make_shared<PrefetcherDaemon>()} {
    //StartViaFork etc.
    // TODO: also expose a 'MainLoop(...) -> daemon::Main(..)' somehow in the base interface.
    auto params = daemon_->StartPipesViaFork();
    if (!params) {
      LOG(FATAL) << "Failed to fork+exec iorap.prefetcherd";
    }
  }

  virtual ~SessionManagerIndirect() {
    Command cmd{};
    cmd.choice = CommandChoice::kExit;

    if (!daemon_->SendCommand(cmd)) {
      LOG(FATAL) << "Failed to nicely exit iorap.prefetcherd";
    }
  }

  virtual void Dump(std::ostream& os, bool multiline) const override {
    Command cmd{};
    cmd.choice = CommandChoice::kDumpEverything;

    if (!daemon_->SendCommand(cmd)) {
      LOG(ERROR) << "Failed to transmit kDumpEverything to iorap.prefetcherd";
    }
  }


 private:
  // No lifetime cycle: PrefetcherDaemon only has a SessionManagerDirect in it.
  std::shared_ptr<PrefetcherDaemon> daemon_;
};

class SessionManagerIndirectSocket : public SessionManagerBase {
 public:
  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description) override {
    DCHECK(false) << "not supposed to create a regular session for Socket";

    LOG(VERBOSE) << "CreateSessionIndirect id=" << session_id << ", description=" << description;

    std::shared_ptr<Session> session =
        std::static_pointer_cast<Session>(std::make_shared<SessionIndirect>(session_id,
                                                                            description,
                                                                            daemon_));
    InsertNewSession(session, description);
    return session;
  }

  virtual std::shared_ptr<Session> CreateSession(size_t session_id,
                                                 std::string description,
                                                 std::optional<int> fd) override {
    CHECK(fd.has_value());
    LOG(VERBOSE) << "CreateSessionIndirectSocket id=" << session_id
                 << ", description=" << description
                 << ", fd=" << *fd;

    std::shared_ptr<Session> session =
        std::static_pointer_cast<Session>(std::make_shared<SessionIndirectSocket>(session_id,
                                                                                  *fd,
                                                                                  description,
                                                                                  daemon_));
    InsertNewSession(session, description);
    return session;
  }

  SessionManagerIndirectSocket() : daemon_{std::make_shared<PrefetcherDaemon>()} {
    auto params = daemon_->StartSocketViaFork();
    if (!params) {
      LOG(FATAL) << "Failed to fork+exec iorap.prefetcherd";
    }
  }

  virtual ~SessionManagerIndirectSocket() {
    Command cmd{};
    cmd.choice = CommandChoice::kExit;

    if (!daemon_->SendCommand(cmd)) {
      LOG(FATAL) << "Failed to nicely exit iorap.prefetcherd";
    }
  }

  virtual void Dump(std::ostream& os, bool multiline) const override {
    Command cmd{};
    cmd.choice = CommandChoice::kDumpEverything;

    if (!daemon_->SendCommand(cmd)) {
      LOG(ERROR) << "Failed to transmit kDumpEverything to iorap.prefetcherd";
    }
  }


 private:
  // No lifetime cycle: PrefetcherDaemon only has a SessionManagerDirect in it.
  std::shared_ptr<PrefetcherDaemon> daemon_;
};

std::unique_ptr<SessionManager> SessionManager::CreateManager(SessionKind kind) {
  LOG(VERBOSE) << "SessionManager::CreateManager kind=" << kind;

  switch (kind) {
    case SessionKind::kInProcessDirect: {
      SessionManager* ptr = new SessionManagerDirect();
      return std::unique_ptr<SessionManager>{ptr};
    }
    case SessionKind::kOutOfProcessIpc: {
      SessionManager* ptr = new SessionManagerIndirect();
      return std::unique_ptr<SessionManager>{ptr};
    }
    case SessionKind::kOutOfProcessSocket: {
      SessionManager* ptr = new SessionManagerIndirectSocket();
      return std::unique_ptr<SessionManager>{ptr};
    }
    default: {
      LOG(FATAL) << "Invalid session kind: " << static_cast<int>(kind);
      break;
    }
  }
}

}  // namespace prefetcher
}  // namespace iorap
