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

#include "code_generator.h"

#ifdef ART_ENABLE_CODEGEN_arm
#include "code_generator_arm_vixl.h"
#endif

#ifdef ART_ENABLE_CODEGEN_arm64
#include "code_generator_arm64.h"
#endif

#ifdef ART_ENABLE_CODEGEN_x86
#include "code_generator_x86.h"
#endif

#ifdef ART_ENABLE_CODEGEN_x86_64
#include "code_generator_x86_64.h"
#endif

#include "base/bit_utils.h"
#include "base/bit_utils_iterator.h"
#include "base/casts.h"
#include "base/leb128.h"
#include "class_linker.h"
#include "compiled_method.h"
#include "dex/bytecode_utils.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/verified_method.h"
#include "graph_visualizer.h"
#include "image.h"
#include "gc/space/image_space.h"
#include "intern_table.h"
#include "intrinsics.h"
#include "mirror/array-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/object_reference.h"
#include "mirror/reference.h"
#include "mirror/string.h"
#include "parallel_move_resolver.h"
#include "scoped_thread_state_change-inl.h"
#include "ssa_liveness_analysis.h"
#include "stack_map.h"
#include "stack_map_stream.h"
#include "string_builder_append.h"
#include "thread-current-inl.h"
#include "utils/assembler.h"

namespace art {

// Return whether a location is consistent with a type.
static bool CheckType(DataType::Type type, Location location) {
  if (location.IsFpuRegister()
      || (location.IsUnallocated() && (location.GetPolicy() == Location::kRequiresFpuRegister))) {
    return (type == DataType::Type::kFloat32) || (type == DataType::Type::kFloat64);
  } else if (location.IsRegister() ||
             (location.IsUnallocated() && (location.GetPolicy() == Location::kRequiresRegister))) {
    return DataType::IsIntegralType(type) || (type == DataType::Type::kReference);
  } else if (location.IsRegisterPair()) {
    return type == DataType::Type::kInt64;
  } else if (location.IsFpuRegisterPair()) {
    return type == DataType::Type::kFloat64;
  } else if (location.IsStackSlot()) {
    return (DataType::IsIntegralType(type) && type != DataType::Type::kInt64)
           || (type == DataType::Type::kFloat32)
           || (type == DataType::Type::kReference);
  } else if (location.IsDoubleStackSlot()) {
    return (type == DataType::Type::kInt64) || (type == DataType::Type::kFloat64);
  } else if (location.IsConstant()) {
    if (location.GetConstant()->IsIntConstant()) {
      return DataType::IsIntegralType(type) && (type != DataType::Type::kInt64);
    } else if (location.GetConstant()->IsNullConstant()) {
      return type == DataType::Type::kReference;
    } else if (location.GetConstant()->IsLongConstant()) {
      return type == DataType::Type::kInt64;
    } else if (location.GetConstant()->IsFloatConstant()) {
      return type == DataType::Type::kFloat32;
    } else {
      return location.GetConstant()->IsDoubleConstant()
          && (type == DataType::Type::kFloat64);
    }
  } else {
    return location.IsInvalid() || (location.GetPolicy() == Location::kAny);
  }
}

// Check that a location summary is consistent with an instruction.
static bool CheckTypeConsistency(HInstruction* instruction) {
  LocationSummary* locations = instruction->GetLocations();
  if (locations == nullptr) {
    return true;
  }

  if (locations->Out().IsUnallocated()
      && (locations->Out().GetPolicy() == Location::kSameAsFirstInput)) {
    DCHECK(CheckType(instruction->GetType(), locations->InAt(0)))
        << instruction->GetType()
        << " " << locations->InAt(0);
  } else {
    DCHECK(CheckType(instruction->GetType(), locations->Out()))
        << instruction->GetType()
        << " " << locations->Out();
  }

  HConstInputsRef inputs = instruction->GetInputs();
  for (size_t i = 0; i < inputs.size(); ++i) {
    DCHECK(CheckType(inputs[i]->GetType(), locations->InAt(i)))
      << inputs[i]->GetType() << " " << locations->InAt(i);
  }

  HEnvironment* environment = instruction->GetEnvironment();
  for (size_t i = 0; i < instruction->EnvironmentSize(); ++i) {
    if (environment->GetInstructionAt(i) != nullptr) {
      DataType::Type type = environment->GetInstructionAt(i)->GetType();
      DCHECK(CheckType(type, environment->GetLocationAt(i)))
        << type << " " << environment->GetLocationAt(i);
    } else {
      DCHECK(environment->GetLocationAt(i).IsInvalid())
        << environment->GetLocationAt(i);
    }
  }
  return true;
}

class CodeGenerator::CodeGenerationData : public DeletableArenaObject<kArenaAllocCodeGenerator> {
 public:
  static std::unique_ptr<CodeGenerationData> Create(ArenaStack* arena_stack,
                                                    InstructionSet instruction_set) {
    ScopedArenaAllocator allocator(arena_stack);
    void* memory = allocator.Alloc<CodeGenerationData>(kArenaAllocCodeGenerator);
    return std::unique_ptr<CodeGenerationData>(
        ::new (memory) CodeGenerationData(std::move(allocator), instruction_set));
  }

  ScopedArenaAllocator* GetScopedAllocator() {
    return &allocator_;
  }

  void AddSlowPath(SlowPathCode* slow_path) {
    slow_paths_.emplace_back(std::unique_ptr<SlowPathCode>(slow_path));
  }

  ArrayRef<const std::unique_ptr<SlowPathCode>> GetSlowPaths() const {
    return ArrayRef<const std::unique_ptr<SlowPathCode>>(slow_paths_);
  }

  StackMapStream* GetStackMapStream() { return &stack_map_stream_; }

  void ReserveJitStringRoot(StringReference string_reference, Handle<mirror::String> string) {
    jit_string_roots_.Overwrite(string_reference,
                                reinterpret_cast64<uint64_t>(string.GetReference()));
  }

  uint64_t GetJitStringRootIndex(StringReference string_reference) const {
    return jit_string_roots_.Get(string_reference);
  }

  size_t GetNumberOfJitStringRoots() const {
    return jit_string_roots_.size();
  }

  void ReserveJitClassRoot(TypeReference type_reference, Handle<mirror::Class> klass) {
    jit_class_roots_.Overwrite(type_reference, reinterpret_cast64<uint64_t>(klass.GetReference()));
  }

  uint64_t GetJitClassRootIndex(TypeReference type_reference) const {
    return jit_class_roots_.Get(type_reference);
  }

  size_t GetNumberOfJitClassRoots() const {
    return jit_class_roots_.size();
  }

  size_t GetNumberOfJitRoots() const {
    return GetNumberOfJitStringRoots() + GetNumberOfJitClassRoots();
  }

  void EmitJitRoots(/*out*/std::vector<Handle<mirror::Object>>* roots)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  CodeGenerationData(ScopedArenaAllocator&& allocator, InstructionSet instruction_set)
      : allocator_(std::move(allocator)),
        stack_map_stream_(&allocator_, instruction_set),
        slow_paths_(allocator_.Adapter(kArenaAllocCodeGenerator)),
        jit_string_roots_(StringReferenceValueComparator(),
                          allocator_.Adapter(kArenaAllocCodeGenerator)),
        jit_class_roots_(TypeReferenceValueComparator(),
                         allocator_.Adapter(kArenaAllocCodeGenerator)) {
    slow_paths_.reserve(kDefaultSlowPathsCapacity);
  }

  static constexpr size_t kDefaultSlowPathsCapacity = 8;

  ScopedArenaAllocator allocator_;
  StackMapStream stack_map_stream_;
  ScopedArenaVector<std::unique_ptr<SlowPathCode>> slow_paths_;

  // Maps a StringReference (dex_file, string_index) to the index in the literal table.
  // Entries are intially added with a pointer in the handle zone, and `EmitJitRoots`
  // will compute all the indices.
  ScopedArenaSafeMap<StringReference, uint64_t, StringReferenceValueComparator> jit_string_roots_;

