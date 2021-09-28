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

#include "quick_exception_handler.h"

#include "arch/context.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/logging.h"  // For VLOG_IS_ON.
#include "base/systrace.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction.h"
#include "entrypoints/entrypoint_utils.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "handle_scope-inl.h"
#include "interpreter/shadow_frame-inl.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/throwable.h"
#include "nterp_helpers.h"
#include "oat_quick_method_header.h"
#include "stack.h"
#include "stack_map.h"

namespace art {

static constexpr bool kDebugExceptionDelivery = false;
static constexpr size_t kInvalidFrameDepth = 0xffffffff;

QuickExceptionHandler::QuickExceptionHandler(Thread* self, bool is_deoptimization)
    : self_(self),
      context_(self->GetLongJumpContext()),
      is_deoptimization_(is_deoptimization),
      method_tracing_active_(is_deoptimization ||
                             Runtime::Current()->GetInstrumentation()->AreExitStubsInstalled()),
      handler_quick_frame_(nullptr),
      handler_quick_frame_pc_(0),
      handler_method_header_(nullptr),
      handler_quick_arg0_(0),
      handler_dex_pc_(0),
      clear_exception_(false),
      handler_frame_depth_(kInvalidFrameDepth),
      full_fragment_done_(false) {}

// Finds catch handler.
class CatchBlockStackVisitor final : public StackVisitor {
 public:
  CatchBlockStackVisitor(Thread* self,
                         Context* context,
                         Handle<mirror::Throwable>* exception,
                         QuickExceptionHandler* exception_handler,
                         uint32_t skip_frames)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(self, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        exception_(exception),
        exception_handler_(exception_handler),
        skip_frames_(skip_frames) {
  }

  bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod* method = GetMethod();
    exception_handler_->SetHandlerFrameDepth(GetFrameDepth());
    if (method == nullptr) {
      DCHECK_EQ(skip_frames_, 0u)
          << "We tried to skip an upcall! We should have returned to the upcall to finish delivery";
      // This is the upcall, we remember the frame and last pc so that we may long jump to them.
      exception_handler_->SetHandlerQuickFramePc(GetCurrentQuickFramePc());
      exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
      return false;  // End stack walk.
    }
    if (skip_frames_ != 0) {
      skip_frames_--;
      return true;
    }
    if (method->IsRuntimeMethod()) {
      // Ignore callee save method.
      DCHECK(method->IsCalleeSaveMethod());
      return true;
    }
    return HandleTryItems(method);
  }

 private:
  bool HandleTryItems(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t dex_pc = dex::kDexNoIndex;
    if (!method->IsNative()) {
      dex_pc = GetDexPc();
    }
    if (dex_pc != dex::kDexNoIndex) {
      bool clear_exception = false;
      StackHandleScope<1> hs(GetThread());
      Handle<mirror::Class> to_find(hs.NewHandle((*exception_)->GetClass()));
      uint32_t found_dex_pc = method->FindCatchBlock(to_find, dex_pc, &clear_exception);
      exception_handler_->SetClearException(clear_exception);
      if (found_dex_pc != dex::kDexNoIndex) {
        exception_handler_->SetHandlerDexPc(found_dex_pc);
        exception_handler_->SetHandlerQuickFramePc(
            GetCurrentOatQuickMethodHeader()->ToNativeQuickPc(
                method, found_dex_pc, /* is_for_catch_handler= */ true));
        exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
        exception_handler_->SetHandlerMethodHeader(GetCurrentOatQuickMethodHeader());
        return false;  // End stack walk.
      } else if (UNLIKELY(GetThread()->HasDebuggerShadowFrames())) {
        // We are going to unwind this frame. Did we prepare a shadow frame for debugging?
        size_t frame_id = GetFrameId();
        ShadowFrame* frame = GetThread()->FindDebuggerShadowFrame(frame_id);
        if (frame != nullptr) {
          // We will not execute this shadow frame so we can safely deallocate it.
          GetThread()->RemoveDebuggerShadowFrameMapping(frame_id);
          ShadowFrame::DeleteDeoptimizedFrame(frame);
        }
      }
    }
    return true;  // Continue stack walk.
  }

