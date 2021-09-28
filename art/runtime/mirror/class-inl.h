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

#ifndef ART_RUNTIME_MIRROR_CLASS_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_INL_H_

#include "class.h"

#include "art_field.h"
#include "art_method.h"
#include "base/array_slice.h"
#include "base/iteration_range.h"
#include "base/length_prefixed_array.h"
#include "base/stride_iterator.h"
#include "base/utils.h"
#include "class_linker.h"
#include "class_loader.h"
#include "common_throws.h"
#include "dex/dex_file-inl.h"
#include "dex/invoke_type.h"
#include "dex_cache.h"
#include "iftable-inl.h"
#include "imtable.h"
#include "object-inl.h"
#include "object_array.h"
#include "read_barrier-inl.h"
#include "runtime.h"
#include "string.h"
#include "subtype_check.h"
#include "thread-current-inl.h"

namespace art {
namespace mirror {

template<VerifyObjectFlags kVerifyFlags>
inline uint32_t Class::GetObjectSize() {
  // Note: Extra parentheses to avoid the comma being interpreted as macro parameter separator.
  DCHECK((!IsVariableSize<kVerifyFlags>())) << "class=" << PrettyTypeOf();
  return GetField32(ObjectSizeOffset());
}

template<VerifyObjectFlags kVerifyFlags>
inline uint32_t Class::GetObjectSizeAllocFastPath() {
  // Note: Extra parentheses to avoid the comma being interpreted as macro parameter separator.
  DCHECK((!IsVariableSize<kVerifyFlags>())) << "class=" << PrettyTypeOf();
  return GetField32(ObjectSizeAllocFastPathOffset());
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<Class> Class::GetSuperClass() {
  // Can only get super class for loaded classes (hack for when runtime is
  // initializing)
  DCHECK(IsLoaded<kVerifyFlags>() ||
         IsErroneous<kVerifyFlags>() ||
         !Runtime::Current()->IsStarted()) << IsLoaded();
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, super_class_));
}

inline void Class::SetSuperClass(ObjPtr<Class> new_super_class) {
  // Super class is assigned once, except during class linker initialization.
  if (kIsDebugBuild) {
    ObjPtr<Class> old_super_class =
        GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(Class, super_class_));
    DCHECK(old_super_class == nullptr || old_super_class == new_super_class);
  }
  DCHECK(new_super_class != nullptr);
  SetFieldObject</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      OFFSET_OF_OBJECT_MEMBER(Class, super_class_), new_super_class);
}

inline bool Class::HasSuperClass() {
  // No read barrier is needed for comparing with null. See ReadBarrierOption.
  return GetSuperClass<kDefaultVerifyFlags, kWithoutReadBarrier>() != nullptr;
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<ClassLoader> Class::GetClassLoader() {
  return GetFieldObject<ClassLoader, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, class_loader_));
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<ClassExt> Class::GetExtData() {
  return GetFieldObject<ClassExt, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, ext_data_));
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<DexCache> Class::GetDexCache() {
  return GetFieldObject<DexCache, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, dex_cache_));
}

inline uint32_t Class::GetCopiedMethodsStartOffset() {
  // Object::GetFieldShort returns an int16_t value, but
  // Class::copied_methods_offset_ is an uint16_t value; cast the
  // latter to uint16_t before returning it as an uint32_t value, so
  // that uint16_t values between 2^15 and 2^16-1 are correctly
  // handled.
  return static_cast<uint16_t>(
      GetFieldShort(OFFSET_OF_OBJECT_MEMBER(Class, copied_methods_offset_)));
}

inline uint32_t Class::GetDirectMethodsStartOffset() {
  return 0;
}

