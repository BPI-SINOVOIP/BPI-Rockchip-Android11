/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
#define ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_

#include "base/allocator.h"
#include "base/arena_bit_vector.h"
#include "base/bit_table.h"
#include "base/bit_vector-inl.h"
#include "base/memory_region.h"
#include "base/scoped_arena_containers.h"
#include "base/value_object.h"
#include "dex_register_location.h"
#include "nodes.h"
#include "stack_map.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public DeletableArenaObject<kArenaAllocStackMapStream> {
 public:
  explicit StackMapStream(ScopedArenaAllocator* allocator, InstructionSet instruction_set)
      : allocator_(allocator),
        instruction_set_(instruction_set),
        stack_maps_(allocator),
        register_masks_(allocator),
        stack_masks_(allocator),
        inline_infos_(allocator),
        method_infos_(allocator),
        dex_register_masks_(allocator),
        dex_register_maps_(allocator),
        dex_register_catalog_(allocator),
        lazy_stack_masks_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_stack_map_(),
        current_inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_dex_registers_(allocator->Adapter(kArenaAllocStackMapStream)),
        previous_dex_registers_(allocator->Adapter(kArenaAllocStackMapStream)),
        dex_register_timestamp_(allocator->Adapter(kArenaAllocStackMapStream)),
        expected_num_dex_registers_(0u),
        temp_dex_register_mask_(allocator, 32, true, kArenaAllocStackMapStream),
        temp_dex_register_map_(allocator->Adapter(kArenaAllocStackMapStream)) {
  }

  void BeginMethod(size_t frame_size_in_bytes,
                   size_t core_spill_mask,
                   size_t fp_spill_mask,
                   uint32_t num_dex_registers,
                   bool baseline = false);
  void EndMethod();

  void BeginStackMapEntry(uint32_t dex_pc,
                          uint32_t native_pc_offset,
                          uint32_t register_mask = 0,
                          BitVector* sp_mask = nullptr,
                          StackMap::Kind kind = StackMap::Kind::Default,
                          bool needs_vreg_info = true);
  void EndStackMapEntry();

  void AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
    current_dex_registers_.push_back(DexRegisterLocation(kind, value));
  }

  void BeginInlineInfoEntry(ArtMethod* method,
                            uint32_t dex_pc,
                            uint32_t num_dex_registers,
                            const DexFile* outer_dex_file = nullptr);
  void EndInlineInfoEntry();

  size_t GetNumberOfStackMaps() const {
    return stack_maps_.size();
  }

  uint32_t GetStackMapNativePcOffset(size_t i);
  void SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset);

  // Encode all stack map data.
  // The returned vector is allocated using the allocator passed to the StackMapStream.
  ScopedArenaVector<uint8_t> Encode();

 private:
  static constexpr uint32_t kNoValue = -1;

  void CreateDexRegisterMap();

  // Invokes the callback with pointer of each BitTableBuilder field.
  template<typename Callback>
  void ForEachBitTable(Callback callback) {
    size_t index = 0;
    callback(index++, &stack_maps_);
    callback(index++, &register_masks_);
    callback(index++, &stack_masks_);
    callback(index++, &inline_infos_);
    callback(index++, &method_infos_);
    callback(index++, &dex_register_masks_);
    callback(index++, &dex_register_maps_);
    callback(index++, &dex_register_catalog_);
    CHECK_EQ(index, CodeInfo::kNumBitTables);
  }

  ScopedArenaAllocator* allocator_;
  const InstructionSet instruction_set_;
  uint32_t packed_frame_size_ = 0;
  uint32_t core_spill_mask_ = 0;
  uint32_t fp_spill_mask_ = 0;
  uint32_t num_dex_registers_ = 0;
  bool baseline_;
  BitTableBuilder<StackMap> stack_maps_;
  BitTableBuilder<RegisterMask> register_masks_;
  BitmapTableBuilder stack_masks_;
  BitTableBuilder<InlineInfo> inline_infos_;
  BitTableBuilder<MethodInfo> method_infos_;
  BitmapTableBuilder dex_register_masks_;
  BitTableBuilder<DexRegisterMapInfo> dex_register_maps_;
  BitTableBuilder<DexRegisterInfo> dex_register_catalog_;

  ScopedArenaVector<BitVector*> lazy_stack_masks_;

  // Variables which track the current state between Begin/End calls;
  bool in_method_ = false;
  bool in_stack_map_ = false;
  bool in_inline_info_ = false;
  BitTableBuilder<StackMap>::Entry current_stack_map_;
  ScopedArenaVector<BitTableBuilder<InlineInfo>::Entry> current_inline_infos_;
  ScopedArenaVector<DexRegisterLocation> current_dex_registers_;
  ScopedArenaVector<DexRegisterLocation> previous_dex_registers_;
  ScopedArenaVector<uint32_t> dex_register_timestamp_;  // Stack map index of last change.
  size_t expected_num_dex_registers_;

  // Temporary variables used in CreateDexRegisterMap.
  // They are here so that we can reuse the reserved memory.
  ArenaBitVector temp_dex_register_mask_;
  ScopedArenaVector<BitTableBuilder<DexRegisterMapInfo>::Entry> temp_dex_register_map_;

  // A set of lambda functions to be executed at the end to verify
  // the encoded data. It is generally only used in debug builds.
  std::vector<std::function<void(CodeInfo&)>> dchecks_;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