  // The exception we're looking for the catch block of.
  Handle<mirror::Throwable>* exception_;
  // The quick exception handler we're visiting for.
  QuickExceptionHandler* const exception_handler_;
  // The number of frames to skip searching for catches in.
  uint32_t skip_frames_;

  DISALLOW_COPY_AND_ASSIGN(CatchBlockStackVisitor);
};

// Finds the appropriate exception catch after calling all method exit instrumentation functions.
// Note that this might change the exception being thrown.
void QuickExceptionHandler::FindCatch(ObjPtr<mirror::Throwable> exception) {
  DCHECK(!is_deoptimization_);
  instrumentation::InstrumentationStackPopper popper(self_);
  // The number of total frames we have so far popped.
  uint32_t already_popped = 0;
  bool popped_to_top = true;
  StackHandleScope<1> hs(self_);
  MutableHandle<mirror::Throwable> exception_ref(hs.NewHandle(exception));
  // Sending the instrumentation events (done by the InstrumentationStackPopper) can cause new
  // exceptions to be thrown which will override the current exception. Therefore we need to perform
  // the search for a catch in a loop until we have successfully popped all the way to a catch or
  // the top of the stack.
  do {
    if (kDebugExceptionDelivery) {
      ObjPtr<mirror::String> msg = exception_ref->GetDetailMessage();
      std::string str_msg(msg != nullptr ? msg->ToModifiedUtf8() : "");
      self_->DumpStack(LOG_STREAM(INFO) << "Delivering exception: " << exception_ref->PrettyTypeOf()
                                        << ": " << str_msg << "\n");
    }

    // Walk the stack to find catch handler.
    CatchBlockStackVisitor visitor(self_, context_,
                                   &exception_ref,
                                   this,
                                   /*skip_frames=*/already_popped);
    visitor.WalkStack(true);
    uint32_t new_pop_count = handler_frame_depth_;
    DCHECK_GE(new_pop_count, already_popped);
    already_popped = new_pop_count;

    if (kDebugExceptionDelivery) {
      if (*handler_quick_frame_ == nullptr) {
        LOG(INFO) << "Handler is upcall";
      }
      if (GetHandlerMethod() != nullptr) {
        const DexFile* dex_file = GetHandlerMethod()->GetDexFile();
        int line_number =
            annotations::GetLineNumFromPC(dex_file, GetHandlerMethod(), handler_dex_pc_);
        LOG(INFO) << "Handler: " << GetHandlerMethod()->PrettyMethod() << " (line: "
                  << line_number << ")";
      }
    }
    // Exception was cleared as part of delivery.
    DCHECK(!self_->IsExceptionPending());
    // If the handler is in optimized code, we need to set the catch environment.
    if (*handler_quick_frame_ != nullptr &&
        handler_method_header_ != nullptr &&
        handler_method_header_->IsOptimized()) {
      SetCatchEnvironmentForOptimizedHandler(&visitor);
    }
    popped_to_top =
        popper.PopFramesTo(reinterpret_cast<uintptr_t>(handler_quick_frame_), exception_ref);
  } while (!popped_to_top);
  if (!clear_exception_) {
    // Put exception back in root set with clear throw location.
    self_->SetException(exception_ref.Get());
  }
}

static VRegKind ToVRegKind(DexRegisterLocation::Kind kind) {
  // Slightly hacky since we cannot map DexRegisterLocationKind and VRegKind
  // one to one. However, StackVisitor::GetVRegFromOptimizedCode only needs to
  // distinguish between core/FPU registers and low/high bits on 64-bit.
  switch (kind) {
    case DexRegisterLocation::Kind::kConstant:
    case DexRegisterLocation::Kind::kInStack:
      // VRegKind is ignored.
      return VRegKind::kUndefined;

    case DexRegisterLocation::Kind::kInRegister:
      // Selects core register. For 64-bit registers, selects low 32 bits.
      return VRegKind::kLongLoVReg;

    case DexRegisterLocation::Kind::kInRegisterHigh:
      // Selects core register. For 64-bit registers, selects high 32 bits.
      return VRegKind::kLongHiVReg;

    case DexRegisterLocation::Kind::kInFpuRegister:
      // Selects FPU register. For 64-bit registers, selects low 32 bits.
      return VRegKind::kDoubleLoVReg;

    case DexRegisterLocation::Kind::kInFpuRegisterHigh:
      // Selects FPU register. For 64-bit registers, selects high 32 bits.
      return VRegKind::kDoubleHiVReg;

    default:
      LOG(FATAL) << "Unexpected vreg location " << kind;
      UNREACHABLE();
  }
}

void QuickExceptionHandler::SetCatchEnvironmentForOptimizedHandler(StackVisitor* stack_visitor) {
  DCHECK(!is_deoptimization_);
  DCHECK(*handler_quick_frame_ != nullptr) << "Method should not be called on upcall exceptions";
  DCHECK(GetHandlerMethod() != nullptr && handler_method_header_->IsOptimized());

  if (kDebugExceptionDelivery) {
    self_->DumpStack(LOG_STREAM(INFO) << "Setting catch phis: ");
  }

  CodeItemDataAccessor accessor(GetHandlerMethod()->DexInstructionData());
  const size_t number_of_vregs = accessor.RegistersSize();
  CodeInfo code_info(handler_method_header_);

  // Find stack map of the catch block.
  StackMap catch_stack_map = code_info.GetCatchStackMapForDexPc(GetHandlerDexPc());
  DCHECK(catch_stack_map.IsValid());
  DexRegisterMap catch_vreg_map = code_info.GetDexRegisterMapOf(catch_stack_map);
  DCHECK_EQ(catch_vreg_map.size(), number_of_vregs);

  if (!catch_vreg_map.HasAnyLiveDexRegisters()) {
    return;
  }

  // Find stack map of the throwing instruction.
  StackMap throw_stack_map =
      code_info.GetStackMapForNativePcOffset(stack_visitor->GetNativePcOffset());
  DCHECK(throw_stack_map.IsValid());
  DexRegisterMap throw_vreg_map = code_info.GetDexRegisterMapOf(throw_stack_map);
  DCHECK_EQ(throw_vreg_map.size(), number_of_vregs);

  // Copy values between them.
  for (uint16_t vreg = 0; vreg < number_of_vregs; ++vreg) {
    DexRegisterLocation::Kind catch_location = catch_vreg_map[vreg].GetKind();
    if (catch_location == DexRegisterLocation::Kind::kNone) {
      continue;
    }
    DCHECK(catch_location == DexRegisterLocation::Kind::kInStack);

    // Get vreg value from its current location.
    uint32_t vreg_value;
    VRegKind vreg_kind = ToVRegKind(throw_vreg_map[vreg].GetKind());
    bool get_vreg_success =
        stack_visitor->GetVReg(stack_visitor->GetMethod(),
                               vreg,
                               vreg_kind,
                               &vreg_value,
                               throw_vreg_map[vreg]);
    CHECK(get_vreg_success) << "VReg " << vreg << " was optimized out ("
                            << "method=" << ArtMethod::PrettyMethod(stack_visitor->GetMethod())
                            << ", dex_pc=" << stack_visitor->GetDexPc() << ", "
                            << "native_pc_offset=" << stack_visitor->GetNativePcOffset() << ")";

    // Copy value to the catch phi's stack slot.
    int32_t slot_offset = catch_vreg_map[vreg].GetStackOffsetInBytes();
    ArtMethod** frame_top = stack_visitor->GetCurrentQuickFrame();
    uint8_t* slot_address = reinterpret_cast<uint8_t*>(frame_top) + slot_offset;
    uint32_t* slot_ptr = reinterpret_cast<uint32_t*>(slot_address);
    *slot_ptr = vreg_value;
  }
}

// Prepares deoptimization.
class DeoptimizeStackVisitor final : public StackVisitor {
 public:
  DeoptimizeStackVisitor(Thread* self,
                         Context* context,
                         QuickExceptionHandler* exception_handler,
                         bool single_frame)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : StackVisitor(self, context, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        exception_handler_(exception_handler),
        prev_shadow_frame_(nullptr),
        stacked_shadow_frame_pushed_(false),
        single_frame_deopt_(single_frame),
        single_frame_done_(false),
        single_frame_deopt_method_(nullptr),
        single_frame_deopt_quick_method_header_(nullptr),
        callee_method_(nullptr) {
  }

  ArtMethod* GetSingleFrameDeoptMethod() const {
    return single_frame_deopt_method_;
  }

  const OatQuickMethodHeader* GetSingleFrameDeoptQuickMethodHeader() const {
    return single_frame_deopt_quick_method_header_;
  }

  void FinishStackWalk() REQUIRES_SHARED(Locks::mutator_lock_) {
    // This is the upcall, or the next full frame in single-frame deopt, or the
    // code isn't deoptimizeable. We remember the frame and last pc so that we
    // may long jump to them.
    exception_handler_->SetHandlerQuickFramePc(GetCurrentQuickFramePc());
    exception_handler_->SetHandlerQuickFrame(GetCurrentQuickFrame());
    exception_handler_->SetHandlerMethodHeader(GetCurrentOatQuickMethodHeader());
    if (!stacked_shadow_frame_pushed_) {
      // In case there is no deoptimized shadow frame for this upcall, we still
      // need to push a nullptr to the stack since there is always a matching pop after
      // the long jump.
      GetThread()->PushStackedShadowFrame(nullptr,
                                          StackedShadowFrameType::kDeoptimizationShadowFrame);
      stacked_shadow_frame_pushed_ = true;
    }
    if (GetMethod() == nullptr) {
      exception_handler_->SetFullFragmentDone(true);
    } else {
      CHECK(callee_method_ != nullptr) << GetMethod()->PrettyMethod(false);
      exception_handler_->SetHandlerQuickArg0(reinterpret_cast<uintptr_t>(callee_method_));
    }
  }

  bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
    exception_handler_->SetHandlerFrameDepth(GetFrameDepth());
    ArtMethod* method = GetMethod();
    VLOG(deopt) << "Deoptimizing stack: depth: " << GetFrameDepth()
                << " at method " << ArtMethod::PrettyMethod(method);
    if (method == nullptr || single_frame_done_) {
      FinishStackWalk();
      return false;  // End stack walk.
    } else if (method->IsRuntimeMethod()) {
      // Ignore callee save method.
      DCHECK(method->IsCalleeSaveMethod());
      return true;
    } else if (method->IsNative()) {
      // If we return from JNI with a pending exception and want to deoptimize, we need to skip
      // the native method.
      // The top method is a runtime method, the native method comes next.
      CHECK_EQ(GetFrameDepth(), 1U);
      callee_method_ = method;
      return true;
    } else if (!single_frame_deopt_ &&
               !Runtime::Current()->IsAsyncDeoptimizeable(GetCurrentQuickFramePc())) {
      // We hit some code that's not deoptimizeable. However, Single-frame deoptimization triggered
      // from compiled code is always allowed since HDeoptimize always saves the full environment.
      LOG(WARNING) << "Got request to deoptimize un-deoptimizable method "
                   << method->PrettyMethod();
      FinishStackWalk();
      return false;  // End stack walk.
    } else {
      // Check if a shadow frame already exists for debugger's set-local-value purpose.
      const size_t frame_id = GetFrameId();
      ShadowFrame* new_frame = GetThread()->FindDebuggerShadowFrame(frame_id);
      const bool* updated_vregs;
      CodeItemDataAccessor accessor(method->DexInstructionData());
      const size_t num_regs = accessor.RegistersSize();
      if (new_frame == nullptr) {
        new_frame = ShadowFrame::CreateDeoptimizedFrame(num_regs, nullptr, method, GetDexPc());
        updated_vregs = nullptr;
      } else {
        updated_vregs = GetThread()->GetUpdatedVRegFlags(frame_id);
        DCHECK(updated_vregs != nullptr);
      }
      if (GetCurrentOatQuickMethodHeader()->IsNterpMethodHeader()) {
        HandleNterpDeoptimization(method, new_frame, updated_vregs);
      } else {
        HandleOptimizingDeoptimization(method, new_frame, updated_vregs);
      }
      if (updated_vregs != nullptr) {
        // Calling Thread::RemoveDebuggerShadowFrameMapping will also delete the updated_vregs
        // array so this must come after we processed the frame.
        GetThread()->RemoveDebuggerShadowFrameMapping(frame_id);
        DCHECK(GetThread()->FindDebuggerShadowFrame(frame_id) == nullptr);
      }
      if (prev_shadow_frame_ != nullptr) {
        prev_shadow_frame_->SetLink(new_frame);
      } else {
        // Will be popped after the long jump after DeoptimizeStack(),
        // right before interpreter::EnterInterpreterFromDeoptimize().
        stacked_shadow_frame_pushed_ = true;
        GetThread()->PushStackedShadowFrame(
            new_frame, StackedShadowFrameType::kDeoptimizationShadowFrame);
      }
      prev_shadow_frame_ = new_frame;

      if (single_frame_deopt_ && !IsInInlinedFrame()) {
        // Single-frame deopt ends at the first non-inlined frame and needs to store that method.
        single_frame_done_ = true;
        single_frame_deopt_method_ = method;
        single_frame_deopt_quick_method_header_ = GetCurrentOatQuickMethodHeader();
      }
      callee_method_ = method;
      return true;
    }
  }