inline uint32_t Class::GetVirtualMethodsStartOffset() {
  // Object::GetFieldShort returns an int16_t value, but
  // Class::virtual_method_offset_ is an uint16_t value; cast the
  // latter to uint16_t before returning it as an uint32_t value, so
  // that uint16_t values between 2^15 and 2^16-1 are correctly
  // handled.
  return static_cast<uint16_t>(
      GetFieldShort(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_offset_)));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDirectMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetDirectMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDirectMethodsSliceUnchecked(PointerSize pointer_size) {
  return GetMethodsSliceRangeUnchecked(GetMethodsPtr(),
                                       pointer_size,
                                       GetDirectMethodsStartOffset(),
                                       GetVirtualMethodsStartOffset());
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDeclaredMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetDeclaredMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredMethodsSliceUnchecked(PointerSize pointer_size) {
  return GetMethodsSliceRangeUnchecked(GetMethodsPtr(),
                                       pointer_size,
                                       GetDirectMethodsStartOffset(),
                                       GetCopiedMethodsStartOffset());
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetDeclaredVirtualMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetDeclaredVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredVirtualMethodsSliceUnchecked(
    PointerSize pointer_size) {
  return GetMethodsSliceRangeUnchecked(GetMethodsPtr(),
                                       pointer_size,
                                       GetVirtualMethodsStartOffset(),
                                       GetCopiedMethodsStartOffset());
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetVirtualMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  return GetVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetVirtualMethodsSliceUnchecked(PointerSize pointer_size) {
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return GetMethodsSliceRangeUnchecked(methods,
                                       pointer_size,
                                       GetVirtualMethodsStartOffset(),
                                       NumMethods(methods));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetCopiedMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  return GetCopiedMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetCopiedMethodsSliceUnchecked(PointerSize pointer_size) {
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return GetMethodsSliceRangeUnchecked(methods,
                                       pointer_size,
                                       GetCopiedMethodsStartOffset(),
                                       NumMethods(methods));
}

inline LengthPrefixedArray<ArtMethod>* Class::GetMethodsPtr() {
  return reinterpret_cast<LengthPrefixedArray<ArtMethod>*>(
      static_cast<uintptr_t>(GetField64(OFFSET_OF_OBJECT_MEMBER(Class, methods_))));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArraySlice<ArtMethod> Class::GetMethodsSlice(PointerSize pointer_size) {
  DCHECK(IsLoaded() || IsErroneous());
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return GetMethodsSliceRangeUnchecked(methods, pointer_size, 0, NumMethods(methods));
}

inline ArraySlice<ArtMethod> Class::GetMethodsSliceRangeUnchecked(
    LengthPrefixedArray<ArtMethod>* methods,
    PointerSize pointer_size,
    uint32_t start_offset,
    uint32_t end_offset) {
  DCHECK_LE(start_offset, end_offset);
  DCHECK_LE(end_offset, NumMethods(methods));
  uint32_t size = end_offset - start_offset;
  if (size == 0u) {
    return ArraySlice<ArtMethod>();
  }
  DCHECK(methods != nullptr);
  DCHECK_LE(end_offset, methods->size());
  size_t method_size = ArtMethod::Size(pointer_size);
  size_t method_alignment = ArtMethod::Alignment(pointer_size);
  ArraySlice<ArtMethod> slice(&methods->At(0u, method_size, method_alignment),
                              methods->size(),
                              method_size);
  return slice.SubArray(start_offset, size);
}

inline uint32_t Class::NumMethods() {
  return NumMethods(GetMethodsPtr());
}

inline uint32_t Class::NumMethods(LengthPrefixedArray<ArtMethod>* methods) {
  return (methods == nullptr) ? 0 : methods->size();
}

inline ArtMethod* Class::GetDirectMethodUnchecked(size_t i, PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return &GetDirectMethodsSliceUnchecked(pointer_size)[i];
}

inline ArtMethod* Class::GetDirectMethod(size_t i, PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return &GetDirectMethodsSlice(pointer_size)[i];
}

inline void Class::SetMethodsPtr(LengthPrefixedArray<ArtMethod>* new_methods,
                                 uint32_t num_direct,
                                 uint32_t num_virtual) {
  DCHECK(GetMethodsPtr() == nullptr);
  SetMethodsPtrUnchecked(new_methods, num_direct, num_virtual);
}


inline void Class::SetMethodsPtrUnchecked(LengthPrefixedArray<ArtMethod>* new_methods,
                                          uint32_t num_direct,
                                          uint32_t num_virtual) {
  DCHECK_LE(num_direct + num_virtual, (new_methods == nullptr) ? 0 : new_methods->size());
  SetField64<false>(OFFSET_OF_OBJECT_MEMBER(Class, methods_),
                    static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new_methods)));
  SetFieldShort<false>(OFFSET_OF_OBJECT_MEMBER(Class, copied_methods_offset_),
                    dchecked_integral_cast<uint16_t>(num_direct + num_virtual));
  SetFieldShort<false>(OFFSET_OF_OBJECT_MEMBER(Class, virtual_methods_offset_),
                       dchecked_integral_cast<uint16_t>(num_direct));
}

template<VerifyObjectFlags kVerifyFlags>
inline ArtMethod* Class::GetVirtualMethod(size_t i, PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  DCHECK(IsResolved<kVerifyFlags>() || IsErroneous<kVerifyFlags>())
      << Class::PrettyClass() << " status=" << GetStatus();
  return GetVirtualMethodUnchecked(i, pointer_size);
}

inline ArtMethod* Class::GetVirtualMethodDuringLinking(size_t i, PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  DCHECK(IsLoaded() || IsErroneous());
  return GetVirtualMethodUnchecked(i, pointer_size);
}

inline ArtMethod* Class::GetVirtualMethodUnchecked(size_t i, PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return &GetVirtualMethodsSliceUnchecked(pointer_size)[i];
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<PointerArray> Class::GetVTable() {
  DCHECK(IsLoaded<kVerifyFlags>() || IsErroneous<kVerifyFlags>());
  return GetFieldObject<PointerArray, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline ObjPtr<PointerArray> Class::GetVTableDuringLinking() {
  DCHECK(IsLoaded() || IsErroneous());
  return GetFieldObject<PointerArray>(OFFSET_OF_OBJECT_MEMBER(Class, vtable_));
}

inline void Class::SetVTable(ObjPtr<PointerArray> new_vtable) {
  SetFieldObject</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      OFFSET_OF_OBJECT_MEMBER(Class, vtable_), new_vtable);
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::ShouldHaveImt() {
  return ShouldHaveEmbeddedVTable<kVerifyFlags>();
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::ShouldHaveEmbeddedVTable() {
  return IsInstantiable<kVerifyFlags>();
}

inline bool Class::HasVTable() {
  // No read barrier is needed for comparing with null. See ReadBarrierOption.
  return GetVTable<kDefaultVerifyFlags, kWithoutReadBarrier>() != nullptr ||
         ShouldHaveEmbeddedVTable();
}

template<VerifyObjectFlags kVerifyFlags>
inline int32_t Class::GetVTableLength() {
  if (ShouldHaveEmbeddedVTable<kVerifyFlags>()) {
    return GetEmbeddedVTableLength();
  }
  // We do not need a read barrier here as the length is constant,
  // both from-space and to-space vtables shall yield the same result.
  ObjPtr<PointerArray> vtable = GetVTable<kVerifyFlags, kWithoutReadBarrier>();
  return vtable != nullptr ? vtable->GetLength() : 0;
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ArtMethod* Class::GetVTableEntry(uint32_t i, PointerSize pointer_size) {
  if (ShouldHaveEmbeddedVTable<kVerifyFlags>()) {
    return GetEmbeddedVTableEntry(i, pointer_size);
  }
  ObjPtr<PointerArray> vtable = GetVTable<kVerifyFlags, kReadBarrierOption>();
  DCHECK(vtable != nullptr);
  return vtable->GetElementPtrSize<ArtMethod*, kVerifyFlags>(i, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags>
inline int32_t Class::GetEmbeddedVTableLength() {
  return GetField32<kVerifyFlags>(MemberOffset(EmbeddedVTableLengthOffset()));
}

inline void Class::SetEmbeddedVTableLength(int32_t len) {
  SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      MemberOffset(EmbeddedVTableLengthOffset()), len);
}

inline ImTable* Class::GetImt(PointerSize pointer_size) {
  return GetFieldPtrWithSize<ImTable*>(ImtPtrOffset(pointer_size), pointer_size);
}

inline void Class::SetImt(ImTable* imt, PointerSize pointer_size) {
  return SetFieldPtrWithSize</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      ImtPtrOffset(pointer_size), imt, pointer_size);
}

inline MemberOffset Class::EmbeddedVTableEntryOffset(uint32_t i, PointerSize pointer_size) {
  return MemberOffset(
      EmbeddedVTableOffset(pointer_size).Uint32Value() + i * VTableEntrySize(pointer_size));
}

inline ArtMethod* Class::GetEmbeddedVTableEntry(uint32_t i, PointerSize pointer_size) {
  return GetFieldPtrWithSize<ArtMethod*>(EmbeddedVTableEntryOffset(i, pointer_size), pointer_size);
}

inline void Class::SetEmbeddedVTableEntryUnchecked(
    uint32_t i, ArtMethod* method, PointerSize pointer_size) {
  SetFieldPtrWithSize</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      EmbeddedVTableEntryOffset(i, pointer_size), method, pointer_size);
}

inline void Class::SetEmbeddedVTableEntry(uint32_t i, ArtMethod* method, PointerSize pointer_size) {
  ObjPtr<PointerArray> vtable = GetVTableDuringLinking();
  CHECK_EQ(method, vtable->GetElementPtrSize<ArtMethod*>(i, pointer_size));
  SetEmbeddedVTableEntryUnchecked(i, method, pointer_size);
}

inline bool Class::Implements(ObjPtr<Class> klass) {
  DCHECK(klass != nullptr);
  DCHECK(klass->IsInterface()) << PrettyClass();
  // All interfaces implemented directly and by our superclass, and
  // recursively all super-interfaces of those interfaces, are listed
  // in iftable_, so we can just do a linear scan through that.
  int32_t iftable_count = GetIfTableCount();
  ObjPtr<IfTable> iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == klass) {
      return true;
    }
  }
  return false;
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::IsVariableSize() {
  // Classes, arrays, and strings vary in size, and so the object_size_ field cannot
  // be used to Get their instance size
  return IsClassClass<kVerifyFlags>() ||
         IsArrayClass<kVerifyFlags>() ||
         IsStringClass<kVerifyFlags>();
}

inline void Class::SetObjectSize(uint32_t new_object_size) {
  DCHECK(!IsVariableSize());
  // Not called within a transaction.
  return SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, object_size_), new_object_size);
}

// Determine whether "this" is assignable from "src", where both of these
// are array classes.
//
// Consider an array class, e.g. Y[][], where Y is a subclass of X.
//   Y[][]            = Y[][] --> true (identity)
//   X[][]            = Y[][] --> true (element superclass)
//   Y                = Y[][] --> false
//   Y[]              = Y[][] --> false
//   Object           = Y[][] --> true (everything is an object)
//   Object[]         = Y[][] --> true
//   Object[][]       = Y[][] --> true
//   Object[][][]     = Y[][] --> false (too many []s)
//   Serializable     = Y[][] --> true (all arrays are Serializable)
//   Serializable[]   = Y[][] --> true
//   Serializable[][] = Y[][] --> false (unless Y is Serializable)
//
// Don't forget about primitive types.
//   Object[]         = int[] --> false
//
inline bool Class::IsArrayAssignableFromArray(ObjPtr<Class> src) {
  DCHECK(IsArrayClass()) << PrettyClass();
  DCHECK(src->IsArrayClass()) << src->PrettyClass();
  return GetComponentType()->IsAssignableFrom(src->GetComponentType());
}

inline bool Class::IsAssignableFromArray(ObjPtr<Class> src) {
  DCHECK(!IsInterface()) << PrettyClass();  // handled first in IsAssignableFrom
  DCHECK(src->IsArrayClass()) << src->PrettyClass();
  if (!IsArrayClass()) {
    // If "this" is not also an array, it must be Object.
    // src's super should be java_lang_Object, since it is an array.
    ObjPtr<Class> java_lang_Object = src->GetSuperClass();
    DCHECK(java_lang_Object != nullptr) << src->PrettyClass();
    DCHECK(java_lang_Object->GetSuperClass() == nullptr) << src->PrettyClass();
    return this == java_lang_Object;
  }
  return IsArrayAssignableFromArray(src);
}

template <bool throw_on_failure>
inline bool Class::ResolvedFieldAccessTest(ObjPtr<Class> access_to,
                                           ArtField* field,
                                           ObjPtr<DexCache> dex_cache,
                                           uint32_t field_idx) {
  DCHECK(dex_cache != nullptr);
  if (UNLIKELY(!this->CanAccess(access_to))) {
    // The referrer class can't access the field's declaring class but may still be able
    // to access the field if the FieldId specifies an accessible subclass of the declaring
    // class rather than the declaring class itself.
    dex::TypeIndex class_idx = dex_cache->GetDexFile()->GetFieldId(field_idx).class_idx_;
    // The referenced class has already been resolved with the field, but may not be in the dex
    // cache. Use LookupResolveType here to search the class table if it is not in the dex cache.
    // should be no thread suspension due to the class being resolved.
    ObjPtr<Class> dex_access_to = Runtime::Current()->GetClassLinker()->LookupResolvedType(
        class_idx,
        dex_cache,
        access_to->GetClassLoader());
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClass(this, dex_access_to);
      }
      return false;
    }
  }
  if (LIKELY(this->CanAccessMember(access_to, field->GetAccessFlags()))) {
    return true;
  }
  if (throw_on_failure) {
    ThrowIllegalAccessErrorField(this, field);
  }
  return false;
}

template <bool throw_on_failure>
inline bool Class::ResolvedMethodAccessTest(ObjPtr<Class> access_to,
                                            ArtMethod* method,
                                            ObjPtr<DexCache> dex_cache,
                                            uint32_t method_idx,
                                            InvokeType throw_invoke_type) {
  DCHECK(throw_on_failure || throw_invoke_type == kStatic);
  DCHECK(dex_cache != nullptr);
  if (UNLIKELY(!this->CanAccess(access_to))) {
    // The referrer class can't access the method's declaring class but may still be able
    // to access the method if the MethodId specifies an accessible subclass of the declaring
    // class rather than the declaring class itself.
    dex::TypeIndex class_idx = dex_cache->GetDexFile()->GetMethodId(method_idx).class_idx_;
    // The referenced class has already been resolved with the method, but may not be in the dex
    // cache.
    ObjPtr<Class> dex_access_to = Runtime::Current()->GetClassLinker()->LookupResolvedType(
        class_idx,
        dex_cache,
        access_to->GetClassLoader());
    DCHECK(dex_access_to != nullptr);
    if (UNLIKELY(!this->CanAccess(dex_access_to))) {
      if (throw_on_failure) {
        ThrowIllegalAccessErrorClassForMethodDispatch(this,
                                                      dex_access_to,
                                                      method,
                                                      throw_invoke_type);
      }
      return false;
    }
  }
  if (LIKELY(this->CanAccessMember(access_to, method->GetAccessFlags()))) {
    return true;
  }
  if (throw_on_failure) {
    ThrowIllegalAccessErrorMethod(this, method);
  }
  return false;
}

inline bool Class::CanAccessResolvedField(ObjPtr<Class> access_to,
                                          ArtField* field,
                                          ObjPtr<DexCache> dex_cache,
                                          uint32_t field_idx) {
  return ResolvedFieldAccessTest<false>(access_to, field, dex_cache, field_idx);
}

inline bool Class::CheckResolvedFieldAccess(ObjPtr<Class> access_to,
                                            ArtField* field,
                                            ObjPtr<DexCache> dex_cache,
                                            uint32_t field_idx) {
  return ResolvedFieldAccessTest<true>(access_to, field, dex_cache, field_idx);
}

inline bool Class::CanAccessResolvedMethod(ObjPtr<Class> access_to,
                                           ArtMethod* method,
                                           ObjPtr<DexCache> dex_cache,
                                           uint32_t method_idx) {
  return ResolvedMethodAccessTest<false>(access_to, method, dex_cache, method_idx, kStatic);
}

inline bool Class::CheckResolvedMethodAccess(ObjPtr<Class> access_to,
                                             ArtMethod* method,
                                             ObjPtr<DexCache> dex_cache,
                                             uint32_t method_idx,
                                             InvokeType throw_invoke_type) {
  return ResolvedMethodAccessTest<true>(
      access_to, method, dex_cache, method_idx, throw_invoke_type);
}

inline bool Class::IsObsoleteVersionOf(ObjPtr<Class> klass) {
  DCHECK(!klass->IsObsoleteObject()) << klass->PrettyClass() << " is obsolete!";
  if (LIKELY(!IsObsoleteObject())) {
    return false;
  }
  ObjPtr<Class> current(klass);
  do {
    if (UNLIKELY(current == this)) {
      return true;
    } else {
      current = current->GetObsoleteClass();
    }
  } while (!current.IsNull());
  return false;
}

inline bool Class::IsSubClass(ObjPtr<Class> klass) {
  // Since the SubtypeCheck::IsSubtypeOf needs to lookup the Depth,
  // it is always O(Depth) in terms of speed to do the check.
  //
  // So always do the "slow" linear scan in normal release builds.
  //
  // Future note: If we could have the depth in O(1) we could use the 'fast'
  // method instead as it avoids a loop and a read barrier.
  bool result = false;
  DCHECK(!IsInterface()) << PrettyClass();
  DCHECK(!IsArrayClass()) << PrettyClass();
  ObjPtr<Class> current = this;
  do {
    if (current == klass) {
      result = true;
      break;
    }
    current = current->GetSuperClass();
  } while (current != nullptr);

  if (kIsDebugBuild && kBitstringSubtypeCheckEnabled) {
    ObjPtr<mirror::Class> dis(this);

    SubtypeCheckInfo::Result sc_result = SubtypeCheck<ObjPtr<Class>>::IsSubtypeOf(dis, klass);
    if (sc_result != SubtypeCheckInfo::kUnknownSubtypeOf) {
      // Note: The "kUnknownSubTypeOf" can be avoided if and only if:
      //   SubtypeCheck::EnsureInitialized(source)
      //       happens-before source.IsSubClass(target)
      //   SubtypeCheck::EnsureAssigned(target).GetState() == Assigned
      //       happens-before source.IsSubClass(target)
      //
      // When code generated by optimizing compiler executes this operation, both
      // happens-before are guaranteed, so there is no fallback code there.
      SubtypeCheckInfo::Result expected_result =
          result ? SubtypeCheckInfo::kSubtypeOf : SubtypeCheckInfo::kNotSubtypeOf;
      DCHECK_EQ(expected_result, sc_result)
          << "source: " << PrettyClass() << "target: " << klass->PrettyClass();
    }
  }

  return result;
}

inline ArtMethod* Class::FindVirtualMethodForInterface(ArtMethod* method,
                                                       PointerSize pointer_size) {
  ObjPtr<Class> declaring_class = method->GetDeclaringClass();
  DCHECK(declaring_class != nullptr) << PrettyClass();
  if (UNLIKELY(!declaring_class->IsInterface())) {
    DCHECK(declaring_class->IsObjectClass()) << method->PrettyMethod();
    DCHECK(method->IsPublic() && !method->IsStatic());
    return FindVirtualMethodForVirtual(method, pointer_size);
  }
  DCHECK(!method->IsCopied());
  // TODO cache to improve lookup speed
  const int32_t iftable_count = GetIfTableCount();
  ObjPtr<IfTable> iftable = GetIfTable();
  for (int32_t i = 0; i < iftable_count; i++) {
    if (iftable->GetInterface(i) == declaring_class) {
      return iftable->GetMethodArray(i)->GetElementPtrSize<ArtMethod*>(
          method->GetMethodIndex(), pointer_size);
    }
  }
  return nullptr;
}

inline ArtMethod* Class::FindVirtualMethodForVirtual(ArtMethod* method, PointerSize pointer_size) {
  // Only miranda or default methods may come from interfaces and be used as a virtual.
  DCHECK(!method->GetDeclaringClass()->IsInterface() || method->IsDefault() || method->IsMiranda());
  // The argument method may from a super class.
  // Use the index to a potentially overridden one for this instance's class.
  return GetVTableEntry(method->GetMethodIndex(), pointer_size);
}

inline ArtMethod* Class::FindVirtualMethodForSuper(ArtMethod* method, PointerSize pointer_size) {
  DCHECK(!method->GetDeclaringClass()->IsInterface());
  return GetSuperClass()->GetVTableEntry(method->GetMethodIndex(), pointer_size);
}

inline ArtMethod* Class::FindVirtualMethodForVirtualOrInterface(ArtMethod* method,
                                                                PointerSize pointer_size) {
  if (method->IsDirect()) {
    return method;
  }
  if (method->GetDeclaringClass()->IsInterface() && !method->IsCopied()) {
    return FindVirtualMethodForInterface(method, pointer_size);
  }
  return FindVirtualMethodForVirtual(method, pointer_size);
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<IfTable> Class::GetIfTable() {
  ObjPtr<IfTable> ret = GetFieldObject<IfTable, kVerifyFlags, kReadBarrierOption>(IfTableOffset());
  DCHECK(ret != nullptr) << PrettyClass(this);
  return ret;
}

template<VerifyObjectFlags kVerifyFlags>
inline int32_t Class::GetIfTableCount() {
  // We do not need a read barrier here as the length is constant,
  // both from-space and to-space iftables shall yield the same result.
  return GetIfTable<kVerifyFlags, kWithoutReadBarrier>()->Count();
}

inline void Class::SetIfTable(ObjPtr<IfTable> new_iftable) {
  DCHECK(new_iftable != nullptr) << PrettyClass(this);
  SetFieldObject</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      IfTableOffset(), new_iftable);
}

inline LengthPrefixedArray<ArtField>* Class::GetIFieldsPtr() {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline MemberOffset Class::GetFirstReferenceInstanceFieldOffset() {
  ObjPtr<Class> super_class = GetSuperClass<kVerifyFlags, kReadBarrierOption>();
  return (super_class != nullptr)
      ? MemberOffset(RoundUp(super_class->GetObjectSize<kVerifyFlags>(), kHeapReferenceSize))
      : ClassOffset();
}

template <VerifyObjectFlags kVerifyFlags>
inline MemberOffset Class::GetFirstReferenceStaticFieldOffset(PointerSize pointer_size) {
  DCHECK(IsResolved<kVerifyFlags>());
  uint32_t base = sizeof(Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedVTable<kVerifyFlags>()) {
    // Static fields come after the embedded tables.
    base = Class::ComputeClassSize(
        true, GetEmbeddedVTableLength<kVerifyFlags>(), 0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline MemberOffset Class::GetFirstReferenceStaticFieldOffsetDuringLinking(
    PointerSize pointer_size) {
  DCHECK(IsLoaded());
  uint32_t base = sizeof(Class);  // Static fields come after the class.
  if (ShouldHaveEmbeddedVTable()) {
    // Static fields come after the embedded tables.
    base = Class::ComputeClassSize(true, GetVTableDuringLinking()->GetLength(),
                                           0, 0, 0, 0, 0, pointer_size);
  }
  return MemberOffset(base);
}

inline void Class::SetIFieldsPtr(LengthPrefixedArray<ArtField>* new_ifields) {
  DCHECK(GetIFieldsPtrUnchecked() == nullptr);
  return SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline void Class::SetIFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_ifields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_), new_ifields);
}

inline LengthPrefixedArray<ArtField>* Class::GetSFieldsPtrUnchecked() {
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_));
}

inline LengthPrefixedArray<ArtField>* Class::GetIFieldsPtrUnchecked() {
  return GetFieldPtr<LengthPrefixedArray<ArtField>*>(OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
}

inline LengthPrefixedArray<ArtField>* Class::GetSFieldsPtr() {
  DCHECK(IsLoaded() || IsErroneous()) << GetStatus();
  return GetSFieldsPtrUnchecked();
}

inline void Class::SetSFieldsPtr(LengthPrefixedArray<ArtField>* new_sfields) {
  DCHECK((IsRetired() && new_sfields == nullptr) ||
         GetFieldPtr<ArtField*>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_)) == nullptr);
  SetFieldPtr<false>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline void Class::SetSFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_sfields) {
  SetFieldPtr<false, true, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Class, sfields_), new_sfields);
}

