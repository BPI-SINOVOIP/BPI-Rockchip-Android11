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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <audio_utils/format.h>
#include <system/audio-base.h>

/** returns true if the format is a common source or destination format.
    memcpy_by_audio_format() allows interchange between any PCM format and the
    "common" PCM 16 bit and PCM float formats. */
static bool is_common_src_format(audio_format_t format) {
    return format == AUDIO_FORMAT_PCM_16_BIT || format == AUDIO_FORMAT_PCM_FLOAT;
}

static bool is_common_dst_format(audio_format_t format) {
    return format == AUDIO_FORMAT_PCM_8_BIT       // Allowed for HAL to AudioRecord conversion.
            || format == AUDIO_FORMAT_PCM_16_BIT
            || format == AUDIO_FORMAT_PCM_FLOAT;
}

const static audio_format_t formats[] = {AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_FLOAT,
AUDIO_FORMAT_PCM_8_BIT, AUDIO_FORMAT_PCM_24_BIT_PACKED, AUDIO_FORMAT_PCM_32_BIT,
AUDIO_FORMAT_PCM_8_24_BIT};

// Initialize PCM 16 bit ramp for basic data sanity check (generated from PCM 8 bit data).
template<size_t size>
static void fillBuffer(const uint8_t bytes[], int16_t(&buffer)[size], size_t input_size)
{
    if (size < input_size) {
       input_size  = size;
    }

    // convert to PCM 16 bit
    memcpy_by_audio_format(
            buffer, AUDIO_FORMAT_PCM_16_BIT,
            bytes, AUDIO_FORMAT_PCM_8_BIT, input_size);

    uint8_t check[size];
    memcpy_by_audio_format(
            check, AUDIO_FORMAT_PCM_8_BIT,
            buffer, AUDIO_FORMAT_PCM_16_BIT, input_size);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t size) {
  if (size < 4 || size > UINT8_MAX) {
     return 0;
  }
  size_t formats_len = sizeof(formats)/sizeof(audio_format_t);
  int src = size % formats_len;
  int dst = formats_len - 1 - src;

  // fetch parameters
  const audio_format_t src_encoding = formats[src];
  const audio_format_t dst_encoding = formats[dst];

  // either source or destination (or both) need to be a common format
  if (!is_common_src_format(src_encoding) && !is_common_dst_format(dst_encoding)) {
      return 0;
  }

  constexpr size_t SAMPLES = UINT8_MAX;
  constexpr audio_format_t orig_encoding = AUDIO_FORMAT_PCM_16_BIT;
  int16_t data[SAMPLES];
  fillBuffer(bytes, data, size);

  // data buffer for in-place conversion (uint32_t is maximum sample size of 4 bytes)
  uint32_t databuff[size];
  // check buffer is used to compare out-of-place vs in-place conversion.
  uint32_t check[size];

  // Copy original data to data buffer at src_encoding.
  memcpy_by_audio_format(
          databuff, src_encoding,
          data, orig_encoding, size);

  // Convert from src encoding to dst encoding.
  memcpy_by_audio_format(
          check, dst_encoding,
          databuff, src_encoding, size);

  // Check in-place is same as out-of-place conversion.
  memcpy_by_audio_format(
          databuff, dst_encoding,
          databuff, src_encoding, size);

  // Go back to the original data encoding for comparison.
  memcpy_by_audio_format(
          databuff, orig_encoding,
          databuff, dst_encoding, size);

  return 0;
}