 private:
  void HandleNterpDeoptimization(ArtMethod* m,
                                 ShadowFrame* new_frame,
                                 const bool* updated_vregs)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ArtMethod** cur_quick_frame = GetCurrentQuickFrame();
    StackReference<mirror::Object>* vreg_ref_base =
        reinterpret_cast<StackReference<mirror::Object>*>(NterpGetReferenceArray(cur_quick_frame));
    int32_t* vreg_int_base =
        reinterpret_cast<int32_t*>(NterpGetRegistersArray(cur_quick_frame));
    CodeItemDataAccessor accessor(m->DexInstructionData());
    const uint16_t num_regs = accessor.RegistersSize();
    // An nterp frame has two arrays: a dex register array and a reference array
    // that shadows the dex register array but only containing references
    // (non-reference dex registers have nulls). See nterp_helpers.cc.
    for (size_t reg = 0; reg < num_regs; ++reg) {
      if (updated_vregs != nullptr && updated_vregs[reg]) {
        // Keep the value set by debugger.
        continue;
      }
      StackReference<mirror::Object>* ref_addr = vreg_ref_base + reg;
      mirror::Object* ref = ref_addr->AsMirrorPtr();
      if (ref != nullptr) {
        new_frame->SetVRegReference(reg, ref);
      } else {
        new_frame->SetVReg(reg, vreg_int_base[reg]);
      }
    }
  }

