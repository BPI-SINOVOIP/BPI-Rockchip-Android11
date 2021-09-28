/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/test_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Options.h"

using ::testing::ElementsAre;
using ::testing::StrEq;

namespace android {
namespace gtest_extras {

class OptionsTest : public ::testing::Test {
 protected:
  void ClearChildArgs() {
    for (auto arg : child_args_) {
      free(arg);
    }
    child_args_.clear();
  }

  void TearDown() { ClearChildArgs(); }

  void CheckIncompatible(const std::string arg);
  void CheckIncompatibleFromEnv(const std::string env_var);

  std::vector<char*> child_args_;
};

TEST_F(OptionsTest, unknown_arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--unknown_arg"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Unknown argument: --unknown_arg\n", capture.str());
}

TEST_F(OptionsTest, unknown_arg_single_dash) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "-unknown_arg"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Unknown argument: -unknown_arg\n", capture.str());
}

TEST_F(OptionsTest, extra_arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "extra"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Unexpected argument 'extra'\n", capture.str());
}

TEST_F(OptionsTest, check_defaults) {
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_LT(0U, options.job_count());
  EXPECT_EQ(90000ULL, options.deadline_threshold_ms());
  EXPECT_EQ(2000ULL, options.slow_threshold_ms());
  EXPECT_EQ(0ULL, options.shard_index());
  EXPECT_EQ(0ULL, options.total_shards());
  EXPECT_EQ("auto", options.color());
  EXPECT_EQ("", options.xml_file());
  EXPECT_EQ("", options.filter());
  EXPECT_EQ(1, options.num_iterations());
  EXPECT_TRUE(options.print_time());
  EXPECT_FALSE(options.allow_disabled_tests());
  EXPECT_FALSE(options.list_tests());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_list_tests) {
  std::vector<const char*> cur_args{"ignore", "--gtest_list_tests"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.list_tests());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_list_tests_error_argument) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_list_tests=nothing"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_list_tests does not take an argument.\n", capture.str());
}

