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
#include "compiler/compiler.h"
#include "inode2filename/inode_resolver.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <gtest/gtest.h>

namespace iorap::compiler {

static std::string GetTestDataPath(const std::string& fn) {
  static std::string exec_dir = android::base::GetExecutableDirectory();
  return exec_dir + "/tests/src/compiler/testdata/" + fn;
}

class CompilerTest: public ::testing::Test{
 protected:
  void SetUp() override {
    ir_dependencies.data_source = inode2filename::DataSourceKind::kTextCache;
    ir_dependencies.text_cache_filename = GetTestDataPath("common_textcache");
    ir_dependencies.verify = inode2filename::VerifyKind::kNone;  // required for determinism.
    ir_dependencies.root_directories.push_back("/system");
    ir_dependencies.root_directories.push_back("/apex");
    ir_dependencies.root_directories.push_back("/data");
    ir_dependencies.root_directories.push_back("/vendor");
    ir_dependencies.root_directories.push_back("/product");
    ir_dependencies.root_directories.push_back("/metadata");
  }

  inode2filename::InodeResolverDependencies ir_dependencies;
};

TEST_F(CompilerTest, SingleTraceDuration) {
  std::vector<std::string> input_file_names{GetTestDataPath("common_perfetto_trace.pb")};
  std::vector<uint64_t> timestamp_limit_ns{260390390018596UL};
  TemporaryFile tmp_file;
  char* output_file_name = tmp_file.path;
  bool output_proto = false;


  std::vector<CompilationInput> perfetto_traces =
      MakeCompilationInputs(input_file_names, timestamp_limit_ns);
  bool result = PerformCompilation(perfetto_traces,
                                   output_file_name,
                                   output_proto,
                                   /*blacklist_filter*/std::nullopt,
                                   ir_dependencies);
  std::ifstream ifs(output_file_name);

  // The extra paren is needed to avoid compilation error:
  // "parentheses were disambiguated as a function declaration".
  std::string content{std::istreambuf_iterator<char>(ifs),
    std::istreambuf_iterator<char>()};

  EXPECT_EQ(result, true);
  EXPECT_EQ(content, "{filename:\"/product/app/CalculatorGooglePrebuilt/"
            "CalculatorGooglePrebuilt.apk\","
            "timestamp:7641303,"
            "add_to_page_cache:1,"
            "index:540}\n");
}

TEST_F(CompilerTest, MultiTraceDuration) {
  std::vector<std::string> input_file_names{GetTestDataPath("common_perfetto_trace.pb"),
    GetTestDataPath("common_perfetto_trace2.pb")};
  std::vector<uint64_t> timestamp_limit_ns{260390390018596UL, 333215840452006UL};
  TemporaryFile tmp_file;
  char* output_file_name = tmp_file.path;
  bool output_proto = false;

  std::vector<CompilationInput> perfetto_traces =
      MakeCompilationInputs(input_file_names, timestamp_limit_ns);
  bool result = PerformCompilation(perfetto_traces,
                                   output_file_name,
                                   output_proto,
                                   /*blacklist_filter*/std::nullopt,
                                   ir_dependencies);
  std::ifstream ifs(output_file_name);
  std::string content{std::istreambuf_iterator<char>(ifs),
    std::istreambuf_iterator<char>()};

  EXPECT_EQ(result, true);
  EXPECT_EQ(content, "{filename:\"/apex/com.android.art/lib64/libperfetto_hprof.so\","
            "timestamp:4388958,"
            "add_to_page_cache:1,"
            "index:227}\n"
            "{filename:\"/product/app/CalculatorGooglePrebuilt/"
            "CalculatorGooglePrebuilt.apk\","
            "timestamp:7641303,"
            "add_to_page_cache:1,"
            "index:540}\n");
}

TEST_F(CompilerTest, NoTraceDuration) {
  std::vector<std::string> input_file_names{GetTestDataPath("common_perfetto_trace.pb")};
  TemporaryFile tmp_file;
  char* output_file_name = tmp_file.path;
  bool output_proto = false;

  std::vector<CompilationInput> perfetto_traces =
      MakeCompilationInputs(input_file_names, /* timestamp_limit_ns= */{});
  bool result = PerformCompilation(perfetto_traces,
                                   output_file_name,
                                   output_proto,
                                   /*blacklist_filter*/std::nullopt,
                                   ir_dependencies);
  std::ifstream ifs(output_file_name);
  size_t line_num = std::count((std::istreambuf_iterator<char>(ifs)),
                               (std::istreambuf_iterator<char>()),
                               '\n');

  EXPECT_EQ(result, true);
  EXPECT_EQ(line_num, 1675UL);
}

TEST_F(CompilerTest, BlacklistFilterArtFiles) {
  std::vector<std::string> input_file_names{GetTestDataPath("common_perfetto_trace.pb")};
  TemporaryFile tmp_file;
  char* output_file_name = tmp_file.path;
  bool output_proto = false;

  std::string blacklist_filter = "[.](art|oat|odex|vdex|dex)$";

  // iorap.cmd.compiler -op output.pb -it common_textcache -ot
  //                    --blacklist-filter "[.](art|oat|odex|vdex|dex)$" common_perfetto_trace.pb

  std::vector<CompilationInput> perfetto_traces =
      MakeCompilationInputs(input_file_names, /* timestamp_limit_ns= */{});
  bool result = PerformCompilation(perfetto_traces,
                                   output_file_name,
                                   output_proto,
                                     blacklist_filter,
                                   ir_dependencies);
  std::ifstream ifs(output_file_name);
  size_t line_num = std::count((std::istreambuf_iterator<char>(ifs)),
                               (std::istreambuf_iterator<char>()),
                               '\n');

  EXPECT_EQ(result, true);
  EXPECT_EQ(line_num, 1617UL);
}
}  // namespace iorap::compiler