  void HandleOptimizingDeoptimization(ArtMethod* m,
                                      ShadowFrame* new_frame,
                                      const bool* updated_vregs)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    const OatQuickMethodHeader* method_header = GetCurrentOatQuickMethodHeader();
    CodeInfo code_info(method_header);
    uintptr_t native_pc_offset = method_header->NativeQuickPcOffset(GetCurrentQuickFramePc());
    StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset);
    CodeItemDataAccessor accessor(m->DexInstructionData());
    const size_t number_of_vregs = accessor.RegistersSize();
    uint32_t register_mask = code_info.GetRegisterMaskOf(stack_map);
    BitMemoryRegion stack_mask = code_info.GetStackMaskOf(stack_map);
    DexRegisterMap vreg_map = IsInInlinedFrame()
        ? code_info.GetInlineDexRegisterMapOf(stack_map, GetCurrentInlinedFrame())
        : code_info.GetDexRegisterMapOf(stack_map);

    DCHECK_EQ(vreg_map.size(), number_of_vregs);
    if (vreg_map.empty()) {
      return;
    }

    for (uint16_t vreg = 0; vreg < number_of_vregs; ++vreg) {
      if (updated_vregs != nullptr && updated_vregs[vreg]) {
        // Keep the value set by debugger.
        continue;
      }

      DexRegisterLocation::Kind location = vreg_map[vreg].GetKind();
      static constexpr uint32_t kDeadValue = 0xEBADDE09;
      uint32_t value = kDeadValue;
      bool is_reference = false;

      switch (location) {
        case DexRegisterLocation::Kind::kInStack: {
          const int32_t offset = vreg_map[vreg].GetStackOffsetInBytes();
          const uint8_t* addr = reinterpret_cast<const uint8_t*>(GetCurrentQuickFrame()) + offset;
          value = *reinterpret_cast<const uint32_t*>(addr);
          uint32_t bit = (offset >> 2);
          if (bit < stack_mask.size_in_bits() && stack_mask.LoadBit(bit)) {
            is_reference = true;
          }
          break;
        }
        case DexRegisterLocation::Kind::kInRegister:
        case DexRegisterLocation::Kind::kInRegisterHigh:
        case DexRegisterLocation::Kind::kInFpuRegister:
        case DexRegisterLocation::Kind::kInFpuRegisterHigh: {
          uint32_t reg = vreg_map[vreg].GetMachineRegister();
          bool result = GetRegisterIfAccessible(reg, ToVRegKind(location), &value);
          CHECK(result);
          if (location == DexRegisterLocation::Kind::kInRegister) {
            if (((1u << reg) & register_mask) != 0) {
              is_reference = true;
            }
          }
          break;
        }
        case DexRegisterLocation::Kind::kConstant: {
          value = vreg_map[vreg].GetConstant();
          if (value == 0) {
            // Make it a reference for extra safety.
            is_reference = true;
          }
          break;
        }
        case DexRegisterLocation::Kind::kNone: {
          break;
        }
        default: {
          LOG(FATAL) << "Unexpected location kind " << vreg_map[vreg].GetKind();
          UNREACHABLE();
        }
      }
      if (is_reference) {
        new_frame->SetVRegReference(vreg, reinterpret_cast<mirror::Object*>(value));
      } else {
        new_frame->SetVReg(vreg, value);
      }
    }
  }

  static VRegKind GetVRegKind(uint16_t reg, const std::vector<int32_t>& kinds) {
    return static_cast<VRegKind>(kinds[reg * 2]);
  }

  QuickExceptionHandler* const exception_handler_;
  ShadowFrame* prev_shadow_frame_;
  bool stacked_shadow_frame_pushed_;
  const bool single_frame_deopt_;
  bool single_frame_done_;
  ArtMethod* single_frame_deopt_method_;
  const OatQuickMethodHeader* single_frame_deopt_quick_method_header_;
  ArtMethod* callee_method_;

  DISALLOW_COPY_AND_ASSIGN(DeoptimizeStackVisitor);
};

