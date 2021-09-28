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

#include "interpreter_common.h"

#include <cmath>

#include "base/casts.h"
#include "base/enums.h"
#include "class_root.h"
#include "debugger.h"
#include "dex/dex_file_types.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "handle.h"
#include "intrinsics_enum.h"
#include "jit/jit.h"
#include "jvalue-inl.h"
#include "method_handles-inl.h"
#include "method_handles.h"
#include "mirror/array-alloc-inl.h"
#include "mirror/array-inl.h"
#include "mirror/call_site-inl.h"
#include "mirror/class.h"
#include "mirror/emulated_stack_frame.h"
#include "mirror/method_handle_impl-inl.h"
#include "mirror/method_type-inl.h"
#include "mirror/object_array-alloc-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/var_handle.h"
#include "reflection-inl.h"
#include "reflection.h"
#include "shadow_frame-inl.h"
#include "stack.h"
#include "thread-inl.h"
#include "transaction.h"
#include "var_handles.h"
#include "well_known_classes.h"

namespace art {
namespace interpreter {

void ThrowNullPointerExceptionFromInterpreter() {
  ThrowNullPointerExceptionFromDexPC();
}

bool CheckStackOverflow(Thread* self, size_t frame_size)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  bool implicit_check = !Runtime::Current()->ExplicitStackOverflowChecks();
  uint8_t* stack_end = self->GetStackEndForInterpreter(implicit_check);
  if (UNLIKELY(__builtin_frame_address(0) < stack_end + frame_size)) {
    ThrowStackOverflowError(self);
    return false;
  }
  return true;
}

bool UseFastInterpreterToInterpreterInvoke(ArtMethod* method) {
  Runtime* runtime = Runtime::Current();
  const void* quick_code = method->GetEntryPointFromQuickCompiledCode();
  if (!runtime->GetClassLinker()->IsQuickToInterpreterBridge(quick_code)) {
    return false;
  }
  if (!method->SkipAccessChecks() || method->IsNative() || method->IsProxyMethod()) {
    return false;
  }
  if (method->IsIntrinsic()) {
    return false;
  }
  if (method->GetDeclaringClass()->IsStringClass() && method->IsConstructor()) {
    return false;
  }
  if (method->IsStatic() && !method->GetDeclaringClass()->IsVisiblyInitialized()) {
    return false;
  }
  ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
  if ((profiling_info != nullptr) && (profiling_info->GetSavedEntryPoint() != nullptr)) {
    return false;
  }
  return true;
}

template <typename T>
bool SendMethodExitEvents(Thread* self,
                          const instrumentation::Instrumentation* instrumentation,
                          ShadowFrame& frame,
                          ObjPtr<mirror::Object> thiz,
                          ArtMethod* method,
                          uint32_t dex_pc,
                          T& result) {
  bool had_event = false;
  // We can get additional ForcePopFrame requests during handling of these events. We should
  // respect these and send additional instrumentation events.
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_thiz(hs.NewHandle(thiz));
  do {
    frame.SetForcePopFrame(false);
    if (UNLIKELY(instrumentation->HasMethodExitListeners() && !frame.GetSkipMethodExitEvents())) {
      had_event = true;
      instrumentation->MethodExitEvent(
          self, h_thiz.Get(), method, dex_pc, instrumentation::OptionalFrame{ frame }, result);
    }
    // We don't send method-exit if it's a pop-frame. We still send frame_popped though.
    if (UNLIKELY(frame.NeedsNotifyPop() && instrumentation->HasWatchedFramePopListeners())) {
      had_event = true;
      instrumentation->WatchedFramePopped(self, frame);
    }
  } while (UNLIKELY(frame.GetForcePopFrame()));
  if (UNLIKELY(had_event)) {
    return !self->IsExceptionPending();
  } else {
    return true;
  }
}

template
bool SendMethodExitEvents(Thread* self,
                          const instrumentation::Instrumentation* instrumentation,
                          ShadowFrame& frame,
                          ObjPtr<mirror::Object> thiz,
                          ArtMethod* method,
                          uint32_t dex_pc,
                          MutableHandle<mirror::Object>& result);

template
bool SendMethodExitEvents(Thread* self,
                          const instrumentation::Instrumentation* instrumentation,
                          ShadowFrame& frame,
                          ObjPtr<mirror::Object> thiz,
                          ArtMethod* method,
                          uint32_t dex_pc,
                          JValue& result);

// We execute any instrumentation events that are triggered by this exception and change the
// shadow_frame's dex_pc to that of the exception handler if there is one in the current method.
// Return true if we should continue executing in the current method and false if we need to go up
// the stack to find an exception handler.
// We accept a null Instrumentation* meaning we must not report anything to the instrumentation.
// TODO We should have a better way to skip instrumentation reporting or possibly rethink that
// behavior.
bool MoveToExceptionHandler(Thread* self,
                            ShadowFrame& shadow_frame,
                            const instrumentation::Instrumentation* instrumentation) {
  self->VerifyStack();
  StackHandleScope<2> hs(self);
  Handle<mirror::Throwable> exception(hs.NewHandle(self->GetException()));
  if (instrumentation != nullptr &&
      instrumentation->HasExceptionThrownListeners() &&
      self->IsExceptionThrownByCurrentMethod(exception.Get())) {
    // See b/65049545 for why we don't need to check to see if the exception has changed.
    instrumentation->ExceptionThrownEvent(self, exception.Get());
    if (shadow_frame.GetForcePopFrame()) {
      // We will check in the caller for GetForcePopFrame again. We need to bail out early to
      // prevent an ExceptionHandledEvent from also being sent before popping.
      return true;
    }
  }
  bool clear_exception = false;
  uint32_t found_dex_pc = shadow_frame.GetMethod()->FindCatchBlock(
      hs.NewHandle(exception->GetClass()), shadow_frame.GetDexPC(), &clear_exception);
  if (found_dex_pc == dex::kDexNoIndex) {
    if (instrumentation != nullptr) {
      if (shadow_frame.NeedsNotifyPop()) {
        instrumentation->WatchedFramePopped(self, shadow_frame);
        if (shadow_frame.GetForcePopFrame()) {
          // We will check in the caller for GetForcePopFrame again. We need to bail out early to
          // prevent an ExceptionHandledEvent from also being sent before popping and to ensure we
          // handle other types of non-standard-exits.
          return true;
        }
      }
      // Exception is not caught by the current method. We will unwind to the
      // caller. Notify any instrumentation listener.
      instrumentation->MethodUnwindEvent(self,
                                         shadow_frame.GetThisObject(),
                                         shadow_frame.GetMethod(),
                                         shadow_frame.GetDexPC());
    }
    return shadow_frame.GetForcePopFrame();
  } else {
    shadow_frame.SetDexPC(found_dex_pc);
    if (instrumentation != nullptr && instrumentation->HasExceptionHandledListeners()) {
      self->ClearException();
      instrumentation->ExceptionHandledEvent(self, exception.Get());
      if (UNLIKELY(self->IsExceptionPending())) {
        // Exception handled event threw an exception. Try to find the handler for this one.
        return MoveToExceptionHandler(self, shadow_frame, instrumentation);
      } else if (!clear_exception) {
        self->SetException(exception.Get());
      }
    } else if (clear_exception) {
      self->ClearException();
    }
    return true;
  }
}

void UnexpectedOpcode(const Instruction* inst, const ShadowFrame& shadow_frame) {
  LOG(FATAL) << "Unexpected instruction: "
             << inst->DumpString(shadow_frame.GetMethod()->GetDexFile());
  UNREACHABLE();
}

void AbortTransactionF(Thread* self, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  AbortTransactionV(self, fmt, args);
  va_end(args);
}

void AbortTransactionV(Thread* self, const char* fmt, va_list args) {
  CHECK(Runtime::Current()->IsActiveTransaction());
  // Constructs abort message.
  std::string abort_msg;
  android::base::StringAppendV(&abort_msg, fmt, args);
  // Throws an exception so we can abort the transaction and rollback every change.
  Runtime::Current()->AbortTransactionAndThrowAbortError(self, abort_msg);
}

// START DECLARATIONS :
//
// These additional declarations are required because clang complains
// about ALWAYS_INLINE (-Werror, -Wgcc-compat) in definitions.
//

template <bool is_range, bool do_assignability_check>
static ALWAYS_INLINE bool DoCallCommon(ArtMethod* called_method,
                                       Thread* self,
                                       ShadowFrame& shadow_frame,
                                       JValue* result,
                                       uint16_t number_of_inputs,
                                       uint32_t (&arg)[Instruction::kMaxVarArgRegs],
                                       uint32_t vregC) REQUIRES_SHARED(Locks::mutator_lock_);

template <bool is_range>
ALWAYS_INLINE void CopyRegisters(ShadowFrame& caller_frame,
                                 ShadowFrame* callee_frame,
                                 const uint32_t (&arg)[Instruction::kMaxVarArgRegs],
                                 const size_t first_src_reg,
                                 const size_t first_dest_reg,
                                 const size_t num_regs) REQUIRES_SHARED(Locks::mutator_lock_);

// END DECLARATIONS.

void ArtInterpreterToCompiledCodeBridge(Thread* self,
                                        ArtMethod* caller,
                                        ShadowFrame* shadow_frame,
                                        uint16_t arg_offset,
                                        JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ArtMethod* method = shadow_frame->GetMethod();
  // Ensure static methods are initialized.
  if (method->IsStatic()) {
    ObjPtr<mirror::Class> declaringClass = method->GetDeclaringClass();
    if (UNLIKELY(!declaringClass->IsVisiblyInitialized())) {
      self->PushShadowFrame(shadow_frame);
      StackHandleScope<1> hs(self);
      Handle<mirror::Class> h_class(hs.NewHandle(declaringClass));
      if (UNLIKELY(!Runtime::Current()->GetClassLinker()->EnsureInitialized(
                        self, h_class, /*can_init_fields=*/ true, /*can_init_parents=*/ true))) {
        self->PopShadowFrame();
        DCHECK(self->IsExceptionPending());
        return;
      }
      self->PopShadowFrame();
      DCHECK(h_class->IsInitializing());
      // Reload from shadow frame in case the method moved, this is faster than adding a handle.
      method = shadow_frame->GetMethod();
    }
  }
  // Basic checks for the arg_offset. If there's no code item, the arg_offset must be 0. Otherwise,
  // check that the arg_offset isn't greater than the number of registers. A stronger check is
  // difficult since the frame may contain space for all the registers in the method, or only enough
  // space for the arguments.
  if (kIsDebugBuild) {
    if (method->GetCodeItem() == nullptr) {
      DCHECK_EQ(0u, arg_offset) << method->PrettyMethod();
    } else {
      DCHECK_LE(arg_offset, shadow_frame->NumberOfVRegs());
    }
  }
  jit::Jit* jit = Runtime::Current()->GetJit();
  if (jit != nullptr && caller != nullptr) {
    jit->NotifyInterpreterToCompiledCodeTransition(self, caller);
  }
  method->Invoke(self, shadow_frame->GetVRegArgs(arg_offset),
                 (shadow_frame->NumberOfVRegs() - arg_offset) * sizeof(uint32_t),
                 result, method->GetInterfaceMethodIfProxy(kRuntimePointerSize)->GetShorty());
}

void SetStringInitValueToAllAliases(ShadowFrame* shadow_frame,
                                    uint16_t this_obj_vreg,
                                    JValue result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Object> existing = shadow_frame->GetVRegReference(this_obj_vreg);
  if (existing == nullptr) {
    // If it's null, we come from compiled code that was deoptimized. Nothing to do,
    // as the compiler verified there was no alias.
    // Set the new string result of the StringFactory.
    shadow_frame->SetVRegReference(this_obj_vreg, result.GetL());
    return;
  }
  // Set the string init result into all aliases.
  for (uint32_t i = 0, e = shadow_frame->NumberOfVRegs(); i < e; ++i) {
    if (shadow_frame->GetVRegReference(i) == existing) {
      DCHECK_EQ(shadow_frame->GetVRegReference(i),
                reinterpret_cast32<mirror::Object*>(shadow_frame->GetVReg(i)));
      shadow_frame->SetVRegReference(i, result.GetL());
      DCHECK_EQ(shadow_frame->GetVRegReference(i),
                reinterpret_cast32<mirror::Object*>(shadow_frame->GetVReg(i)));
    }
  }
}

template<bool is_range>
static bool DoMethodHandleInvokeCommon(Thread* self,
                                       ShadowFrame& shadow_frame,
                                       bool invoke_exact,
                                       const Instruction* inst,
                                       uint16_t inst_data,
                                       JValue* result)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Make sure to check for async exceptions
  if (UNLIKELY(self->ObserveAsyncException())) {
    return false;
  }
  // Invoke-polymorphic instructions always take a receiver. i.e, they are never static.
  const uint32_t vRegC = (is_range) ? inst->VRegC_4rcc() : inst->VRegC_45cc();
  const int invoke_method_idx = (is_range) ? inst->VRegB_4rcc() : inst->VRegB_45cc();

