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

#ifndef ART_RUNTIME_ART_METHOD_INL_H_
#define ART_RUNTIME_ART_METHOD_INL_H_

#include "art_method.h"

#include "art_field.h"
#include "base/callee_save_type.h"
#include "class_linker-inl.h"
#include "common_throws.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/dex_file_types.h"
#include "dex/invoke_type.h"
#include "dex/primitive.h"
#include "dex/signature.h"
#include "gc_root-inl.h"
#include "imtable-inl.h"
#include "intrinsics_enum.h"
#include "jit/profiling_info.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array.h"
#include "mirror/string.h"
#include "obj_ptr-inl.h"
#include "quick/quick_method_frame_info.h"
#include "read_barrier-inl.h"
#include "runtime-inl.h"
#include "thread-current-inl.h"

namespace art {

template <ReadBarrierOption kReadBarrierOption>
inline ObjPtr<mirror::Class> ArtMethod::GetDeclaringClassUnchecked() {
  GcRootSource gc_root_source(this);
  return declaring_class_.Read<kReadBarrierOption>(&gc_root_source);
}

template <ReadBarrierOption kReadBarrierOption>
inline ObjPtr<mirror::Class> ArtMethod::GetDeclaringClass() {
  ObjPtr<mirror::Class> result = GetDeclaringClassUnchecked<kReadBarrierOption>();
  if (kIsDebugBuild) {
    if (!IsRuntimeMethod()) {
      CHECK(result != nullptr) << this;
    } else {
      CHECK(result == nullptr) << this;
    }
  }
  return result;
}

inline void ArtMethod::SetDeclaringClass(ObjPtr<mirror::Class> new_declaring_class) {
  declaring_class_ = GcRoot<mirror::Class>(new_declaring_class);
}

inline bool ArtMethod::CASDeclaringClass(ObjPtr<mirror::Class> expected_class,
                                         ObjPtr<mirror::Class> desired_class) {
  GcRoot<mirror::Class> expected_root(expected_class);
  GcRoot<mirror::Class> desired_root(desired_class);
  auto atomic_root_class = reinterpret_cast<Atomic<GcRoot<mirror::Class>>*>(&declaring_class_);
  return atomic_root_class->CompareAndSetStrongSequentiallyConsistent(expected_root, desired_root);
}

inline uint16_t ArtMethod::GetMethodIndex() {
  DCHECK(IsRuntimeMethod() || GetDeclaringClass()->IsResolved());
  return method_index_;
}

inline uint16_t ArtMethod::GetMethodIndexDuringLinking() {
  return method_index_;
}

inline ObjPtr<mirror::Class> ArtMethod::LookupResolvedClassFromTypeIndex(dex::TypeIndex type_idx) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);
  ObjPtr<mirror::Class> type =
      Runtime::Current()->GetClassLinker()->LookupResolvedType(type_idx, this);
  DCHECK(!Thread::Current()->IsExceptionPending());
  return type;
}

inline ObjPtr<mirror::Class> ArtMethod::ResolveClassFromTypeIndex(dex::TypeIndex type_idx) {
  ObjPtr<mirror::Class> type = Runtime::Current()->GetClassLinker()->ResolveType(type_idx, this);
  DCHECK_EQ(type == nullptr, Thread::Current()->IsExceptionPending());
  return type;
}

inline bool ArtMethod::CheckIncompatibleClassChange(InvokeType type) {
  switch (type) {
    case kStatic:
      return !IsStatic();
    case kDirect:
      return !IsDirect() || IsStatic();
    case kVirtual: {
      // We have an error if we are direct or a non-copied (i.e. not part of a real class) interface
      // method.
      ObjPtr<mirror::Class> methods_class = GetDeclaringClass();
      return IsDirect() || (methods_class->IsInterface() && !IsCopied());
    }
    case kSuper:
      // Constructors and static methods are called with invoke-direct.
      return IsConstructor() || IsStatic();
    case kInterface: {
      ObjPtr<mirror::Class> methods_class = GetDeclaringClass();
      return IsDirect() || !(methods_class->IsInterface() || methods_class->IsObjectClass());
    }
    default:
      LOG(FATAL) << "Unreachable - invocation type: " << type;
      UNREACHABLE();
  }
}