void QuickExceptionHandler::PrepareForLongJumpToInvokeStubOrInterpreterBridge() {
  if (full_fragment_done_) {
    // Restore deoptimization exception. When returning from the invoke stub,
    // ArtMethod::Invoke() will see the special exception to know deoptimization
    // is needed.
    self_->SetException(Thread::GetDeoptimizationException());
  } else {
    // PC needs to be of the quick-to-interpreter bridge.
    int32_t offset;
    offset = GetThreadOffset<kRuntimePointerSize>(kQuickQuickToInterpreterBridge).Int32Value();
    handler_quick_frame_pc_ = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uint8_t*>(self_) + offset);
  }
}

void QuickExceptionHandler::DeoptimizeStack() {
  DCHECK(is_deoptimization_);
  if (kDebugExceptionDelivery) {
    self_->DumpStack(LOG_STREAM(INFO) << "Deoptimizing: ");
  }

  DeoptimizeStackVisitor visitor(self_, context_, this, false);
  visitor.WalkStack(true);
  PrepareForLongJumpToInvokeStubOrInterpreterBridge();
}

void QuickExceptionHandler::DeoptimizeSingleFrame(DeoptimizationKind kind) {
  DCHECK(is_deoptimization_);

  DeoptimizeStackVisitor visitor(self_, context_, this, true);
  visitor.WalkStack(true);

  // Compiled code made an explicit deoptimization.
  ArtMethod* deopt_method = visitor.GetSingleFrameDeoptMethod();
  SCOPED_TRACE << "Deoptimizing "
               <<  deopt_method->PrettyMethod()
               << ": " << GetDeoptimizationKindName(kind);

  DCHECK(deopt_method != nullptr);
  if (VLOG_IS_ON(deopt) || kDebugExceptionDelivery) {
    LOG(INFO) << "Single-frame deopting: "
              << deopt_method->PrettyMethod()
              << " due to "
              << GetDeoptimizationKindName(kind);
    DumpFramesWithType(self_, /* details= */ true);
  }
  if (Runtime::Current()->UseJitCompilation()) {
    Runtime::Current()->GetJit()->GetCodeCache()->InvalidateCompiledCodeFor(
        deopt_method, visitor.GetSingleFrameDeoptQuickMethodHeader());
  } else {
    // Transfer the code to interpreter.
    Runtime::Current()->GetInstrumentation()->UpdateMethodsCode(
        deopt_method, GetQuickToInterpreterBridge());
  }

  PrepareForLongJumpToInvokeStubOrInterpreterBridge();
}