inline ArtField* Class::GetStaticField(uint32_t i) {
  return &GetSFieldsPtr()->At(i);
}

inline ArtField* Class::GetInstanceField(uint32_t i) {
  return &GetIFieldsPtr()->At(i);
}

template<VerifyObjectFlags kVerifyFlags>
inline uint32_t Class::GetReferenceInstanceOffsets() {
  DCHECK(IsResolved<kVerifyFlags>() || IsErroneous<kVerifyFlags>());
  return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, reference_instance_offsets_));
}

inline void Class::SetClinitThreadId(pid_t new_clinit_thread_id) {
  SetField32Transaction(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_), new_clinit_thread_id);
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline ObjPtr<String> Class::GetName() {
  return GetFieldObject<String, kVerifyFlags, kReadBarrierOption>(
      OFFSET_OF_OBJECT_MEMBER(Class, name_));
}

inline void Class::SetName(ObjPtr<String> name) {
  SetFieldObjectTransaction(OFFSET_OF_OBJECT_MEMBER(Class, name_), name);
}

template<VerifyObjectFlags kVerifyFlags>
inline Primitive::Type Class::GetPrimitiveType() {
  static_assert(sizeof(Primitive::Type) == sizeof(int32_t),
                "art::Primitive::Type and int32_t have different sizes.");
  int32_t v32 = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_));
  Primitive::Type type = static_cast<Primitive::Type>(v32 & kPrimitiveTypeMask);
  DCHECK_EQ(static_cast<size_t>(v32 >> kPrimitiveTypeSizeShiftShift),
            Primitive::ComponentSizeShift(type));
  return type;
}

