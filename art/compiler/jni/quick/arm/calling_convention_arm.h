/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_
#define ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_

#include "base/enums.h"
#include "jni/quick/calling_convention.h"

namespace art {
namespace arm {

class ArmManagedRuntimeCallingConvention final : public ManagedRuntimeCallingConvention {
 public:
  ArmManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, const char* shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k32) {}
  ~ArmManagedRuntimeCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() override;
  ManagedRegister InterproceduralScratchRegister() const override;
  // Managed runtime calling convention
  ManagedRegister MethodRegister() override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;
  const ManagedRegisterEntrySpills& EntrySpills() override;

 private:
  ManagedRegisterEntrySpills entry_spills_;

  DISALLOW_COPY_AND_ASSIGN(ArmManagedRuntimeCallingConvention);
};

class ArmJniCallingConvention final : public JniCallingConvention {
 public:
  ArmJniCallingConvention(bool is_static,
                          bool is_synchronized,
                          bool is_critical_native,
                          const char* shorty);
  ~ArmJniCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() override;
  ManagedRegister IntReturnRegister() override;
  ManagedRegister InterproceduralScratchRegister() const override;
  // JNI calling convention
  void Next() override;  // Override default behavior for AAPCS
  size_t FrameSize() const override;
  size_t OutArgSize() const override;
  ArrayRef<const ManagedRegister> CalleeSaveRegisters() const override;
  ManagedRegister ReturnScratchRegister() const override;
  uint32_t CoreSpillMask() const override;
  uint32_t FpSpillMask() const override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;

  // AAPCS mandates return values are extended.
  bool RequiresSmallResultTypeExtension() const override {
    return false;
  }

  // Hidden argument register, used to pass the method pointer for @CriticalNative call.
  ManagedRegister HiddenArgumentRegister() const override;

  // Whether to use tail call (used only for @CriticalNative).
  bool UseTailCall() const override;

 private:
  // Padding to ensure longs and doubles are not split in AAPCS
  size_t padding_;

  DISALLOW_COPY_AND_ASSIGN(ArmJniCallingConvention);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_