  // Initialize |result| to 0 as this is the default return value for
  // polymorphic invocations of method handle types with void return
  // and provides sane return result in error cases.
  result->SetJ(0);

  // The invoke_method_idx here is the name of the signature polymorphic method that
  // was symbolically invoked in bytecode (say MethodHandle.invoke or MethodHandle.invokeExact)
  // and not the method that we'll dispatch to in the end.
  StackHandleScope<2> hs(self);
  Handle<mirror::MethodHandle> method_handle(hs.NewHandle(
      ObjPtr<mirror::MethodHandle>::DownCast(shadow_frame.GetVRegReference(vRegC))));
  if (UNLIKELY(method_handle == nullptr)) {
    // Note that the invoke type is kVirtual here because a call to a signature
    // polymorphic method is shaped like a virtual call at the bytecode level.
    ThrowNullPointerExceptionForMethodAccess(invoke_method_idx, InvokeType::kVirtual);
    return false;
  }

  // The vRegH value gives the index of the proto_id associated with this
  // signature polymorphic call site.
  const uint16_t vRegH = (is_range) ? inst->VRegH_4rcc() : inst->VRegH_45cc();
  const dex::ProtoIndex callsite_proto_id(vRegH);

  // Call through to the classlinker and ask it to resolve the static type associated
  // with the callsite. This information is stored in the dex cache so it's
  // guaranteed to be fast after the first resolution.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::MethodType> callsite_type(hs.NewHandle(
      class_linker->ResolveMethodType(self, callsite_proto_id, shadow_frame.GetMethod())));

  // This implies we couldn't resolve one or more types in this method handle.
  if (UNLIKELY(callsite_type == nullptr)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  // There is a common dispatch method for method handles that takes
  // arguments either from a range or an array of arguments depending
  // on whether the DEX instruction is invoke-polymorphic/range or
  // invoke-polymorphic. The array here is for the latter.
  if (UNLIKELY(is_range)) {
    // VRegC is the register holding the method handle. Arguments passed
    // to the method handle's target do not include the method handle.
    RangeInstructionOperands operands(inst->VRegC_4rcc() + 1, inst->VRegA_4rcc() - 1);
    if (invoke_exact) {
      return MethodHandleInvokeExact(self,
                                     shadow_frame,
                                     method_handle,
                                     callsite_type,
                                     &operands,
                                     result);
    } else {
      return MethodHandleInvoke(self,
                                shadow_frame,
                                method_handle,
                                callsite_type,
                                &operands,
                                result);
    }
  } else {
    // Get the register arguments for the invoke.
    uint32_t args[Instruction::kMaxVarArgRegs] = {};
    inst->GetVarArgs(args, inst_data);
    // Drop the first register which is the method handle performing the invoke.
    memmove(args, args + 1, sizeof(args[0]) * (Instruction::kMaxVarArgRegs - 1));
    args[Instruction::kMaxVarArgRegs - 1] = 0;
    VarArgsInstructionOperands operands(args, inst->VRegA_45cc() - 1);
    if (invoke_exact) {
      return MethodHandleInvokeExact(self,
                                     shadow_frame,
                                     method_handle,
                                     callsite_type,
                                     &operands,
                                     result);
    } else {
      return MethodHandleInvoke(self,
                                shadow_frame,
                                method_handle,
                                callsite_type,
                                &operands,
                                result);
    }
  }
}

bool DoMethodHandleInvokeExact(Thread* self,
                               ShadowFrame& shadow_frame,
                               const Instruction* inst,
                               uint16_t inst_data,
                               JValue* result) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (inst->Opcode() == Instruction::INVOKE_POLYMORPHIC) {
    static const bool kIsRange = false;
    return DoMethodHandleInvokeCommon<kIsRange>(
        self, shadow_frame, /* invoke_exact= */ true, inst, inst_data, result);
  } else {
    DCHECK_EQ(inst->Opcode(), Instruction::INVOKE_POLYMORPHIC_RANGE);
    static const bool kIsRange = true;
    return DoMethodHandleInvokeCommon<kIsRange>(
        self, shadow_frame, /* invoke_exact= */ true, inst, inst_data, result);
  }
}