template<VerifyObjectFlags kVerifyFlags>
inline size_t Class::GetPrimitiveTypeSizeShift() {
  static_assert(sizeof(Primitive::Type) == sizeof(int32_t),
                "art::Primitive::Type and int32_t have different sizes.");
  int32_t v32 = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_));
  size_t size_shift = static_cast<Primitive::Type>(v32 >> kPrimitiveTypeSizeShiftShift);
  DCHECK_EQ(size_shift,
            Primitive::ComponentSizeShift(static_cast<Primitive::Type>(v32 & kPrimitiveTypeMask)));
  return size_shift;
}

inline uint32_t Class::ComputeClassSize(bool has_embedded_vtable,
                                        uint32_t num_vtable_entries,
                                        uint32_t num_8bit_static_fields,
                                        uint32_t num_16bit_static_fields,
                                        uint32_t num_32bit_static_fields,
                                        uint32_t num_64bit_static_fields,
                                        uint32_t num_ref_static_fields,
                                        PointerSize pointer_size) {
  // Space used by java.lang.Class and its instance fields.
  uint32_t size = sizeof(Class);
  // Space used by embedded tables.
  if (has_embedded_vtable) {
    size = RoundUp(size + sizeof(uint32_t), static_cast<size_t>(pointer_size));
    size += static_cast<size_t>(pointer_size);  // size of pointer to IMT
    size += num_vtable_entries * VTableEntrySize(pointer_size);
  }

  // Space used by reference statics.
  size += num_ref_static_fields * kHeapReferenceSize;
  if (!IsAligned<8>(size) && num_64bit_static_fields > 0) {
    uint32_t gap = 8 - (size & 0x7);
    size += gap;  // will be padded
    // Shuffle 4-byte fields forward.
    while (gap >= sizeof(uint32_t) && num_32bit_static_fields != 0) {
      --num_32bit_static_fields;
      gap -= sizeof(uint32_t);
    }
    // Shuffle 2-byte fields forward.
    while (gap >= sizeof(uint16_t) && num_16bit_static_fields != 0) {
      --num_16bit_static_fields;
      gap -= sizeof(uint16_t);
    }
    // Shuffle byte fields forward.
    while (gap >= sizeof(uint8_t) && num_8bit_static_fields != 0) {
      --num_8bit_static_fields;
      gap -= sizeof(uint8_t);
    }
  }
  // Guaranteed to be at least 4 byte aligned. No need for further alignments.
  // Space used for primitive static fields.
  size += num_8bit_static_fields * sizeof(uint8_t) + num_16bit_static_fields * sizeof(uint16_t) +
      num_32bit_static_fields * sizeof(uint32_t) + num_64bit_static_fields * sizeof(uint64_t);
  return size;
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::IsClassClass() {
  // OK to look at from-space copies since java.lang.Class.class is non-moveable
  // (even when running without boot image, see ClassLinker::InitWithoutImage())
  // and we're reading it for comparison only. See ReadBarrierOption.
  ObjPtr<Class> java_lang_Class = GetClass<kVerifyFlags, kWithoutReadBarrier>();
  return this == java_lang_Class;
}

inline const DexFile& Class::GetDexFile() {
  // From-space version is the same as the to-space version since the dex file never changes.
  // Avoiding the read barrier here is important to prevent recursive AssertToSpaceInvariant issues
  // from PrettyTypeOf.
  return *GetDexCache<kDefaultVerifyFlags, kWithoutReadBarrier>()->GetDexFile();
}

inline bool Class::DescriptorEquals(const char* match) {
  ObjPtr<mirror::Class> klass = this;
  while (klass->IsArrayClass()) {
    if (match[0] != '[') {
      return false;
    }
    ++match;
    // No read barrier needed, we're reading a chain of constant references for comparison
    // with null. Then we follow up below with reading constant references to read constant
    // primitive data in both proxy and non-proxy paths. See ReadBarrierOption.
    klass = klass->GetComponentType<kDefaultVerifyFlags, kWithoutReadBarrier>();
  }
  if (klass->IsPrimitive()) {
    return strcmp(Primitive::Descriptor(klass->GetPrimitiveType()), match) == 0;
  } else if (klass->IsProxyClass()) {
    return klass->ProxyDescriptorEquals(match);
  } else {
    const DexFile& dex_file = klass->GetDexFile();
    const dex::TypeId& type_id = dex_file.GetTypeId(klass->GetDexTypeIndex());
    return strcmp(dex_file.GetTypeDescriptor(type_id), match) == 0;
  }
}

inline void Class::AssertInitializedOrInitializingInThread(Thread* self) {
  if (kIsDebugBuild && !IsInitialized()) {
    CHECK(IsInitializing()) << PrettyClass() << " is not initializing: " << GetStatus();
    CHECK_EQ(GetClinitThreadId(), self->GetTid())
        << PrettyClass() << " is initializing in a different thread";
  }
}

inline ObjPtr<ObjectArray<Class>> Class::GetProxyInterfaces() {
  CHECK(IsProxyClass());
  // First static field.
  ArtField* field = GetStaticField(0);
  DCHECK_STREQ(field->GetName(), "interfaces");
  MemberOffset field_offset = field->GetOffset();
  return GetFieldObject<ObjectArray<Class>>(field_offset);
}

inline ObjPtr<ObjectArray<ObjectArray<Class>>> Class::GetProxyThrows() {
  CHECK(IsProxyClass());
  // Second static field.
  ArtField* field = GetStaticField(1);
  DCHECK_STREQ(field->GetName(), "throws");
  MemberOffset field_offset = field->GetOffset();
  return GetFieldObject<ObjectArray<ObjectArray<Class>>>(field_offset);
}

inline bool Class::IsBootStrapClassLoaded() {
  // No read barrier is needed for comparing with null. See ReadBarrierOption.
  return GetClassLoader<kDefaultVerifyFlags, kWithoutReadBarrier>() == nullptr;
}

inline void Class::InitializeClassVisitor::operator()(ObjPtr<Object> obj,
                                                      size_t usable_size) const {
  DCHECK_LE(class_size_, usable_size);
  // Avoid AsClass as object is not yet in live bitmap or allocation stack.
  ObjPtr<Class> klass = ObjPtr<Class>::DownCast(obj);
  klass->SetClassSize(class_size_);
  klass->SetPrimitiveType(Primitive::kPrimNot);  // Default to not being primitive.
  klass->SetDexClassDefIndex(DexFile::kDexNoIndex16);  // Default to no valid class def index.
  klass->SetDexTypeIndex(dex::TypeIndex(DexFile::kDexNoIndex16));  // Default to no valid type
                                                                   // index.
  // Default to force slow path until visibly initialized.
  // There is no need for release store (volatile) in pre-fence visitor.
  klass->SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      ObjectSizeAllocFastPathOffset(), std::numeric_limits<uint32_t>::max());
}

