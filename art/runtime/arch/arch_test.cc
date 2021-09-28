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

#include <stdint.h>

#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "entrypoints/quick/callee_save_frame.h"
#include "common_runtime_test.h"
#include "quick/quick_method_frame_info.h"

namespace art {

class ArchTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions *options) override {
    // Use 64-bit ISA for runtime setup to make method size potentially larger
    // than necessary (rather than smaller) during CreateCalleeSaveMethod
    options->push_back(std::make_pair("imageinstructionset", "x86_64"));
  }

  // Do not do any of the finalization. We don't want to run any code, we don't need the heap
  // prepared, it actually will be a problem with setting the instruction set to x86_64 in
  // SetUpRuntimeOptions.
  void FinalizeSetup() override {
    ASSERT_EQ(InstructionSet::kX86_64, Runtime::Current()->GetInstructionSet());
  }
};

// Grab architecture specific constants.
namespace arm {
#include "arch/arm/asm_support_arm.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_WIDE_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_NARROW_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_WIDE_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_NARROW_OFFSET
}  // namespace arm

namespace arm64 {
#include "arch/arm64/asm_support_arm64.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
#undef BAKER_MARK_INTROSPECTION_ARRAY_SWITCH_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_ENTRYPOINT_OFFSET
#undef BAKER_MARK_INTROSPECTION_FIELD_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_ARRAY_LDR_OFFSET
#undef BAKER_MARK_INTROSPECTION_GC_ROOT_LDR_OFFSET
}  // namespace arm64

namespace x86 {
#include "arch/x86/asm_support_x86.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
}  // namespace x86

namespace x86_64 {
#include "arch/x86_64/asm_support_x86_64.h"
static constexpr size_t kFrameSizeSaveAllCalleeSaves = FRAME_SIZE_SAVE_ALL_CALLEE_SAVES;
#undef FRAME_SIZE_SAVE_ALL_CALLEE_SAVES
static constexpr size_t kFrameSizeSaveRefsOnly = FRAME_SIZE_SAVE_REFS_ONLY;
#undef FRAME_SIZE_SAVE_REFS_ONLY
static constexpr size_t kFrameSizeSaveRefsAndArgs = FRAME_SIZE_SAVE_REFS_AND_ARGS;
#undef FRAME_SIZE_SAVE_REFS_AND_ARGS
static constexpr size_t kFrameSizeSaveEverythingForClinit = FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_CLINIT
static constexpr size_t kFrameSizeSaveEverythingForSuspendCheck =
    FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK;
#undef FRAME_SIZE_SAVE_EVERYTHING_FOR_SUSPEND_CHECK
static constexpr size_t kFrameSizeSaveEverything = FRAME_SIZE_SAVE_EVERYTHING;
#undef FRAME_SIZE_SAVE_EVERYTHING
}  // namespace x86_64

// Check architecture specific constants are sound.
// We expect the return PC to be stored at the highest address slot in the frame.
#define TEST_ARCH_TYPE(Arch, arch, type)                                              \
  EXPECT_EQ(arch::Arch##CalleeSaveFrame::GetFrameSize(CalleeSaveType::k##type),       \
            arch::kFrameSize##type);                                                  \
  EXPECT_EQ(arch::Arch##CalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::k##type),  \
            arch::kFrameSize##type - static_cast<size_t>(k##Arch##PointerSize))
#define TEST_ARCH(Arch, arch)                                   \
  TEST_F(ArchTest, Arch) {                                      \
    TEST_ARCH_TYPE(Arch, arch, SaveAllCalleeSaves);             \
    TEST_ARCH_TYPE(Arch, arch, SaveRefsOnly);                   \
    TEST_ARCH_TYPE(Arch, arch, SaveRefsAndArgs);                \
    TEST_ARCH_TYPE(Arch, arch, SaveEverything);                 \
    TEST_ARCH_TYPE(Arch, arch, SaveEverythingForClinit);        \
    TEST_ARCH_TYPE(Arch, arch, SaveEverythingForSuspendCheck);  \
  }
TEST_ARCH(Arm, arm)
TEST_ARCH(Arm64, arm64)
TEST_ARCH(X86, x86)
TEST_ARCH(X86_64, x86_64)

}  // namespace art
