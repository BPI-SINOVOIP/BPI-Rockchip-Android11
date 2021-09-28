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

#include "common/debug.h"
#include "maintenance/controller.h"

#include "db/file_models.h"
#include "db/models.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::Return;
using ::testing::_;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;

namespace iorap::maintenance {

static std::string GetTestDataPath(const std::string& fn) {
  static std::string exec_dir = android::base::GetExecutableDirectory();
  return exec_dir + "/tests/src/maintenance/testdata/" + fn;
}

class MockExec : public IExec {
 public:
  MOCK_METHOD(int,
              Execve,
              (const std::string& pathname, std::vector<std::string>& argv_vec, char *const envp[]),
              (override));

  MOCK_METHOD(pid_t, Fork, (), (override));
};

class ControllerTest: public ::testing::Test {
 protected:
  void SetUp() override {
    // The db is a fake db with the following tables:
    //
    // packages:
    // id, name, version
    // 1, com.android.settings, 1
    // 2, com.yawanng, 1
    //
    // activities:
    // id, name
    // 1, Setting
    // 2, yawanng
    //
    // app_launch_histories:
    // id, activity_id, temperature, trace_enabled, readahead_enabled, intent_start_ns, total_time_ns, report_fully_drawn_ns
    // 1, 1, 1, 1, 1, 1, 2, NULL
    // 2, 1, 1, 1, 1, NULL, 4, 5
    // 3, 1, 1, 1, 1, 3, NULL, NULL
    // 4, 1, 1, 1, 1, 3, 7, 8
    // 5, 1, 1, 0, 1, 4, 9, 10
    // 6, 1, 2, 1, 1, 5, 11, 12
    // 7, 2, 1, 1, 1, 6, 21, 22
    // 8, 2, 1, 1, 1, 7, 22, 23
    //
    // raw_traces:
    // id, history_id, file_path
    // 1, 1, 1.txt
    // 2, 3, 3.txt
    // 3, 4, 4.txt
    // 4, 5, 5.txt
    // 5, 6, 6.txt
    // 6, 7, 7.txt
    // 7, 8, 8.txt
    db_path = GetTestDataPath("test_sqlite.db");
  }

  std::string db_path;
  TemporaryDir root_path;
};

MATCHER_P(AreArgsExpected, compiled_trace_path, "") {
  std::vector<std::string> expect =
    { "1.txt",
      "3.txt",
      "4.txt",
      "--timestamp_limit_ns", "2",
      "--timestamp_limit_ns", "18446744073709551615",
      "--timestamp_limit_ns", "8",
      "--output-text",
      "--output-proto", compiled_trace_path,
      "--verbose" };
  return arg == expect;
}

TEST_F(ControllerTest, CompilationController) {
  auto mock_exec = std::make_shared<MockExec>();
  iorap::db::SchemaModel db_schema = db::SchemaModel::GetOrCreate(db_path);
  db::DbHandle db{db_schema.db()};

  setenv("IORAPD_ROOT_DIR", root_path.path ,1);
  std::string compiled_trace_path = std::string(root_path.path) +
      "/com.android.settings/1/Setting/compiled_traces/compiled_trace.pb";

  // No recompile
  ControllerParameters params{
    /*output_text=*/true,
    /*inode_textcache=*/std::nullopt,
    /*verbose=*/true,
    /*recompile=*/false,
    /*min_traces=*/3,
    mock_exec};

  ON_CALL(*mock_exec, Fork())
      .WillByDefault(Return(-2));

  EXPECT_CALL(*mock_exec,
              Execve("/system/bin/iorap.cmd.compiler",
                     AreArgsExpected(compiled_trace_path),
                     nullptr))
      .Times(2);

  CompileAppsOnDevice(db, params);

  // Recompile
  ControllerParameters params2{
    /*output_text=*/true,
    /*inode_textcache=*/std::nullopt,
    /*verbose=*/true,
    /*recompile=*/true,
    /*min_traces=*/3,
    mock_exec};

  // Create a fake compiled trace file to test recompile.
  std::ofstream tmp_file;
  tmp_file.open(compiled_trace_path);
  tmp_file.close();

  CompileAppsOnDevice(db, params2);
}

}  // namespace iorap::maintenance