inline bool ArtMethod::IsCalleeSaveMethod() {
  if (!IsRuntimeMethod()) {
    return false;
  }
  Runtime* runtime = Runtime::Current();
  bool result = false;
  for (uint32_t i = 0; i < static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType); i++) {
    if (this == runtime->GetCalleeSaveMethod(CalleeSaveType(i))) {
      result = true;
      break;
    }
  }
  return result;
}

inline bool ArtMethod::IsResolutionMethod() {
  bool result = this == Runtime::Current()->GetResolutionMethod();
  // Check that if we do think it is phony it looks like the resolution method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline bool ArtMethod::IsImtUnimplementedMethod() {
  bool result = this == Runtime::Current()->GetImtUnimplementedMethod();
  // Check that if we do think it is phony it looks like the imt unimplemented method.
  DCHECK(!result || IsRuntimeMethod());
  return result;
}

inline const DexFile* ArtMethod::GetDexFile() {
  // It is safe to avoid the read barrier here since the dex file is constant, so if we read the
  // from-space dex file pointer it will be equal to the to-space copy.
  return GetDexCache<kWithoutReadBarrier>()->GetDexFile();
}

inline const char* ArtMethod::GetDeclaringClassDescriptor() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (UNLIKELY(dex_method_idx == dex::kDexNoIndex)) {
    return "<runtime method>";
  }
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodDeclaringClassDescriptor(dex_file->GetMethodId(dex_method_idx));
}

inline const char* ArtMethod::GetShorty() {
  uint32_t unused_length;
  return GetShorty(&unused_length);
}

inline const char* ArtMethod::GetShorty(uint32_t* out_length) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodShorty(dex_file->GetMethodId(GetDexMethodIndex()), out_length);
}

inline const Signature ArtMethod::GetSignature() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (dex_method_idx != dex::kDexNoIndex) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodSignature(dex_file->GetMethodId(dex_method_idx));
  }
  return Signature::NoSignature();
}

inline const char* ArtMethod::GetName() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (LIKELY(dex_method_idx != dex::kDexNoIndex)) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    return dex_file->GetMethodName(dex_file->GetMethodId(dex_method_idx));
  }
  return GetRuntimeMethodName();
}

inline std::string_view ArtMethod::GetNameView() {
  uint32_t dex_method_idx = GetDexMethodIndex();
  if (LIKELY(dex_method_idx != dex::kDexNoIndex)) {
    DCHECK(!IsProxyMethod());
    const DexFile* dex_file = GetDexFile();
    uint32_t length = 0;
    const char* name = dex_file->GetMethodName(dex_file->GetMethodId(dex_method_idx), &length);
    return StringViewFromUtf16Length(name, length);
  }
  return GetRuntimeMethodName();
}

inline ObjPtr<mirror::String> ArtMethod::ResolveNameString() {
  DCHECK(!IsProxyMethod());
  const dex::MethodId& method_id = GetDexFile()->GetMethodId(GetDexMethodIndex());
  return Runtime::Current()->GetClassLinker()->ResolveString(method_id.name_idx_, this);
}

inline const dex::CodeItem* ArtMethod::GetCodeItem() {
  return GetDexFile()->GetCodeItem(GetCodeItemOffset());
}

inline bool ArtMethod::IsResolvedTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  return LookupResolvedClassFromTypeIndex(type_idx) != nullptr;
}

inline int32_t ArtMethod::GetLineNumFromDexPC(uint32_t dex_pc) {
  DCHECK(!IsProxyMethod());
  if (dex_pc == dex::kDexNoIndex) {
    return IsNative() ? -2 : -1;
  }
  return annotations::GetLineNumFromPC(GetDexFile(), this, dex_pc);
}

