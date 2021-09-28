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
#include <audio_utils/spdif/SPDIFEncoder.h>
#include <audio_utils/format.h>
#include <system/audio-base.h>

namespace android {

  class MySPDIFEncoder : public SPDIFEncoder {
  public:
  explicit MySPDIFEncoder(audio_format_t format)
    :  SPDIFEncoder(format) {
  }

  ssize_t writeOutput(const void* buffer, size_t size) override {
    if (buffer == nullptr) {
      return 0;
    }
    return size;
  }
  };

const static audio_format_t dts_formats[] = {AUDIO_FORMAT_DTS, AUDIO_FORMAT_DTS_HD};
const size_t formats_len = sizeof(dts_formats)/sizeof(audio_format_t);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (data == nullptr) {
    return 0;
  }
  audio_format_t encoding = dts_formats[size % formats_len];
  MySPDIFEncoder scanner(encoding);
  scanner.isFormatSupported(encoding);
  scanner.write(data, size); // parsing will be triggered based on sync keywords found in dict
  scanner.reset();
  return 0;
}
}