  // Maps a ClassReference (dex_file, type_index) to the index in the literal table.
  // Entries are intially added with a pointer in the handle zone, and `EmitJitRoots`
  // will compute all the indices.
  ScopedArenaSafeMap<TypeReference, uint64_t, TypeReferenceValueComparator> jit_class_roots_;
};

void CodeGenerator::CodeGenerationData::EmitJitRoots(
    /*out*/std::vector<Handle<mirror::Object>>* roots) {
  DCHECK(roots->empty());
  roots->reserve(GetNumberOfJitRoots());
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  size_t index = 0;
  for (auto& entry : jit_string_roots_) {
    // Update the `roots` with the string, and replace the address temporarily
    // stored to the index in the table.
    uint64_t address = entry.second;
    roots->emplace_back(reinterpret_cast<StackReference<mirror::Object>*>(address));
    DCHECK(roots->back() != nullptr);
    DCHECK(roots->back()->IsString());
    entry.second = index;
    // Ensure the string is strongly interned. This is a requirement on how the JIT
    // handles strings. b/32995596
    class_linker->GetInternTable()->InternStrong(roots->back()->AsString());
    ++index;
  }
  for (auto& entry : jit_class_roots_) {
    // Update the `roots` with the class, and replace the address temporarily
    // stored to the index in the table.
    uint64_t address = entry.second;
    roots->emplace_back(reinterpret_cast<StackReference<mirror::Object>*>(address));
    DCHECK(roots->back() != nullptr);
    DCHECK(roots->back()->IsClass());
    entry.second = index;
    ++index;
  }
}

ScopedArenaAllocator* CodeGenerator::GetScopedAllocator() {
  DCHECK(code_generation_data_ != nullptr);
  return code_generation_data_->GetScopedAllocator();
}

StackMapStream* CodeGenerator::GetStackMapStream() {
  DCHECK(code_generation_data_ != nullptr);
  return code_generation_data_->GetStackMapStream();
}

void CodeGenerator::ReserveJitStringRoot(StringReference string_reference,
                                         Handle<mirror::String> string) {
  DCHECK(code_generation_data_ != nullptr);
  code_generation_data_->ReserveJitStringRoot(string_reference, string);
}

uint64_t CodeGenerator::GetJitStringRootIndex(StringReference string_reference) {
  DCHECK(code_generation_data_ != nullptr);
  return code_generation_data_->GetJitStringRootIndex(string_reference);
}

void CodeGenerator::ReserveJitClassRoot(TypeReference type_reference, Handle<mirror::Class> klass) {
  DCHECK(code_generation_data_ != nullptr);
  code_generation_data_->ReserveJitClassRoot(type_reference, klass);
}

uint64_t CodeGenerator::GetJitClassRootIndex(TypeReference type_reference) {
  DCHECK(code_generation_data_ != nullptr);
  return code_generation_data_->GetJitClassRootIndex(type_reference);
}

void CodeGenerator::EmitJitRootPatches(uint8_t* code ATTRIBUTE_UNUSED,
                                       const uint8_t* roots_data ATTRIBUTE_UNUSED) {
  DCHECK(code_generation_data_ != nullptr);
  DCHECK_EQ(code_generation_data_->GetNumberOfJitStringRoots(), 0u);
  DCHECK_EQ(code_generation_data_->GetNumberOfJitClassRoots(), 0u);
}

uint32_t CodeGenerator::GetArrayLengthOffset(HArrayLength* array_length) {
  return array_length->IsStringLength()
      ? mirror::String::CountOffset().Uint32Value()
      : mirror::Array::LengthOffset().Uint32Value();
}

uint32_t CodeGenerator::GetArrayDataOffset(HArrayGet* array_get) {
  DCHECK(array_get->GetType() == DataType::Type::kUint16 || !array_get->IsStringCharAt());
  return array_get->IsStringCharAt()
      ? mirror::String::ValueOffset().Uint32Value()
      : mirror::Array::DataOffset(DataType::Size(array_get->GetType())).Uint32Value();
}

bool CodeGenerator::GoesToNextBlock(HBasicBlock* current, HBasicBlock* next) const {
  DCHECK_EQ((*block_order_)[current_block_index_], current);
  return GetNextBlockToEmit() == FirstNonEmptyBlock(next);
}

HBasicBlock* CodeGenerator::GetNextBlockToEmit() const {
  for (size_t i = current_block_index_ + 1; i < block_order_->size(); ++i) {
    HBasicBlock* block = (*block_order_)[i];
    if (!block->IsSingleJump()) {
      return block;
    }
  }
  return nullptr;
}

HBasicBlock* CodeGenerator::FirstNonEmptyBlock(HBasicBlock* block) const {
  while (block->IsSingleJump()) {
    block = block->GetSuccessors()[0];
  }
  return block;
}

class DisassemblyScope {
 public:
  DisassemblyScope(HInstruction* instruction, const CodeGenerator& codegen)
      : codegen_(codegen), instruction_(instruction), start_offset_(static_cast<size_t>(-1)) {
    if (codegen_.GetDisassemblyInformation() != nullptr) {
      start_offset_ = codegen_.GetAssembler().CodeSize();
    }
  }

  ~DisassemblyScope() {
    // We avoid building this data when we know it will not be used.
    if (codegen_.GetDisassemblyInformation() != nullptr) {
      codegen_.GetDisassemblyInformation()->AddInstructionInterval(
          instruction_, start_offset_, codegen_.GetAssembler().CodeSize());
    }
  }

 private:
  const CodeGenerator& codegen_;
  HInstruction* instruction_;
  size_t start_offset_;
};


void CodeGenerator::GenerateSlowPaths() {
  DCHECK(code_generation_data_ != nullptr);
  size_t code_start = 0;
  for (const std::unique_ptr<SlowPathCode>& slow_path_ptr : code_generation_data_->GetSlowPaths()) {
    SlowPathCode* slow_path = slow_path_ptr.get();
    current_slow_path_ = slow_path;
    if (disasm_info_ != nullptr) {
      code_start = GetAssembler()->CodeSize();
    }
    // Record the dex pc at start of slow path (required for java line number mapping).
    MaybeRecordNativeDebugInfo(slow_path->GetInstruction(), slow_path->GetDexPc(), slow_path);
    slow_path->EmitNativeCode(this);
    if (disasm_info_ != nullptr) {
      disasm_info_->AddSlowPathInterval(slow_path, code_start, GetAssembler()->CodeSize());
    }
  }
  current_slow_path_ = nullptr;
}

void CodeGenerator::InitializeCodeGenerationData() {
  DCHECK(code_generation_data_ == nullptr);
  code_generation_data_ = CodeGenerationData::Create(graph_->GetArenaStack(), GetInstructionSet());
}

void CodeGenerator::Compile(CodeAllocator* allocator) {
  InitializeCodeGenerationData();

  // The register allocator already called `InitializeCodeGeneration`,
  // where the frame size has been computed.
  DCHECK(block_order_ != nullptr);
  Initialize();

  HGraphVisitor* instruction_visitor = GetInstructionVisitor();
  DCHECK_EQ(current_block_index_, 0u);

  GetStackMapStream()->BeginMethod(HasEmptyFrame() ? 0 : frame_size_,
                                   core_spill_mask_,
                                   fpu_spill_mask_,
                                   GetGraph()->GetNumberOfVRegs(),
                                   GetGraph()->IsCompilingBaseline());

  size_t frame_start = GetAssembler()->CodeSize();
  GenerateFrameEntry();
  DCHECK_EQ(GetAssembler()->cfi().GetCurrentCFAOffset(), static_cast<int>(frame_size_));
  if (disasm_info_ != nullptr) {
    disasm_info_->SetFrameEntryInterval(frame_start, GetAssembler()->CodeSize());
  }

  for (size_t e = block_order_->size(); current_block_index_ < e; ++current_block_index_) {
    HBasicBlock* block = (*block_order_)[current_block_index_];
    // Don't generate code for an empty block. Its predecessors will branch to its successor
    // directly. Also, the label of that block will not be emitted, so this helps catch
    // errors where we reference that label.
    if (block->IsSingleJump()) continue;
    Bind(block);
    // This ensures that we have correct native line mapping for all native instructions.
    // It is necessary to make stepping over a statement work. Otherwise, any initial
    // instructions (e.g. moves) would be assumed to be the start of next statement.
    MaybeRecordNativeDebugInfo(/* instruction= */ nullptr, block->GetDexPc());
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* current = it.Current();
      if (current->HasEnvironment()) {
        // Create stackmap for HNativeDebugInfo or any instruction which calls native code.
        // Note that we need correct mapping for the native PC of the call instruction,
        // so the runtime's stackmap is not sufficient since it is at PC after the call.
        MaybeRecordNativeDebugInfo(current, block->GetDexPc());
      }
      DisassemblyScope disassembly_scope(current, *this);
      DCHECK(CheckTypeConsistency(current));
      current->Accept(instruction_visitor);
    }
  }

  GenerateSlowPaths();

  // Emit catch stack maps at the end of the stack map stream as expected by the
  // runtime exception handler.
  if (graph_->HasTryCatch()) {
    RecordCatchBlockInfo();
  }

  // Finalize instructions in assember;
  Finalize(allocator);

  GetStackMapStream()->EndMethod();
}

void CodeGenerator::Finalize(CodeAllocator* allocator) {
  size_t code_size = GetAssembler()->CodeSize();
  uint8_t* buffer = allocator->Allocate(code_size);

  MemoryRegion code(buffer, code_size);
  GetAssembler()->FinalizeInstructions(code);
}

void CodeGenerator::EmitLinkerPatches(
    ArenaVector<linker::LinkerPatch>* linker_patches ATTRIBUTE_UNUSED) {
  // No linker patches by default.
}

bool CodeGenerator::NeedsThunkCode(const linker::LinkerPatch& patch ATTRIBUTE_UNUSED) const {
  // Code generators that create patches requiring thunk compilation should override this function.
  return false;
}

void CodeGenerator::EmitThunkCode(const linker::LinkerPatch& patch ATTRIBUTE_UNUSED,
                                  /*out*/ ArenaVector<uint8_t>* code ATTRIBUTE_UNUSED,
                                  /*out*/ std::string* debug_name ATTRIBUTE_UNUSED) {
  // Code generators that create patches requiring thunk compilation should override this function.
  LOG(FATAL) << "Unexpected call to EmitThunkCode().";
}