inline const dex::ProtoId& ArtMethod::GetPrototype() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetMethodPrototype(dex_file->GetMethodId(GetDexMethodIndex()));
}

inline const dex::TypeList* ArtMethod::GetParameterTypeList() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const dex::ProtoId& proto = dex_file->GetMethodPrototype(
      dex_file->GetMethodId(GetDexMethodIndex()));
  return dex_file->GetProtoParameters(proto);
}

inline const char* ArtMethod::GetDeclaringClassSourceFile() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetSourceFile();
}

inline uint16_t ArtMethod::GetClassDefIndex() {
  DCHECK(!IsProxyMethod());
  if (LIKELY(!IsObsolete())) {
    return GetDeclaringClass()->GetDexClassDefIndex();
  } else {
    return FindObsoleteDexClassDefIndex();
  }
}

inline const dex::ClassDef& ArtMethod::GetClassDef() {
  DCHECK(!IsProxyMethod());
  return GetDexFile()->GetClassDef(GetClassDefIndex());
}

inline size_t ArtMethod::GetNumberOfParameters() {
  constexpr size_t return_type_count = 1u;
  return strlen(GetShorty()) - return_type_count;
}

inline const char* ArtMethod::GetReturnTypeDescriptor() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(GetReturnTypeIndex()));
}

inline Primitive::Type ArtMethod::GetReturnTypePrimitive() {
  return Primitive::GetType(GetReturnTypeDescriptor()[0]);
}

inline const char* ArtMethod::GetTypeDescriptorFromTypeIdx(dex::TypeIndex type_idx) {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  return dex_file->GetTypeDescriptor(dex_file->GetTypeId(type_idx));
}

inline ObjPtr<mirror::ClassLoader> ArtMethod::GetClassLoader() {
  DCHECK(!IsProxyMethod());
  return GetDeclaringClass()->GetClassLoader();
}

template <ReadBarrierOption kReadBarrierOption>
inline ObjPtr<mirror::DexCache> ArtMethod::GetDexCache() {
  if (LIKELY(!IsObsolete())) {
    ObjPtr<mirror::Class> klass = GetDeclaringClass<kReadBarrierOption>();
    return klass->GetDexCache<kDefaultVerifyFlags, kReadBarrierOption>();
  } else {
    DCHECK(!IsProxyMethod());
    return GetObsoleteDexCache();
  }
}

inline bool ArtMethod::IsProxyMethod() {
  DCHECK(!IsRuntimeMethod()) << "ArtMethod::IsProxyMethod called on a runtime method";
  // No read barrier needed, we're reading the constant declaring class only to read
  // the constant proxy flag. See ReadBarrierOption.
  return GetDeclaringClass<kWithoutReadBarrier>()->IsProxyClass();
}

inline ArtMethod* ArtMethod::GetInterfaceMethodForProxyUnchecked(PointerSize pointer_size) {
  DCHECK(IsProxyMethod());
  // Do not check IsAssignableFrom() here as it relies on raw reference comparison
  // which may give false negatives while visiting references for a non-CC moving GC.
  return reinterpret_cast<ArtMethod*>(GetDataPtrSize(pointer_size));
}

inline ArtMethod* ArtMethod::GetInterfaceMethodIfProxy(PointerSize pointer_size) {
  if (LIKELY(!IsProxyMethod())) {
    return this;
  }
  ArtMethod* interface_method = GetInterfaceMethodForProxyUnchecked(pointer_size);
  // We can check that the proxy class implements the interface only if the proxy class
  // is resolved, otherwise the interface table is not yet initialized.
  DCHECK(!GetDeclaringClass()->IsResolved() ||
         interface_method->GetDeclaringClass()->IsAssignableFrom(GetDeclaringClass()));
  return interface_method;
}

inline dex::TypeIndex ArtMethod::GetReturnTypeIndex() {
  DCHECK(!IsProxyMethod());
  const DexFile* dex_file = GetDexFile();
  const dex::MethodId& method_id = dex_file->GetMethodId(GetDexMethodIndex());
  const dex::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);
  return proto_id.return_type_idx_;
}

