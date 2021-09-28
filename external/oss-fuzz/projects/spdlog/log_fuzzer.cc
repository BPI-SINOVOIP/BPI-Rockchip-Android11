// Copyright 2019 Google LLC
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

#include <cstddef>

#include <fuzzer/FuzzedDataProvider.h>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static std::shared_ptr<spdlog::logger> my_logger;
  if (!my_logger.get()) {
    my_logger = spdlog::basic_logger_mt("basic_logger", "/dev/null");
    spdlog::set_default_logger(my_logger);
  }

  if (size == 0) {
    return 0;
  }

  FuzzedDataProvider stream(data, size);

  const size_t size_arg = stream.ConsumeIntegral<size_t>();
  const int int_arg = stream.ConsumeIntegral<int>();
  const std::string string_arg = stream.ConsumeRandomLengthString(size);
  const std::string format_string = stream.ConsumeRemainingBytesAsString();
  spdlog::info(format_string.c_str(), size_arg, int_arg, string_arg);

  return 0;
}
