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

#include "reg_type-inl.h"

#include "android-base/stringprintf.h"

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/casts.h"
#include "class_linker-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file-inl.h"
#include "method_verifier.h"
#include "mirror/class-inl.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "reg_type_cache-inl.h"
#include "scoped_thread_state_change-inl.h"

#include <limits>
#include <sstream>

namespace art {
namespace verifier {

using android::base::StringPrintf;

const UndefinedType* UndefinedType::instance_ = nullptr;
const ConflictType* ConflictType::instance_ = nullptr;
const BooleanType* BooleanType::instance_ = nullptr;
const ByteType* ByteType::instance_ = nullptr;
const ShortType* ShortType::instance_ = nullptr;
const CharType* CharType::instance_ = nullptr;
const FloatType* FloatType::instance_ = nullptr;
const LongLoType* LongLoType::instance_ = nullptr;
const LongHiType* LongHiType::instance_ = nullptr;
const DoubleLoType* DoubleLoType::instance_ = nullptr;
const DoubleHiType* DoubleHiType::instance_ = nullptr;
const IntegerType* IntegerType::instance_ = nullptr;
const NullType* NullType::instance_ = nullptr;

PrimitiveType::PrimitiveType(ObjPtr<mirror::Class> klass,
                             const std::string_view& descriptor,
                             uint16_t cache_id)
    : RegType(klass, descriptor, cache_id) {
  CHECK(klass != nullptr);
  CHECK(!descriptor.empty());
}

Cat1Type::Cat1Type(ObjPtr<mirror::Class> klass,
                   const std::string_view& descriptor,
                   uint16_t cache_id)
    : PrimitiveType(klass, descriptor, cache_id) {
}

Cat2Type::Cat2Type(ObjPtr<mirror::Class> klass,
                   const std::string_view& descriptor,
                   uint16_t cache_id)
    : PrimitiveType(klass, descriptor, cache_id) {
}

std::string PreciseConstType::Dump() const {
  std::stringstream result;
  uint32_t val = ConstantValue();
  if (val == 0) {
    CHECK(IsPreciseConstant());
    result << "Zero/null";
  } else {
    result << "Precise ";
    if (IsConstantShort()) {
      result << StringPrintf("Constant: %d", val);
    } else {
      result << StringPrintf("Constant: 0x%x", val);
    }
  }
  return result.str();
}

std::string BooleanType::Dump() const {
  return "Boolean";
}

std::string ConflictType::Dump() const {
    return "Conflict";
}

std::string ByteType::Dump() const {
  return "Byte";
}

std::string ShortType::Dump() const {
  return "Short";
}

std::string CharType::Dump() const {
  return "Char";
}

std::string FloatType::Dump() const {
  return "Float";
}

std::string LongLoType::Dump() const {
  return "Long (Low Half)";
}

std::string LongHiType::Dump() const {
  return "Long (High Half)";
}

std::string DoubleLoType::Dump() const {
  return "Double (Low Half)";
}

std::string DoubleHiType::Dump() const {
  return "Double (High Half)";
}

std::string IntegerType::Dump() const {
  return "Integer";
}

const DoubleHiType* DoubleHiType::CreateInstance(ObjPtr<mirror::Class> klass,
                                                 const std::string_view& descriptor,
                                                 uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new DoubleHiType(klass, descriptor, cache_id);
  return instance_;
}

void DoubleHiType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const DoubleLoType* DoubleLoType::CreateInstance(ObjPtr<mirror::Class> klass,
                                                 const std::string_view& descriptor,
                                                 uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new DoubleLoType(klass, descriptor, cache_id);
  return instance_;
}

void DoubleLoType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const LongLoType* LongLoType::CreateInstance(ObjPtr<mirror::Class> klass,
                                             const std::string_view& descriptor,
                                             uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new LongLoType(klass, descriptor, cache_id);
  return instance_;
}

const LongHiType* LongHiType::CreateInstance(ObjPtr<mirror::Class> klass,
                                             const std::string_view& descriptor,
                                             uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new LongHiType(klass, descriptor, cache_id);
  return instance_;
}

void LongHiType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

void LongLoType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const FloatType* FloatType::CreateInstance(ObjPtr<mirror::Class> klass,
                                           const std::string_view& descriptor,
                                           uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new FloatType(klass, descriptor, cache_id);
  return instance_;
}

void FloatType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const CharType* CharType::CreateInstance(ObjPtr<mirror::Class> klass,
                                         const std::string_view& descriptor,
                                         uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new CharType(klass, descriptor, cache_id);
  return instance_;
}

void CharType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const ShortType* ShortType::CreateInstance(ObjPtr<mirror::Class> klass,
                                           const std::string_view& descriptor,
                                           uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new ShortType(klass, descriptor, cache_id);
  return instance_;
}

void ShortType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const ByteType* ByteType::CreateInstance(ObjPtr<mirror::Class> klass,
                                         const std::string_view& descriptor,
                                         uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new ByteType(klass, descriptor, cache_id);
  return instance_;
}

void ByteType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const IntegerType* IntegerType::CreateInstance(ObjPtr<mirror::Class> klass,
                                               const std::string_view& descriptor,
                                               uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new IntegerType(klass, descriptor, cache_id);
  return instance_;
}

void IntegerType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const ConflictType* ConflictType::CreateInstance(ObjPtr<mirror::Class> klass,
                                                 const std::string_view& descriptor,
                                                 uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new ConflictType(klass, descriptor, cache_id);
  return instance_;
}

void ConflictType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

const BooleanType* BooleanType::CreateInstance(ObjPtr<mirror::Class> klass,
                                               const std::string_view& descriptor,
                                               uint16_t cache_id) {
  CHECK(BooleanType::instance_ == nullptr);
  instance_ = new BooleanType(klass, descriptor, cache_id);
  return BooleanType::instance_;
}

void BooleanType::Destroy() {
  if (BooleanType::instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

std::string UndefinedType::Dump() const REQUIRES_SHARED(Locks::mutator_lock_) {
  return "Undefined";
}

const UndefinedType* UndefinedType::CreateInstance(ObjPtr<mirror::Class> klass,
                                                   const std::string_view& descriptor,
                                                   uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new UndefinedType(klass, descriptor, cache_id);
  return instance_;
}

void UndefinedType::Destroy() {
  if (instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}

PreciseReferenceType::PreciseReferenceType(ObjPtr<mirror::Class> klass,
                                           const std::string_view& descriptor,
                                           uint16_t cache_id)
    : RegType(klass, descriptor, cache_id) {
  // Note: no check for IsInstantiable() here. We may produce this in case an InstantiationError
  //       would be thrown at runtime, but we need to continue verification and *not* create a
  //       hard failure or abort.
  CheckConstructorInvariants(this);
}

std::string UnresolvedMergedType::Dump() const {
  std::stringstream result;
  result << "UnresolvedMergedReferences(" << GetResolvedPart().Dump() << " | ";
  const BitVector& types = GetUnresolvedTypes();

  bool first = true;
  for (uint32_t idx : types.Indexes()) {
    if (!first) {
      result << ", ";
    } else {
      first = false;
    }
    result << reg_type_cache_->GetFromId(idx).Dump();
  }
  result << ")";
  return result.str();
}

std::string UnresolvedSuperClass::Dump() const {
  std::stringstream result;
  uint16_t super_type_id = GetUnresolvedSuperClassChildId();
  result << "UnresolvedSuperClass(" << reg_type_cache_->GetFromId(super_type_id).Dump() << ")";
  return result.str();
}

std::string UnresolvedReferenceType::Dump() const {
  std::stringstream result;
  result << "Unresolved Reference: " << PrettyDescriptor(std::string(GetDescriptor()).c_str());
  return result.str();
}

std::string UnresolvedUninitializedRefType::Dump() const {
  std::stringstream result;
  result << "Unresolved And Uninitialized Reference: "
      << PrettyDescriptor(std::string(GetDescriptor()).c_str())
      << " Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string UnresolvedUninitializedThisRefType::Dump() const {
  std::stringstream result;
  result << "Unresolved And Uninitialized This Reference: "
      << PrettyDescriptor(std::string(GetDescriptor()).c_str());
  return result.str();
}

std::string ReferenceType::Dump() const {
  std::stringstream result;
  result << "Reference: " << mirror::Class::PrettyDescriptor(GetClass());
  return result.str();
}

std::string PreciseReferenceType::Dump() const {
  std::stringstream result;
  result << "Precise Reference: " << mirror::Class::PrettyDescriptor(GetClass());
  return result.str();
}

std::string UninitializedReferenceType::Dump() const {
  std::stringstream result;
  result << "Uninitialized Reference: " << mirror::Class::PrettyDescriptor(GetClass());
  result << " Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string UninitializedThisReferenceType::Dump() const {
  std::stringstream result;
  result << "Uninitialized This Reference: " << mirror::Class::PrettyDescriptor(GetClass());
  result << "Allocation PC: " << GetAllocationPc();
  return result.str();
}

std::string ImpreciseConstType::Dump() const {
  std::stringstream result;
  uint32_t val = ConstantValue();
  if (val == 0) {
    result << "Zero/null";
  } else {
    result << "Imprecise ";
    if (IsConstantShort()) {
      result << StringPrintf("Constant: %d", val);
    } else {
      result << StringPrintf("Constant: 0x%x", val);
    }
  }
  return result.str();
}
std::string PreciseConstLoType::Dump() const {
  std::stringstream result;

  int32_t val = ConstantValueLo();
  result << "Precise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("Low-half Constant: %d", val);
  } else {
    result << StringPrintf("Low-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string ImpreciseConstLoType::Dump() const {
  std::stringstream result;

  int32_t val = ConstantValueLo();
  result << "Imprecise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("Low-half Constant: %d", val);
  } else {
    result << StringPrintf("Low-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string PreciseConstHiType::Dump() const {
  std::stringstream result;
  int32_t val = ConstantValueHi();
  result << "Precise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("High-half Constant: %d", val);
  } else {
    result << StringPrintf("High-half Constant: 0x%x", val);
  }
  return result.str();
}

std::string ImpreciseConstHiType::Dump() const {
  std::stringstream result;
  int32_t val = ConstantValueHi();
  result << "Imprecise ";
  if (val >= std::numeric_limits<jshort>::min() &&
      val <= std::numeric_limits<jshort>::max()) {
    result << StringPrintf("High-half Constant: %d", val);
  } else {
    result << StringPrintf("High-half Constant: 0x%x", val);
  }
  return result.str();
}

const RegType& RegType::HighHalf(RegTypeCache* cache) const {
  DCHECK(IsLowHalf());
  if (IsLongLo()) {
    return cache->LongHi();
  } else if (IsDoubleLo()) {
    return cache->DoubleHi();
  } else {
    DCHECK(IsImpreciseConstantLo());
    const ConstantType* const_val = down_cast<const ConstantType*>(this);
    return cache->FromCat2ConstHi(const_val->ConstantValue(), false);
  }
}

Primitive::Type RegType::GetPrimitiveType() const {
  if (IsNonZeroReferenceTypes()) {
    return Primitive::kPrimNot;
  } else if (IsBooleanTypes()) {
    return Primitive::kPrimBoolean;
  } else if (IsByteTypes()) {
    return Primitive::kPrimByte;
  } else if (IsShortTypes()) {
    return Primitive::kPrimShort;
  } else if (IsCharTypes()) {
    return Primitive::kPrimChar;
  } else if (IsFloat()) {
    return Primitive::kPrimFloat;
  } else if (IsIntegralTypes()) {
    return Primitive::kPrimInt;
  } else if (IsDoubleLo()) {
    return Primitive::kPrimDouble;
  } else {
    DCHECK(IsLongTypes());
    return Primitive::kPrimLong;
  }
}

bool UninitializedType::IsUninitializedTypes() const {
  return true;
}

bool UninitializedType::IsNonZeroReferenceTypes() const {
  return true;
}

bool UnresolvedType::IsNonZeroReferenceTypes() const {
  return true;
}

const RegType& RegType::GetSuperClass(RegTypeCache* cache) const {
  if (!IsUnresolvedTypes()) {
    ObjPtr<mirror::Class> super_klass = GetClass()->GetSuperClass();
    if (super_klass != nullptr) {
      // A super class of a precise type isn't precise as a precise type indicates the register
      // holds exactly that type.
      std::string temp;
      return cache->FromClass(super_klass->GetDescriptor(&temp), super_klass, false);
    } else {
      return cache->Zero();
    }
  } else {
    if (!IsUnresolvedMergedReference() && !IsUnresolvedSuperClass() &&
        GetDescriptor()[0] == '[') {
      // Super class of all arrays is Object.
      return cache->JavaLangObject(true);
    } else {
      return cache->FromUnresolvedSuperClass(*this);
    }
  }
}

bool RegType::IsJavaLangObject() const REQUIRES_SHARED(Locks::mutator_lock_) {
  return IsReference() && GetClass()->IsObjectClass();
}

bool RegType::IsObjectArrayTypes() const REQUIRES_SHARED(Locks::mutator_lock_) {
  if (IsUnresolvedTypes()) {
    DCHECK(!IsUnresolvedMergedReference());

    if (IsUnresolvedSuperClass()) {
      // Cannot be an array, as the superclass of arrays is java.lang.Object (which cannot be
      // unresolved).
      return false;
    }

    // Primitive arrays will always resolve.
    DCHECK(descriptor_[1] == 'L' || descriptor_[1] == '[');
    return descriptor_[0] == '[';
  } else if (HasClass()) {
    ObjPtr<mirror::Class> type = GetClass();
    return type->IsArrayClass() && !type->GetComponentType()->IsPrimitive();
  } else {
    return false;
  }
}

bool RegType::IsArrayTypes() const REQUIRES_SHARED(Locks::mutator_lock_) {
  if (IsUnresolvedTypes()) {
    DCHECK(!IsUnresolvedMergedReference());

    if (IsUnresolvedSuperClass()) {
      // Cannot be an array, as the superclass of arrays is java.lang.Object (which cannot be
      // unresolved).
      return false;
    }
    return descriptor_[0] == '[';
  } else if (HasClass()) {
    return GetClass()->IsArrayClass();
  } else {
    return false;
  }
}

bool RegType::IsJavaLangObjectArray() const {
  if (HasClass()) {
    ObjPtr<mirror::Class> type = GetClass();
    return type->IsArrayClass() && type->GetComponentType()->IsObjectClass();
  }
  return false;
}

bool RegType::IsInstantiableTypes() const {
  return IsUnresolvedTypes() || (IsNonZeroReferenceTypes() && GetClass()->IsInstantiable());
}

static const RegType& SelectNonConstant(const RegType& a, const RegType& b) {
  return a.IsConstantTypes() ? b : a;
}

static const RegType& SelectNonConstant2(const RegType& a, const RegType& b) {
  return a.IsConstantTypes() ? (b.IsZero() ? a : b) : a;
}


namespace {

ObjPtr<mirror::Class> ArrayClassJoin(ObjPtr<mirror::Class> s,
                                     ObjPtr<mirror::Class> t,
                                     ClassLinker* class_linker)
    REQUIRES_SHARED(Locks::mutator_lock_);

ObjPtr<mirror::Class> InterfaceClassJoin(ObjPtr<mirror::Class> s, ObjPtr<mirror::Class> t)
    REQUIRES_SHARED(Locks::mutator_lock_);

/*
 * A basic Join operation on classes. For a pair of types S and T the Join, written S v T = J, is
 * S <: J, T <: J and for-all U such that S <: U, T <: U then J <: U. That is J is the parent of
 * S and T such that there isn't a parent of both S and T that isn't also the parent of J (ie J
 * is the deepest (lowest upper bound) parent of S and T).
 *
 * This operation applies for regular classes and arrays, however, for interface types there
 * needn't be a partial ordering on the types. We could solve the problem of a lack of a partial
 * order by introducing sets of types, however, the only operation permissible on an interface is
 * invoke-interface. In the tradition of Java verifiers [1] we defer the verification of interface
 * types until an invoke-interface call on the interface typed reference at runtime and allow
 * the perversion of Object being assignable to an interface type (note, however, that we don't
 * allow assignment of Object or Interface to any concrete class and are therefore type safe).
 *
 * Note: This may return null in case of internal errors, e.g., OOME when a new class would have
 *       to be created but there is no heap space. The exception will stay pending, and it is
 *       the job of the caller to handle it.
 *
 * [1] Java bytecode verification: algorithms and formalizations, Xavier Leroy
 */
ObjPtr<mirror::Class> ClassJoin(ObjPtr<mirror::Class> s,
                                ObjPtr<mirror::Class> t,
                                ClassLinker* class_linker)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(!s->IsPrimitive()) << s->PrettyClass();
  DCHECK(!t->IsPrimitive()) << t->PrettyClass();
  if (s == t) {
    return s;
  } else if (s->IsAssignableFrom(t)) {
    return s;
  } else if (t->IsAssignableFrom(s)) {
    return t;
  } else if (s->IsArrayClass() && t->IsArrayClass()) {
    return ArrayClassJoin(s, t, class_linker);
  } else if (s->IsInterface() || t->IsInterface()) {
    return InterfaceClassJoin(s, t);
  } else {
    size_t s_depth = s->Depth();
    size_t t_depth = t->Depth();
    // Get s and t to the same depth in the hierarchy
    if (s_depth > t_depth) {
      while (s_depth > t_depth) {
        s = s->GetSuperClass();
        s_depth--;
      }
    } else {
      while (t_depth > s_depth) {
        t = t->GetSuperClass();
        t_depth--;
      }
    }
    // Go up the hierarchy until we get to the common parent
    while (s != t) {
      s = s->GetSuperClass();
      t = t->GetSuperClass();
    }
    return s;
  }
}

ObjPtr<mirror::Class> ArrayClassJoin(ObjPtr<mirror::Class> s,
                                     ObjPtr<mirror::Class> t,
                                     ClassLinker* class_linker) {
  ObjPtr<mirror::Class> s_ct = s->GetComponentType();
  ObjPtr<mirror::Class> t_ct = t->GetComponentType();
  if (s_ct->IsPrimitive() || t_ct->IsPrimitive()) {
    // Given the types aren't the same, if either array is of primitive types then the only
    // common parent is java.lang.Object
    ObjPtr<mirror::Class> result = s->GetSuperClass();  // short-cut to java.lang.Object
    DCHECK(result->IsObjectClass());
    return result;
  }
  Thread* self = Thread::Current();
  ObjPtr<mirror::Class> common_elem = ClassJoin(s_ct, t_ct, class_linker);
  if (UNLIKELY(common_elem == nullptr)) {
    self->AssertPendingException();
    return nullptr;
  }
  // Note: The following lookup invalidates existing ObjPtr<>s.
  ObjPtr<mirror::Class> array_class = class_linker->FindArrayClass(self, common_elem);
  if (UNLIKELY(array_class == nullptr)) {
    self->AssertPendingException();
    return nullptr;
  }
  return array_class;
}

ObjPtr<mirror::Class> InterfaceClassJoin(ObjPtr<mirror::Class> s, ObjPtr<mirror::Class> t) {
  // This is expensive, as we do not have good data structures to do this even halfway
  // efficiently.
  //
  // We're not following JVMS for interface verification (not everything is assignable to an
  // interface, we trade this for IMT dispatch). We also don't have set types to make up for
  // it. So we choose one arbitrary common ancestor interface by walking the interface tables
  // backwards.
  //
  // For comparison, runtimes following the JVMS will punt all interface type checking to
  // runtime.
  ObjPtr<mirror::IfTable> s_if = s->GetIfTable();
  int32_t s_if_count = s->GetIfTableCount();
  ObjPtr<mirror::IfTable> t_if = t->GetIfTable();
  int32_t t_if_count = t->GetIfTableCount();

  // Note: we'll be using index == count to stand for the argument itself.
  for (int32_t s_it = s_if_count; s_it >= 0; --s_it) {
    ObjPtr<mirror::Class> s_cl = s_it == s_if_count ? s : s_if->GetInterface(s_it);
    if (!s_cl->IsInterface()) {
      continue;
    }

    for (int32_t t_it = t_if_count; t_it >= 0; --t_it) {
      ObjPtr<mirror::Class> t_cl = t_it == t_if_count ? t : t_if->GetInterface(t_it);
      if (!t_cl->IsInterface()) {
        continue;
      }

      if (s_cl == t_cl) {
        // Found something arbitrary in common.
        return s_cl;
      }
    }
  }

  // Return java.lang.Object.
  ObjPtr<mirror::Class> obj_class = s->IsInterface() ? s->GetSuperClass() : t->GetSuperClass();
  DCHECK(obj_class->IsObjectClass());
  return obj_class;
}

}  // namespace

const RegType& RegType::Merge(const RegType& incoming_type,
                              RegTypeCache* reg_types,
                              MethodVerifier* verifier) const {
  DCHECK(!Equals(incoming_type));  // Trivial equality handled by caller
  // Perform pointer equality tests for undefined and conflict to avoid virtual method dispatch.
  const UndefinedType& undefined = reg_types->Undefined();
  const ConflictType& conflict = reg_types->Conflict();
  DCHECK_EQ(this == &undefined, IsUndefined());
  DCHECK_EQ(&incoming_type == &undefined, incoming_type.IsUndefined());
  DCHECK_EQ(this == &conflict, IsConflict());
  DCHECK_EQ(&incoming_type == &conflict, incoming_type.IsConflict());
  if (this == &undefined || &incoming_type == &undefined) {
    // There is a difference between undefined and conflict. Conflicts may be copied around, but
    // not used. Undefined registers must not be copied. So any merge with undefined should return
    // undefined.
    return undefined;
  } else if (this == &conflict || &incoming_type == &conflict) {
    return conflict;  // (Conflict MERGE *) or (* MERGE Conflict) => Conflict
  } else if (IsConstant() && incoming_type.IsConstant()) {
    const ConstantType& type1 = *down_cast<const ConstantType*>(this);
    const ConstantType& type2 = *down_cast<const ConstantType*>(&incoming_type);
    int32_t val1 = type1.ConstantValue();
    int32_t val2 = type2.ConstantValue();
    if (val1 >= 0 && val2 >= 0) {
      // +ve1 MERGE +ve2 => MAX(+ve1, +ve2)
      if (val1 >= val2) {
        if (!type1.IsPreciseConstant()) {
          return *this;
        } else {
          return reg_types->FromCat1Const(val1, false);
        }
      } else {
        if (!type2.IsPreciseConstant()) {
          return type2;
        } else {
          return reg_types->FromCat1Const(val2, false);
        }
      }
    } else if (val1 < 0 && val2 < 0) {
      // -ve1 MERGE -ve2 => MIN(-ve1, -ve2)
      if (val1 <= val2) {
        if (!type1.IsPreciseConstant()) {
          return *this;
        } else {
          return reg_types->FromCat1Const(val1, false);
        }
      } else {
        if (!type2.IsPreciseConstant()) {
          return type2;
        } else {
          return reg_types->FromCat1Const(val2, false);
        }
      }
    } else {
      // Values are +ve and -ve, choose smallest signed type in which they both fit
      if (type1.IsConstantByte()) {
        if (type2.IsConstantByte()) {
          return reg_types->ByteConstant();
        } else if (type2.IsConstantShort()) {
          return reg_types->ShortConstant();
        } else {
          return reg_types->IntConstant();
        }
      } else if (type1.IsConstantShort()) {
        if (type2.IsConstantShort()) {
          return reg_types->ShortConstant();
        } else {
          return reg_types->IntConstant();
        }
      } else {
        return reg_types->IntConstant();
      }
    }
  } else if (IsConstantLo() && incoming_type.IsConstantLo()) {
    const ConstantType& type1 = *down_cast<const ConstantType*>(this);
    const ConstantType& type2 = *down_cast<const ConstantType*>(&incoming_type);
    int32_t val1 = type1.ConstantValueLo();
    int32_t val2 = type2.ConstantValueLo();
    return reg_types->FromCat2ConstLo(val1 | val2, false);
  } else if (IsConstantHi() && incoming_type.IsConstantHi()) {
    const ConstantType& type1 = *down_cast<const ConstantType*>(this);
    const ConstantType& type2 = *down_cast<const ConstantType*>(&incoming_type);
    int32_t val1 = type1.ConstantValueHi();
    int32_t val2 = type2.ConstantValueHi();
    return reg_types->FromCat2ConstHi(val1 | val2, false);
  } else if (IsIntegralTypes() && incoming_type.IsIntegralTypes()) {
    if (IsBooleanTypes() && incoming_type.IsBooleanTypes()) {
      return reg_types->Boolean();  // boolean MERGE boolean => boolean
    }
    if (IsByteTypes() && incoming_type.IsByteTypes()) {
      return reg_types->Byte();  // byte MERGE byte => byte
    }
    if (IsShortTypes() && incoming_type.IsShortTypes()) {
      return reg_types->Short();  // short MERGE short => short
    }
    if (IsCharTypes() && incoming_type.IsCharTypes()) {
      return reg_types->Char();  // char MERGE char => char
    }
    return reg_types->Integer();  // int MERGE * => int
  } else if ((IsFloatTypes() && incoming_type.IsFloatTypes()) ||
             (IsLongTypes() && incoming_type.IsLongTypes()) ||
             (IsLongHighTypes() && incoming_type.IsLongHighTypes()) ||
             (IsDoubleTypes() && incoming_type.IsDoubleTypes()) ||
             (IsDoubleHighTypes() && incoming_type.IsDoubleHighTypes())) {
    // check constant case was handled prior to entry
    DCHECK(!IsConstant() || !incoming_type.IsConstant());
    // float/long/double MERGE float/long/double_constant => float/long/double
    return SelectNonConstant(*this, incoming_type);
  } else if (IsReferenceTypes() && incoming_type.IsReferenceTypes()) {
    if (IsUninitializedTypes() || incoming_type.IsUninitializedTypes()) {
      // Something that is uninitialized hasn't had its constructor called. Unitialized types are
      // special. They may only ever be merged with themselves (must be taken care of by the
      // caller of Merge(), see the DCHECK on entry). So mark any other merge as conflicting here.
      return conflict;
    } else if (IsZeroOrNull() || incoming_type.IsZeroOrNull()) {
      return SelectNonConstant2(*this, incoming_type);  // 0 MERGE ref => ref
    } else if (IsJavaLangObject() || incoming_type.IsJavaLangObject()) {
      return reg_types->JavaLangObject(false);  // Object MERGE ref => Object
    } else if (IsUnresolvedTypes() || incoming_type.IsUnresolvedTypes()) {
      // We know how to merge an unresolved type with itself, 0 or Object. In this case we
      // have two sub-classes and don't know how to merge. Create a new string-based unresolved
      // type that reflects our lack of knowledge and that allows the rest of the unresolved
      // mechanics to continue.
      return reg_types->FromUnresolvedMerge(*this, incoming_type, verifier);
    } else {  // Two reference types, compute Join
      // Do not cache the classes as ClassJoin() can suspend and invalidate ObjPtr<>s.
      DCHECK(GetClass() != nullptr && !GetClass()->IsPrimitive());
      DCHECK(incoming_type.GetClass() != nullptr && !incoming_type.GetClass()->IsPrimitive());
      ObjPtr<mirror::Class> join_class = ClassJoin(GetClass(),
                                                   incoming_type.GetClass(),
                                                   reg_types->GetClassLinker());
      if (UNLIKELY(join_class == nullptr)) {
        // Internal error joining the classes (e.g., OOME). Report an unresolved reference type.
        // We cannot report an unresolved merge type, as that will attempt to merge the resolved
        // components, leaving us in an infinite loop.
        // We do not want to report the originating exception, as that would require a fast path
        // out all the way to VerifyClass. Instead attempt to continue on without a detailed type.
        Thread* self = Thread::Current();
        self->AssertPendingException();
        self->ClearException();

        // When compiling on the host, we rather want to abort to ensure determinism for preopting.
        // (In that case, it is likely a misconfiguration of dex2oat.)
        if (!kIsTargetBuild && (verifier != nullptr && verifier->IsAotMode())) {
          LOG(FATAL) << "Could not create class join of "
                     << GetClass()->PrettyClass()
                     << " & "
                     << incoming_type.GetClass()->PrettyClass();
          UNREACHABLE();
        }

        return reg_types->MakeUnresolvedReference();
      }

      // Record the dependency that both `GetClass()` and `incoming_type.GetClass()`
      // are assignable to `join_class`. The `verifier` is null during unit tests.
      if (verifier != nullptr) {
        VerifierDeps::MaybeRecordAssignability(verifier->GetDexFile(),
                                               join_class,
                                               GetClass(),
                                               /* is_strict= */ true,
                                               /* is_assignable= */ true);
        VerifierDeps::MaybeRecordAssignability(verifier->GetDexFile(),
                                               join_class,
                                               incoming_type.GetClass(),
                                               /* is_strict= */ true,
                                               /* is_assignable= */ true);
      }
      if (GetClass() == join_class && !IsPreciseReference()) {
        return *this;
      } else if (incoming_type.GetClass() == join_class && !incoming_type.IsPreciseReference()) {
        return incoming_type;
      } else {
        std::string temp;
        const char* descriptor = join_class->GetDescriptor(&temp);
        return reg_types->FromClass(descriptor, join_class, /* precise= */ false);
      }
    }
  } else {
    return conflict;  // Unexpected types => Conflict
  }
}

void RegType::CheckInvariants() const {
  if (IsConstant() || IsConstantLo() || IsConstantHi()) {
    CHECK(descriptor_.empty()) << *this;
    CHECK(klass_.IsNull()) << *this;
  }
  if (!klass_.IsNull()) {
    CHECK(!descriptor_.empty()) << *this;
    std::string temp;
    CHECK_EQ(descriptor_, klass_.Read()->GetDescriptor(&temp)) << *this;
  }
}

void RegType::VisitRoots(RootVisitor* visitor, const RootInfo& root_info) const {
  klass_.VisitRootIfNonNull(visitor, root_info);
}

void UninitializedThisReferenceType::CheckInvariants() const {
  CHECK_EQ(GetAllocationPc(), 0U) << *this;
}

void UnresolvedUninitializedThisRefType::CheckInvariants() const {
  CHECK_EQ(GetAllocationPc(), 0U) << *this;
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_.IsNull()) << *this;
}

void UnresolvedUninitializedRefType::CheckInvariants() const {
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_.IsNull()) << *this;
}

UnresolvedMergedType::UnresolvedMergedType(const RegType& resolved,
                                           const BitVector& unresolved,
                                           const RegTypeCache* reg_type_cache,
                                           uint16_t cache_id)
    : UnresolvedType("", cache_id),
      reg_type_cache_(reg_type_cache),
      resolved_part_(resolved),
      unresolved_types_(unresolved, false, unresolved.GetAllocator()) {
  CheckConstructorInvariants(this);
}
void UnresolvedMergedType::CheckInvariants() const {
  CHECK(reg_type_cache_ != nullptr);

  // Unresolved merged types: merged types should be defined.
  CHECK(descriptor_.empty()) << *this;
  CHECK(klass_.IsNull()) << *this;

  CHECK(!resolved_part_.IsConflict());
  CHECK(resolved_part_.IsReferenceTypes());
  CHECK(!resolved_part_.IsUnresolvedTypes());

  CHECK(resolved_part_.IsZero() ||
        !(resolved_part_.IsArrayTypes() && !resolved_part_.IsObjectArrayTypes()));

  CHECK_GT(unresolved_types_.NumSetBits(), 0U);
  bool unresolved_is_array =
      reg_type_cache_->GetFromId(unresolved_types_.GetHighestBitSet()).IsArrayTypes();
  for (uint32_t idx : unresolved_types_.Indexes()) {
    const RegType& t = reg_type_cache_->GetFromId(idx);
    CHECK_EQ(unresolved_is_array, t.IsArrayTypes());
  }

  if (!resolved_part_.IsZero()) {
    CHECK_EQ(resolved_part_.IsArrayTypes(), unresolved_is_array);
  }
}

bool UnresolvedMergedType::IsArrayTypes() const {
  // For a merge to be an array, both the resolved and the unresolved part need to be object
  // arrays.
  // (Note: we encode a missing resolved part [which doesn't need to be an array] as zero.)

  if (!resolved_part_.IsZero() && !resolved_part_.IsArrayTypes()) {
    return false;
  }

  // It is enough to check just one of the merged types. Otherwise the merge should have been
  // collapsed (checked in CheckInvariants on construction).
  uint32_t idx = unresolved_types_.GetHighestBitSet();
  const RegType& unresolved = reg_type_cache_->GetFromId(idx);
  return unresolved.IsArrayTypes();
}
bool UnresolvedMergedType::IsObjectArrayTypes() const {
  // Same as IsArrayTypes, as primitive arrays are always resolved.
  return IsArrayTypes();
}

void UnresolvedReferenceType::CheckInvariants() const {
  CHECK(!descriptor_.empty()) << *this;
  CHECK(klass_.IsNull()) << *this;
}

void UnresolvedSuperClass::CheckInvariants() const {
  // Unresolved merged types: merged types should be defined.
  CHECK(descriptor_.empty()) << *this;
  CHECK(klass_.IsNull()) << *this;
  CHECK_NE(unresolved_child_id_, 0U) << *this;
}

std::ostream& operator<<(std::ostream& os, const RegType& rhs) {
  os << rhs.Dump();
  return os;
}

bool RegType::CanAssignArray(const RegType& src,
                             RegTypeCache& reg_types,
                             Handle<mirror::ClassLoader> class_loader,
                             MethodVerifier* verifier,
                             bool* soft_error) const {
  if (!IsArrayTypes() || !src.IsArrayTypes()) {
    *soft_error = false;
    return false;
  }

  if (IsUnresolvedMergedReference() || src.IsUnresolvedMergedReference()) {
    // An unresolved array type means that it's an array of some reference type. Reference arrays
    // can never be assigned to primitive-type arrays, and vice versa. So it is a soft error if
    // both arrays are reference arrays, otherwise a hard error.
    *soft_error = IsObjectArrayTypes() && src.IsObjectArrayTypes();
    return false;
  }

  const RegType& cmp1 = reg_types.GetComponentType(*this, class_loader.Get());
  const RegType& cmp2 = reg_types.GetComponentType(src, class_loader.Get());

  if (cmp1.IsAssignableFrom(cmp2, verifier)) {
    return true;
  }
  if (cmp1.IsUnresolvedTypes()) {
    if (cmp2.IsIntegralTypes() || cmp2.IsFloatTypes() || cmp2.IsArrayTypes()) {
      *soft_error = false;
      return false;
    }
    *soft_error = true;
    return false;
  }
  if (cmp2.IsUnresolvedTypes()) {
    if (cmp1.IsIntegralTypes() || cmp1.IsFloatTypes() || cmp1.IsArrayTypes()) {
      *soft_error = false;
      return false;
    }
    *soft_error = true;
    return false;
  }
  if (!cmp1.IsArrayTypes() || !cmp2.IsArrayTypes()) {
    *soft_error = false;
    return false;
  }
  return cmp1.CanAssignArray(cmp2, reg_types, class_loader, verifier, soft_error);
}

const NullType* NullType::CreateInstance(ObjPtr<mirror::Class> klass,
                                         const std::string_view& descriptor,
                                         uint16_t cache_id) {
  CHECK(instance_ == nullptr);
  instance_ = new NullType(klass, descriptor, cache_id);
  return instance_;
}

void NullType::Destroy() {
  if (NullType::instance_ != nullptr) {
    delete instance_;
    instance_ = nullptr;
  }
}


}  // namespace verifier
}  // namespace art