inline void Class::SetAccessFlagsDuringLinking(uint32_t new_access_flags) {
  SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      AccessFlagsOffset(), new_access_flags);
}

inline void Class::SetAccessFlags(uint32_t new_access_flags) {
  if (kIsDebugBuild) {
    SetAccessFlagsDCheck(new_access_flags);
  }
  // Called inside a transaction when setting pre-verified flag during boot image compilation.
  if (Runtime::Current()->IsActiveTransaction()) {
    SetField32<true>(AccessFlagsOffset(), new_access_flags);
  } else {
    SetField32<false>(AccessFlagsOffset(), new_access_flags);
  }
}

inline void Class::SetClassFlags(uint32_t new_flags) {
  SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      OFFSET_OF_OBJECT_MEMBER(Class, class_flags_), new_flags);
}

inline uint32_t Class::NumDirectInterfaces() {
  if (IsPrimitive()) {
    return 0;
  } else if (IsArrayClass()) {
    return 2;
  } else if (IsProxyClass()) {
    ObjPtr<ObjectArray<Class>> interfaces = GetProxyInterfaces();
    return interfaces != nullptr ? interfaces->GetLength() : 0;
  } else {
    const dex::TypeList* interfaces = GetInterfaceTypeList();
    if (interfaces == nullptr) {
      return 0;
    } else {
      return interfaces->Size();
    }
  }
}

