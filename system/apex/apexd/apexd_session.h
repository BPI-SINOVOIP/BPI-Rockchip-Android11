/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_APEXD_APEXD_SESSION_H_
#define ANDROID_APEXD_APEXD_SESSION_H_

#include <android-base/result.h>

#include "apex_constants.h"

#include "session_state.pb.h"

#include <optional>

namespace android {
namespace apex {

class ApexSession {
 public:
  // Returns top-level directory to store sessions metadata in.
  // If device has /metadata partition, this will return
  // /metadata/apex/sessions, on all other devices it will return
  // /data/apex/sessions.
  static std::string GetSessionsDir();
  // Migrates content of /data/apex/sessions to /metadata/apex/sessions.
  // If device doesn't have /metadata partition this call will be a no-op.
  // If /data/apex/sessions this call will also be a no-op.
  static android::base::Result<void> MigrateToMetadataSessionsDir();

  static android::base::Result<ApexSession> CreateSession(int session_id);
  static android::base::Result<ApexSession> GetSession(int session_id);
  static std::vector<ApexSession> GetSessions();
  static std::vector<ApexSession> GetSessionsInState(
      ::apex::proto::SessionState::State state);
  static android::base::Result<std::optional<ApexSession>> GetActiveSession();
  static std::vector<ApexSession> GetActiveSessions();
  ApexSession() = delete;

  const google::protobuf::RepeatedField<int> GetChildSessionIds() const;
  ::apex::proto::SessionState::State GetState() const;
  int GetId() const;
  std::string GetBuildFingerprint() const;
  std::string GetCrashingNativeProcess() const;
  bool IsFinalized() const;
  bool HasRollbackEnabled() const;
  bool IsRollback() const;
  int GetRollbackId() const;
  const google::protobuf::RepeatedPtrField<std::string> GetApexNames() const;

  void SetChildSessionIds(const std::vector<int>& child_session_ids);
  void SetBuildFingerprint(const std::string& fingerprint);
  void SetHasRollbackEnabled(const bool enabled);
  void SetIsRollback(const bool is_rollback);
  void SetRollbackId(const int rollback_id);
  void SetCrashingNativeProcess(const std::string& crashing_process);
  void AddApexName(const std::string& apex_name);

  android::base::Result<void> UpdateStateAndCommit(
      const ::apex::proto::SessionState::State& state);

  android::base::Result<void> DeleteSession() const;

 private:
  ApexSession(::apex::proto::SessionState state);
  ::apex::proto::SessionState state_;

  static android::base::Result<ApexSession> GetSessionFromFile(
      const std::string& path);
};

std::ostream& operator<<(std::ostream& out, const ApexSession& session);

}  // namespace apex
}  // namespace android

#endif  // ANDROID_APEXD_APEXD_SESSION_H
