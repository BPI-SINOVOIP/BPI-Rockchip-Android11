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

#include <memory>
#include <vector>

#include "arch/instruction_set.h"
#include "base/runtime_debug.h"
#include "cfi_test.h"
#include "driver/compiler_options.h"
#include "gtest/gtest.h"
#include "optimizing/code_generator.h"
#include "optimizing/optimizing_unit_test.h"
#include "read_barrier_config.h"
#include "utils/arm/assembler_arm_vixl.h"
#include "utils/assembler.h"

#include "optimizing/optimizing_cfi_test_expected.inc"

namespace vixl32 = vixl::aarch32;

namespace art {

// Run the tests only on host.
#ifndef ART_TARGET_ANDROID

class OptimizingCFITest : public CFITest, public OptimizingUnitTestHelper {
 public:
  // Enable this flag to generate the expected outputs.
  static constexpr bool kGenerateExpected = false;

  OptimizingCFITest()
      : graph_(nullptr),
        code_gen_(),
        blocks_(GetAllocator()->Adapter()) {}

  void SetUpFrame(InstructionSet isa) {
    OverrideInstructionSetFeatures(isa, "default");

    // Ensure that slow-debug is off, so that there is no unexpected read-barrier check emitted.
    SetRuntimeDebugFlagsEnabled(false);

    // Setup simple context.
    graph_ = CreateGraph();
    // Generate simple frame with some spills.
    code_gen_ = CodeGenerator::Create(graph_, *compiler_options_);
    code_gen_->GetAssembler()->cfi().SetEnabled(true);
    code_gen_->InitializeCodeGenerationData();
    const int frame_size = 64;
    int core_reg = 0;
    int fp_reg = 0;
    for (int i = 0; i < 2; i++) {  // Two registers of each kind.
      for (; core_reg < 32; core_reg++) {
        if (code_gen_->IsCoreCalleeSaveRegister(core_reg)) {
          auto location = Location::RegisterLocation(core_reg);
          code_gen_->AddAllocatedRegister(location);
          core_reg++;
          break;
        }
      }
      for (; fp_reg < 32; fp_reg++) {
        if (code_gen_->IsFloatingPointCalleeSaveRegister(fp_reg)) {
          auto location = Location::FpuRegisterLocation(fp_reg);
          code_gen_->AddAllocatedRegister(location);
          fp_reg++;
          break;
        }
      }
    }
    code_gen_->block_order_ = &blocks_;
    code_gen_->ComputeSpillMask();
    code_gen_->SetFrameSize(frame_size);
    code_gen_->GenerateFrameEntry();
  }

  void Finish() {
    code_gen_->GenerateFrameExit();
    code_gen_->Finalize(&code_allocator_);
  }

  void Check(InstructionSet isa,
             const char* isa_str,
             const std::vector<uint8_t>& expected_asm,
             const std::vector<uint8_t>& expected_cfi) {
    // Get the outputs.
    ArrayRef<const uint8_t> actual_asm = code_allocator_.GetMemory();
    Assembler* opt_asm = code_gen_->GetAssembler();
    ArrayRef<const uint8_t> actual_cfi(*(opt_asm->cfi().data()));

    if (kGenerateExpected) {
      GenerateExpected(stdout, isa, isa_str, actual_asm, actual_cfi);
    } else {
      EXPECT_EQ(ArrayRef<const uint8_t>(expected_asm), actual_asm);
      EXPECT_EQ(ArrayRef<const uint8_t>(expected_cfi), actual_cfi);
    }
  }

  void TestImpl(InstructionSet isa, const char*
                isa_str,
                const std::vector<uint8_t>& expected_asm,
                const std::vector<uint8_t>& expected_cfi) {
    SetUpFrame(isa);
    Finish();
    Check(isa, isa_str, expected_asm, expected_cfi);
  }

  CodeGenerator* GetCodeGenerator() {
    return code_gen_.get();
  }

 private:
  class InternalCodeAllocator : public CodeAllocator {
   public:
    InternalCodeAllocator() {}

    uint8_t* Allocate(size_t size) override {
      memory_.resize(size);
      return memory_.data();
    }

    ArrayRef<const uint8_t> GetMemory() const override { return ArrayRef<const uint8_t>(memory_); }

   private:
    std::vector<uint8_t> memory_;

    DISALLOW_COPY_AND_ASSIGN(InternalCodeAllocator);
  };

  HGraph* graph_;
  std::unique_ptr<CodeGenerator> code_gen_;
  ArenaVector<HBasicBlock*> blocks_;
  InternalCodeAllocator code_allocator_;
};

#define TEST_ISA(isa)                                                 \
  TEST_F(OptimizingCFITest, isa) {                                    \
    std::vector<uint8_t> expected_asm(                                \
        expected_asm_##isa,                                           \
        expected_asm_##isa + arraysize(expected_asm_##isa));          \
    std::vector<uint8_t> expected_cfi(                                \
        expected_cfi_##isa,                                           \
        expected_cfi_##isa + arraysize(expected_cfi_##isa));          \
    TestImpl(InstructionSet::isa, #isa, expected_asm, expected_cfi);  \
  }

#ifdef ART_ENABLE_CODEGEN_arm
TEST_ISA(kThumb2)
#endif

#ifdef ART_ENABLE_CODEGEN_arm64
// Run the tests for ARM64 only with Baker read barriers, as the
// expected generated code saves and restore X21 and X22 (instead of
// X20 and X21), as X20 is used as Marking Register in the Baker read
// barrier configuration, and as such is removed from the set of
// callee-save registers in the ARM64 code generator of the Optimizing
// compiler.
#if defined(USE_READ_BARRIER) && defined(USE_BAKER_READ_BARRIER)
TEST_ISA(kArm64)
#endif
#endif

#ifdef ART_ENABLE_CODEGEN_x86
TEST_ISA(kX86)
#endif

#ifdef ART_ENABLE_CODEGEN_x86_64
TEST_ISA(kX86_64)
#endif

#ifdef ART_ENABLE_CODEGEN_arm
TEST_F(OptimizingCFITest, kThumb2Adjust) {
  using vixl32::r0;
  std::vector<uint8_t> expected_asm(
      expected_asm_kThumb2_adjust,
      expected_asm_kThumb2_adjust + arraysize(expected_asm_kThumb2_adjust));
  std::vector<uint8_t> expected_cfi(
      expected_cfi_kThumb2_adjust,
      expected_cfi_kThumb2_adjust + arraysize(expected_cfi_kThumb2_adjust));
  SetUpFrame(InstructionSet::kThumb2);
#define __ down_cast<arm::ArmVIXLAssembler*>(GetCodeGenerator() \
    ->GetAssembler())->GetVIXLAssembler()->
  vixl32::Label target;
  __ CompareAndBranchIfZero(r0, &target);
  // Push the target out of range of CBZ.
  for (size_t i = 0; i != 65; ++i) {
    __ Ldr(r0, vixl32::MemOperand(r0));
  }
  __ Bind(&target);
#undef __
  Finish();
  Check(InstructionSet::kThumb2, "kThumb2_adjust", expected_asm, expected_cfi);
}
#endif

#endif  // ART_TARGET_ANDROID

}  // namespace art
