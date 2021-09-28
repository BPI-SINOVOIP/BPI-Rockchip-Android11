// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <vector>

#include "bsdiff/bspatch.h"
#include "bsdiff/memory_file.h"
#include "bsdiff/sink_file.h"

namespace {

void FuzzBspatch(const uint8_t* data, size_t size) {
  const size_t kBufferSize = 1024;
  std::vector<uint8_t> source_buffer(kBufferSize);
  std::unique_ptr<bsdiff::FileInterface> source(
      new bsdiff::MemoryFile(source_buffer.data(), source_buffer.size()));
  std::unique_ptr<bsdiff::FileInterface> target(new bsdiff::SinkFile(
      [](const uint8_t* data, size_t size) { return size; }));
  bsdiff::bspatch(source, target, data, size);
}

struct Environment {
  Environment() {
    // To turn off logging for bsdiff library.
    std::cerr.setstate(std::ios_base::failbit);
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzBspatch(data, size);
  return 0;
}