void CodeGenerator::InitializeCodeGeneration(size_t number_of_spill_slots,
                                             size_t maximum_safepoint_spill_size,
                                             size_t number_of_out_slots,
                                             const ArenaVector<HBasicBlock*>& block_order) {
  block_order_ = &block_order;
  DCHECK(!block_order.empty());
  DCHECK(block_order[0] == GetGraph()->GetEntryBlock());
  ComputeSpillMask();
  first_register_slot_in_slow_path_ = RoundUp(
      (number_of_out_slots + number_of_spill_slots) * kVRegSize, GetPreferredSlotsAlignment());

  if (number_of_spill_slots == 0
      && !HasAllocatedCalleeSaveRegisters()
      && IsLeafMethod()
      && !RequiresCurrentMethod()) {
    DCHECK_EQ(maximum_safepoint_spill_size, 0u);
    SetFrameSize(CallPushesPC() ? GetWordSize() : 0);
  } else {
    SetFrameSize(RoundUp(
        first_register_slot_in_slow_path_
        + maximum_safepoint_spill_size
        + (GetGraph()->HasShouldDeoptimizeFlag() ? kShouldDeoptimizeFlagSize : 0)
        + FrameEntrySpillSize(),
        kStackAlignment));
  }
}

void CodeGenerator::CreateCommonInvokeLocationSummary(
    HInvoke* invoke, InvokeDexCallingConventionVisitor* visitor) {
  ArenaAllocator* allocator = invoke->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations = new (allocator) LocationSummary(invoke,
                                                               LocationSummary::kCallOnMainOnly);

  for (size_t i = 0; i < invoke->GetNumberOfArguments(); i++) {
    HInstruction* input = invoke->InputAt(i);
    locations->SetInAt(i, visitor->GetNextLocation(input->GetType()));
  }

  locations->SetOut(visitor->GetReturnLocation(invoke->GetType()));

  if (invoke->IsInvokeStaticOrDirect()) {
    HInvokeStaticOrDirect* call = invoke->AsInvokeStaticOrDirect();
    switch (call->GetMethodLoadKind()) {
      case HInvokeStaticOrDirect::MethodLoadKind::kRecursive:
        locations->SetInAt(call->GetSpecialInputIndex(), visitor->GetMethodLocation());
        break;
      case HInvokeStaticOrDirect::MethodLoadKind::kRuntimeCall:
        locations->AddTemp(visitor->GetMethodLocation());
        locations->SetInAt(call->GetSpecialInputIndex(), Location::RequiresRegister());
        break;
      default:
        locations->AddTemp(visitor->GetMethodLocation());
        break;
    }
  } else if (!invoke->IsInvokePolymorphic()) {
    locations->AddTemp(visitor->GetMethodLocation());
  }
}

void CodeGenerator::GenerateInvokeStaticOrDirectRuntimeCall(
    HInvokeStaticOrDirect* invoke, Location temp, SlowPathCode* slow_path) {
  MoveConstant(temp, invoke->GetDexMethodIndex());

  // The access check is unnecessary but we do not want to introduce
  // extra entrypoints for the codegens that do not support some
  // invoke type and fall back to the runtime call.

  // Initialize to anything to silent compiler warnings.
  QuickEntrypointEnum entrypoint = kQuickInvokeStaticTrampolineWithAccessCheck;
  switch (invoke->GetInvokeType()) {
    case kStatic:
      entrypoint = kQuickInvokeStaticTrampolineWithAccessCheck;
      break;
    case kDirect:
      entrypoint = kQuickInvokeDirectTrampolineWithAccessCheck;
      break;
    case kSuper:
      entrypoint = kQuickInvokeSuperTrampolineWithAccessCheck;
      break;
    case kVirtual:
    case kInterface:
    case kPolymorphic:
    case kCustom:
      LOG(FATAL) << "Unexpected invoke type: " << invoke->GetInvokeType();
      UNREACHABLE();
  }

  InvokeRuntime(entrypoint, invoke, invoke->GetDexPc(), slow_path);
}
void CodeGenerator::GenerateInvokeUnresolvedRuntimeCall(HInvokeUnresolved* invoke) {
  MoveConstant(invoke->GetLocations()->GetTemp(0), invoke->GetDexMethodIndex());

  // Initialize to anything to silent compiler warnings.
  QuickEntrypointEnum entrypoint = kQuickInvokeStaticTrampolineWithAccessCheck;
  switch (invoke->GetInvokeType()) {
    case kStatic:
      entrypoint = kQuickInvokeStaticTrampolineWithAccessCheck;
      break;
    case kDirect:
      entrypoint = kQuickInvokeDirectTrampolineWithAccessCheck;
      break;
    case kVirtual:
      entrypoint = kQuickInvokeVirtualTrampolineWithAccessCheck;
      break;
    case kSuper:
      entrypoint = kQuickInvokeSuperTrampolineWithAccessCheck;
      break;
    case kInterface:
      entrypoint = kQuickInvokeInterfaceTrampolineWithAccessCheck;
      break;
    case kPolymorphic:
    case kCustom:
      LOG(FATAL) << "Unexpected invoke type: " << invoke->GetInvokeType();
      UNREACHABLE();
  }
  InvokeRuntime(entrypoint, invoke, invoke->GetDexPc(), nullptr);
}

void CodeGenerator::GenerateInvokePolymorphicCall(HInvokePolymorphic* invoke) {
  // invoke-polymorphic does not use a temporary to convey any additional information (e.g. a
  // method index) since it requires multiple info from the instruction (registers A, B, H). Not
  // using the reservation has no effect on the registers used in the runtime call.
  QuickEntrypointEnum entrypoint = kQuickInvokePolymorphic;
  InvokeRuntime(entrypoint, invoke, invoke->GetDexPc(), nullptr);
}

void CodeGenerator::GenerateInvokeCustomCall(HInvokeCustom* invoke) {
  MoveConstant(invoke->GetLocations()->GetTemp(0), invoke->GetCallSiteIndex());
  QuickEntrypointEnum entrypoint = kQuickInvokeCustom;
  InvokeRuntime(entrypoint, invoke, invoke->GetDexPc(), nullptr);
}

void CodeGenerator::CreateStringBuilderAppendLocations(HStringBuilderAppend* instruction,
                                                       Location out) {
  ArenaAllocator* allocator = GetGraph()->GetAllocator();
  LocationSummary* locations =
      new (allocator) LocationSummary(instruction, LocationSummary::kCallOnMainOnly);
  locations->SetOut(out);
  instruction->GetLocations()->SetInAt(instruction->FormatIndex(),
                                       Location::ConstantLocation(instruction->GetFormat()));

  uint32_t format = static_cast<uint32_t>(instruction->GetFormat()->GetValue());
  uint32_t f = format;
  PointerSize pointer_size = InstructionSetPointerSize(GetInstructionSet());
  size_t stack_offset = static_cast<size_t>(pointer_size);  // Start after the ArtMethod*.
  for (size_t i = 0, num_args = instruction->GetNumberOfArguments(); i != num_args; ++i) {
    StringBuilderAppend::Argument arg_type =
        static_cast<StringBuilderAppend::Argument>(f & StringBuilderAppend::kArgMask);
    switch (arg_type) {
      case StringBuilderAppend::Argument::kStringBuilder:
      case StringBuilderAppend::Argument::kString:
      case StringBuilderAppend::Argument::kCharArray:
        static_assert(sizeof(StackReference<mirror::Object>) == sizeof(uint32_t), "Size check.");
        FALLTHROUGH_INTENDED;
      case StringBuilderAppend::Argument::kBoolean:
      case StringBuilderAppend::Argument::kChar:
      case StringBuilderAppend::Argument::kInt:
      case StringBuilderAppend::Argument::kFloat:
        locations->SetInAt(i, Location::StackSlot(stack_offset));
        break;
      case StringBuilderAppend::Argument::kLong:
      case StringBuilderAppend::Argument::kDouble:
        stack_offset = RoundUp(stack_offset, sizeof(uint64_t));
        locations->SetInAt(i, Location::DoubleStackSlot(stack_offset));
        // Skip the low word, let the common code skip the high word.
        stack_offset += sizeof(uint32_t);
        break;
      default:
        LOG(FATAL) << "Unexpected arg format: 0x" << std::hex
            << (f & StringBuilderAppend::kArgMask) << " full format: 0x" << format;
        UNREACHABLE();
    }
    f >>= StringBuilderAppend::kBitsPerArg;
    stack_offset += sizeof(uint32_t);
  }
  DCHECK_EQ(f, 0u);

  size_t param_size = stack_offset - static_cast<size_t>(pointer_size);
  DCHECK_ALIGNED(param_size, kVRegSize);
  size_t num_vregs = param_size / kVRegSize;
  graph_->UpdateMaximumNumberOfOutVRegs(num_vregs);
}

void CodeGenerator::CreateUnresolvedFieldLocationSummary(
    HInstruction* field_access,
    DataType::Type field_type,
    const FieldAccessCallingConvention& calling_convention) {
  bool is_instance = field_access->IsUnresolvedInstanceFieldGet()
      || field_access->IsUnresolvedInstanceFieldSet();
  bool is_get = field_access->IsUnresolvedInstanceFieldGet()
      || field_access->IsUnresolvedStaticFieldGet();

  ArenaAllocator* allocator = field_access->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations =
      new (allocator) LocationSummary(field_access, LocationSummary::kCallOnMainOnly);

  locations->AddTemp(calling_convention.GetFieldIndexLocation());

  if (is_instance) {
    // Add the `this` object for instance field accesses.
    locations->SetInAt(0, calling_convention.GetObjectLocation());
  }

  // Note that pSetXXStatic/pGetXXStatic always takes/returns an int or int64
  // regardless of the the type. Because of that we forced to special case
  // the access to floating point values.
  if (is_get) {
    if (DataType::IsFloatingPointType(field_type)) {
      // The return value will be stored in regular registers while register
      // allocator expects it in a floating point register.
      // Note We don't need to request additional temps because the return
      // register(s) are already blocked due the call and they may overlap with
      // the input or field index.
      // The transfer between the two will be done at codegen level.
      locations->SetOut(calling_convention.GetFpuLocation(field_type));
    } else {
      locations->SetOut(calling_convention.GetReturnLocation(field_type));
    }
  } else {
     size_t set_index = is_instance ? 1 : 0;
     if (DataType::IsFloatingPointType(field_type)) {
      // The set value comes from a float location while the calling convention
      // expects it in a regular register location. Allocate a temp for it and
      // make the transfer at codegen.
      AddLocationAsTemp(calling_convention.GetSetValueLocation(field_type, is_instance), locations);
      locations->SetInAt(set_index, calling_convention.GetFpuLocation(field_type));
    } else {
      locations->SetInAt(set_index,
          calling_convention.GetSetValueLocation(field_type, is_instance));
    }
  }
}

