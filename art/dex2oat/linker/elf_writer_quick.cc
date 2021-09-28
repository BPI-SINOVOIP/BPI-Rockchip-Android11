/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "elf_writer_quick.h"

#include <memory>
#include <openssl/sha.h>
#include <unordered_map>
#include <unordered_set>

#include <android-base/logging.h>

#include "base/casts.h"
#include "base/globals.h"
#include "base/leb128.h"
#include "base/utils.h"
#include "compiled_method.h"
#include "debug/elf_debug_writer.h"
#include "debug/method_debug_info.h"
#include "driver/compiler_options.h"
#include "elf/elf_builder.h"
#include "elf/elf_utils.h"
#include "stream/buffered_output_stream.h"
#include "stream/file_output_stream.h"
#include "thread-current-inl.h"
#include "thread_pool.h"

namespace art {
namespace linker {

class DebugInfoTask : public Task {
 public:
  DebugInfoTask(InstructionSet isa,
                const InstructionSetFeatures* features,
                uint64_t text_section_address,
                size_t text_section_size,
                uint64_t dex_section_address,
                size_t dex_section_size,
                const debug::DebugInfo& debug_info)
      : isa_(isa),
        instruction_set_features_(features),
        text_section_address_(text_section_address),
        text_section_size_(text_section_size),
        dex_section_address_(dex_section_address),
        dex_section_size_(dex_section_size),
        debug_info_(debug_info) {
  }

  void Run(Thread*) override {
    result_ = debug::MakeMiniDebugInfo(isa_,
                                       instruction_set_features_,
                                       text_section_address_,
                                       text_section_size_,
                                       dex_section_address_,
                                       dex_section_size_,
                                       debug_info_);
  }

  std::vector<uint8_t>* GetResult() {
    return &result_;
  }

 private:
  InstructionSet isa_;
  const InstructionSetFeatures* instruction_set_features_;
  uint64_t text_section_address_;
  size_t text_section_size_;
  uint64_t dex_section_address_;
  size_t dex_section_size_;
  const debug::DebugInfo& debug_info_;
  std::vector<uint8_t> result_;
};

template <typename ElfTypes>
class ElfWriterQuick final : public ElfWriter {
 public:
  ElfWriterQuick(const CompilerOptions& compiler_options,
                 File* elf_file);
  ~ElfWriterQuick();

  void Start() override;
  void PrepareDynamicSection(size_t rodata_size,
                             size_t text_size,
                             size_t data_bimg_rel_ro_size,
                             size_t bss_size,
                             size_t bss_methods_offset,
                             size_t bss_roots_offset,
                             size_t dex_section_size) override;
  void PrepareDebugInfo(const debug::DebugInfo& debug_info) override;
  OutputStream* StartRoData() override;
  void EndRoData(OutputStream* rodata) override;
  OutputStream* StartText() override;
  void EndText(OutputStream* text) override;
  OutputStream* StartDataBimgRelRo() override;
  void EndDataBimgRelRo(OutputStream* data_bimg_rel_ro) override;
  void WriteDynamicSection() override;
  void WriteDebugInfo(const debug::DebugInfo& debug_info) override;
  bool StripDebugInfo() override;
  bool End() override;

  OutputStream* GetStream() override;

  size_t GetLoadedSize() override;

  static void EncodeOatPatches(const std::vector<uintptr_t>& locations,
                               std::vector<uint8_t>* buffer);

 private:
  const CompilerOptions& compiler_options_;
  File* const elf_file_;
  size_t rodata_size_;
  size_t text_size_;
  size_t data_bimg_rel_ro_size_;
  size_t bss_size_;
  size_t dex_section_size_;
  std::unique_ptr<BufferedOutputStream> output_stream_;
  std::unique_ptr<ElfBuilder<ElfTypes>> builder_;
  std::unique_ptr<DebugInfoTask> debug_info_task_;
  std::unique_ptr<ThreadPool> debug_info_thread_pool_;