inline ObjPtr<mirror::Class> ArtMethod::LookupResolvedReturnType() {
  return LookupResolvedClassFromTypeIndex(GetReturnTypeIndex());
}

inline ObjPtr<mirror::Class> ArtMethod::ResolveReturnType() {
  return ResolveClassFromTypeIndex(GetReturnTypeIndex());
}

template <ReadBarrierOption kReadBarrierOption>
inline bool ArtMethod::HasSingleImplementation() {
  if (IsFinal() || GetDeclaringClass<kReadBarrierOption>()->IsFinal()) {
    // We don't set kAccSingleImplementation for these cases since intrinsic
    // can use the flag also.
    return true;
  }
  return (GetAccessFlags() & kAccSingleImplementation) != 0;
}

template<ReadBarrierOption kReadBarrierOption, typename RootVisitorType>
void ArtMethod::VisitRoots(RootVisitorType& visitor, PointerSize pointer_size) {
  if (LIKELY(!declaring_class_.IsNull())) {
    visitor.VisitRoot(declaring_class_.AddressWithoutBarrier());
    ObjPtr<mirror::Class> klass = declaring_class_.Read<kReadBarrierOption>();
    if (UNLIKELY(klass->IsProxyClass())) {
      // For normal methods, dex cache shortcuts will be visited through the declaring class.
      // However, for proxies we need to keep the interface method alive, so we visit its roots.
      ArtMethod* interface_method = GetInterfaceMethodForProxyUnchecked(pointer_size);
      DCHECK(interface_method != nullptr);
      interface_method->VisitRoots(visitor, pointer_size);
    }
  }
}

template <typename Visitor>
inline void ArtMethod::UpdateEntrypoints(const Visitor& visitor, PointerSize pointer_size) {
  if (IsNative()) {
    const void* old_native_code = GetEntryPointFromJniPtrSize(pointer_size);
    const void* new_native_code = visitor(old_native_code);
    if (old_native_code != new_native_code) {
      SetEntryPointFromJniPtrSize(new_native_code, pointer_size);
    }
  } else {
    DCHECK(GetDataPtrSize(pointer_size) == nullptr);
  }
  const void* old_code = GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
  const void* new_code = visitor(old_code);
  if (old_code != new_code) {
    SetEntryPointFromQuickCompiledCodePtrSize(new_code, pointer_size);
  }
}

inline CodeItemInstructionAccessor ArtMethod::DexInstructions() {
  return CodeItemInstructionAccessor(*GetDexFile(), GetCodeItem());
}

inline CodeItemDataAccessor ArtMethod::DexInstructionData() {
  return CodeItemDataAccessor(*GetDexFile(), GetCodeItem());
}

inline CodeItemDebugInfoAccessor ArtMethod::DexInstructionDebugInfo() {
  return CodeItemDebugInfoAccessor(*GetDexFile(), GetCodeItem(), GetDexMethodIndex());
}

inline void ArtMethod::SetCounter(uint16_t hotness_count) {
  DCHECK(!IsAbstract()) << PrettyMethod();
  hotness_count_ = hotness_count;
}

inline uint16_t ArtMethod::GetCounter() {
  DCHECK(!IsAbstract()) << PrettyMethod();
  return hotness_count_;
}

inline uint32_t ArtMethod::GetImtIndex() {
  if (LIKELY(IsAbstract() && imt_index_ != 0)) {
    uint16_t imt_index = ~imt_index_;
    DCHECK_EQ(imt_index, ImTable::GetImtIndex(this)) << PrettyMethod();
    return imt_index;
  } else {
    return ImTable::GetImtIndex(this);
  }
}

inline void ArtMethod::CalculateAndSetImtIndex() {
  DCHECK(IsAbstract()) << PrettyMethod();
  imt_index_ = ~ImTable::GetImtIndex(this);
}

}  // namespace art

#endif  // ART_RUNTIME_ART_METHOD_INL_H_