void QuickExceptionHandler::DeoptimizePartialFragmentFixup(uintptr_t return_pc) {
  // At this point, the instrumentation stack has been updated. We need to install
  // the real return pc on stack, in case instrumentation stub is stored there,
  // so that the interpreter bridge code can return to the right place.
  if (return_pc != 0) {
    uintptr_t* pc_addr = reinterpret_cast<uintptr_t*>(handler_quick_frame_);
    CHECK(pc_addr != nullptr);
    pc_addr--;
    *reinterpret_cast<uintptr_t*>(pc_addr) = return_pc;
  }

  // Architecture-dependent work. This is to get the LR right for x86 and x86-64.
  if (kRuntimeISA == InstructionSet::kX86 || kRuntimeISA == InstructionSet::kX86_64) {
    // On x86, the return address is on the stack, so just reuse it. Otherwise we would have to
    // change how longjump works.
    handler_quick_frame_ = reinterpret_cast<ArtMethod**>(
        reinterpret_cast<uintptr_t>(handler_quick_frame_) - sizeof(void*));
  }
}

uintptr_t QuickExceptionHandler::UpdateInstrumentationStack() {
  DCHECK(is_deoptimization_) << "Non-deoptimization handlers should use FindCatch";
  uintptr_t return_pc = 0;
  if (method_tracing_active_) {
    instrumentation::Instrumentation* instrumentation = Runtime::Current()->GetInstrumentation();
    return_pc = instrumentation->PopFramesForDeoptimization(
        self_, reinterpret_cast<uintptr_t>(handler_quick_frame_));
  }
  return return_pc;
}