void CodeGenerator::GenerateUnresolvedFieldAccess(
    HInstruction* field_access,
    DataType::Type field_type,
    uint32_t field_index,
    uint32_t dex_pc,
    const FieldAccessCallingConvention& calling_convention) {
  LocationSummary* locations = field_access->GetLocations();

  MoveConstant(locations->GetTemp(0), field_index);

  bool is_instance = field_access->IsUnresolvedInstanceFieldGet()
      || field_access->IsUnresolvedInstanceFieldSet();
  bool is_get = field_access->IsUnresolvedInstanceFieldGet()
      || field_access->IsUnresolvedStaticFieldGet();

  if (!is_get && DataType::IsFloatingPointType(field_type)) {
    // Copy the float value to be set into the calling convention register.
    // Note that using directly the temp location is problematic as we don't
    // support temp register pairs. To avoid boilerplate conversion code, use
    // the location from the calling convention.
    MoveLocation(calling_convention.GetSetValueLocation(field_type, is_instance),
                 locations->InAt(is_instance ? 1 : 0),
                 (DataType::Is64BitType(field_type) ? DataType::Type::kInt64
                                                    : DataType::Type::kInt32));
  }

  QuickEntrypointEnum entrypoint = kQuickSet8Static;  // Initialize to anything to avoid warnings.
  switch (field_type) {
    case DataType::Type::kBool:
      entrypoint = is_instance
          ? (is_get ? kQuickGetBooleanInstance : kQuickSet8Instance)
          : (is_get ? kQuickGetBooleanStatic : kQuickSet8Static);
      break;
    case DataType::Type::kInt8:
      entrypoint = is_instance
          ? (is_get ? kQuickGetByteInstance : kQuickSet8Instance)
          : (is_get ? kQuickGetByteStatic : kQuickSet8Static);
      break;
    case DataType::Type::kInt16:
      entrypoint = is_instance
          ? (is_get ? kQuickGetShortInstance : kQuickSet16Instance)
          : (is_get ? kQuickGetShortStatic : kQuickSet16Static);
      break;
    case DataType::Type::kUint16:
      entrypoint = is_instance
          ? (is_get ? kQuickGetCharInstance : kQuickSet16Instance)
          : (is_get ? kQuickGetCharStatic : kQuickSet16Static);
      break;
    case DataType::Type::kInt32:
    case DataType::Type::kFloat32:
      entrypoint = is_instance
          ? (is_get ? kQuickGet32Instance : kQuickSet32Instance)
          : (is_get ? kQuickGet32Static : kQuickSet32Static);
      break;
    case DataType::Type::kReference:
      entrypoint = is_instance
          ? (is_get ? kQuickGetObjInstance : kQuickSetObjInstance)
          : (is_get ? kQuickGetObjStatic : kQuickSetObjStatic);
      break;
    case DataType::Type::kInt64:
    case DataType::Type::kFloat64:
      entrypoint = is_instance
          ? (is_get ? kQuickGet64Instance : kQuickSet64Instance)
          : (is_get ? kQuickGet64Static : kQuickSet64Static);
      break;
    default:
      LOG(FATAL) << "Invalid type " << field_type;
  }
  InvokeRuntime(entrypoint, field_access, dex_pc, nullptr);

  if (is_get && DataType::IsFloatingPointType(field_type)) {
    MoveLocation(locations->Out(), calling_convention.GetReturnLocation(field_type), field_type);
  }
}

void CodeGenerator::CreateLoadClassRuntimeCallLocationSummary(HLoadClass* cls,
                                                              Location runtime_type_index_location,
                                                              Location runtime_return_location) {
  DCHECK_EQ(cls->GetLoadKind(), HLoadClass::LoadKind::kRuntimeCall);
  DCHECK_EQ(cls->InputCount(), 1u);
  LocationSummary* locations = new (cls->GetBlock()->GetGraph()->GetAllocator()) LocationSummary(
      cls, LocationSummary::kCallOnMainOnly);
  locations->SetInAt(0, Location::NoLocation());
  locations->AddTemp(runtime_type_index_location);
  locations->SetOut(runtime_return_location);
}

void CodeGenerator::GenerateLoadClassRuntimeCall(HLoadClass* cls) {
  DCHECK_EQ(cls->GetLoadKind(), HLoadClass::LoadKind::kRuntimeCall);
  DCHECK(!cls->MustGenerateClinitCheck());
  LocationSummary* locations = cls->GetLocations();
  MoveConstant(locations->GetTemp(0), cls->GetTypeIndex().index_);
  if (cls->NeedsAccessCheck()) {
    CheckEntrypointTypes<kQuickResolveTypeAndVerifyAccess, void*, uint32_t>();
    InvokeRuntime(kQuickResolveTypeAndVerifyAccess, cls, cls->GetDexPc());
  } else {
    CheckEntrypointTypes<kQuickResolveType, void*, uint32_t>();
    InvokeRuntime(kQuickResolveType, cls, cls->GetDexPc());
  }
}

void CodeGenerator::CreateLoadMethodHandleRuntimeCallLocationSummary(
    HLoadMethodHandle* method_handle,
    Location runtime_proto_index_location,
    Location runtime_return_location) {
  DCHECK_EQ(method_handle->InputCount(), 1u);
  LocationSummary* locations =
      new (method_handle->GetBlock()->GetGraph()->GetAllocator()) LocationSummary(
          method_handle, LocationSummary::kCallOnMainOnly);
  locations->SetInAt(0, Location::NoLocation());
  locations->AddTemp(runtime_proto_index_location);
  locations->SetOut(runtime_return_location);
}

void CodeGenerator::GenerateLoadMethodHandleRuntimeCall(HLoadMethodHandle* method_handle) {
  LocationSummary* locations = method_handle->GetLocations();
  MoveConstant(locations->GetTemp(0), method_handle->GetMethodHandleIndex());
  CheckEntrypointTypes<kQuickResolveMethodHandle, void*, uint32_t>();
  InvokeRuntime(kQuickResolveMethodHandle, method_handle, method_handle->GetDexPc());
}

void CodeGenerator::CreateLoadMethodTypeRuntimeCallLocationSummary(
    HLoadMethodType* method_type,
    Location runtime_proto_index_location,
    Location runtime_return_location) {
  DCHECK_EQ(method_type->InputCount(), 1u);
  LocationSummary* locations =
      new (method_type->GetBlock()->GetGraph()->GetAllocator()) LocationSummary(
          method_type, LocationSummary::kCallOnMainOnly);
  locations->SetInAt(0, Location::NoLocation());
  locations->AddTemp(runtime_proto_index_location);
  locations->SetOut(runtime_return_location);
}

void CodeGenerator::GenerateLoadMethodTypeRuntimeCall(HLoadMethodType* method_type) {
  LocationSummary* locations = method_type->GetLocations();
  MoveConstant(locations->GetTemp(0), method_type->GetProtoIndex().index_);
  CheckEntrypointTypes<kQuickResolveMethodType, void*, uint32_t>();
  InvokeRuntime(kQuickResolveMethodType, method_type, method_type->GetDexPc());
}

static uint32_t GetBootImageOffsetImpl(const void* object, ImageHeader::ImageSections section) {
  Runtime* runtime = Runtime::Current();
  DCHECK(runtime->IsAotCompiler());
  const std::vector<gc::space::ImageSpace*>& boot_image_spaces =
      runtime->GetHeap()->GetBootImageSpaces();
  // Check that the `object` is in the expected section of one of the boot image files.
  DCHECK(std::any_of(boot_image_spaces.begin(),
                     boot_image_spaces.end(),
                     [object, section](gc::space::ImageSpace* space) {
                       uintptr_t begin = reinterpret_cast<uintptr_t>(space->Begin());
                       uintptr_t offset = reinterpret_cast<uintptr_t>(object) - begin;
                       return space->GetImageHeader().GetImageSection(section).Contains(offset);
                     }));
  uintptr_t begin = reinterpret_cast<uintptr_t>(boot_image_spaces.front()->Begin());
  uintptr_t offset = reinterpret_cast<uintptr_t>(object) - begin;
  return dchecked_integral_cast<uint32_t>(offset);
}

// NO_THREAD_SAFETY_ANALYSIS: Avoid taking the mutator lock, boot image classes are non-moveable.
uint32_t CodeGenerator::GetBootImageOffset(HLoadClass* load_class) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK_EQ(load_class->GetLoadKind(), HLoadClass::LoadKind::kBootImageRelRo);
  ObjPtr<mirror::Class> klass = load_class->GetClass().Get();
  DCHECK(klass != nullptr);
  return GetBootImageOffsetImpl(klass.Ptr(), ImageHeader::kSectionObjects);
}