  void ComputeFileBuildId(uint8_t (*build_id)[ElfBuilder<ElfTypes>::kBuildIdLen]);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfWriterQuick);
};

std::unique_ptr<ElfWriter> CreateElfWriterQuick(const CompilerOptions& compiler_options,
                                                File* elf_file) {
  if (Is64BitInstructionSet(compiler_options.GetInstructionSet())) {
    return std::make_unique<ElfWriterQuick<ElfTypes64>>(compiler_options, elf_file);
  } else {
    return std::make_unique<ElfWriterQuick<ElfTypes32>>(compiler_options, elf_file);
  }
}

template <typename ElfTypes>
ElfWriterQuick<ElfTypes>::ElfWriterQuick(const CompilerOptions& compiler_options, File* elf_file)
    : ElfWriter(),
      compiler_options_(compiler_options),
      elf_file_(elf_file),
      rodata_size_(0u),
      text_size_(0u),
      data_bimg_rel_ro_size_(0u),
      bss_size_(0u),
      dex_section_size_(0u),
      output_stream_(
          std::make_unique<BufferedOutputStream>(std::make_unique<FileOutputStream>(elf_file))),
      builder_(new ElfBuilder<ElfTypes>(compiler_options_.GetInstructionSet(),
                                        output_stream_.get())) {}

template <typename ElfTypes>
ElfWriterQuick<ElfTypes>::~ElfWriterQuick() {}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::Start() {
  builder_->Start();
  if (compiler_options_.GetGenerateBuildId()) {
    builder_->GetBuildId()->AllocateVirtualMemory(builder_->GetBuildId()->GetSize());
    builder_->WriteBuildIdSection();
  }
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::PrepareDynamicSection(size_t rodata_size,
                                                     size_t text_size,
                                                     size_t data_bimg_rel_ro_size,
                                                     size_t bss_size,
                                                     size_t bss_methods_offset,
                                                     size_t bss_roots_offset,
                                                     size_t dex_section_size) {
  DCHECK_EQ(rodata_size_, 0u);
  rodata_size_ = rodata_size;
  DCHECK_EQ(text_size_, 0u);
  text_size_ = text_size;
  DCHECK_EQ(data_bimg_rel_ro_size_, 0u);
  data_bimg_rel_ro_size_ = data_bimg_rel_ro_size;
  DCHECK_EQ(bss_size_, 0u);
  bss_size_ = bss_size;
  DCHECK_EQ(dex_section_size_, 0u);
  dex_section_size_ = dex_section_size;
  builder_->PrepareDynamicSection(elf_file_->GetPath(),
                                  rodata_size_,
                                  text_size_,
                                  data_bimg_rel_ro_size_,
                                  bss_size_,
                                  bss_methods_offset,
                                  bss_roots_offset,
                                  dex_section_size);
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::StartRoData() {
  auto* rodata = builder_->GetRoData();
  rodata->Start();
  return rodata;
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::EndRoData(OutputStream* rodata) {
  CHECK_EQ(builder_->GetRoData(), rodata);
  builder_->GetRoData()->End();
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::StartText() {
  auto* text = builder_->GetText();
  text->Start();
  return text;
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::EndText(OutputStream* text) {
  CHECK_EQ(builder_->GetText(), text);
  builder_->GetText()->End();
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::StartDataBimgRelRo() {
  auto* data_bimg_rel_ro = builder_->GetDataBimgRelRo();
  data_bimg_rel_ro->Start();
  return data_bimg_rel_ro;
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::EndDataBimgRelRo(OutputStream* data_bimg_rel_ro) {
  CHECK_EQ(builder_->GetDataBimgRelRo(), data_bimg_rel_ro);
  builder_->GetDataBimgRelRo()->End();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::WriteDynamicSection() {
  builder_->WriteDynamicSection();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::PrepareDebugInfo(const debug::DebugInfo& debug_info) {
  if (compiler_options_.GetGenerateMiniDebugInfo()) {
    // Prepare the mini-debug-info in background while we do other I/O.
    Thread* self = Thread::Current();
    debug_info_task_ = std::make_unique<DebugInfoTask>(
        builder_->GetIsa(),
        compiler_options_.GetInstructionSetFeatures(),
        builder_->GetText()->GetAddress(),
        text_size_,
        builder_->GetDex()->Exists() ? builder_->GetDex()->GetAddress() : 0,
        dex_section_size_,
        debug_info);
    debug_info_thread_pool_ = std::make_unique<ThreadPool>("Mini-debug-info writer", 1);
    debug_info_thread_pool_->AddTask(self, debug_info_task_.get());
    debug_info_thread_pool_->StartWorkers(self);
  }
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::WriteDebugInfo(const debug::DebugInfo& debug_info) {
  if (compiler_options_.GetGenerateMiniDebugInfo()) {
    // Wait for the mini-debug-info generation to finish and write it to disk.
    Thread* self = Thread::Current();
    DCHECK(debug_info_thread_pool_ != nullptr);
    debug_info_thread_pool_->Wait(self, true, false);
    builder_->WriteSection(".gnu_debugdata", debug_info_task_->GetResult());
  }
  // The Strip method expects debug info to be last (mini-debug-info is not stripped).
  if (!debug_info.Empty() && compiler_options_.GetGenerateDebugInfo()) {
    // Generate all the debug information we can.
    debug::WriteDebugInfo(builder_.get(), debug_info);
  }
}

template <typename ElfTypes>
bool ElfWriterQuick<ElfTypes>::StripDebugInfo() {
  off_t file_size = builder_->Strip();
  return elf_file_->SetLength(file_size) == 0;
}

template <typename ElfTypes>
bool ElfWriterQuick<ElfTypes>::End() {
  builder_->End();
  if (compiler_options_.GetGenerateBuildId()) {
    uint8_t build_id[ElfBuilder<ElfTypes>::kBuildIdLen];
    ComputeFileBuildId(&build_id);
    builder_->WriteBuildId(build_id);
  }
  return builder_->Good();
}

template <typename ElfTypes>
void ElfWriterQuick<ElfTypes>::ComputeFileBuildId(
    uint8_t (*build_id)[ElfBuilder<ElfTypes>::kBuildIdLen]) {
  constexpr int kBufSize = 8192;
  std::vector<char> buffer(kBufSize);
  int64_t offset = 0;
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  while (true) {
    int64_t bytes_read = elf_file_->Read(buffer.data(), kBufSize, offset);
    CHECK_GE(bytes_read, 0);
    if (bytes_read == 0) {
      // End of file.
      break;
    }
    SHA1_Update(&ctx, buffer.data(), bytes_read);
    offset += bytes_read;
  }
  SHA1_Final(*build_id, &ctx);
}

template <typename ElfTypes>
OutputStream* ElfWriterQuick<ElfTypes>::GetStream() {
  return builder_->GetStream();
}

template <typename ElfTypes>
size_t ElfWriterQuick<ElfTypes>::GetLoadedSize() {
  return builder_->GetLoadedSize();
}

// Explicit instantiations
template class ElfWriterQuick<ElfTypes32>;
template class ElfWriterQuick<ElfTypes64>;

}  // namespace linker
}  // namespace art
