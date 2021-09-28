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

#include "aidl.h"
#include "fake_io_delegate.h"
#include "options.h"

#include <iostream>

#ifdef FUZZ_LOG
constexpr bool kFuzzLog = true;
#else
constexpr bool kFuzzLog = false;
#endif

using android::aidl::test::FakeIoDelegate;

void fuzz(const std::string& langOpt, const std::string& content) {
  // TODO: fuzz multiple files
  // TODO: fuzz arguments
  FakeIoDelegate io;
  io.SetFileContents("a/path/Foo.aidl", content);

  std::vector<std::string> args;
  args.emplace_back("aidl");
  args.emplace_back("--lang=" + langOpt);
  args.emplace_back("-b");
  args.emplace_back("-I .");
  args.emplace_back("-o out");
  // corresponding items also in aidl_parser_fuzzer.dict
  args.emplace_back("a/path/Foo.aidl");

  if (kFuzzLog) {
    std::cout << "lang: " << langOpt << " content: " << content << std::endl;
  }

  int ret = android::aidl::compile_aidl(Options::From(args), io);
  if (ret != 0) return;

  if (kFuzzLog) {
    for (const std::string& f : io.ListOutputFiles()) {
      std::string output;
      if (io.GetWrittenContents(f, &output)) {
        std::cout << "OUTPUT " << f << ": " << std::endl;
        std::cout << output << std::endl;
      }
    }
  }
}

void fuzz(uint8_t options, const std::string& content) {
  // keeping a byte of options we can use for various flags in the future (do
  // not remove or add unless absolutely necessary in order to preserve the
  // corpus).
  (void)options;

  // Process for each backend.
  //
  // This is unfortunate because we are parsing multiple times, but we want to
  // check generation of content for each backend. If output fails in one
  // backend, it's likely to fail in another.
  fuzz("ndk", content);
  fuzz("cpp", content);
  fuzz("java", content);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size <= 1) return 0;  // no use

  // b/145447540, large nested expressions sometimes hit the stack depth limit.
  // Fuzzing things of this size don't provide any additional meaningful
  // coverage. This is an approximate value which should allow us to explore all
  // of the language w/o hitting a stack overflow.
  if (size > 2000) return 0;

  uint8_t options = *data;
  data++;
  size--;

  std::string content(reinterpret_cast<const char*>(data), size);
  fuzz(options, content);

  return 0;
}