// NO_THREAD_SAFETY_ANALYSIS: Avoid taking the mutator lock, boot image strings are non-moveable.
uint32_t CodeGenerator::GetBootImageOffset(HLoadString* load_string) NO_THREAD_SAFETY_ANALYSIS {
  DCHECK_EQ(load_string->GetLoadKind(), HLoadString::LoadKind::kBootImageRelRo);
  ObjPtr<mirror::String> string = load_string->GetString().Get();
  DCHECK(string != nullptr);
  return GetBootImageOffsetImpl(string.Ptr(), ImageHeader::kSectionObjects);
}

uint32_t CodeGenerator::GetBootImageOffset(HInvokeStaticOrDirect* invoke) {
  DCHECK_EQ(invoke->GetMethodLoadKind(), HInvokeStaticOrDirect::MethodLoadKind::kBootImageRelRo);
  ArtMethod* method = invoke->GetResolvedMethod();
  DCHECK(method != nullptr);
  return GetBootImageOffsetImpl(method, ImageHeader::kSectionArtMethods);
}

void CodeGenerator::BlockIfInRegister(Location location, bool is_out) const {
  // The DCHECKS below check that a register is not specified twice in
  // the summary. The out location can overlap with an input, so we need
  // to special case it.
  if (location.IsRegister()) {
    DCHECK(is_out || !blocked_core_registers_[location.reg()]);
    blocked_core_registers_[location.reg()] = true;
  } else if (location.IsFpuRegister()) {
    DCHECK(is_out || !blocked_fpu_registers_[location.reg()]);
    blocked_fpu_registers_[location.reg()] = true;
  } else if (location.IsFpuRegisterPair()) {
    DCHECK(is_out || !blocked_fpu_registers_[location.AsFpuRegisterPairLow<int>()]);
    blocked_fpu_registers_[location.AsFpuRegisterPairLow<int>()] = true;
    DCHECK(is_out || !blocked_fpu_registers_[location.AsFpuRegisterPairHigh<int>()]);
    blocked_fpu_registers_[location.AsFpuRegisterPairHigh<int>()] = true;
  } else if (location.IsRegisterPair()) {
    DCHECK(is_out || !blocked_core_registers_[location.AsRegisterPairLow<int>()]);
    blocked_core_registers_[location.AsRegisterPairLow<int>()] = true;
    DCHECK(is_out || !blocked_core_registers_[location.AsRegisterPairHigh<int>()]);
    blocked_core_registers_[location.AsRegisterPairHigh<int>()] = true;
  }
}

void CodeGenerator::AllocateLocations(HInstruction* instruction) {
  for (HEnvironment* env = instruction->GetEnvironment(); env != nullptr; env = env->GetParent()) {
    env->AllocateLocations();
  }
  instruction->Accept(GetLocationBuilder());
  DCHECK(CheckTypeConsistency(instruction));
  LocationSummary* locations = instruction->GetLocations();
  if (!instruction->IsSuspendCheckEntry()) {
    if (locations != nullptr) {
      if (locations->CanCall()) {
        MarkNotLeaf();
      } else if (locations->Intrinsified() &&
                 instruction->IsInvokeStaticOrDirect() &&
                 !instruction->AsInvokeStaticOrDirect()->HasCurrentMethodInput()) {
        // A static method call that has been fully intrinsified, and cannot call on the slow
        // path or refer to the current method directly, no longer needs current method.
        return;
      }
    }
    if (instruction->NeedsCurrentMethod()) {
      SetRequiresCurrentMethod();
    }
  }
}

std::unique_ptr<CodeGenerator> CodeGenerator::Create(HGraph* graph,
                                                     const CompilerOptions& compiler_options,
                                                     OptimizingCompilerStats* stats) {
  ArenaAllocator* allocator = graph->GetAllocator();
  switch (compiler_options.GetInstructionSet()) {
#ifdef ART_ENABLE_CODEGEN_arm
    case InstructionSet::kArm:
    case InstructionSet::kThumb2: {
      return std::unique_ptr<CodeGenerator>(
          new (allocator) arm::CodeGeneratorARMVIXL(graph, compiler_options, stats));
    }
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64: {
      return std::unique_ptr<CodeGenerator>(
          new (allocator) arm64::CodeGeneratorARM64(graph, compiler_options, stats));
    }
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case InstructionSet::kX86: {
      return std::unique_ptr<CodeGenerator>(
          new (allocator) x86::CodeGeneratorX86(graph, compiler_options, stats));
    }
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
    case InstructionSet::kX86_64: {
      return std::unique_ptr<CodeGenerator>(
          new (allocator) x86_64::CodeGeneratorX86_64(graph, compiler_options, stats));
    }
#endif
    default:
      return nullptr;
  }
}

CodeGenerator::CodeGenerator(HGraph* graph,
                             size_t number_of_core_registers,
                             size_t number_of_fpu_registers,
                             size_t number_of_register_pairs,
                             uint32_t core_callee_save_mask,
                             uint32_t fpu_callee_save_mask,
                             const CompilerOptions& compiler_options,
                             OptimizingCompilerStats* stats)
    : frame_size_(0),
      core_spill_mask_(0),
      fpu_spill_mask_(0),
      first_register_slot_in_slow_path_(0),
      allocated_registers_(RegisterSet::Empty()),
      blocked_core_registers_(graph->GetAllocator()->AllocArray<bool>(number_of_core_registers,
                                                                      kArenaAllocCodeGenerator)),
      blocked_fpu_registers_(graph->GetAllocator()->AllocArray<bool>(number_of_fpu_registers,
                                                                     kArenaAllocCodeGenerator)),
      number_of_core_registers_(number_of_core_registers),
      number_of_fpu_registers_(number_of_fpu_registers),
      number_of_register_pairs_(number_of_register_pairs),
      core_callee_save_mask_(core_callee_save_mask),
      fpu_callee_save_mask_(fpu_callee_save_mask),
      block_order_(nullptr),
      disasm_info_(nullptr),
      stats_(stats),
      graph_(graph),
      compiler_options_(compiler_options),
      current_slow_path_(nullptr),
      current_block_index_(0),
      is_leaf_(true),
      requires_current_method_(false),
      code_generation_data_() {
  if (GetGraph()->IsCompilingOsr()) {
    // Make OSR methods have all registers spilled, this simplifies the logic of
    // jumping to the compiled code directly.
    for (size_t i = 0; i < number_of_core_registers_; ++i) {
      if (IsCoreCalleeSaveRegister(i)) {
        AddAllocatedRegister(Location::RegisterLocation(i));
      }
    }
    for (size_t i = 0; i < number_of_fpu_registers_; ++i) {
      if (IsFloatingPointCalleeSaveRegister(i)) {
        AddAllocatedRegister(Location::FpuRegisterLocation(i));
      }
    }
  }
}

CodeGenerator::~CodeGenerator() {}

size_t CodeGenerator::GetNumberOfJitRoots() const {
  DCHECK(code_generation_data_ != nullptr);
  return code_generation_data_->GetNumberOfJitRoots();
}

static void CheckCovers(uint32_t dex_pc,
                        const HGraph& graph,
                        const CodeInfo& code_info,
                        const ArenaVector<HSuspendCheck*>& loop_headers,
                        ArenaVector<size_t>* covered) {
  for (size_t i = 0; i < loop_headers.size(); ++i) {
    if (loop_headers[i]->GetDexPc() == dex_pc) {
      if (graph.IsCompilingOsr()) {
        DCHECK(code_info.GetOsrStackMapForDexPc(dex_pc).IsValid());
      }
      ++(*covered)[i];
    }
  }
}

// Debug helper to ensure loop entries in compiled code are matched by
// dex branch instructions.
static void CheckLoopEntriesCanBeUsedForOsr(const HGraph& graph,
                                            const CodeInfo& code_info,
                                            const dex::CodeItem& code_item) {
  if (graph.HasTryCatch()) {
    // One can write loops through try/catch, which we do not support for OSR anyway.
    return;
  }
  ArenaVector<HSuspendCheck*> loop_headers(graph.GetAllocator()->Adapter(kArenaAllocMisc));
  for (HBasicBlock* block : graph.GetReversePostOrder()) {
    if (block->IsLoopHeader()) {
      HSuspendCheck* suspend_check = block->GetLoopInformation()->GetSuspendCheck();
      if (!suspend_check->GetEnvironment()->IsFromInlinedInvoke()) {
        loop_headers.push_back(suspend_check);
      }
    }
  }
  ArenaVector<size_t> covered(
      loop_headers.size(), 0, graph.GetAllocator()->Adapter(kArenaAllocMisc));
  for (const DexInstructionPcPair& pair : CodeItemInstructionAccessor(graph.GetDexFile(),
                                                                      &code_item)) {
    const uint32_t dex_pc = pair.DexPc();
    const Instruction& instruction = pair.Inst();
    if (instruction.IsBranch()) {
      uint32_t target = dex_pc + instruction.GetTargetOffset();
      CheckCovers(target, graph, code_info, loop_headers, &covered);
    } else if (instruction.IsSwitch()) {
      DexSwitchTable table(instruction, dex_pc);
      uint16_t num_entries = table.GetNumEntries();
      size_t offset = table.GetFirstValueIndex();

      // Use a larger loop counter type to avoid overflow issues.
      for (size_t i = 0; i < num_entries; ++i) {
        // The target of the case.
        uint32_t target = dex_pc + table.GetEntryAt(i + offset);
        CheckCovers(target, graph, code_info, loop_headers, &covered);
      }
    }
  }

  for (size_t i = 0; i < covered.size(); ++i) {
    DCHECK_NE(covered[i], 0u) << "Loop in compiled code has no dex branch equivalent";
  }
}

