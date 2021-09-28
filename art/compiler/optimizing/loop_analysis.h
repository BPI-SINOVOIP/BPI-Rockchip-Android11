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

#ifndef ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
#define ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_

#include "nodes.h"

namespace art {

class InductionVarRange;
class LoopAnalysis;

// Class to hold cached information on properties of the loop.
class LoopAnalysisInfo : public ValueObject {
 public:
  // No loop unrolling factor (just one copy of the loop-body).
  static constexpr uint32_t kNoUnrollingFactor = 1;
  // Used for unknown and non-constant trip counts (see InductionVarRange::HasKnownTripCount).
  static constexpr int64_t kUnknownTripCount = -1;

  explicit LoopAnalysisInfo(HLoopInformation* loop_info)
      : trip_count_(kUnknownTripCount),
        bb_num_(0),
        instr_num_(0),
        exits_num_(0),
        invariant_exits_num_(0),
        has_instructions_preventing_scalar_peeling_(false),
        has_instructions_preventing_scalar_unrolling_(false),
        has_long_type_instructions_(false),
        loop_info_(loop_info) {}

  int64_t GetTripCount() const { return trip_count_; }
  size_t GetNumberOfBasicBlocks() const { return bb_num_; }
  size_t GetNumberOfInstructions() const { return instr_num_; }
  size_t GetNumberOfExits() const { return exits_num_; }
  size_t GetNumberOfInvariantExits() const { return invariant_exits_num_; }

  bool HasInstructionsPreventingScalarPeeling() const {
    return has_instructions_preventing_scalar_peeling_;
  }

  bool HasInstructionsPreventingScalarUnrolling() const {
    return has_instructions_preventing_scalar_unrolling_;
  }

  bool HasInstructionsPreventingScalarOpts() const {
    return HasInstructionsPreventingScalarPeeling() || HasInstructionsPreventingScalarUnrolling();
  }

  bool HasLongTypeInstructions() const {
    return has_long_type_instructions_;
  }

  HLoopInformation* GetLoopInfo() const { return loop_info_; }

 private:
  // Trip count of the loop if known, kUnknownTripCount otherwise.
  int64_t trip_count_;
  // Number of basic blocks in the loop body.
  size_t bb_num_;
  // Number of instructions in the loop body.
  size_t instr_num_;
  // Number of loop's exits.
  size_t exits_num_;
  // Number of "if" loop exits (with HIf instruction) whose condition is loop-invariant.
  size_t invariant_exits_num_;
  // Whether the loop has instructions which make scalar loop peeling non-beneficial.
  bool has_instructions_preventing_scalar_peeling_;
  // Whether the loop has instructions which make scalar loop unrolling non-beneficial.
  bool has_instructions_preventing_scalar_unrolling_;
  // Whether the loop has instructions of primitive long type; unrolling these loop will
  // likely introduce spill/fills on 32-bit targets.
  bool has_long_type_instructions_;

  // Corresponding HLoopInformation.
  HLoopInformation* loop_info_;

  friend class LoopAnalysis;
};

// Placeholder class for methods and routines used to analyse loops, calculate loop properties
// and characteristics.
class LoopAnalysis : public ValueObject {
 public:
  // Calculates loops basic properties like body size, exits number, etc. and fills
  // 'analysis_results' with this information.
  static void CalculateLoopBasicProperties(HLoopInformation* loop_info,
                                           LoopAnalysisInfo* analysis_results,
                                           int64_t trip_count);

  // Returns the trip count of the loop if it is known and kUnknownTripCount otherwise.
  static int64_t GetLoopTripCount(HLoopInformation* loop_info,
                                  const InductionVarRange* induction_range);

 private:
  // Returns whether an instruction makes scalar loop peeling/unrolling non-beneficial.
  //
  // If in the loop body we have a dex/runtime call then its contribution to the whole
  // loop performance will probably prevail. So peeling/unrolling optimization will not bring
  // any noticeable performance improvement. It will increase the code size.
  static bool MakesScalarPeelingUnrollingNonBeneficial(HInstruction* instruction) {
    return (instruction->IsNewArray() ||
        instruction->IsNewInstance() ||
        instruction->IsUnresolvedInstanceFieldGet() ||
        instruction->IsUnresolvedInstanceFieldSet() ||
        instruction->IsUnresolvedStaticFieldGet() ||
        instruction->IsUnresolvedStaticFieldSet() ||
        // TODO: Support loops with intrinsified invokes.
        instruction->IsInvoke());
  }
};

//
// Helper class which holds target-dependent methods and constants needed for loop optimizations.
//
// To support peeling/unrolling for a new architecture one needs to create new helper class,
// inherit it from this and add implementation for the following methods.
//
class ArchNoOptsLoopHelper : public ArenaObject<kArenaAllocOptimization> {
 public:
  virtual ~ArchNoOptsLoopHelper() {}

  // Creates an instance of specialised helper for the target or default helper if the target
  // doesn't support loop peeling and unrolling.
  static ArchNoOptsLoopHelper* Create(InstructionSet isa, ArenaAllocator* allocator);

  // Returns whether the loop is not beneficial for loop peeling/unrolling.
  //
  // For example, if the loop body has too many instructions then peeling/unrolling optimization
  // will not bring any noticeable performance improvement however will increase the code size.
  //
  // Returns 'true' by default, should be overridden by particular target loop helper.
  virtual bool IsLoopNonBeneficialForScalarOpts(
      LoopAnalysisInfo* loop_analysis_info ATTRIBUTE_UNUSED) const { return true; }

  // Returns optimal scalar unrolling factor for the loop.
  //
  // Returns kNoUnrollingFactor by default, should be overridden by particular target loop helper.
  virtual uint32_t GetScalarUnrollingFactor(
      const LoopAnalysisInfo* analysis_info ATTRIBUTE_UNUSED) const {
    return LoopAnalysisInfo::kNoUnrollingFactor;
  }

  // Returns whether scalar loop peeling is enabled,
  //
  // Returns 'false' by default, should be overridden by particular target loop helper.
  virtual bool IsLoopPeelingEnabled() const { return false; }

  // Returns whether it is beneficial to fully unroll the loop.
  //
  // Returns 'false' by default, should be overridden by particular target loop helper.
  virtual bool IsFullUnrollingBeneficial(LoopAnalysisInfo* analysis_info ATTRIBUTE_UNUSED) const {
    return false;
  }

  // Returns optimal SIMD unrolling factor for the loop.
  //
  // Returns kNoUnrollingFactor by default, should be overridden by particular target loop helper.
  virtual uint32_t GetSIMDUnrollingFactor(HBasicBlock* block ATTRIBUTE_UNUSED,
                                          int64_t trip_count ATTRIBUTE_UNUSED,
                                          uint32_t max_peel ATTRIBUTE_UNUSED,
                                          uint32_t vector_length ATTRIBUTE_UNUSED) const {
    return LoopAnalysisInfo::kNoUnrollingFactor;
  }
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LOOP_ANALYSIS_H_
