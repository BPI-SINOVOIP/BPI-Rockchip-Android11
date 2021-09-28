/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// NOTE: this is a Android version of the file with the same name in parent folder.
// The main difference is the removal of absl, re2 and tensorflow core dependencies.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <gtest/gtest.h>
#include "parse_testdata.h"
#include "tflite_driver.h"
#include "util.h"

namespace tflite {
namespace testing {

namespace {
bool FLAGS_use_nnapi = true;
}  // namespace

// Get a list of tests from the manifest file.
std::vector<string> FindAllTests() {
  const string test_dir = "/data/local/tmp";

  std::vector<string> test_paths;
  // Read the newline delimited list of entries in the manifest.
  std::ifstream manifest_fp(test_dir + "/test_manifest.txt");
  string manifest((std::istreambuf_iterator<char>(manifest_fp)),
                   std::istreambuf_iterator<char>());
  size_t pos = 0;
  int added = 0;
  while (true) {
    size_t end_pos = manifest.find("\n", pos);
    if (end_pos == string::npos) break;
    // ignore disabled tests.
    if (manifest.substr(pos, 8) != "DISABLED") {
      string filename = manifest.substr(pos, end_pos - pos);
      test_paths.push_back(test_dir + "/models/" + filename);
    }
    pos = end_pos + 1;
    added += 1;
  }
  return test_paths;
}

class OpsTest : public ::testing::TestWithParam<string> {};

TEST_P(OpsTest, RunZipTests) {
  string test_path = GetParam();
  string tflite_test_case = test_path + "_tests.txt";
  string tflite_dir = test_path.substr(0, test_path.find_last_of("/"));
  string test_name = test_path.substr(test_path.find_last_of('/'));

  std::ifstream tflite_stream(tflite_test_case);
  ASSERT_TRUE(tflite_stream.is_open()) << tflite_test_case;
  tflite::testing::TfLiteDriver test_driver(FLAGS_use_nnapi);
  test_driver.SetModelBaseDir(tflite_dir);

  bool result = tflite::testing::ParseAndRunTests(&tflite_stream, &test_driver);
  string message = test_driver.GetErrorMessage();
  EXPECT_TRUE(result) << message;
}

struct ZipPathParamName {
  template <class ParamType>
  string operator()(const ::testing::TestParamInfo<ParamType>& info) const {
    string param_name = info.param;
    size_t last_slash = param_name.find_last_of("\\/");
    if (last_slash != string::npos) {
      param_name = param_name.substr(last_slash);
    }
    for (size_t index = 0; index < param_name.size(); ++index) {
      if (!isalnum(param_name[index]) && param_name[index] != '_')
        param_name[index] = '_';
    }
    return param_name;
  }
};

INSTANTIATE_TEST_CASE_P(tests, OpsTest, ::testing::ValuesIn(FindAllTests()),
                        ZipPathParamName());

}  // namespace testing
}  // namespace tflite

int main(int argc, char** argv) {
  ::tflite::LogToStderr();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
