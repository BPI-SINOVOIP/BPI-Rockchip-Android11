// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bsdiff/brotli_decompressor.h"

#include "bsdiff/logging.h"

namespace bsdiff {

BrotliDecompressor::~BrotliDecompressor() {
  if (brotli_decoder_state_)
    BrotliDecoderDestroyInstance(brotli_decoder_state_);
}

bool BrotliDecompressor::SetInputData(const uint8_t* input_data, size_t size) {
  brotli_decoder_state_ =
      BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
  if (brotli_decoder_state_ == nullptr) {
    LOG(ERROR) << "Failed to initialize brotli decoder.";
    return false;
  }
  next_in_ = input_data;
  available_in_ = size;
  return true;
}

bool BrotliDecompressor::Read(uint8_t* output_data, size_t bytes_to_output) {
  if (!brotli_decoder_state_) {
    LOG(ERROR) << "BrotliDecompressor not initialized";
    return false;
  }
  auto next_out = output_data;
  size_t available_out = bytes_to_output;

  while (available_out > 0) {
    // The brotli decoder will update |available_in_|, |available_in_|,
    // |next_out| and |available_out|.
    BrotliDecoderResult result = BrotliDecoderDecompressStream(
        brotli_decoder_state_, &available_in_, &next_in_, &available_out,
        &next_out, nullptr);

    if (result == BROTLI_DECODER_RESULT_ERROR) {
      LOG(ERROR) << "Decompression failed with "
                 << BrotliDecoderErrorString(
                        BrotliDecoderGetErrorCode(brotli_decoder_state_));
      return false;
    } else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      LOG(ERROR) << "Decompressor reached EOF while reading from input stream.";
      return false;
    } else if (result == BROTLI_DECODER_RESULT_SUCCESS) {
      // This means that decoding is finished, no more input might be consumed
      // and no more output will be produced. In the normal case, when there is
      // more data available than what was requested in this Read() call it
      // returns BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT.
      if (available_out > 0) {
        LOG(ERROR) << "Expected to read " << available_out
                   << " more bytes but reached the end of compressed brotli "
                      "stream";
        return false;
      }
      return true;
    }
  }
  return true;
}

bool BrotliDecompressor::Close() {
  if (!brotli_decoder_state_) {
    LOG(ERROR) << "BrotliDecompressor not initialized";
    return false;
  }
  // In some cases, the brotli compressed stream could be empty. As a result,
  // the function BrotliDecoderIsFinished() will return false because we never
  // start the decompression. When that happens, we just destroy the decoder
  // and return true.
  if (BrotliDecoderIsUsed(brotli_decoder_state_) &&
      !BrotliDecoderIsFinished(brotli_decoder_state_)) {
    LOG(ERROR) << "Unfinished brotli decoder.";
    return false;
  }

  BrotliDecoderDestroyInstance(brotli_decoder_state_);
  brotli_decoder_state_ = nullptr;
  return true;
}

}  // namespace bsdiff
