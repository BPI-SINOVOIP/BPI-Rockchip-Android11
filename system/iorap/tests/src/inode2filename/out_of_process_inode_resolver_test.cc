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

#include "inode2filename/out_of_process_inode_resolver.h"

#include <cstdio>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using ::testing::ElementsAre;

namespace iorap::inode2filename {

void WriteInt(int val, std::FILE* f) {
  char buf[4];
  memcpy(buf, &val, 4);
  fwrite(buf, 1, 4, f);
}

TEST(OutOfProcessInodeResolverTest, ReadOneline) {
  std::FILE* tmpf = std::tmpfile();

  WriteInt(16, tmpf);
  std::fputs("K 253:9:6 ./test", tmpf);
  WriteInt(22, tmpf);
  std::fputs("K 253:9:7 ./test\ntest\n", tmpf);
  WriteInt(21, tmpf);
  std::fputs("E 253:9:7 ./test\ntest", tmpf);
  WriteInt(15, tmpf);
  std::fputs("K 253:9:8 ./tmp", tmpf);

  std::rewind(tmpf);
  bool file_eof = false;
  std::vector<std::string> result;

  while (!file_eof) {
    std::string line = ReadOneLine(tmpf, /*out*/&file_eof);
    if (!line.empty()) {
      result.push_back(line);
    }
  }

  ASSERT_THAT(result, ElementsAre("K 253:9:6 ./test",
                                  "K 253:9:7 ./test\ntest\n",
                                  "E 253:9:7 ./test\ntest",
                                  "K 253:9:8 ./tmp" ));
}

} // namespace iorap::inode2filename