bool DoMethodHandleInvoke(Thread* self,
                          ShadowFrame& shadow_frame,
                          const Instruction* inst,
                          uint16_t inst_data,
                          JValue* result) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (inst->Opcode() == Instruction::INVOKE_POLYMORPHIC) {
    static const bool kIsRange = false;
    return DoMethodHandleInvokeCommon<kIsRange>(
        self, shadow_frame, /* invoke_exact= */ false, inst, inst_data, result);
  } else {
    DCHECK_EQ(inst->Opcode(), Instruction::INVOKE_POLYMORPHIC_RANGE);
    static const bool kIsRange = true;
    return DoMethodHandleInvokeCommon<kIsRange>(
        self, shadow_frame, /* invoke_exact= */ false, inst, inst_data, result);
  }
}

static bool DoVarHandleInvokeCommon(Thread* self,
                                    ShadowFrame& shadow_frame,
                                    const Instruction* inst,
                                    uint16_t inst_data,
                                    JValue* result,
                                    mirror::VarHandle::AccessMode access_mode)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Make sure to check for async exceptions
  if (UNLIKELY(self->ObserveAsyncException())) {
    return false;
  }

  StackHandleScope<2> hs(self);
  bool is_var_args = inst->HasVarArgs();
  const uint16_t vRegH = is_var_args ? inst->VRegH_45cc() : inst->VRegH_4rcc();
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::MethodType> callsite_type(hs.NewHandle(
      class_linker->ResolveMethodType(self, dex::ProtoIndex(vRegH), shadow_frame.GetMethod())));
  // This implies we couldn't resolve one or more types in this VarHandle.
  if (UNLIKELY(callsite_type == nullptr)) {
    CHECK(self->IsExceptionPending());
    return false;
  }

  const uint32_t vRegC = is_var_args ? inst->VRegC_45cc() : inst->VRegC_4rcc();
  ObjPtr<mirror::Object> receiver(shadow_frame.GetVRegReference(vRegC));
  Handle<mirror::VarHandle> var_handle(hs.NewHandle(ObjPtr<mirror::VarHandle>::DownCast(receiver)));
  if (is_var_args) {
    uint32_t args[Instruction::kMaxVarArgRegs];
    inst->GetVarArgs(args, inst_data);
    VarArgsInstructionOperands all_operands(args, inst->VRegA_45cc());
    NoReceiverInstructionOperands operands(&all_operands);
    return VarHandleInvokeAccessor(self,
                                   shadow_frame,
                                   var_handle,
                                   callsite_type,
                                   access_mode,
                                   &operands,
                                   result);
  } else {
    RangeInstructionOperands all_operands(inst->VRegC_4rcc(), inst->VRegA_4rcc());
    NoReceiverInstructionOperands operands(&all_operands);
    return VarHandleInvokeAccessor(self,
                                   shadow_frame,
                                   var_handle,
                                   callsite_type,
                                   access_mode,
                                   &operands,
                                   result);
  }
}

#define DO_VAR_HANDLE_ACCESSOR(_access_mode)                                                \
bool DoVarHandle ## _access_mode(Thread* self,                                              \
                                 ShadowFrame& shadow_frame,                                 \
                                 const Instruction* inst,                                   \
                                 uint16_t inst_data,                                        \
                                 JValue* result) REQUIRES_SHARED(Locks::mutator_lock_) {    \
  const auto access_mode = mirror::VarHandle::AccessMode::k ## _access_mode;                \
  return DoVarHandleInvokeCommon(self, shadow_frame, inst, inst_data, result, access_mode); \
}

DO_VAR_HANDLE_ACCESSOR(CompareAndExchange)
DO_VAR_HANDLE_ACCESSOR(CompareAndExchangeAcquire)
DO_VAR_HANDLE_ACCESSOR(CompareAndExchangeRelease)
DO_VAR_HANDLE_ACCESSOR(CompareAndSet)
DO_VAR_HANDLE_ACCESSOR(Get)
DO_VAR_HANDLE_ACCESSOR(GetAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndAdd)
DO_VAR_HANDLE_ACCESSOR(GetAndAddAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndAddRelease)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseAnd)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseAndAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseAndRelease)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseOr)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseOrAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseOrRelease)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseXor)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseXorAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndBitwiseXorRelease)
DO_VAR_HANDLE_ACCESSOR(GetAndSet)
DO_VAR_HANDLE_ACCESSOR(GetAndSetAcquire)
DO_VAR_HANDLE_ACCESSOR(GetAndSetRelease)
DO_VAR_HANDLE_ACCESSOR(GetOpaque)
DO_VAR_HANDLE_ACCESSOR(GetVolatile)
DO_VAR_HANDLE_ACCESSOR(Set)
DO_VAR_HANDLE_ACCESSOR(SetOpaque)
DO_VAR_HANDLE_ACCESSOR(SetRelease)
DO_VAR_HANDLE_ACCESSOR(SetVolatile)
DO_VAR_HANDLE_ACCESSOR(WeakCompareAndSet)
DO_VAR_HANDLE_ACCESSOR(WeakCompareAndSetAcquire)
DO_VAR_HANDLE_ACCESSOR(WeakCompareAndSetPlain)
DO_VAR_HANDLE_ACCESSOR(WeakCompareAndSetRelease)

#undef DO_VAR_HANDLE_ACCESSOR

template<bool is_range>
bool DoInvokePolymorphic(Thread* self,
                         ShadowFrame& shadow_frame,
                         const Instruction* inst,
                         uint16_t inst_data,
                         JValue* result) {
  const int invoke_method_idx = inst->VRegB();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ArtMethod* invoke_method =
      class_linker->ResolveMethod<ClassLinker::ResolveMode::kCheckICCEAndIAE>(
          self, invoke_method_idx, shadow_frame.GetMethod(), kVirtual);

  // Ensure intrinsic identifiers are initialized.
  DCHECK(invoke_method->IsIntrinsic());

  // Dispatch based on intrinsic identifier associated with method.
  switch (static_cast<art::Intrinsics>(invoke_method->GetIntrinsic())) {
#define CASE_SIGNATURE_POLYMORPHIC_INTRINSIC(Name, ...) \
    case Intrinsics::k##Name:                           \
      return Do ## Name(self, shadow_frame, inst, inst_data, result);
#include "intrinsics_list.h"
    SIGNATURE_POLYMORPHIC_INTRINSICS_LIST(CASE_SIGNATURE_POLYMORPHIC_INTRINSIC)
#undef INTRINSICS_LIST
#undef SIGNATURE_POLYMORPHIC_INTRINSICS_LIST
#undef CASE_SIGNATURE_POLYMORPHIC_INTRINSIC
    default:
      LOG(FATAL) << "Unreachable: " << invoke_method->GetIntrinsic();
      UNREACHABLE();
      return false;
  }
}

static JValue ConvertScalarBootstrapArgument(jvalue value) {
  // value either contains a primitive scalar value if it corresponds
  // to a primitive type, or it contains an integer value if it
  // corresponds to an object instance reference id (e.g. a string id).
  return JValue::FromPrimitive(value.j);
}

static ObjPtr<mirror::Class> GetClassForBootstrapArgument(EncodedArrayValueIterator::ValueType type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots = class_linker->GetClassRoots();
  switch (type) {
    case EncodedArrayValueIterator::ValueType::kBoolean:
    case EncodedArrayValueIterator::ValueType::kByte:
    case EncodedArrayValueIterator::ValueType::kChar:
    case EncodedArrayValueIterator::ValueType::kShort:
      // These types are disallowed by JVMS. Treat as integers. This
      // will result in CCE's being raised if the BSM has one of these
      // types.
    case EncodedArrayValueIterator::ValueType::kInt:
      return GetClassRoot(ClassRoot::kPrimitiveInt, class_roots);
    case EncodedArrayValueIterator::ValueType::kLong:
      return GetClassRoot(ClassRoot::kPrimitiveLong, class_roots);
    case EncodedArrayValueIterator::ValueType::kFloat:
      return GetClassRoot(ClassRoot::kPrimitiveFloat, class_roots);
    case EncodedArrayValueIterator::ValueType::kDouble:
      return GetClassRoot(ClassRoot::kPrimitiveDouble, class_roots);
    case EncodedArrayValueIterator::ValueType::kMethodType:
      return GetClassRoot<mirror::MethodType>(class_roots);
    case EncodedArrayValueIterator::ValueType::kMethodHandle:
      return GetClassRoot<mirror::MethodHandle>(class_roots);
    case EncodedArrayValueIterator::ValueType::kString:
      return GetClassRoot<mirror::String>();
    case EncodedArrayValueIterator::ValueType::kType:
      return GetClassRoot<mirror::Class>();
    case EncodedArrayValueIterator::ValueType::kField:
    case EncodedArrayValueIterator::ValueType::kMethod:
    case EncodedArrayValueIterator::ValueType::kEnum:
    case EncodedArrayValueIterator::ValueType::kArray:
    case EncodedArrayValueIterator::ValueType::kAnnotation:
    case EncodedArrayValueIterator::ValueType::kNull:
      return nullptr;
  }
}