ScopedArenaVector<uint8_t> CodeGenerator::BuildStackMaps(const dex::CodeItem* code_item) {
  ScopedArenaVector<uint8_t> stack_map = GetStackMapStream()->Encode();
  if (kIsDebugBuild && code_item != nullptr) {
    CheckLoopEntriesCanBeUsedForOsr(*graph_, CodeInfo(stack_map.data()), *code_item);
  }
  return stack_map;
}

// Returns whether stackmap dex register info is needed for the instruction.
//
// The following cases mandate having a dex register map:
//  * Deoptimization
//    when we need to obtain the values to restore actual vregisters for interpreter.
//  * Debuggability
//    when we want to observe the values / asynchronously deoptimize.
//  * Monitor operations
//    to allow dumping in a stack trace locked dex registers for non-debuggable code.
//  * On-stack-replacement (OSR)
//    when entering compiled for OSR code from the interpreter we need to initialize the compiled
//    code values with the values from the vregisters.
//  * Method local catch blocks
//    a catch block must see the environment of the instruction from the same method that can
//    throw to this block.
static bool NeedsVregInfo(HInstruction* instruction, bool osr) {
  HGraph* graph = instruction->GetBlock()->GetGraph();
  return instruction->IsDeoptimize() ||
         graph->IsDebuggable() ||
         graph->HasMonitorOperations() ||
         osr ||
         instruction->CanThrowIntoCatchBlock();
}

void CodeGenerator::RecordPcInfo(HInstruction* instruction,
                                 uint32_t dex_pc,
                                 SlowPathCode* slow_path,
                                 bool native_debug_info) {
  RecordPcInfo(instruction, dex_pc, GetAssembler()->CodePosition(), slow_path, native_debug_info);
}

void CodeGenerator::RecordPcInfo(HInstruction* instruction,
                                 uint32_t dex_pc,
                                 uint32_t native_pc,
                                 SlowPathCode* slow_path,
                                 bool native_debug_info) {
  if (instruction != nullptr) {
    // The code generated for some type conversions
    // may call the runtime, thus normally requiring a subsequent
    // call to this method. However, the method verifier does not
    // produce PC information for certain instructions, which are
    // considered "atomic" (they cannot join a GC).
    // Therefore we do not currently record PC information for such
    // instructions.  As this may change later, we added this special
    // case so that code generators may nevertheless call
    // CodeGenerator::RecordPcInfo without triggering an error in
    // CodeGenerator::BuildNativeGCMap ("Missing ref for dex pc 0x")
    // thereafter.
    if (instruction->IsTypeConversion()) {
      return;
    }
    if (instruction->IsRem()) {
      DataType::Type type = instruction->AsRem()->GetResultType();
      if ((type == DataType::Type::kFloat32) || (type == DataType::Type::kFloat64)) {
        return;
      }
    }
  }

  StackMapStream* stack_map_stream = GetStackMapStream();
  if (instruction == nullptr) {
    // For stack overflow checks and native-debug-info entries without dex register
    // mapping (i.e. start of basic block or start of slow path).
    stack_map_stream->BeginStackMapEntry(dex_pc, native_pc);
    stack_map_stream->EndStackMapEntry();
    return;
  }

  LocationSummary* locations = instruction->GetLocations();
  uint32_t register_mask = locations->GetRegisterMask();
  DCHECK_EQ(register_mask & ~locations->GetLiveRegisters()->GetCoreRegisters(), 0u);
  if (locations->OnlyCallsOnSlowPath()) {
    // In case of slow path, we currently set the location of caller-save registers
    // to register (instead of their stack location when pushed before the slow-path
    // call). Therefore register_mask contains both callee-save and caller-save
    // registers that hold objects. We must remove the spilled caller-save from the
    // mask, since they will be overwritten by the callee.
    uint32_t spills = GetSlowPathSpills(locations, /* core_registers= */ true);
    register_mask &= ~spills;
  } else {
    // The register mask must be a subset of callee-save registers.
    DCHECK_EQ(register_mask & core_callee_save_mask_, register_mask);
  }

  uint32_t outer_dex_pc = dex_pc;
  uint32_t outer_environment_size = 0u;
  uint32_t inlining_depth = 0;
  HEnvironment* const environment = instruction->GetEnvironment();
  if (environment != nullptr) {
    HEnvironment* outer_environment = environment;
    while (outer_environment->GetParent() != nullptr) {
      outer_environment = outer_environment->GetParent();
      ++inlining_depth;
    }
    outer_dex_pc = outer_environment->GetDexPc();
    outer_environment_size = outer_environment->Size();
  }

  HLoopInformation* info = instruction->GetBlock()->GetLoopInformation();
  bool osr =
      instruction->IsSuspendCheck() &&
      (info != nullptr) &&
      graph_->IsCompilingOsr() &&
      (inlining_depth == 0);
  StackMap::Kind kind = native_debug_info
      ? StackMap::Kind::Debug
      : (osr ? StackMap::Kind::OSR : StackMap::Kind::Default);
  bool needs_vreg_info = NeedsVregInfo(instruction, osr);
  stack_map_stream->BeginStackMapEntry(outer_dex_pc,
                                       native_pc,
                                       register_mask,
                                       locations->GetStackMask(),
                                       kind,
                                       needs_vreg_info);

  EmitEnvironment(environment, slow_path, needs_vreg_info);
  stack_map_stream->EndStackMapEntry();

  if (osr) {
    DCHECK_EQ(info->GetSuspendCheck(), instruction);
    DCHECK(info->IsIrreducible());
    DCHECK(environment != nullptr);
    if (kIsDebugBuild) {
      for (size_t i = 0, environment_size = environment->Size(); i < environment_size; ++i) {
        HInstruction* in_environment = environment->GetInstructionAt(i);
        if (in_environment != nullptr) {
          DCHECK(in_environment->IsPhi() || in_environment->IsConstant());
          Location location = environment->GetLocationAt(i);
          DCHECK(location.IsStackSlot() ||
                 location.IsDoubleStackSlot() ||
                 location.IsConstant() ||
                 location.IsInvalid());
          if (location.IsStackSlot() || location.IsDoubleStackSlot()) {
            DCHECK_LT(location.GetStackIndex(), static_cast<int32_t>(GetFrameSize()));
          }
        }
      }
    }
  }
}

bool CodeGenerator::HasStackMapAtCurrentPc() {
  uint32_t pc = GetAssembler()->CodeSize();
  StackMapStream* stack_map_stream = GetStackMapStream();
  size_t count = stack_map_stream->GetNumberOfStackMaps();
  if (count == 0) {
    return false;
  }
  return stack_map_stream->GetStackMapNativePcOffset(count - 1) == pc;
}

void CodeGenerator::MaybeRecordNativeDebugInfo(HInstruction* instruction,
                                               uint32_t dex_pc,
                                               SlowPathCode* slow_path) {
  if (GetCompilerOptions().GetNativeDebuggable() && dex_pc != kNoDexPc) {
    if (HasStackMapAtCurrentPc()) {
      // Ensure that we do not collide with the stack map of the previous instruction.
      GenerateNop();
    }
    RecordPcInfo(instruction, dex_pc, slow_path, /* native_debug_info= */ true);
  }
}

void CodeGenerator::RecordCatchBlockInfo() {
  StackMapStream* stack_map_stream = GetStackMapStream();

  for (HBasicBlock* block : *block_order_) {
    if (!block->IsCatchBlock()) {
      continue;
    }

    uint32_t dex_pc = block->GetDexPc();
    uint32_t num_vregs = graph_->GetNumberOfVRegs();
    uint32_t native_pc = GetAddressOf(block);

    stack_map_stream->BeginStackMapEntry(dex_pc,
                                         native_pc,
                                         /* register_mask= */ 0,
                                         /* sp_mask= */ nullptr,
                                         StackMap::Kind::Catch);

    HInstruction* current_phi = block->GetFirstPhi();
    for (size_t vreg = 0; vreg < num_vregs; ++vreg) {
      while (current_phi != nullptr && current_phi->AsPhi()->GetRegNumber() < vreg) {
        HInstruction* next_phi = current_phi->GetNext();
        DCHECK(next_phi == nullptr ||
               current_phi->AsPhi()->GetRegNumber() <= next_phi->AsPhi()->GetRegNumber())
            << "Phis need to be sorted by vreg number to keep this a linear-time loop.";
        current_phi = next_phi;
      }

      if (current_phi == nullptr || current_phi->AsPhi()->GetRegNumber() != vreg) {
        stack_map_stream->AddDexRegisterEntry(DexRegisterLocation::Kind::kNone, 0);
      } else {
        Location location = current_phi->GetLocations()->Out();
        switch (location.GetKind()) {
          case Location::kStackSlot: {
            stack_map_stream->AddDexRegisterEntry(
                DexRegisterLocation::Kind::kInStack, location.GetStackIndex());
            break;
          }
          case Location::kDoubleStackSlot: {
            stack_map_stream->AddDexRegisterEntry(
                DexRegisterLocation::Kind::kInStack, location.GetStackIndex());
            stack_map_stream->AddDexRegisterEntry(
                DexRegisterLocation::Kind::kInStack, location.GetHighStackIndex(kVRegSize));
            ++vreg;
            DCHECK_LT(vreg, num_vregs);
            break;
          }
          default: {
            // All catch phis must be allocated to a stack slot.
            LOG(FATAL) << "Unexpected kind " << location.GetKind();
            UNREACHABLE();
          }
        }
      }
    }

    stack_map_stream->EndStackMapEntry();
  }
}

