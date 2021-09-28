/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <filesystem>
#include <fstream>
#include <string>

#include <errno.h>

#include <android-base/file.h>
#include <android-base/result.h>
#include <android-base/scopeguard.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <gtest/gtest.h>

#include "apexd_session.h"
#include "apexd_test_utils.h"
#include "apexd_utils.h"
#include "session_state.pb.h"

namespace android {
namespace apex {
namespace {

using android::apex::testing::IsOk;
using android::base::Join;
using android::base::make_scope_guard;

// TODO(b/170329726): add unit tests for apexd_sessions.h

TEST(ApexdSessionTest, GetSessionsDirSessionsStoredInMetadata) {
  if (access("/metadata", F_OK) != 0) {
    GTEST_SKIP() << "Device doesn't have /metadata partition";
  }

  std::string result = ApexSession::GetSessionsDir();
  ASSERT_EQ(result, "/metadata/apex/sessions");
}

TEST(ApexdSessionTest, GetSessionsDirNoMetadataPartitionFallbackToData) {
  if (access("/metadata", F_OK) == 0) {
    GTEST_SKIP() << "Device has /metadata partition";
  }

  std::string result = ApexSession::GetSessionsDir();
  ASSERT_EQ(result, "/data/apex/sessions");
}

TEST(ApexdSessionTest, MigrateToMetadataSessionsDir) {
  namespace fs = std::filesystem;

  if (access("/metadata", F_OK) != 0) {
    GTEST_SKIP() << "Device doesn't have /metadata partition";
  }

  // This is ugly, but does the job. To have a truly hermetic unit tests we
  // need to refactor ApexSession class.
  for (const auto& entry : fs::directory_iterator("/metadata/apex/sessions")) {
    fs::remove_all(entry.path());
  }

  // This is a very ugly test set up, but to have something better we need to
  // refactor ApexSession class.
  class TestApexSession {
   public:
    TestApexSession(int id, const SessionState::State& state) {
      path_ = "/data/apex/sessions/" + std::to_string(id);
      if (auto status = createDirIfNeeded(path_, 0700); !status.ok()) {
        ADD_FAILURE() << "Failed to create " << path_ << " : "
                      << status.error();
      }
      SessionState session;
      session.set_id(id);
      session.set_state(state);
      std::fstream state_file(
          path_ + "/state", std::ios::out | std::ios::trunc | std::ios::binary);
      if (!session.SerializeToOstream(&state_file)) {
        ADD_FAILURE() << "Failed to write to " << path_;
      }
    }

    ~TestApexSession() { fs::remove_all(path_); }

   private:
    std::string path_;
  };

  auto deleter = make_scope_guard([&]() {
    fs::remove_all("/metadata/apex/sessions/239");
    fs::remove_all("/metadata/apex/sessions/1543");
  });

  TestApexSession session1(239, SessionState::SUCCESS);
  TestApexSession session2(1543, SessionState::ACTIVATION_FAILED);

  ASSERT_TRUE(IsOk(ApexSession::MigrateToMetadataSessionsDir()));

  auto sessions = ApexSession::GetSessions();
  ASSERT_EQ(2u, sessions.size()) << Join(sessions, ',');

  auto migrated_session_1 = ApexSession::GetSession(239);
  ASSERT_TRUE(IsOk(migrated_session_1));
  ASSERT_EQ(SessionState::SUCCESS, migrated_session_1->GetState());

  auto migrated_session_2 = ApexSession::GetSession(1543);
  ASSERT_TRUE(IsOk(migrated_session_2));
  ASSERT_EQ(SessionState::ACTIVATION_FAILED, migrated_session_2->GetState());
}

}  // namespace
}  // namespace apex
}  // namespace android
