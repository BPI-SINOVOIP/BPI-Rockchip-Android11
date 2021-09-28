/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Functions to compress and decompress low entropy entries in the model.

#ifndef LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_H_
#define LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_H_

#include <vector>

#include "utils/base/integral_types.h"
#include "utils/zlib/buffer_generated.h"
#include <zlib.h>

namespace libtextclassifier3 {

class ZlibDecompressor {
 public:
  static std::unique_ptr<ZlibDecompressor> Instance(
      const unsigned char* dictionary = nullptr,
      unsigned int dictionary_size = 0);
  ~ZlibDecompressor();

  bool Decompress(const uint8* buffer, const int buffer_size,
                  const int uncompressed_size, std::string* out);
  bool MaybeDecompress(const CompressedBuffer* compressed_buffer,
                       std::string* out);
  bool MaybeDecompress(const CompressedBufferT* compressed_buffer,
                       std::string* out);
  bool MaybeDecompressOptionallyCompressedBuffer(
      const flatbuffers::String* uncompressed_buffer,
      const CompressedBuffer* compressed_buffer, std::string* out);
  bool MaybeDecompressOptionallyCompressedBuffer(
      const flatbuffers::Vector<uint8>* uncompressed_buffer,
      const CompressedBuffer* compressed_buffer, std::string* out);

 private:
  ZlibDecompressor(const unsigned char* dictionary,
                   const unsigned int dictionary_size);
  z_stream stream_;
  bool initialized_;
};

class ZlibCompressor {
 public:
  static std::unique_ptr<ZlibCompressor> Instance(
      const unsigned char* dictionary = nullptr,
      unsigned int dictionary_size = 0);
  ~ZlibCompressor();

  void Compress(const std::string& uncompressed_content,
                CompressedBufferT* out);

  bool GetDictionary(std::vector<unsigned char>* dictionary);

 private:
  explicit ZlibCompressor(const unsigned char* dictionary = nullptr,
                          const unsigned int dictionary_size = 0,
                          const int level = Z_BEST_COMPRESSION,
                          // Tmp. buffer size was set based on the current set
                          // of patterns to be compressed.
                          const int tmp_buffer_size = 64 * 1024);
  z_stream stream_;
  std::unique_ptr<Bytef[]> buffer_;
  unsigned int buffer_size_;
  bool initialized_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_H_
