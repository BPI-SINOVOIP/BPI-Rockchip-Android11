// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BSDIFF_PATCH_WRITER_H_
#define _BSDIFF_PATCH_WRITER_H_

#include <memory>
#include <string>
#include <vector>

#include "bsdiff/compressor_interface.h"
#include "bsdiff/patch_writer_interface.h"

namespace bsdiff {

// A PatchWriterInterface class with three compressors and a 32-byte header.
class BsdiffPatchWriter : public PatchWriterInterface {
 public:
  // Create the patch writer using the upstream's "BSDIFF40" format. It uses
  // bz2 as the compression algorithm and the file |patch_filename| to write
  // the patch data.
  explicit BsdiffPatchWriter(const std::string& patch_filename);

  // Create the patch writer using the "BSDF2" format. It uses the compressor
  // with algorithm |type|; and quality |brotli_quality| if it's brotli. This
  // writer also writes the patch data to the file |patch_filename|.
  BsdiffPatchWriter(const std::string& patch_filename,
                    const std::vector<CompressorType>& types,
                    int brotli_quality);

  // PatchWriterInterface overrides.
  bool Init(size_t new_size) override;
  bool WriteDiffStream(const uint8_t* data, size_t size) override;
  bool WriteExtraStream(const uint8_t* data, size_t size) override;
  bool AddControlEntry(const ControlEntry& entry) override;
  bool Close() override;

 private:
  // Add supported compressors to |compressor_list|; return false if we failed
  // to initialize one of them.
  bool InitializeCompressorList(
      std::vector<std::unique_ptr<CompressorInterface>>* compressor_list);

  // Select the compressor in |compressor_list| that produces the smallest
  // patch, and put the result in |smallest_compressor|.
  bool SelectSmallestResult(
      const std::vector<std::unique_ptr<CompressorInterface>>& compressor_list,
      CompressorInterface** smallest_compressor);


  // Write the BSDIFF patch header to the |fp_|.
  // Arguments:
  //   A three bytes array with the compressor types of ctrl|diff|extra stream
  //   Size of the compressed control block
  //   Size of the compressed diff block.
  bool WriteHeader(uint8_t types[3], uint64_t ctrl_size, uint64_t diff_size);

  // Bytes of the new files already written. Needed to store the new length in
  // the header of the file.
  uint64_t written_output_{0};

  // The current file we are writing to.
  FILE* fp_{nullptr};
  std::string patch_filename_;

  // The format of bsdiff we're using.
  BsdiffFormat format_;

  // The compressors we're using.
  std::vector<CompressorType> types_;

  // The compression quality of the brotli compressor.
  int brotli_quality_;

  // The list of compressors to try for each stream.
  std::vector<std::unique_ptr<CompressorInterface>> ctrl_stream_list_;
  std::vector<std::unique_ptr<CompressorInterface>> diff_stream_list_;
  std::vector<std::unique_ptr<CompressorInterface>> extra_stream_list_;
};

}  // namespace bsdiff

#endif  // _BSDIFF_PATCH_WRITER_H_