static bool GetArgumentForBootstrapMethod(Thread* self,
                                          ArtMethod* referrer,
                                          EncodedArrayValueIterator::ValueType type,
                                          const JValue* encoded_value,
                                          JValue* decoded_value)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // The encoded_value contains either a scalar value (IJDF) or a
  // scalar DEX file index to a reference type to be materialized.
  switch (type) {
    case EncodedArrayValueIterator::ValueType::kInt:
    case EncodedArrayValueIterator::ValueType::kFloat:
      decoded_value->SetI(encoded_value->GetI());
      return true;
    case EncodedArrayValueIterator::ValueType::kLong:
    case EncodedArrayValueIterator::ValueType::kDouble:
      decoded_value->SetJ(encoded_value->GetJ());
      return true;
    case EncodedArrayValueIterator::ValueType::kMethodType: {
      StackHandleScope<2> hs(self);
      Handle<mirror::ClassLoader> class_loader(hs.NewHandle(referrer->GetClassLoader()));
      Handle<mirror::DexCache> dex_cache(hs.NewHandle(referrer->GetDexCache()));
      dex::ProtoIndex proto_idx(encoded_value->GetC());
      ClassLinker* cl = Runtime::Current()->GetClassLinker();
      ObjPtr<mirror::MethodType> o =
          cl->ResolveMethodType(self, proto_idx, dex_cache, class_loader);
      if (UNLIKELY(o.IsNull())) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      decoded_value->SetL(o);
      return true;
    }
    case EncodedArrayValueIterator::ValueType::kMethodHandle: {
      uint32_t index = static_cast<uint32_t>(encoded_value->GetI());
      ClassLinker* cl = Runtime::Current()->GetClassLinker();
      ObjPtr<mirror::MethodHandle> o = cl->ResolveMethodHandle(self, index, referrer);
      if (UNLIKELY(o.IsNull())) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      decoded_value->SetL(o);
      return true;
    }
    case EncodedArrayValueIterator::ValueType::kString: {
      dex::StringIndex index(static_cast<uint32_t>(encoded_value->GetI()));
      ClassLinker* cl = Runtime::Current()->GetClassLinker();
      ObjPtr<mirror::String> o = cl->ResolveString(index, referrer);
      if (UNLIKELY(o.IsNull())) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      decoded_value->SetL(o);
      return true;
    }
    case EncodedArrayValueIterator::ValueType::kType: {
      dex::TypeIndex index(static_cast<uint32_t>(encoded_value->GetI()));
      ClassLinker* cl = Runtime::Current()->GetClassLinker();
      ObjPtr<mirror::Class> o = cl->ResolveType(index, referrer);
      if (UNLIKELY(o.IsNull())) {
        DCHECK(self->IsExceptionPending());
        return false;
      }
      decoded_value->SetL(o);
      return true;
    }
    case EncodedArrayValueIterator::ValueType::kBoolean:
    case EncodedArrayValueIterator::ValueType::kByte:
    case EncodedArrayValueIterator::ValueType::kChar:
    case EncodedArrayValueIterator::ValueType::kShort:
    case EncodedArrayValueIterator::ValueType::kField:
    case EncodedArrayValueIterator::ValueType::kMethod:
    case EncodedArrayValueIterator::ValueType::kEnum:
    case EncodedArrayValueIterator::ValueType::kArray:
    case EncodedArrayValueIterator::ValueType::kAnnotation:
    case EncodedArrayValueIterator::ValueType::kNull:
      // Unreachable - unsupported types that have been checked when
      // determining the effect call site type based on the bootstrap
      // argument types.
      UNREACHABLE();
  }
}

static bool PackArgumentForBootstrapMethod(Thread* self,
                                           ArtMethod* referrer,
                                           CallSiteArrayValueIterator* it,
                                           ShadowFrameSetter* setter)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  auto type = it->GetValueType();
  const JValue encoded_value = ConvertScalarBootstrapArgument(it->GetJavaValue());
  JValue decoded_value;
  if (!GetArgumentForBootstrapMethod(self, referrer, type, &encoded_value, &decoded_value)) {
    return false;
  }
  switch (it->GetValueType()) {
    case EncodedArrayValueIterator::ValueType::kInt:
    case EncodedArrayValueIterator::ValueType::kFloat:
      setter->Set(static_cast<uint32_t>(decoded_value.GetI()));
      return true;
    case EncodedArrayValueIterator::ValueType::kLong:
    case EncodedArrayValueIterator::ValueType::kDouble:
      setter->SetLong(decoded_value.GetJ());
      return true;
    case EncodedArrayValueIterator::ValueType::kMethodType:
    case EncodedArrayValueIterator::ValueType::kMethodHandle:
    case EncodedArrayValueIterator::ValueType::kString:
    case EncodedArrayValueIterator::ValueType::kType:
      setter->SetReference(decoded_value.GetL());
      return true;
    case EncodedArrayValueIterator::ValueType::kBoolean:
    case EncodedArrayValueIterator::ValueType::kByte:
    case EncodedArrayValueIterator::ValueType::kChar:
    case EncodedArrayValueIterator::ValueType::kShort:
    case EncodedArrayValueIterator::ValueType::kField:
    case EncodedArrayValueIterator::ValueType::kMethod:
    case EncodedArrayValueIterator::ValueType::kEnum:
    case EncodedArrayValueIterator::ValueType::kArray:
    case EncodedArrayValueIterator::ValueType::kAnnotation:
    case EncodedArrayValueIterator::ValueType::kNull:
      // Unreachable - unsupported types that have been checked when
      // determining the effect call site type based on the bootstrap
      // argument types.
      UNREACHABLE();
  }
}

static bool PackCollectorArrayForBootstrapMethod(Thread* self,
                                                 ArtMethod* referrer,
                                                 ObjPtr<mirror::Class> array_type,
                                                 int32_t array_length,
                                                 CallSiteArrayValueIterator* it,
                                                 ShadowFrameSetter* setter)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  JValue decoded_value;

