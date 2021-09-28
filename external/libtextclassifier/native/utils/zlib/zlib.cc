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

#include "utils/zlib/zlib.h"

#include "utils/flatbuffers.h"

namespace libtextclassifier3 {

std::unique_ptr<ZlibDecompressor> ZlibDecompressor::Instance(
    const unsigned char* dictionary, const unsigned int dictionary_size) {
  std::unique_ptr<ZlibDecompressor> result(
      new ZlibDecompressor(dictionary, dictionary_size));
  if (!result->initialized_) {
    result.reset();
  }
  return result;
}

ZlibDecompressor::ZlibDecompressor(const unsigned char* dictionary,
                                   const unsigned int dictionary_size) {
  memset(&stream_, 0, sizeof(stream_));
  stream_.zalloc = Z_NULL;
  stream_.zfree = Z_NULL;
  initialized_ = false;
  if (inflateInit(&stream_) != Z_OK) {
    TC3_LOG(ERROR) << "Could not initialize decompressor.";
    return;
  }
  if (dictionary != nullptr &&
      inflateSetDictionary(&stream_, dictionary, dictionary_size) != Z_OK) {
    TC3_LOG(ERROR) << "Could not set dictionary.";
    return;
  }
  initialized_ = true;
}

ZlibDecompressor::~ZlibDecompressor() {
  if (initialized_) {
    inflateEnd(&stream_);
  }
}

bool ZlibDecompressor::Decompress(const uint8* buffer, const int buffer_size,
                                  const int uncompressed_size,
                                  std::string* out) {
  if (out == nullptr) {
    return false;
  }
  out->resize(uncompressed_size);
  stream_.next_in = reinterpret_cast<const Bytef*>(buffer);
  stream_.avail_in = buffer_size;
  stream_.next_out = reinterpret_cast<Bytef*>(const_cast<char*>(out->c_str()));
  stream_.avail_out = uncompressed_size;
  return (inflate(&stream_, Z_SYNC_FLUSH) == Z_OK);
}

bool ZlibDecompressor::MaybeDecompress(
    const CompressedBuffer* compressed_buffer, std::string* out) {
  if (!compressed_buffer) {
    return true;
  }
  return Decompress(compressed_buffer->buffer()->Data(),
                    compressed_buffer->buffer()->size(),
                    compressed_buffer->uncompressed_size(), out);
}

bool ZlibDecompressor::MaybeDecompress(
    const CompressedBufferT* compressed_buffer, std::string* out) {
  if (!compressed_buffer) {
    return true;
  }
  return Decompress(compressed_buffer->buffer.data(),
                    compressed_buffer->buffer.size(),
                    compressed_buffer->uncompressed_size, out);
}

bool ZlibDecompressor::MaybeDecompressOptionallyCompressedBuffer(
    const flatbuffers::String* uncompressed_buffer,
    const CompressedBuffer* compressed_buffer, std::string* out) {
  if (uncompressed_buffer != nullptr) {
    *out = uncompressed_buffer->str();
    return true;
  }
  return MaybeDecompress(compressed_buffer, out);
}

bool ZlibDecompressor::MaybeDecompressOptionallyCompressedBuffer(
    const flatbuffers::Vector<uint8>* uncompressed_buffer,
    const CompressedBuffer* compressed_buffer, std::string* out) {
  if (uncompressed_buffer != nullptr) {
    *out =
        std::string(reinterpret_cast<const char*>(uncompressed_buffer->data()),
                    uncompressed_buffer->size());
    return true;
  }
  return MaybeDecompress(compressed_buffer, out);
}

std::unique_ptr<ZlibCompressor> ZlibCompressor::Instance(
    const unsigned char* dictionary, const unsigned int dictionary_size) {
  std::unique_ptr<ZlibCompressor> result(
      new ZlibCompressor(dictionary, dictionary_size));
  if (!result->initialized_) {
    result.reset();
  }
  return result;
}

ZlibCompressor::ZlibCompressor(const unsigned char* dictionary,
                               const unsigned int dictionary_size,
                               const int level, const int tmp_buffer_size) {
  memset(&stream_, 0, sizeof(stream_));
  stream_.zalloc = Z_NULL;
  stream_.zfree = Z_NULL;
  buffer_size_ = tmp_buffer_size;
  buffer_.reset(new Bytef[buffer_size_]);
  initialized_ = false;
  if (deflateInit(&stream_, level) != Z_OK) {
    TC3_LOG(ERROR) << "Could not initialize compressor.";
    return;
  }
  if (dictionary != nullptr &&
      deflateSetDictionary(&stream_, dictionary, dictionary_size) != Z_OK) {
    TC3_LOG(ERROR) << "Could not set dictionary.";
    return;
  }
  initialized_ = true;
}

ZlibCompressor::~ZlibCompressor() { deflateEnd(&stream_); }

void ZlibCompressor::Compress(const std::string& uncompressed_content,
                              CompressedBufferT* out) {
  out->uncompressed_size = uncompressed_content.size();
  out->buffer.clear();
  stream_.next_in =
      reinterpret_cast<const Bytef*>(uncompressed_content.c_str());
  stream_.avail_in = uncompressed_content.size();
  stream_.next_out = buffer_.get();
  stream_.avail_out = buffer_size_;
  unsigned char* buffer_deflate_start_position =
      reinterpret_cast<unsigned char*>(buffer_.get());
  int status;
  do {
    // Deflate chunk-wise.
    // Z_SYNC_FLUSH causes all pending output to be flushed, but doesn't
    // reset the compression state.
    // As we do not know how big the compressed buffer will be, we compress
    // chunk wise and append the flushed content to the output string buffer.
    // As we store the uncompressed size, we do not have to do this during
    // decompression.
    status = deflate(&stream_, Z_SYNC_FLUSH);
    unsigned char* buffer_deflate_end_position =
        reinterpret_cast<unsigned char*>(stream_.next_out);
    if (buffer_deflate_end_position != buffer_deflate_start_position) {
      out->buffer.insert(out->buffer.end(), buffer_deflate_start_position,
                         buffer_deflate_end_position);
      stream_.next_out = buffer_deflate_start_position;
      stream_.avail_out = buffer_size_;
    } else {
      break;
    }
  } while (status == Z_OK);
}

bool ZlibCompressor::GetDictionary(std::vector<unsigned char>* dictionary) {
  // Retrieve first the size of the dictionary.
  unsigned int size;
  if (deflateGetDictionary(&stream_, /*dictionary=*/Z_NULL, &size) != Z_OK) {
    return false;
  }
  dictionary->resize(size);
  return deflateGetDictionary(&stream_, dictionary->data(), &size) == Z_OK;
}

}  // namespace libtextclassifier3