void QuickExceptionHandler::DoLongJump(bool smash_caller_saves) {
  // Place context back on thread so it will be available when we continue.
  self_->ReleaseLongJumpContext(context_);
  context_->SetSP(reinterpret_cast<uintptr_t>(handler_quick_frame_));
  CHECK_NE(handler_quick_frame_pc_, 0u);
  context_->SetPC(handler_quick_frame_pc_);
  context_->SetArg0(handler_quick_arg0_);
  if (smash_caller_saves) {
    context_->SmashCallerSaves();
  }
  if (!is_deoptimization_ &&
      handler_method_header_ != nullptr &&
      handler_method_header_->IsNterpMethodHeader()) {
    context_->SetNterpDexPC(reinterpret_cast<uintptr_t>(
        GetHandlerMethod()->DexInstructions().Insns() + handler_dex_pc_));
  }
  context_->DoLongJump();
  UNREACHABLE();
}

void QuickExceptionHandler::DumpFramesWithType(Thread* self, bool details) {
  StackVisitor::WalkStack(
      [&](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* method = stack_visitor->GetMethod();
        if (details) {
          LOG(INFO) << "|> pc   = " << std::hex << stack_visitor->GetCurrentQuickFramePc();
          LOG(INFO) << "|> addr = " << std::hex
              << reinterpret_cast<uintptr_t>(stack_visitor->GetCurrentQuickFrame());
          if (stack_visitor->GetCurrentQuickFrame() != nullptr && method != nullptr) {
            LOG(INFO) << "|> ret  = " << std::hex << stack_visitor->GetReturnPc();
          }
        }
        if (method == nullptr) {
          // Transition, do go on, we want to unwind over bridges, all the way.
          if (details) {
            LOG(INFO) << "N  <transition>";
          }
          return true;
        } else if (method->IsRuntimeMethod()) {
          if (details) {
            LOG(INFO) << "R  " << method->PrettyMethod(true);
          }
          return true;
        } else {
          bool is_shadow = stack_visitor->GetCurrentShadowFrame() != nullptr;
          LOG(INFO) << (is_shadow ? "S" : "Q")
                    << ((!is_shadow && stack_visitor->IsInInlinedFrame()) ? "i" : " ")
                    << " "
                    << method->PrettyMethod(true);
          return true;  // Go on.
        }
      },
      self,
      /* context= */ nullptr,
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
}

}  // namespace art