TEST_F(OptionsTest, job_count_single_arg) {
  std::vector<const char*> cur_args{"ignore", "-j11"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(11U, options.job_count());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, job_count_second_arg) {
  std::vector<const char*> cur_args{"ignore", "-j", "23"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(23U, options.job_count());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, job_count_error_single_arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "-j0bad"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("-j value is not formatted as a numeric value (0bad)\n", capture.str());
}

TEST_F(OptionsTest, job_count_error_second_arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "-j", "34b"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("-j value is not formatted as a numeric value (34b)\n", capture.str());
}

TEST_F(OptionsTest, job_count_error_no_arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "-j"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("-j requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, deadline_threshold_ms) {
  std::vector<const char*> cur_args{"ignore", "--deadline_threshold_ms=3200"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(3200ULL, options.deadline_threshold_ms());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, deadline_threshold_ms_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--deadline_threshold_ms"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--deadline_threshold_ms requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, deadline_threshold_ms_error_not_a_number) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--deadline_threshold_ms=bad"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--deadline_threshold_ms value is not formatted as a numeric value (bad)\n",
            capture.str());
}

TEST_F(OptionsTest, deadline_threshold_ms_error_illegal_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--deadline_threshold_ms=0"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--deadline_threshold_ms requires a number greater than zero.\n", capture.str());
}

TEST_F(OptionsTest, slow_threshold_ms) {
  std::vector<const char*> cur_args{"ignore", "--slow_threshold_ms=4580"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(4580ULL, options.slow_threshold_ms());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, slow_threshold_ms_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--slow_threshold_ms"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--slow_threshold_ms requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, slow_threshold_ms_error_not_a_number) {
  CapturedStdout capture;
  Options options;
  std::vector<const char*> cur_args{"ignore", "--slow_threshold_ms=not"};
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--slow_threshold_ms value is not formatted as a numeric value (not)\n", capture.str());
}

TEST_F(OptionsTest, slow_threshold_ms_error_illegal_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--slow_threshold_ms=0"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--slow_threshold_ms requires a number greater than zero.\n", capture.str());
}

TEST_F(OptionsTest, shard_index) {
  ASSERT_NE(-1, setenv("GTEST_SHARD_INDEX", "100", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(100ULL, options.shard_index());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  ASSERT_NE(-1, setenv("GTEST_SHARD_INDEX", "0", 1));
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(0ULL, options.shard_index());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_SHARD_INDEX"));
}

TEST_F(OptionsTest, shard_index_error_no_value) {
  ASSERT_NE(-1, setenv("GTEST_SHARD_INDEX", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_SHARD_INDEX] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_SHARD_INDEX"));
}

TEST_F(OptionsTest, shard_index_error_not_a_number) {
  ASSERT_NE(-1, setenv("GTEST_SHARD_INDEX", "bad", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_SHARD_INDEX] value is not formatted as a numeric value (bad)\n",
            capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_SHARD_INDEX"));
}

TEST_F(OptionsTest, shard_index_error_not_from_env) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_shard_index=100"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_shard_index is only supported as an environment variable.\n", capture.str());
}

TEST_F(OptionsTest, total_shards) {
  ASSERT_NE(-1, setenv("GTEST_TOTAL_SHARDS", "500", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(500ULL, options.total_shards());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  ASSERT_NE(-1, setenv("GTEST_TOTAL_SHARDS", "0", 1));
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(0ULL, options.total_shards());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_TOTAL_SHARDS"));
}

TEST_F(OptionsTest, total_shards_error_no_value) {
  ASSERT_NE(-1, setenv("GTEST_TOTAL_SHARDS", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_TOTAL_SHARDS] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_TOTAL_SHARDS"));
}

TEST_F(OptionsTest, total_shards_error_not_a_number) {
  ASSERT_NE(-1, setenv("GTEST_TOTAL_SHARDS", "bad", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_TOTAL_SHARDS] value is not formatted as a numeric value (bad)\n",
            capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_TOTAL_SHARDS"));
}

TEST_F(OptionsTest, total_shards_error_not_from_env) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_total_shards=100"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_total_shards is only supported as an environment variable.\n", capture.str());
}

TEST_F(OptionsTest, gtest_color) {
  std::vector<const char*> cur_args{"ignore", "--gtest_color=yes"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("yes", options.color());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore"), StrEq("--gtest_color=yes")));
}

TEST_F(OptionsTest, gtest_color_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_color="};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_color requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_filter) {
  std::vector<const char*> cur_args{"ignore", "--gtest_filter=filter"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("filter", options.filter());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_filter_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_filter"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_filter requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_also_run_disabled_tests) {
  std::vector<const char*> cur_args{"ignore", "--gtest_also_run_disabled_tests"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.allow_disabled_tests());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore"), StrEq("--gtest_also_run_disabled_tests")));
}

TEST_F(OptionsTest, gtest_also_run_disabled_tests_error_argument) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_also_run_disabled_tests=nothing"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_also_run_disabled_tests does not take an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_repeat) {
  std::vector<const char*> cur_args{"ignore", "--gtest_repeat=10"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(10, options.num_iterations());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  cur_args = std::vector<const char*>{"ignore", "--gtest_repeat=-1"};
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(-1, options.num_iterations());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_repeat_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_repeat"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_repeat requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_repeat_error_overflow) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_repeat=2147483747"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_repeat value overflows (2147483747)\n", capture.str());

  ClearChildArgs();
  capture.Reset();
  capture.Start();
  cur_args = std::vector<const char*>{"ignore", "--gtest_repeat=-2147483747"};
  parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_repeat value overflows (-2147483747)\n", capture.str());
}

TEST_F(OptionsTest, gtest_print_time) {
  std::vector<const char*> cur_args{"ignore", "--gtest_print_time"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.print_time());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  cur_args = std::vector<const char*>{"ignore", "--gtest_print_time=0"};
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_FALSE(options.print_time());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  cur_args = std::vector<const char*>{"ignore", "--gtest_print_time=1"};
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.print_time());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_output) {
  std::vector<const char*> cur_args{"ignore", "--gtest_output=xml:/file.xml"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("/file.xml", options.xml_file());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  cur_args = std::vector<const char*>{"ignore", "--gtest_output=xml:/directory/"};
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("/directory/test_details.xml", options.xml_file());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ClearChildArgs();
  cur_args = std::vector<const char*>{"ignore", "--gtest_output=xml:cwd.xml"};
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  char* cwd = getcwd(nullptr, 0);
  std::string expected_file(cwd);
  expected_file += "/cwd.xml";
  free(cwd);
  EXPECT_EQ(expected_file, options.xml_file());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_output_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_output"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_output requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_output_error_no_xml) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_output=xml:"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_output requires a file name after xml:\n", capture.str());

  ClearChildArgs();
  capture.Reset();
  capture.Start();
  cur_args = std::vector<const char*>{"ignore", "--gtest_output=not_xml"};
  parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_output only supports an xml output file.\n", capture.str());
}

TEST_F(OptionsTest, gtest_death_test_style) {
  std::vector<const char*> cur_args{"ignore", "--gtest_death_test_style=something"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_THAT(child_args_,
              ElementsAre(StrEq("ignore"), StrEq("--gtest_death_test_style=something")));
}

TEST_F(OptionsTest, gtest_death_test_style_error_no_value) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_death_test_style"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("--gtest_death_test_style requires an argument.\n", capture.str());
}

TEST_F(OptionsTest, gtest_flagfile) {
  TemporaryFile tf;
  ASSERT_TRUE(
      android::base::WriteStringToFile("--gtest_color=no\n"
                                       "\n"
                                       "--gtest_print_time=0\n"
                                       "--gtest_repeat=10\n",
                                       tf.path));

  std::string flag("--gtest_flagfile=");
  flag += tf.path;
  std::vector<const char*> cur_args{"ignore", flag.c_str()};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("no", options.color());
  EXPECT_FALSE(options.print_time());
  EXPECT_EQ(10U, options.num_iterations());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore"), StrEq("--gtest_color=no")));
}

TEST_F(OptionsTest, gtest_flagfile_no_newline) {
  TemporaryFile tf;
  ASSERT_TRUE(android::base::WriteStringToFile("--gtest_color=no", tf.path));

  std::string flag("--gtest_flagfile=");
  flag += tf.path;
  std::vector<const char*> cur_args{"ignore", flag.c_str()};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("no", options.color());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore"), StrEq("--gtest_color=no")));
}

TEST_F(OptionsTest, gtest_flagfile_empty_file) {
  TemporaryFile tf;

  std::string flag("--gtest_flagfile=");
  flag += tf.path;
  std::vector<const char*> cur_args{"ignore", flag.c_str()};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));
}

TEST_F(OptionsTest, gtest_flagfile_disallow_j_option) {
  TemporaryFile tf;
  ASSERT_TRUE(android::base::WriteStringToFile("-j1\n", tf.path));

  CapturedStdout capture;
  std::string flag("--gtest_flagfile=");
  flag += tf.path;
  std::vector<const char*> cur_args{"ignore", flag.c_str()};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Unknown argument: -j1\n", capture.str());
}

TEST_F(OptionsTest, gtest_flagfile_disallow_gtest_flagfile_option_in_file) {
  TemporaryFile tf;
  ASSERT_TRUE(android::base::WriteStringToFile("--gtest_flagfile=nothing\n", tf.path));

  CapturedStdout capture;
  std::string flag("--gtest_flagfile=");
  flag += tf.path;
  std::vector<const char*> cur_args{"ignore", flag.c_str()};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Argument: --gtest_flagfile=nothing is not allowed in flag file.\n", capture.str());
}

TEST_F(OptionsTest, gtest_flagfile_does_not_exist) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", "--gtest_flagfile=/this/does/not/exist"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("Unable to read data from file /this/does/not/exist\n", capture.str());
}

void OptionsTest::CheckIncompatible(const std::string arg) {
  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore", arg.c_str()};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly for arg " + arg;
  EXPECT_EQ(arg + " is not compatible with isolation runs.\n", capture.str());
}

TEST_F(OptionsTest, incompatible) {
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_break_on_failure"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_catch_exceptions"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_random_seed"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_shuffle"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_stream_result_to"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatible("--gtest_throw_on_failure"));
}

TEST_F(OptionsTest, verify_non_env_variables) {
  EXPECT_NE(-1, setenv("DEADLINE_THRESHOLD_MS", "VALUE", 1));
  EXPECT_NE(-1, setenv("SLOW_THRESHOLD_MS", "VALUE", 1));
  EXPECT_NE(-1, setenv("GTEST_FORMAT", "VALUE", 1));
  EXPECT_NE(-1, setenv("GTEST_LIST_TESTS", "VALUE", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  EXPECT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_LT(0U, options.job_count());
  EXPECT_EQ(90000ULL, options.deadline_threshold_ms());
  EXPECT_EQ(2000ULL, options.slow_threshold_ms());
  EXPECT_EQ("auto", options.color());
  EXPECT_EQ("", options.xml_file());
  EXPECT_EQ("", options.filter());
  EXPECT_EQ(1, options.num_iterations());
  EXPECT_TRUE(options.print_time());
  EXPECT_FALSE(options.allow_disabled_tests());
  EXPECT_FALSE(options.list_tests());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  EXPECT_NE(-1, unsetenv("DEADLINE_THRESHOLD_MS"));
  EXPECT_NE(-1, unsetenv("SLOW_THRESHOLD_MS"));
  EXPECT_NE(-1, unsetenv("GTEST_FORMAT"));
  EXPECT_NE(-1, unsetenv("GTEST_LIST_TESTS"));
}

TEST_F(OptionsTest, gtest_filter_from_env) {
  ASSERT_NE(-1, setenv("GTEST_FILTER", "filter_value", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  EXPECT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("filter_value", options.filter());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_FILTER"));
}

TEST_F(OptionsTest, gtest_filter_error_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_FILTER", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_FILTER] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_FILTER"));
}

TEST_F(OptionsTest, gtest_also_run_disabled_tests_from_env) {
  ASSERT_NE(-1, setenv("GTEST_ALSO_RUN_DISABLED_TESTS", "", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.allow_disabled_tests());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_ALSO_RUN_DISABLED_TESTS"));
}

TEST_F(OptionsTest, gtest_also_run_disabled_tests_error_argument_from_env) {
  ASSERT_NE(-1, setenv("GTEST_ALSO_RUN_DISABLED_TESTS", "one", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_ALSO_RUN_DISABLED_TESTS] does not take an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_ALSO_RUN_DISABLED_TESTS"));
}

TEST_F(OptionsTest, gtest_repeat_from_env) {
  ASSERT_NE(-1, setenv("GTEST_REPEAT", "34", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ(34, options.num_iterations());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_REPEAT"));
}

TEST_F(OptionsTest, gtest_repeat_error_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_REPEAT", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_REPEAT] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_REPEAT"));
}

TEST_F(OptionsTest, gtest_repeat_error_overflow_from_env) {
  ASSERT_NE(-1, setenv("GTEST_REPEAT", "2147483747", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_REPEAT] value overflows (2147483747)\n", capture.str());

  ASSERT_NE(-1, setenv("GTEST_REPEAT", "-2147483747", 1));

  ClearChildArgs();
  capture.Reset();
  capture.Start();
  parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_REPEAT] value overflows (-2147483747)\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_REPEAT"));
}

TEST_F(OptionsTest, gtest_color_from_env) {
  ASSERT_NE(-1, setenv("GTEST_COLOR", "yes", 1));

  std::vector<const char*> cur_args{"ignore"};
  std::vector<const char*> child_args;
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("yes", options.color());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_COLOR"));
}

TEST_F(OptionsTest, gtest_color_error_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_COLOR", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_COLOR] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_COLOR"));
}

TEST_F(OptionsTest, gtest_print_time_from_env) {
  ASSERT_NE(-1, setenv("GTEST_PRINT_TIME", "0", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_FALSE(options.print_time());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_PRINT_TIME"));
}

TEST_F(OptionsTest, gtest_print_time_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_PRINT_TIME", "", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_TRUE(options.print_time());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_PRINT_TIME"));
}

TEST_F(OptionsTest, gtest_output_from_env) {
  ASSERT_NE(-1, setenv("GTEST_OUTPUT", "xml:/file.xml", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_EQ("/file.xml", options.xml_file());
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_PRINT_TIME"));
}

TEST_F(OptionsTest, gtest_output_error_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_OUTPUT", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_OUTPUT] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_OUTPUT"));
}

TEST_F(OptionsTest, gtest_output_error_no_xml_from_env) {
  ASSERT_NE(-1, setenv("GTEST_OUTPUT", "xml:", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_OUTPUT] requires a file name after xml:\n", capture.str());

  ASSERT_NE(-1, setenv("GTEST_OUTPUT", "not_xml", 1));

  ClearChildArgs();
  capture.Reset();
  capture.Start();
  parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_OUTPUT] only supports an xml output file.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_OUTPUT"));
}

TEST_F(OptionsTest, gtest_death_test_style_from_env) {
  ASSERT_NE(-1, setenv("GTEST_DEATH_TEST_STYLE", "fast", 1));

  std::vector<const char*> cur_args{"ignore"};
  Options options;
  ASSERT_TRUE(options.Process(cur_args, &child_args_));
  EXPECT_THAT(child_args_, ElementsAre(StrEq("ignore")));

  ASSERT_NE(-1, unsetenv("GTEST_DEATH_TEST_STYLE"));
}

TEST_F(OptionsTest, gtest_death_test_style_error_no_value_from_env) {
  ASSERT_NE(-1, setenv("GTEST_DEATH_TEST_STYLE", "", 1));

  CapturedStdout capture;
  std::vector<const char*> cur_args{"ignore"};
  Options options;
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly.";
  EXPECT_EQ("env[GTEST_DEATH_TEST_STYLE] requires an argument.\n", capture.str());

  ASSERT_NE(-1, unsetenv("GTEST_DEATH_TEST_STYLE"));
}

void OptionsTest::CheckIncompatibleFromEnv(const std::string env_var) {
  ASSERT_NE(-1, setenv(env_var.c_str(), "", 1));

  CapturedStdout capture;
  Options options;
  std::vector<const char*> cur_args{"ignore"};
  bool parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly for env var " + env_var;
  EXPECT_EQ("env[" + env_var + "] is not compatible with isolation runs.\n", capture.str());

  ASSERT_NE(-1, setenv(env_var.c_str(), "not_empty", 1));

  ClearChildArgs();
  capture.Reset();
  capture.Start();
  parsed = options.Process(cur_args, &child_args_);
  capture.Stop();
  ASSERT_FALSE(parsed) << "Process did not fail properly for env var " + env_var;
  EXPECT_EQ("env[" + env_var + "] is not compatible with isolation runs.\n", capture.str());

  ASSERT_NE(-1, unsetenv(env_var.c_str()));
}

TEST_F(OptionsTest, incompatible_from_env) {
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_BREAK_ON_FAILURE"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_CATCH_EXCEPTIONS"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_RANDOM_SEED"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_SHUFFLE"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_STREAM_RESULT_TO"));
  ASSERT_NO_FATAL_FAILURE(CheckIncompatibleFromEnv("GTEST_THROW_ON_FAILURE"));
}

}  // namespace gtest_extras
}  // namespace android