void CodeGenerator::AddSlowPath(SlowPathCode* slow_path) {
  DCHECK(code_generation_data_ != nullptr);
  code_generation_data_->AddSlowPath(slow_path);
}

void CodeGenerator::EmitVRegInfo(HEnvironment* environment, SlowPathCode* slow_path) {
  StackMapStream* stack_map_stream = GetStackMapStream();
  // Walk over the environment, and record the location of dex registers.
  for (size_t i = 0, environment_size = environment->Size(); i < environment_size; ++i) {
    HInstruction* current = environment->GetInstructionAt(i);
    if (current == nullptr) {
      stack_map_stream->AddDexRegisterEntry(DexRegisterLocation::Kind::kNone, 0);
      continue;
    }

    using Kind = DexRegisterLocation::Kind;
    Location location = environment->GetLocationAt(i);
    switch (location.GetKind()) {
      case Location::kConstant: {
        DCHECK_EQ(current, location.GetConstant());
        if (current->IsLongConstant()) {
          int64_t value = current->AsLongConstant()->GetValue();
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, Low32Bits(value));
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, High32Bits(value));
          ++i;
          DCHECK_LT(i, environment_size);
        } else if (current->IsDoubleConstant()) {
          int64_t value = bit_cast<int64_t, double>(current->AsDoubleConstant()->GetValue());
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, Low32Bits(value));
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, High32Bits(value));
          ++i;
          DCHECK_LT(i, environment_size);
        } else if (current->IsIntConstant()) {
          int32_t value = current->AsIntConstant()->GetValue();
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, value);
        } else if (current->IsNullConstant()) {
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, 0);
        } else {
          DCHECK(current->IsFloatConstant()) << current->DebugName();
          int32_t value = bit_cast<int32_t, float>(current->AsFloatConstant()->GetValue());
          stack_map_stream->AddDexRegisterEntry(Kind::kConstant, value);
        }
        break;
      }

      case Location::kStackSlot: {
        stack_map_stream->AddDexRegisterEntry(Kind::kInStack, location.GetStackIndex());
        break;
      }

      case Location::kDoubleStackSlot: {
        stack_map_stream->AddDexRegisterEntry(Kind::kInStack, location.GetStackIndex());
        stack_map_stream->AddDexRegisterEntry(
            Kind::kInStack, location.GetHighStackIndex(kVRegSize));
        ++i;
        DCHECK_LT(i, environment_size);
        break;
      }

      case Location::kRegister : {
        int id = location.reg();
        if (slow_path != nullptr && slow_path->IsCoreRegisterSaved(id)) {
          uint32_t offset = slow_path->GetStackOffsetOfCoreRegister(id);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
          if (current->GetType() == DataType::Type::kInt64) {
            stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset + kVRegSize);
            ++i;
            DCHECK_LT(i, environment_size);
          }
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInRegister, id);
          if (current->GetType() == DataType::Type::kInt64) {
            stack_map_stream->AddDexRegisterEntry(Kind::kInRegisterHigh, id);
            ++i;
            DCHECK_LT(i, environment_size);
          }
        }
        break;
      }

      case Location::kFpuRegister : {
        int id = location.reg();
        if (slow_path != nullptr && slow_path->IsFpuRegisterSaved(id)) {
          uint32_t offset = slow_path->GetStackOffsetOfFpuRegister(id);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
          if (current->GetType() == DataType::Type::kFloat64) {
            stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset + kVRegSize);
            ++i;
            DCHECK_LT(i, environment_size);
          }
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInFpuRegister, id);
          if (current->GetType() == DataType::Type::kFloat64) {
            stack_map_stream->AddDexRegisterEntry(Kind::kInFpuRegisterHigh, id);
            ++i;
            DCHECK_LT(i, environment_size);
          }
        }
        break;
      }

      case Location::kFpuRegisterPair : {
        int low = location.low();
        int high = location.high();
        if (slow_path != nullptr && slow_path->IsFpuRegisterSaved(low)) {
          uint32_t offset = slow_path->GetStackOffsetOfFpuRegister(low);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInFpuRegister, low);
        }
        if (slow_path != nullptr && slow_path->IsFpuRegisterSaved(high)) {
          uint32_t offset = slow_path->GetStackOffsetOfFpuRegister(high);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
          ++i;
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInFpuRegister, high);
          ++i;
        }
        DCHECK_LT(i, environment_size);
        break;
      }

      case Location::kRegisterPair : {
        int low = location.low();
        int high = location.high();
        if (slow_path != nullptr && slow_path->IsCoreRegisterSaved(low)) {
          uint32_t offset = slow_path->GetStackOffsetOfCoreRegister(low);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInRegister, low);
        }
        if (slow_path != nullptr && slow_path->IsCoreRegisterSaved(high)) {
          uint32_t offset = slow_path->GetStackOffsetOfCoreRegister(high);
          stack_map_stream->AddDexRegisterEntry(Kind::kInStack, offset);
        } else {
          stack_map_stream->AddDexRegisterEntry(Kind::kInRegister, high);
        }
        ++i;
        DCHECK_LT(i, environment_size);
        break;
      }

      case Location::kInvalid: {
        stack_map_stream->AddDexRegisterEntry(Kind::kNone, 0);
        break;
      }

      default:
        LOG(FATAL) << "Unexpected kind " << location.GetKind();
    }
  }
}

void CodeGenerator::EmitEnvironment(HEnvironment* environment,
                                    SlowPathCode* slow_path,
                                    bool needs_vreg_info) {
  if (environment == nullptr) return;

  StackMapStream* stack_map_stream = GetStackMapStream();
  bool emit_inline_info = environment->GetParent() != nullptr;

  if (emit_inline_info) {
    // We emit the parent environment first.
    EmitEnvironment(environment->GetParent(), slow_path, needs_vreg_info);
    stack_map_stream->BeginInlineInfoEntry(environment->GetMethod(),
                                           environment->GetDexPc(),
                                           needs_vreg_info ? environment->Size() : 0,
                                           &graph_->GetDexFile());
  }

  if (needs_vreg_info) {
    // If a dex register map is not required we just won't emit it.
    EmitVRegInfo(environment, slow_path);
  }

  if (emit_inline_info) {
    stack_map_stream->EndInlineInfoEntry();
  }
}

bool CodeGenerator::CanMoveNullCheckToUser(HNullCheck* null_check) {
  return null_check->IsEmittedAtUseSite();
}

void CodeGenerator::MaybeRecordImplicitNullCheck(HInstruction* instr) {
  HNullCheck* null_check = instr->GetImplicitNullCheck();
  if (null_check != nullptr) {
    RecordPcInfo(null_check, null_check->GetDexPc(), GetAssembler()->CodePosition());
  }
}

LocationSummary* CodeGenerator::CreateThrowingSlowPathLocations(HInstruction* instruction,
                                                                RegisterSet caller_saves) {
  // Note: Using kNoCall allows the method to be treated as leaf (and eliminate the
  // HSuspendCheck from entry block). However, it will still get a valid stack frame
  // because the HNullCheck needs an environment.
  LocationSummary::CallKind call_kind = LocationSummary::kNoCall;
  // When throwing from a try block, we may need to retrieve dalvik registers from
  // physical registers and we also need to set up stack mask for GC. This is
  // implicitly achieved by passing kCallOnSlowPath to the LocationSummary.
  bool can_throw_into_catch_block = instruction->CanThrowIntoCatchBlock();
  if (can_throw_into_catch_block) {
    call_kind = LocationSummary::kCallOnSlowPath;
  }
  LocationSummary* locations =
      new (GetGraph()->GetAllocator()) LocationSummary(instruction, call_kind);
  if (can_throw_into_catch_block && compiler_options_.GetImplicitNullChecks()) {
    locations->SetCustomSlowPathCallerSaves(caller_saves);  // Default: no caller-save registers.
  }
  DCHECK(!instruction->HasUses());
  return locations;
}

void CodeGenerator::GenerateNullCheck(HNullCheck* instruction) {
  if (compiler_options_.GetImplicitNullChecks()) {
    MaybeRecordStat(stats_, MethodCompilationStat::kImplicitNullCheckGenerated);
    GenerateImplicitNullCheck(instruction);
  } else {
    MaybeRecordStat(stats_, MethodCompilationStat::kExplicitNullCheckGenerated);
    GenerateExplicitNullCheck(instruction);
  }
}

void CodeGenerator::ClearSpillSlotsFromLoopPhisInStackMap(HSuspendCheck* suspend_check,
                                                          HParallelMove* spills) const {
  LocationSummary* locations = suspend_check->GetLocations();
  HBasicBlock* block = suspend_check->GetBlock();
  DCHECK(block->GetLoopInformation()->GetSuspendCheck() == suspend_check);
  DCHECK(block->IsLoopHeader());
  DCHECK(block->GetFirstInstruction() == spills);

  for (size_t i = 0, num_moves = spills->NumMoves(); i != num_moves; ++i) {
    Location dest = spills->MoveOperandsAt(i)->GetDestination();
    // All parallel moves in loop headers are spills.
    DCHECK(dest.IsStackSlot() || dest.IsDoubleStackSlot() || dest.IsSIMDStackSlot()) << dest;
    // Clear the stack bit marking a reference. Do not bother to check if the spill is
    // actually a reference spill, clearing bits that are already zero is harmless.
    locations->ClearStackBit(dest.GetStackIndex() / kVRegSize);
  }
}