inline ArraySlice<ArtMethod> Class::GetDirectMethods(PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return GetDirectMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredMethods(PointerSize pointer_size) {
  return GetDeclaredMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetDeclaredVirtualMethods(PointerSize pointer_size) {
  return GetDeclaredVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetVirtualMethods(PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return GetVirtualMethodsSliceUnchecked(pointer_size);
}

inline ArraySlice<ArtMethod> Class::GetCopiedMethods(PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  return GetCopiedMethodsSliceUnchecked(pointer_size);
}


inline ArraySlice<ArtMethod> Class::GetMethods(PointerSize pointer_size) {
  CheckPointerSize(pointer_size);
  LengthPrefixedArray<ArtMethod>* methods = GetMethodsPtr();
  return GetMethodsSliceRangeUnchecked(methods, pointer_size, 0u, NumMethods(methods));
}

inline IterationRange<StrideIterator<ArtField>> Class::GetIFields() {
  return MakeIterationRangeFromLengthPrefixedArray(GetIFieldsPtr());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetSFields() {
  return MakeIterationRangeFromLengthPrefixedArray(GetSFieldsPtr());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetIFieldsUnchecked() {
  return MakeIterationRangeFromLengthPrefixedArray(GetIFieldsPtrUnchecked());
}

inline IterationRange<StrideIterator<ArtField>> Class::GetSFieldsUnchecked() {
  return MakeIterationRangeFromLengthPrefixedArray(GetSFieldsPtrUnchecked());
}

inline void Class::CheckPointerSize(PointerSize pointer_size) {
  DCHECK_EQ(pointer_size, Runtime::Current()->GetClassLinker()->GetImagePointerSize());
}

template<VerifyObjectFlags kVerifyFlags, ReadBarrierOption kReadBarrierOption>
inline ObjPtr<Class> Class::GetComponentType() {
  return GetFieldObject<Class, kVerifyFlags, kReadBarrierOption>(ComponentTypeOffset());
}

inline void Class::SetComponentType(ObjPtr<Class> new_component_type) {
  DCHECK(GetComponentType() == nullptr);
  DCHECK(new_component_type != nullptr);
  // Component type is invariant: use non-transactional mode without check.
  SetFieldObject</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      ComponentTypeOffset(), new_component_type);
}

inline size_t Class::GetComponentSize() {
  return 1U << GetComponentSizeShift();
}

inline size_t Class::GetComponentSizeShift() {
  // No read barrier is needed for reading a constant primitive field through
  // constant reference field. See ReadBarrierOption.
  return GetComponentType<kDefaultVerifyFlags, kWithoutReadBarrier>()->GetPrimitiveTypeSizeShift();
}

inline bool Class::IsObjectClass() {
  // No read barrier is needed for comparing with null. See ReadBarrierOption.
  return !IsPrimitive() && GetSuperClass<kDefaultVerifyFlags, kWithoutReadBarrier>() == nullptr;
}

inline bool Class::IsInstantiableNonArray() {
  return !IsPrimitive() && !IsInterface() && !IsAbstract() && !IsArrayClass();
}

template<VerifyObjectFlags kVerifyFlags>
bool Class::IsInstantiable() {
  return (!IsPrimitive<kVerifyFlags>() &&
          !IsInterface<kVerifyFlags>() &&
          !IsAbstract<kVerifyFlags>()) ||
      (IsAbstract<kVerifyFlags>() && IsArrayClass<kVerifyFlags>());
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::IsArrayClass() {
  // We do not need a read barrier for comparing with null.
  return GetComponentType<kVerifyFlags, kWithoutReadBarrier>() != nullptr;
}

template<VerifyObjectFlags kVerifyFlags>
inline bool Class::IsObjectArrayClass() {
  // We do not need a read barrier here as the primitive type is constant,
  // both from-space and to-space component type classes shall yield the same result.
  const ObjPtr<Class> component_type = GetComponentType<kVerifyFlags, kWithoutReadBarrier>();
  constexpr VerifyObjectFlags kNewFlags = RemoveThisFlags(kVerifyFlags);
  return component_type != nullptr && !component_type->IsPrimitive<kNewFlags>();
}

template<VerifyObjectFlags kVerifyFlags>
bool Class::IsPrimitiveArray() {
  // We do not need a read barrier here as the primitive type is constant,
  // both from-space and to-space component type classes shall yield the same result.
  const ObjPtr<Class> component_type = GetComponentType<kVerifyFlags, kWithoutReadBarrier>();
  constexpr VerifyObjectFlags kNewFlags = RemoveThisFlags(kVerifyFlags);
  return component_type != nullptr && component_type->IsPrimitive<kNewFlags>();
}

inline bool Class::IsAssignableFrom(ObjPtr<Class> src) {
  DCHECK(src != nullptr);
  if (this == src) {
    // Can always assign to things of the same type.
    return true;
  } else if (IsObjectClass()) {
    // Can assign any reference to java.lang.Object.
    return !src->IsPrimitive();
  } else if (IsInterface()) {
    return src->Implements(this);
  } else if (src->IsArrayClass()) {
    return IsAssignableFromArray(src);
  } else {
    return !src->IsInterface() && src->IsSubClass(this);
  }
}

inline uint32_t Class::NumDirectMethods() {
  return GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumDeclaredVirtualMethods() {
  return GetCopiedMethodsStartOffset() - GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumVirtualMethods() {
  return NumMethods() - GetVirtualMethodsStartOffset();
}

inline uint32_t Class::NumInstanceFields() {
  LengthPrefixedArray<ArtField>* arr = GetIFieldsPtrUnchecked();
  return arr != nullptr ? arr->size() : 0u;
}

inline uint32_t Class::NumStaticFields() {
  LengthPrefixedArray<ArtField>* arr = GetSFieldsPtrUnchecked();
  return arr != nullptr ? arr->size() : 0u;
}

template <typename T, VerifyObjectFlags kVerifyFlags, typename Visitor>
inline void Class::FixupNativePointer(
    Class* dest, PointerSize pointer_size, const Visitor& visitor, MemberOffset member_offset) {
  void** address =
      reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(dest) + member_offset.Uint32Value());
  T old_value = GetFieldPtrWithSize<T, kVerifyFlags>(member_offset, pointer_size);
  T new_value = visitor(old_value, address);
  if (old_value != new_value) {
    dest->SetFieldPtrWithSize</* kTransactionActive= */ false,
                              /* kCheckTransaction= */ true,
                              kVerifyNone>(member_offset, new_value, pointer_size);
  }
}

template <VerifyObjectFlags kVerifyFlags, typename Visitor>
inline void Class::FixupNativePointers(Class* dest,
                                       PointerSize pointer_size,
                                       const Visitor& visitor) {
  // Update the field arrays.
  FixupNativePointer<LengthPrefixedArray<ArtField>*, kVerifyFlags>(
      dest, pointer_size, visitor, OFFSET_OF_OBJECT_MEMBER(Class, sfields_));
  FixupNativePointer<LengthPrefixedArray<ArtField>*, kVerifyFlags>(
      dest, pointer_size, visitor, OFFSET_OF_OBJECT_MEMBER(Class, ifields_));
  // Update method array.
  FixupNativePointer<LengthPrefixedArray<ArtMethod>*, kVerifyFlags>(
      dest, pointer_size, visitor, OFFSET_OF_OBJECT_MEMBER(Class, methods_));
  // Fix up embedded tables.
  if (!IsTemp<kVerifyNone>() && ShouldHaveEmbeddedVTable<kVerifyNone>()) {
    for (int32_t i = 0, count = GetEmbeddedVTableLength<kVerifyFlags>(); i < count; ++i) {
      FixupNativePointer<ArtMethod*, kVerifyFlags>(
          dest, pointer_size, visitor, EmbeddedVTableEntryOffset(i, pointer_size));
    }
  }
  if (!IsTemp<kVerifyNone>() && ShouldHaveImt<kVerifyNone>()) {
    FixupNativePointer<ImTable*, kVerifyFlags>(
        dest, pointer_size, visitor, ImtPtrOffset(pointer_size));
  }
}

inline bool Class::CanAccess(ObjPtr<Class> that) {
  return that->IsPublic() || this->IsInSamePackage(that);
}


inline bool Class::CanAccessMember(ObjPtr<Class> access_to, uint32_t member_flags) {
  // Classes can access all of their own members
  if (this == access_to) {
    return true;
  }
  // Public members are trivially accessible
  if (member_flags & kAccPublic) {
    return true;
  }
  // Private members are trivially not accessible
  if (member_flags & kAccPrivate) {
    return false;
  }
  // Check for protected access from a sub-class, which may or may not be in the same package.
  if (member_flags & kAccProtected) {
    if (!this->IsInterface() && this->IsSubClass(access_to)) {
      return true;
    }
  }
  // Allow protected access from other classes in the same package.
  return this->IsInSamePackage(access_to);
}

inline bool Class::CannotBeAssignedFromOtherTypes() {
  if (!IsArrayClass()) {
    return IsFinal();
  }
  ObjPtr<Class> component = GetComponentType();
  return component->IsPrimitive() || component->CannotBeAssignedFromOtherTypes();
}

inline void Class::SetClassLoader(ObjPtr<ClassLoader> new_class_loader) {
  SetFieldObject</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
      OFFSET_OF_OBJECT_MEMBER(Class, class_loader_), new_class_loader);
}

inline void Class::SetRecursivelyInitialized() {
  DCHECK_EQ(GetLockOwnerThreadId(), Thread::Current()->GetThreadId());
  uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
  SetAccessFlags(flags | kAccRecursivelyInitialized);
}

inline void Class::SetHasDefaultMethods() {
  DCHECK_EQ(GetLockOwnerThreadId(), Thread::Current()->GetThreadId());
  uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
  SetAccessFlagsDuringLinking(flags | kAccHasDefaultMethod);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_INL_H_
