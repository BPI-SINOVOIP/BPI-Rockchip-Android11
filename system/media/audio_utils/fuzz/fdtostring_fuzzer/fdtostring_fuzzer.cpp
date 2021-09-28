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

#include <string>
#include <stddef.h>
#include <stdint.h>
#include <audio_utils/FdToString.h>

using namespace android::audio_utils;

extern "C" int LLVMFuzzerTestOneInput(const char *data, size_t size) {
  if (size < 4) {
     return 0;
  }

  std::string data_str (reinterpret_cast<char const*>(data), size);
  const std::string PREFIX{data_str.substr(0, 3)};
  const std::string TEST_STRING{data_str.substr(3)+"\n"};

  FdToString fdToString(PREFIX);
  const int fd = fdToString.fd();
  write(fd, TEST_STRING.c_str(), TEST_STRING.size());

  (void)fdToString.getStringAndClose();
  return 0;
}