void CodeGenerator::EmitParallelMoves(Location from1,
                                      Location to1,
                                      DataType::Type type1,
                                      Location from2,
                                      Location to2,
                                      DataType::Type type2) {
  HParallelMove parallel_move(GetGraph()->GetAllocator());
  parallel_move.AddMove(from1, to1, type1, nullptr);
  parallel_move.AddMove(from2, to2, type2, nullptr);
  GetMoveResolver()->EmitNativeCode(&parallel_move);
}

void CodeGenerator::ValidateInvokeRuntime(QuickEntrypointEnum entrypoint,
                                          HInstruction* instruction,
                                          SlowPathCode* slow_path) {
  // Ensure that the call kind indication given to the register allocator is
  // coherent with the runtime call generated.
  if (slow_path == nullptr) {
    DCHECK(instruction->GetLocations()->WillCall())
        << "instruction->DebugName()=" << instruction->DebugName();
  } else {
    DCHECK(instruction->GetLocations()->CallsOnSlowPath() || slow_path->IsFatal())
        << "instruction->DebugName()=" << instruction->DebugName()
        << " slow_path->GetDescription()=" << slow_path->GetDescription();
  }

  // Check that the GC side effect is set when required.
  // TODO: Reverse EntrypointCanTriggerGC
  if (EntrypointCanTriggerGC(entrypoint)) {
    if (slow_path == nullptr) {
      DCHECK(instruction->GetSideEffects().Includes(SideEffects::CanTriggerGC()))
          << "instruction->DebugName()=" << instruction->DebugName()
          << " instruction->GetSideEffects().ToString()="
          << instruction->GetSideEffects().ToString();
    } else {
      // 'CanTriggerGC' side effect is used to restrict optimization of instructions which depend
      // on GC (e.g. IntermediateAddress) - to ensure they are not alive across GC points. However
      // if execution never returns to the compiled code from a GC point this restriction is
      // unnecessary - in particular for fatal slow paths which might trigger GC.
      DCHECK((slow_path->IsFatal() && !instruction->GetLocations()->WillCall()) ||
             instruction->GetSideEffects().Includes(SideEffects::CanTriggerGC()) ||
             // When (non-Baker) read barriers are enabled, some instructions
             // use a slow path to emit a read barrier, which does not trigger
             // GC.
             (kEmitCompilerReadBarrier &&
              !kUseBakerReadBarrier &&
              (instruction->IsInstanceFieldGet() ||
               instruction->IsStaticFieldGet() ||
               instruction->IsArrayGet() ||
               instruction->IsLoadClass() ||
               instruction->IsLoadString() ||
               instruction->IsInstanceOf() ||
               instruction->IsCheckCast() ||
               (instruction->IsInvokeVirtual() && instruction->GetLocations()->Intrinsified()))))
          << "instruction->DebugName()=" << instruction->DebugName()
          << " instruction->GetSideEffects().ToString()="
          << instruction->GetSideEffects().ToString()
          << " slow_path->GetDescription()=" << slow_path->GetDescription();
    }
  } else {
    // The GC side effect is not required for the instruction. But the instruction might still have
    // it, for example if it calls other entrypoints requiring it.
  }

  // Check the coherency of leaf information.
  DCHECK(instruction->IsSuspendCheck()
         || ((slow_path != nullptr) && slow_path->IsFatal())
         || instruction->GetLocations()->CanCall()
         || !IsLeafMethod())
      << instruction->DebugName() << ((slow_path != nullptr) ? slow_path->GetDescription() : "");
}

void CodeGenerator::ValidateInvokeRuntimeWithoutRecordingPcInfo(HInstruction* instruction,
                                                                SlowPathCode* slow_path) {
  DCHECK(instruction->GetLocations()->OnlyCallsOnSlowPath())
      << "instruction->DebugName()=" << instruction->DebugName()
      << " slow_path->GetDescription()=" << slow_path->GetDescription();
  // Only the Baker read barrier marking slow path used by certains
  // instructions is expected to invoke the runtime without recording
  // PC-related information.
  DCHECK(kUseBakerReadBarrier);
  DCHECK(instruction->IsInstanceFieldGet() ||
         instruction->IsStaticFieldGet() ||
         instruction->IsArrayGet() ||
         instruction->IsArraySet() ||
         instruction->IsLoadClass() ||
         instruction->IsLoadString() ||
         instruction->IsInstanceOf() ||
         instruction->IsCheckCast() ||
         (instruction->IsInvokeVirtual() && instruction->GetLocations()->Intrinsified()) ||
         (instruction->IsInvokeStaticOrDirect() && instruction->GetLocations()->Intrinsified()))
      << "instruction->DebugName()=" << instruction->DebugName()
      << " slow_path->GetDescription()=" << slow_path->GetDescription();
}

void SlowPathCode::SaveLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  size_t stack_offset = codegen->GetFirstRegisterSlotInSlowPath();

  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers= */ true);
  for (uint32_t i : LowToHighBits(core_spills)) {
    // If the register holds an object, update the stack mask.
    if (locations->RegisterContainsObject(i)) {
      locations->SetStackBit(stack_offset / kVRegSize);
    }
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_core_stack_offsets_[i] = stack_offset;
    stack_offset += codegen->SaveCoreRegister(stack_offset, i);
  }

  const uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers= */ false);
  for (uint32_t i : LowToHighBits(fp_spills)) {
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    saved_fpu_stack_offsets_[i] = stack_offset;
    stack_offset += codegen->SaveFloatingPointRegister(stack_offset, i);
  }
}

void SlowPathCode::RestoreLiveRegisters(CodeGenerator* codegen, LocationSummary* locations) {
  size_t stack_offset = codegen->GetFirstRegisterSlotInSlowPath();

  const uint32_t core_spills = codegen->GetSlowPathSpills(locations, /* core_registers= */ true);
  for (uint32_t i : LowToHighBits(core_spills)) {
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    stack_offset += codegen->RestoreCoreRegister(stack_offset, i);
  }

  const uint32_t fp_spills = codegen->GetSlowPathSpills(locations, /* core_registers= */ false);
  for (uint32_t i : LowToHighBits(fp_spills)) {
    DCHECK_LT(stack_offset, codegen->GetFrameSize() - codegen->FrameEntrySpillSize());
    DCHECK_LT(i, kMaximumNumberOfExpectedRegisters);
    stack_offset += codegen->RestoreFloatingPointRegister(stack_offset, i);
  }
}

void CodeGenerator::CreateSystemArrayCopyLocationSummary(HInvoke* invoke) {
  // Check to see if we have known failures that will cause us to have to bail out
  // to the runtime, and just generate the runtime call directly.
  HIntConstant* src_pos = invoke->InputAt(1)->AsIntConstant();
  HIntConstant* dest_pos = invoke->InputAt(3)->AsIntConstant();

  // The positions must be non-negative.
  if ((src_pos != nullptr && src_pos->GetValue() < 0) ||
      (dest_pos != nullptr && dest_pos->GetValue() < 0)) {
    // We will have to fail anyways.
    return;
  }

  // The length must be >= 0.
  HIntConstant* length = invoke->InputAt(4)->AsIntConstant();
  if (length != nullptr) {
    int32_t len = length->GetValue();
    if (len < 0) {
      // Just call as normal.
      return;
    }
  }

  SystemArrayCopyOptimizations optimizations(invoke);

  if (optimizations.GetDestinationIsSource()) {
    if (src_pos != nullptr && dest_pos != nullptr && src_pos->GetValue() < dest_pos->GetValue()) {
      // We only support backward copying if source and destination are the same.
      return;
    }
  }

  if (optimizations.GetDestinationIsPrimitiveArray() || optimizations.GetSourceIsPrimitiveArray()) {
    // We currently don't intrinsify primitive copying.
    return;
  }

  ArenaAllocator* allocator = invoke->GetBlock()->GetGraph()->GetAllocator();
  LocationSummary* locations = new (allocator) LocationSummary(invoke,
                                                               LocationSummary::kCallOnSlowPath,
                                                               kIntrinsified);
  // arraycopy(Object src, int src_pos, Object dest, int dest_pos, int length).
  locations->SetInAt(0, Location::RequiresRegister());
  locations->SetInAt(1, Location::RegisterOrConstant(invoke->InputAt(1)));
  locations->SetInAt(2, Location::RequiresRegister());
  locations->SetInAt(3, Location::RegisterOrConstant(invoke->InputAt(3)));
  locations->SetInAt(4, Location::RegisterOrConstant(invoke->InputAt(4)));

  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
  locations->AddTemp(Location::RequiresRegister());
}

void CodeGenerator::EmitJitRoots(uint8_t* code,
                                 const uint8_t* roots_data,
                                 /*out*/std::vector<Handle<mirror::Object>>* roots) {
  code_generation_data_->EmitJitRoots(roots);
  EmitJitRootPatches(code, roots_data);
}

QuickEntrypointEnum CodeGenerator::GetArrayAllocationEntrypoint(HNewArray* new_array) {
  switch (new_array->GetComponentSizeShift()) {
    case 0: return kQuickAllocArrayResolved8;
    case 1: return kQuickAllocArrayResolved16;
    case 2: return kQuickAllocArrayResolved32;
    case 3: return kQuickAllocArrayResolved64;
  }
  LOG(FATAL) << "Unreachable";
  UNREACHABLE();
}

}  // namespace art