#define COLLECT_PRIMITIVE_ARRAY(Descriptor, Type)                       \
  Handle<mirror::Type ## Array> array =                                 \
      hs.NewHandle(mirror::Type ## Array::Alloc(self, array_length));   \
  if (array.IsNull()) {                                                 \
    return false;                                                       \
  }                                                                     \
  for (int32_t i = 0; it->HasNext(); it->Next(), ++i) {                 \
    auto type = it->GetValueType();                                     \
    DCHECK_EQ(type, EncodedArrayValueIterator::ValueType::k ## Type);   \
    const JValue encoded_value =                                        \
        ConvertScalarBootstrapArgument(it->GetJavaValue());             \
    GetArgumentForBootstrapMethod(self,                                 \
                                  referrer,                             \
                                  type,                                 \
                                  &encoded_value,                       \
                                  &decoded_value);                      \
    array->Set(i, decoded_value.Get ## Descriptor());                   \
  }                                                                     \
  setter->SetReference(array.Get());                                    \
  return true;

#define COLLECT_REFERENCE_ARRAY(T, Type)                                \
  Handle<mirror::ObjectArray<T>> array =                   /* NOLINT */ \
      hs.NewHandle(mirror::ObjectArray<T>::Alloc(self,                  \
                                                 array_type,            \
                                                 array_length));        \
  if (array.IsNull()) {                                                 \
    return false;                                                       \
  }                                                                     \
  for (int32_t i = 0; it->HasNext(); it->Next(), ++i) {                 \
    auto type = it->GetValueType();                                     \
    DCHECK_EQ(type, EncodedArrayValueIterator::ValueType::k ## Type);   \
    const JValue encoded_value =                                        \
        ConvertScalarBootstrapArgument(it->GetJavaValue());             \
    if (!GetArgumentForBootstrapMethod(self,                            \
                                       referrer,                        \
                                       type,                            \
                                       &encoded_value,                  \
                                       &decoded_value)) {               \
      return false;                                                     \
    }                                                                   \
    ObjPtr<mirror::Object> o = decoded_value.GetL();                    \
    if (Runtime::Current()->IsActiveTransaction()) {                    \
      array->Set<true>(i, ObjPtr<T>::DownCast(o));                      \
    } else {                                                            \
      array->Set<false>(i, ObjPtr<T>::DownCast(o));                     \
    }                                                                   \
  }                                                                     \
  setter->SetReference(array.Get());                                    \
  return true;

  ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots = class_linker->GetClassRoots();
  ObjPtr<mirror::Class> component_type = array_type->GetComponentType();
  if (component_type == GetClassRoot(ClassRoot::kPrimitiveInt, class_roots)) {
    COLLECT_PRIMITIVE_ARRAY(I, Int);
  } else if (component_type == GetClassRoot(ClassRoot::kPrimitiveLong, class_roots)) {
    COLLECT_PRIMITIVE_ARRAY(J, Long);
  } else if (component_type == GetClassRoot(ClassRoot::kPrimitiveFloat, class_roots)) {
    COLLECT_PRIMITIVE_ARRAY(F, Float);
  } else if (component_type == GetClassRoot(ClassRoot::kPrimitiveDouble, class_roots)) {
    COLLECT_PRIMITIVE_ARRAY(D, Double);
  } else if (component_type == GetClassRoot<mirror::MethodType>()) {
    COLLECT_REFERENCE_ARRAY(mirror::MethodType, MethodType);
  } else if (component_type == GetClassRoot<mirror::MethodHandle>()) {
    COLLECT_REFERENCE_ARRAY(mirror::MethodHandle, MethodHandle);
  } else if (component_type == GetClassRoot<mirror::String>(class_roots)) {
    COLLECT_REFERENCE_ARRAY(mirror::String, String);
  } else if (component_type == GetClassRoot<mirror::Class>()) {
    COLLECT_REFERENCE_ARRAY(mirror::Class, Type);
  } else {
    UNREACHABLE();
  }
  #undef COLLECT_PRIMITIVE_ARRAY
  #undef COLLECT_REFERENCE_ARRAY
}

static ObjPtr<mirror::MethodType> BuildCallSiteForBootstrapMethod(Thread* self,
                                                                  const DexFile* dex_file,
                                                                  uint32_t call_site_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const dex::CallSiteIdItem& csi = dex_file->GetCallSiteId(call_site_idx);
  CallSiteArrayValueIterator it(*dex_file, csi);
  DCHECK_GE(it.Size(), 1u);

  StackHandleScope<2> hs(self);
  // Create array for parameter types.
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  ObjPtr<mirror::Class> class_array_type =
      GetClassRoot<mirror::ObjectArray<mirror::Class>>(class_linker);
  Handle<mirror::ObjectArray<mirror::Class>> ptypes = hs.NewHandle(
      mirror::ObjectArray<mirror::Class>::Alloc(self,
                                                class_array_type,
                                                static_cast<int>(it.Size())));
  if (ptypes.IsNull()) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  // Populate the first argument with an instance of j.l.i.MethodHandles.Lookup
  // that the runtime will construct.
  ptypes->Set(0, GetClassRoot<mirror::MethodHandlesLookup>(class_linker));
  it.Next();

  // The remaining parameter types are derived from the types of
  // arguments present in the DEX file.
  int index = 1;
  while (it.HasNext()) {
    ObjPtr<mirror::Class> ptype = GetClassForBootstrapArgument(it.GetValueType());
    if (ptype.IsNull()) {
      ThrowClassCastException("Unsupported bootstrap argument type");
      return nullptr;
    }
    ptypes->Set(index, ptype);
    index++;
    it.Next();
  }
  DCHECK_EQ(static_cast<size_t>(index), it.Size());

  // By definition, the return type is always a j.l.i.CallSite.
  Handle<mirror::Class> rtype = hs.NewHandle(GetClassRoot<mirror::CallSite>());
  return mirror::MethodType::Create(self, rtype, ptypes);
}

static ObjPtr<mirror::CallSite> InvokeBootstrapMethod(Thread* self,
                                                      ShadowFrame& shadow_frame,
                                                      uint32_t call_site_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<5> hs(self);
  // There are three mandatory arguments expected from the call site
  // value array in the DEX file: the bootstrap method handle, the
  // method name to pass to the bootstrap method, and the method type
  // to pass to the bootstrap method.
  static constexpr size_t kMandatoryArgumentsCount = 3;
  ArtMethod* referrer = shadow_frame.GetMethod();
  const DexFile* dex_file = referrer->GetDexFile();
  const dex::CallSiteIdItem& csi = dex_file->GetCallSiteId(call_site_idx);
  CallSiteArrayValueIterator it(*dex_file, csi);
  if (it.Size() < kMandatoryArgumentsCount) {
    ThrowBootstrapMethodError("Truncated bootstrap arguments (%zu < %zu)",
                              it.Size(), kMandatoryArgumentsCount);
    return nullptr;
  }

  if (it.GetValueType() != EncodedArrayValueIterator::ValueType::kMethodHandle) {
    ThrowBootstrapMethodError("First bootstrap argument is not a method handle");
    return nullptr;
  }

  uint32_t bsm_index = static_cast<uint32_t>(it.GetJavaValue().i);
  it.Next();

  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::MethodHandle> bsm =
      hs.NewHandle(class_linker->ResolveMethodHandle(self, bsm_index, referrer));
  if (bsm.IsNull()) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  if (bsm->GetHandleKind() != mirror::MethodHandle::Kind::kInvokeStatic) {
    // JLS suggests also accepting constructors. This is currently
    // hard as constructor invocations happen via transformers in ART
    // today. The constructor would need to be a class derived from java.lang.invoke.CallSite.
    ThrowBootstrapMethodError("Unsupported bootstrap method invocation kind");
    return nullptr;
  }

  // Construct the local call site type information based on the 3
  // mandatory arguments provided by the runtime and the static arguments
  // in the DEX file. We will use these arguments to build a shadow frame.
  MutableHandle<mirror::MethodType> call_site_type =
      hs.NewHandle(BuildCallSiteForBootstrapMethod(self, dex_file, call_site_idx));
  if (call_site_type.IsNull()) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  // Check if this BSM is targeting a variable arity method. If so,
  // we'll need to collect the trailing arguments into an array.
  Handle<mirror::Array> collector_arguments;
  int32_t collector_arguments_length;
  if (bsm->GetTargetMethod()->IsVarargs()) {
    int number_of_bsm_parameters = bsm->GetMethodType()->GetNumberOfPTypes();
    if (number_of_bsm_parameters == 0) {
      ThrowBootstrapMethodError("Variable arity BSM does not have any arguments");
      return nullptr;
    }
    Handle<mirror::Class> collector_array_class =
        hs.NewHandle(bsm->GetMethodType()->GetPTypes()->Get(number_of_bsm_parameters - 1));
    if (!collector_array_class->IsArrayClass()) {
      ThrowBootstrapMethodError("Variable arity BSM does not have array as final argument");
      return nullptr;
    }
    // The call site may include no arguments to be collected. In this
    // case the number of arguments must be at least the number of BSM
    // parameters less the collector array.
    if (call_site_type->GetNumberOfPTypes() < number_of_bsm_parameters - 1) {
      ThrowWrongMethodTypeException(bsm->GetMethodType(), call_site_type.Get());
      return nullptr;
    }
    // Check all the arguments to be collected match the collector array component type.
    for (int i = number_of_bsm_parameters - 1; i < call_site_type->GetNumberOfPTypes(); ++i) {
      if (call_site_type->GetPTypes()->Get(i) != collector_array_class->GetComponentType()) {
        ThrowClassCastException(collector_array_class->GetComponentType(),
                                call_site_type->GetPTypes()->Get(i));
        return nullptr;
      }
    }
    // Update the call site method type so it now includes the collector array.
    int32_t collector_arguments_start = number_of_bsm_parameters - 1;
    collector_arguments_length = call_site_type->GetNumberOfPTypes() - number_of_bsm_parameters + 1;
    call_site_type.Assign(
        mirror::MethodType::CollectTrailingArguments(self,
                                                     call_site_type.Get(),
                                                     collector_array_class.Get(),
                                                     collector_arguments_start));
    if (call_site_type.IsNull()) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
  } else {
    collector_arguments_length = 0;
  }

  if (call_site_type->GetNumberOfPTypes() != bsm->GetMethodType()->GetNumberOfPTypes()) {
    ThrowWrongMethodTypeException(bsm->GetMethodType(), call_site_type.Get());
    return nullptr;
  }

  // BSM invocation has a different set of exceptions that
  // j.l.i.MethodHandle.invoke(). Scan arguments looking for CCE
  // "opportunities". Unfortunately we cannot just leave this to the
  // method handle invocation as this might generate a WMTE.
  for (int32_t i = 0; i < call_site_type->GetNumberOfPTypes(); ++i) {
    ObjPtr<mirror::Class> from = call_site_type->GetPTypes()->Get(i);
    ObjPtr<mirror::Class> to = bsm->GetMethodType()->GetPTypes()->Get(i);
    if (!IsParameterTypeConvertible(from, to)) {
      ThrowClassCastException(from, to);
      return nullptr;
    }
  }
  if (!IsReturnTypeConvertible(call_site_type->GetRType(), bsm->GetMethodType()->GetRType())) {
    ThrowClassCastException(bsm->GetMethodType()->GetRType(), call_site_type->GetRType());
    return nullptr;
  }

  // Set-up a shadow frame for invoking the bootstrap method handle.
  ShadowFrameAllocaUniquePtr bootstrap_frame =
      CREATE_SHADOW_FRAME(call_site_type->NumberOfVRegs(),
                          nullptr,
                          referrer,
                          shadow_frame.GetDexPC());
  ScopedStackedShadowFramePusher pusher(
      self, bootstrap_frame.get(), StackedShadowFrameType::kShadowFrameUnderConstruction);
  ShadowFrameSetter setter(bootstrap_frame.get(), 0u);

  // The first parameter is a MethodHandles lookup instance.
  Handle<mirror::Class> lookup_class =
      hs.NewHandle(shadow_frame.GetMethod()->GetDeclaringClass());
  ObjPtr<mirror::MethodHandlesLookup> lookup =
      mirror::MethodHandlesLookup::Create(self, lookup_class);
  if (lookup.IsNull()) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }
  setter.SetReference(lookup);

  // Pack the remaining arguments into the frame.
  int number_of_arguments = call_site_type->GetNumberOfPTypes();
  int argument_index;
  for (argument_index = 1; argument_index < number_of_arguments; ++argument_index) {
    if (argument_index == number_of_arguments - 1 &&
        call_site_type->GetPTypes()->Get(argument_index)->IsArrayClass()) {
      ObjPtr<mirror::Class> array_type = call_site_type->GetPTypes()->Get(argument_index);
      if (!PackCollectorArrayForBootstrapMethod(self,
                                                referrer,
                                                array_type,
                                                collector_arguments_length,
                                                &it,
                                                &setter)) {
        DCHECK(self->IsExceptionPending());
        return nullptr;
      }
    } else if (!PackArgumentForBootstrapMethod(self, referrer, &it, &setter)) {
      DCHECK(self->IsExceptionPending());
      return nullptr;
    }
    it.Next();
  }
  DCHECK(!it.HasNext());
  DCHECK(setter.Done());

  // Invoke the bootstrap method handle.
  JValue result;
  RangeInstructionOperands operands(0, bootstrap_frame->NumberOfVRegs());
  bool invoke_success = MethodHandleInvoke(self,
                                           *bootstrap_frame,
                                           bsm,
                                           call_site_type,
                                           &operands,
                                           &result);
  if (!invoke_success) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  Handle<mirror::Object> object(hs.NewHandle(result.GetL()));
  if (UNLIKELY(object.IsNull())) {
    // This will typically be for LambdaMetafactory which is not supported.
    ThrowClassCastException("Bootstrap method returned null");
    return nullptr;
  }

  // Check the result type is a subclass of j.l.i.CallSite.
  ObjPtr<mirror::Class> call_site_class = GetClassRoot<mirror::CallSite>(class_linker);
  if (UNLIKELY(!object->InstanceOf(call_site_class))) {
    ThrowClassCastException(object->GetClass(), call_site_class);
    return nullptr;
  }

  // Check the call site target is not null as we're going to invoke it.
  ObjPtr<mirror::CallSite> call_site = ObjPtr<mirror::CallSite>::DownCast(result.GetL());
  ObjPtr<mirror::MethodHandle> target = call_site->GetTarget();
  if (UNLIKELY(target == nullptr)) {
    ThrowClassCastException("Bootstrap method returned a CallSite with a null target");
    return nullptr;
  }
  return call_site;
}

namespace {

ObjPtr<mirror::CallSite> DoResolveCallSite(Thread* self,
                                           ShadowFrame& shadow_frame,
                                           uint32_t call_site_idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  StackHandleScope<1> hs(self);
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(shadow_frame.GetMethod()->GetDexCache()));

  // Get the call site from the DexCache if present.
  ObjPtr<mirror::CallSite> call_site = dex_cache->GetResolvedCallSite(call_site_idx);
  if (LIKELY(call_site != nullptr)) {
    return call_site;
  }

  // Invoke the bootstrap method to get a candidate call site.
  call_site = InvokeBootstrapMethod(self, shadow_frame, call_site_idx);
  if (UNLIKELY(call_site == nullptr)) {
    if (!self->GetException()->IsError()) {
      // Use a BootstrapMethodError if the exception is not an instance of java.lang.Error.
      ThrowWrappedBootstrapMethodError("Exception from call site #%u bootstrap method",
                                       call_site_idx);
    }
    return nullptr;
  }

  // Attempt to place the candidate call site into the DexCache, return the winning call site.
  return dex_cache->SetResolvedCallSite(call_site_idx, call_site);
}

}  // namespace

bool DoInvokeCustom(Thread* self,
                    ShadowFrame& shadow_frame,
                    uint32_t call_site_idx,
                    const InstructionOperands* operands,
                    JValue* result) {
  // Make sure to check for async exceptions
  if (UNLIKELY(self->ObserveAsyncException())) {
    return false;
  }

  // invoke-custom is not supported in transactions. In transactions
  // there is a limited set of types supported. invoke-custom allows
  // running arbitrary code and instantiating arbitrary types.
  CHECK(!Runtime::Current()->IsActiveTransaction());

  ObjPtr<mirror::CallSite> call_site = DoResolveCallSite(self, shadow_frame, call_site_idx);
  if (call_site.IsNull()) {
    DCHECK(self->IsExceptionPending());
    return false;
  }

  StackHandleScope<2> hs(self);
  Handle<mirror::MethodHandle> target = hs.NewHandle(call_site->GetTarget());
  Handle<mirror::MethodType> target_method_type = hs.NewHandle(target->GetMethodType());
  DCHECK_EQ(operands->GetNumberOfOperands(), target_method_type->NumberOfVRegs())
      << " call_site_idx" << call_site_idx;
  return MethodHandleInvokeExact(self,
                                 shadow_frame,
                                 target,
                                 target_method_type,
                                 operands,
                                 result);
}

// Assign register 'src_reg' from shadow_frame to register 'dest_reg' into new_shadow_frame.
static inline void AssignRegister(ShadowFrame* new_shadow_frame, const ShadowFrame& shadow_frame,
                                  size_t dest_reg, size_t src_reg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Uint required, so that sign extension does not make this wrong on 64b systems
  uint32_t src_value = shadow_frame.GetVReg(src_reg);
  ObjPtr<mirror::Object> o = shadow_frame.GetVRegReference<kVerifyNone>(src_reg);

  // If both register locations contains the same value, the register probably holds a reference.
  // Note: As an optimization, non-moving collectors leave a stale reference value
  // in the references array even after the original vreg was overwritten to a non-reference.
  if (src_value == reinterpret_cast32<uint32_t>(o.Ptr())) {
    new_shadow_frame->SetVRegReference(dest_reg, o);
  } else {
    new_shadow_frame->SetVReg(dest_reg, src_value);
  }
}

template <bool is_range>
inline void CopyRegisters(ShadowFrame& caller_frame,
                          ShadowFrame* callee_frame,
                          const uint32_t (&arg)[Instruction::kMaxVarArgRegs],
                          const size_t first_src_reg,
                          const size_t first_dest_reg,
                          const size_t num_regs) {
  if (is_range) {
    const size_t dest_reg_bound = first_dest_reg + num_regs;
    for (size_t src_reg = first_src_reg, dest_reg = first_dest_reg; dest_reg < dest_reg_bound;
        ++dest_reg, ++src_reg) {
      AssignRegister(callee_frame, caller_frame, dest_reg, src_reg);
    }
  } else {
    DCHECK_LE(num_regs, arraysize(arg));

    for (size_t arg_index = 0; arg_index < num_regs; ++arg_index) {
      AssignRegister(callee_frame, caller_frame, first_dest_reg + arg_index, arg[arg_index]);
    }
  }
}

template <bool is_range,
          bool do_assignability_check>
static inline bool DoCallCommon(ArtMethod* called_method,
                                Thread* self,
                                ShadowFrame& shadow_frame,
                                JValue* result,
                                uint16_t number_of_inputs,
                                uint32_t (&arg)[Instruction::kMaxVarArgRegs],
                                uint32_t vregC) {
  bool string_init = false;
  // Replace calls to String.<init> with equivalent StringFactory call.
  if (UNLIKELY(called_method->GetDeclaringClass()->IsStringClass()
               && called_method->IsConstructor())) {
    called_method = WellKnownClasses::StringInitToStringFactory(called_method);
    string_init = true;
  }

  // Compute method information.
  CodeItemDataAccessor accessor(called_method->DexInstructionData());
  // Number of registers for the callee's call frame.
  uint16_t num_regs;
  // Test whether to use the interpreter or compiler entrypoint, and save that result to pass to
  // PerformCall. A deoptimization could occur at any time, and we shouldn't change which
  // entrypoint to use once we start building the shadow frame.

  // For unstarted runtimes, always use the interpreter entrypoint. This fixes the case where we are
  // doing cross compilation. Note that GetEntryPointFromQuickCompiledCode doesn't use the image
  // pointer size here and this may case an overflow if it is called from the compiler. b/62402160
  const bool use_interpreter_entrypoint = !Runtime::Current()->IsStarted() ||
      ClassLinker::ShouldUseInterpreterEntrypoint(
          called_method,
          called_method->GetEntryPointFromQuickCompiledCode());
  if (LIKELY(accessor.HasCodeItem())) {
    // When transitioning to compiled code, space only needs to be reserved for the input registers.
    // The rest of the frame gets discarded. This also prevents accessing the called method's code
    // item, saving memory by keeping code items of compiled code untouched.
    if (!use_interpreter_entrypoint) {
      DCHECK(!Runtime::Current()->IsAotCompiler()) << "Compiler should use interpreter entrypoint";
      num_regs = number_of_inputs;
    } else {
      num_regs = accessor.RegistersSize();
      DCHECK_EQ(string_init ? number_of_inputs - 1 : number_of_inputs, accessor.InsSize());
    }
  } else {
    DCHECK(called_method->IsNative() || called_method->IsProxyMethod());
    num_regs = number_of_inputs;
  }

  // Hack for String init:
  //
  // Rewrite invoke-x java.lang.String.<init>(this, a, b, c, ...) into:
  //         invoke-x StringFactory(a, b, c, ...)
  // by effectively dropping the first virtual register from the invoke.
  //
  // (at this point the ArtMethod has already been replaced,
  // so we just need to fix-up the arguments)
  //
  // Note that FindMethodFromCode in entrypoint_utils-inl.h was also special-cased
  // to handle the compiler optimization of replacing `this` with null without
  // throwing NullPointerException.
  uint32_t string_init_vreg_this = is_range ? vregC : arg[0];
  if (UNLIKELY(string_init)) {
    DCHECK_GT(num_regs, 0u);  // As the method is an instance method, there should be at least 1.

    // The new StringFactory call is static and has one fewer argument.
    if (!accessor.HasCodeItem()) {
      DCHECK(called_method->IsNative() || called_method->IsProxyMethod());
      num_regs--;
    }  // else ... don't need to change num_regs since it comes up from the string_init's code item
    number_of_inputs--;

    // Rewrite the var-args, dropping the 0th argument ("this")
    for (uint32_t i = 1; i < arraysize(arg); ++i) {
      arg[i - 1] = arg[i];
    }
    arg[arraysize(arg) - 1] = 0;

    // Rewrite the non-var-arg case
    vregC++;  // Skips the 0th vreg in the range ("this").
  }

  // Parameter registers go at the end of the shadow frame.
  DCHECK_GE(num_regs, number_of_inputs);
  size_t first_dest_reg = num_regs - number_of_inputs;
  DCHECK_NE(first_dest_reg, (size_t)-1);

  // Allocate shadow frame on the stack.
  const char* old_cause = self->StartAssertNoThreadSuspension("DoCallCommon");
  ShadowFrameAllocaUniquePtr shadow_frame_unique_ptr =
      CREATE_SHADOW_FRAME(num_regs, &shadow_frame, called_method, /* dex pc */ 0);
  ShadowFrame* new_shadow_frame = shadow_frame_unique_ptr.get();

  // Initialize new shadow frame by copying the registers from the callee shadow frame.
  if (do_assignability_check) {
    // Slow path.
    // We might need to do class loading, which incurs a thread state change to kNative. So
    // register the shadow frame as under construction and allow suspension again.
    ScopedStackedShadowFramePusher pusher(
        self, new_shadow_frame, StackedShadowFrameType::kShadowFrameUnderConstruction);
    self->EndAssertNoThreadSuspension(old_cause);

    // ArtMethod here is needed to check type information of the call site against the callee.
    // Type information is retrieved from a DexFile/DexCache for that respective declared method.
    //
    // As a special case for proxy methods, which are not dex-backed,
    // we have to retrieve type information from the proxy's method
    // interface method instead (which is dex backed since proxies are never interfaces).
    ArtMethod* method =
        new_shadow_frame->GetMethod()->GetInterfaceMethodIfProxy(kRuntimePointerSize);

    // We need to do runtime check on reference assignment. We need to load the shorty
    // to get the exact type of each reference argument.
    const dex::TypeList* params = method->GetParameterTypeList();
    uint32_t shorty_len = 0;
    const char* shorty = method->GetShorty(&shorty_len);

    // Handle receiver apart since it's not part of the shorty.
    size_t dest_reg = first_dest_reg;
    size_t arg_offset = 0;

    if (!method->IsStatic()) {
      size_t receiver_reg = is_range ? vregC : arg[0];
      new_shadow_frame->SetVRegReference(dest_reg, shadow_frame.GetVRegReference(receiver_reg));
      ++dest_reg;
      ++arg_offset;
      DCHECK(!string_init);  // All StringFactory methods are static.
    }

    // Copy the caller's invoke-* arguments into the callee's parameter registers.
    for (uint32_t shorty_pos = 0; dest_reg < num_regs; ++shorty_pos, ++dest_reg, ++arg_offset) {
      // Skip the 0th 'shorty' type since it represents the return type.
      DCHECK_LT(shorty_pos + 1, shorty_len) << "for shorty '" << shorty << "'";
      const size_t src_reg = (is_range) ? vregC + arg_offset : arg[arg_offset];
      switch (shorty[shorty_pos + 1]) {
        // Handle Object references. 1 virtual register slot.
        case 'L': {
          ObjPtr<mirror::Object> o = shadow_frame.GetVRegReference(src_reg);
          if (do_assignability_check && o != nullptr) {
            const dex::TypeIndex type_idx = params->GetTypeItem(shorty_pos).type_idx_;
            ObjPtr<mirror::Class> arg_type = method->GetDexCache()->GetResolvedType(type_idx);
            if (arg_type == nullptr) {
              StackHandleScope<1> hs(self);
              // Preserve o since it is used below and GetClassFromTypeIndex may cause thread
              // suspension.
              HandleWrapperObjPtr<mirror::Object> h = hs.NewHandleWrapper(&o);
              arg_type = method->ResolveClassFromTypeIndex(type_idx);
              if (arg_type == nullptr) {
                CHECK(self->IsExceptionPending());
                return false;
              }
            }
            if (!o->VerifierInstanceOf(arg_type)) {
              // This should never happen.
              std::string temp1, temp2;
              self->ThrowNewExceptionF("Ljava/lang/InternalError;",
                                       "Invoking %s with bad arg %d, type '%s' not instance of '%s'",
                                       new_shadow_frame->GetMethod()->GetName(), shorty_pos,
                                       o->GetClass()->GetDescriptor(&temp1),
                                       arg_type->GetDescriptor(&temp2));
              return false;
            }
          }
          new_shadow_frame->SetVRegReference(dest_reg, o);
          break;
        }
        // Handle doubles and longs. 2 consecutive virtual register slots.
        case 'J': case 'D': {
          uint64_t wide_value =
              (static_cast<uint64_t>(shadow_frame.GetVReg(src_reg + 1)) << BitSizeOf<uint32_t>()) |
               static_cast<uint32_t>(shadow_frame.GetVReg(src_reg));
          new_shadow_frame->SetVRegLong(dest_reg, wide_value);
          // Skip the next virtual register slot since we already used it.
          ++dest_reg;
          ++arg_offset;
          break;
        }
        // Handle all other primitives that are always 1 virtual register slot.
        default:
          new_shadow_frame->SetVReg(dest_reg, shadow_frame.GetVReg(src_reg));
          break;
      }
    }
  } else {
    if (is_range) {
      DCHECK_EQ(num_regs, first_dest_reg + number_of_inputs);
    }

    CopyRegisters<is_range>(shadow_frame,
                            new_shadow_frame,
                            arg,
                            vregC,
                            first_dest_reg,
                            number_of_inputs);
    self->EndAssertNoThreadSuspension(old_cause);
  }

  PerformCall(self,
              accessor,
              shadow_frame.GetMethod(),
              first_dest_reg,
              new_shadow_frame,
              result,
              use_interpreter_entrypoint);

  if (string_init && !self->IsExceptionPending()) {
    SetStringInitValueToAllAliases(&shadow_frame, string_init_vreg_this, *result);
  }

  return !self->IsExceptionPending();
}

template<bool is_range, bool do_assignability_check>
bool DoCall(ArtMethod* called_method, Thread* self, ShadowFrame& shadow_frame,
            const Instruction* inst, uint16_t inst_data, JValue* result) {
  // Argument word count.
  const uint16_t number_of_inputs =
      (is_range) ? inst->VRegA_3rc(inst_data) : inst->VRegA_35c(inst_data);

  // TODO: find a cleaner way to separate non-range and range information without duplicating
  //       code.
  uint32_t arg[Instruction::kMaxVarArgRegs] = {};  // only used in invoke-XXX.
  uint32_t vregC = 0;
  if (is_range) {
    vregC = inst->VRegC_3rc();
  } else {
    vregC = inst->VRegC_35c();
    inst->GetVarArgs(arg, inst_data);
  }

  return DoCallCommon<is_range, do_assignability_check>(
      called_method, self, shadow_frame,
      result, number_of_inputs, arg, vregC);
}

template <bool is_range, bool do_access_check, bool transaction_active>
bool DoFilledNewArray(const Instruction* inst,
                      const ShadowFrame& shadow_frame,
                      Thread* self,
                      JValue* result) {
  DCHECK(inst->Opcode() == Instruction::FILLED_NEW_ARRAY ||
         inst->Opcode() == Instruction::FILLED_NEW_ARRAY_RANGE);
  const int32_t length = is_range ? inst->VRegA_3rc() : inst->VRegA_35c();
  if (!is_range) {
    // Checks FILLED_NEW_ARRAY's length does not exceed 5 arguments.
    CHECK_LE(length, 5);
  }
  if (UNLIKELY(length < 0)) {
    ThrowNegativeArraySizeException(length);
    return false;
  }
  uint16_t type_idx = is_range ? inst->VRegB_3rc() : inst->VRegB_35c();
  ObjPtr<mirror::Class> array_class = ResolveVerifyAndClinit(dex::TypeIndex(type_idx),
                                                             shadow_frame.GetMethod(),
                                                             self,
                                                             false,
                                                             do_access_check);
  if (UNLIKELY(array_class == nullptr)) {
    DCHECK(self->IsExceptionPending());
    return false;
  }
  CHECK(array_class->IsArrayClass());
  ObjPtr<mirror::Class> component_class = array_class->GetComponentType();
  const bool is_primitive_int_component = component_class->IsPrimitiveInt();
  if (UNLIKELY(component_class->IsPrimitive() && !is_primitive_int_component)) {
    if (component_class->IsPrimitiveLong() || component_class->IsPrimitiveDouble()) {
      ThrowRuntimeException("Bad filled array request for type %s",
                            component_class->PrettyDescriptor().c_str());
    } else {
      self->ThrowNewExceptionF("Ljava/lang/InternalError;",
                               "Found type %s; filled-new-array not implemented for anything but 'int'",
                               component_class->PrettyDescriptor().c_str());
    }
    return false;
  }
  ObjPtr<mirror::Object> new_array = mirror::Array::Alloc(
      self,
      array_class,
      length,
      array_class->GetComponentSizeShift(),
      Runtime::Current()->GetHeap()->GetCurrentAllocator());
  if (UNLIKELY(new_array == nullptr)) {
    self->AssertPendingOOMException();
    return false;
  }
  uint32_t arg[Instruction::kMaxVarArgRegs];  // only used in filled-new-array.
  uint32_t vregC = 0;   // only used in filled-new-array-range.
  if (is_range) {
    vregC = inst->VRegC_3rc();
  } else {
    inst->GetVarArgs(arg);
  }
  for (int32_t i = 0; i < length; ++i) {
    size_t src_reg = is_range ? vregC + i : arg[i];
    if (is_primitive_int_component) {
      new_array->AsIntArray()->SetWithoutChecks<transaction_active>(
          i, shadow_frame.GetVReg(src_reg));
    } else {
      new_array->AsObjectArray<mirror::Object>()->SetWithoutChecks<transaction_active>(
          i, shadow_frame.GetVRegReference(src_reg));
    }
  }

  result->SetL(new_array);
  return true;
}

// TODO: Use ObjPtr here.
template<typename T>
static void RecordArrayElementsInTransactionImpl(ObjPtr<mirror::PrimitiveArray<T>> array,
                                                 int32_t count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Runtime* runtime = Runtime::Current();
  for (int32_t i = 0; i < count; ++i) {
    runtime->RecordWriteArray(array.Ptr(), i, array->GetWithoutChecks(i));
  }
}

void RecordArrayElementsInTransaction(ObjPtr<mirror::Array> array, int32_t count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(Runtime::Current()->IsActiveTransaction());
  DCHECK(array != nullptr);
  DCHECK_LE(count, array->GetLength());
  Primitive::Type primitive_component_type = array->GetClass()->GetComponentType()->GetPrimitiveType();
  switch (primitive_component_type) {
    case Primitive::kPrimBoolean:
      RecordArrayElementsInTransactionImpl(array->AsBooleanArray(), count);
      break;
    case Primitive::kPrimByte:
      RecordArrayElementsInTransactionImpl(array->AsByteArray(), count);
      break;
    case Primitive::kPrimChar:
      RecordArrayElementsInTransactionImpl(array->AsCharArray(), count);
      break;
    case Primitive::kPrimShort:
      RecordArrayElementsInTransactionImpl(array->AsShortArray(), count);
      break;
    case Primitive::kPrimInt:
      RecordArrayElementsInTransactionImpl(array->AsIntArray(), count);
      break;
    case Primitive::kPrimFloat:
      RecordArrayElementsInTransactionImpl(array->AsFloatArray(), count);
      break;
    case Primitive::kPrimLong:
      RecordArrayElementsInTransactionImpl(array->AsLongArray(), count);
      break;
    case Primitive::kPrimDouble:
      RecordArrayElementsInTransactionImpl(array->AsDoubleArray(), count);
      break;
    default:
      LOG(FATAL) << "Unsupported primitive type " << primitive_component_type
                 << " in fill-array-data";
      UNREACHABLE();
  }
}

// Explicit DoCall template function declarations.
#define EXPLICIT_DO_CALL_TEMPLATE_DECL(_is_range, _do_assignability_check)                      \
  template REQUIRES_SHARED(Locks::mutator_lock_)                                                \
  bool DoCall<_is_range, _do_assignability_check>(ArtMethod* method, Thread* self,              \
                                                  ShadowFrame& shadow_frame,                    \
                                                  const Instruction* inst, uint16_t inst_data,  \
                                                  JValue* result)
EXPLICIT_DO_CALL_TEMPLATE_DECL(false, false);
EXPLICIT_DO_CALL_TEMPLATE_DECL(false, true);
EXPLICIT_DO_CALL_TEMPLATE_DECL(true, false);
EXPLICIT_DO_CALL_TEMPLATE_DECL(true, true);
#undef EXPLICIT_DO_CALL_TEMPLATE_DECL

// Explicit DoInvokePolymorphic template function declarations.
#define EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(_is_range)          \
  template REQUIRES_SHARED(Locks::mutator_lock_)                         \
  bool DoInvokePolymorphic<_is_range>(                                   \
      Thread* self, ShadowFrame& shadow_frame, const Instruction* inst,  \
      uint16_t inst_data, JValue* result)
EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(false);
EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL(true);
#undef EXPLICIT_DO_INVOKE_POLYMORPHIC_TEMPLATE_DECL

// Explicit DoFilledNewArray template function declarations.
#define EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL(_is_range_, _check, _transaction_active)       \
  template REQUIRES_SHARED(Locks::mutator_lock_)                                                  \
  bool DoFilledNewArray<_is_range_, _check, _transaction_active>(const Instruction* inst,         \
                                                                 const ShadowFrame& shadow_frame, \
                                                                 Thread* self, JValue* result)
#define EXPLICIT_DO_FILLED_NEW_ARRAY_ALL_TEMPLATE_DECL(_transaction_active)       \
  EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL(false, false, _transaction_active);  \
  EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL(false, true, _transaction_active);   \
  EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL(true, false, _transaction_active);   \
  EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL(true, true, _transaction_active)
EXPLICIT_DO_FILLED_NEW_ARRAY_ALL_TEMPLATE_DECL(false);
EXPLICIT_DO_FILLED_NEW_ARRAY_ALL_TEMPLATE_DECL(true);
#undef EXPLICIT_DO_FILLED_NEW_ARRAY_ALL_TEMPLATE_DECL
#undef EXPLICIT_DO_FILLED_NEW_ARRAY_TEMPLATE_DECL

}  // namespace interpreter
}  // namespace art
