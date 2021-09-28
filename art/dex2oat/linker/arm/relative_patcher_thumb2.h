/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_DEX2OAT_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_
#define ART_DEX2OAT_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_

#include "arch/arm/registers_arm.h"
#include "base/array_ref.h"
#include "linker/arm/relative_patcher_arm_base.h"

namespace art {

namespace arm {
class ArmVIXLAssembler;
}  // namespace arm

namespace linker {

class Thumb2RelativePatcher final : public ArmBaseRelativePatcher {
 public:
  explicit Thumb2RelativePatcher(RelativePatcherThunkProvider* thunk_provider,
                                 RelativePatcherTargetProvider* target_provider);

  void PatchCall(std::vector<uint8_t>* code,
                 uint32_t literal_offset,
                 uint32_t patch_offset,
                 uint32_t target_offset) override;
  void PatchPcRelativeReference(std::vector<uint8_t>* code,
                                const LinkerPatch& patch,
                                uint32_t patch_offset,
                                uint32_t target_offset) override;
  void PatchEntrypointCall(std::vector<uint8_t>* code,
                           const LinkerPatch& patch,
                           uint32_t patch_offset) override;
  void PatchBakerReadBarrierBranch(std::vector<uint8_t>* code,
                                   const LinkerPatch& patch,
                                   uint32_t patch_offset) override;

 protected:
  uint32_t MaxPositiveDisplacement(const ThunkKey& key) override;
  uint32_t MaxNegativeDisplacement(const ThunkKey& key) override;

 private:
  static void PatchBl(std::vector<uint8_t>* code, uint32_t literal_offset, uint32_t displacement);

  static void SetInsn32(std::vector<uint8_t>* code, uint32_t offset, uint32_t value);
  static uint32_t GetInsn32(ArrayRef<const uint8_t> code, uint32_t offset);

  template <typename Vector>
  static uint32_t GetInsn32(Vector* code, uint32_t offset);

  static uint32_t GetInsn16(ArrayRef<const uint8_t> code, uint32_t offset);

  template <typename Vector>
  static uint32_t GetInsn16(Vector* code, uint32_t offset);

  friend class Thumb2RelativePatcherTest;

  DISALLOW_COPY_AND_ASSIGN(Thumb2RelativePatcher);
};

}  // namespace linker
}  // namespace art

#endif  // ART_DEX2OAT_LINKER_ARM_RELATIVE_PATCHER_THUMB2_H_
