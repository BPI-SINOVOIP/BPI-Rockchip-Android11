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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_H_

#include <stdint.h>
#include <limits>
#include <set>
#include <string>
#include <string_view>

#include "base/arena_object.h"
#include "base/bit_vector.h"
#include "base/locks.h"
#include "base/macros.h"
#include "dex/primitive.h"
#include "gc_root.h"
#include "handle_scope.h"
#include "obj_ptr.h"

namespace art {
namespace mirror {
class Class;
class ClassLoader;
}  // namespace mirror

class ArenaBitVector;
class ScopedArenaAllocator;

namespace verifier {

class MethodVerifier;
class RegTypeCache;

/*
 * RegType holds information about the "type" of data held in a register.
 */
class RegType {
 public:
  virtual bool IsUndefined() const { return false; }
  virtual bool IsConflict() const { return false; }
  virtual bool IsBoolean() const { return false; }
  virtual bool IsByte() const { return false; }
  virtual bool IsChar() const { return false; }
  virtual bool IsShort() const { return false; }
  virtual bool IsInteger() const { return false; }
  virtual bool IsLongLo() const { return false; }
  virtual bool IsLongHi() const { return false; }
  virtual bool IsFloat() const { return false; }
  virtual bool IsDouble() const { return false; }
  virtual bool IsDoubleLo() const { return false; }
  virtual bool IsDoubleHi() const { return false; }
  virtual bool IsUnresolvedReference() const { return false; }
  virtual bool IsUninitializedReference() const { return false; }
  virtual bool IsUninitializedThisReference() const { return false; }
  virtual bool IsUnresolvedAndUninitializedReference() const { return false; }
  virtual bool IsUnresolvedAndUninitializedThisReference() const {
    return false;
  }
  virtual bool IsUnresolvedMergedReference() const { return false; }
  virtual bool IsUnresolvedSuperClass() const { return false; }
  virtual bool IsReference() const { return false; }
  virtual bool IsPreciseReference() const { return false; }
  virtual bool IsPreciseConstant() const { return false; }
  virtual bool IsPreciseConstantLo() const { return false; }
  virtual bool IsPreciseConstantHi() const { return false; }
  virtual bool IsImpreciseConstantLo() const { return false; }
  virtual bool IsImpreciseConstantHi() const { return false; }
  virtual bool IsImpreciseConstant() const { return false; }
  virtual bool IsConstantTypes() const { return false; }
  bool IsConstant() const {
    return IsImpreciseConstant() || IsPreciseConstant();
  }
  bool IsConstantLo() const {
    return IsImpreciseConstantLo() || IsPreciseConstantLo();
  }
  bool IsPrecise() const {
    return IsPreciseConstantLo() || IsPreciseConstant() ||
           IsPreciseConstantHi();
  }
  bool IsLongConstant() const { return IsConstantLo(); }
  bool IsConstantHi() const {
    return (IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsLongConstantHigh() const { return IsConstantHi(); }
  virtual bool IsUninitializedTypes() const { return false; }
  virtual bool IsUnresolvedTypes() const { return false; }

  bool IsLowHalf() const {
    return (IsLongLo() || IsDoubleLo() || IsPreciseConstantLo() || IsImpreciseConstantLo());
  }
  bool IsHighHalf() const {
    return (IsLongHi() || IsDoubleHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsLongOrDoubleTypes() const { return IsLowHalf(); }
  // Check this is the low half, and that type_h is its matching high-half.
  inline bool CheckWidePair(const RegType& type_h) const {
    if (IsLowHalf()) {
      return ((IsImpreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsImpreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsPreciseConstantLo() && type_h.IsPreciseConstantHi()) ||
              (IsPreciseConstantLo() && type_h.IsImpreciseConstantHi()) ||
              (IsDoubleLo() && type_h.IsDoubleHi()) ||
              (IsLongLo() && type_h.IsLongHi()));
    }
    return false;
  }
  // The high half that corresponds to this low half
  const RegType& HighHalf(RegTypeCache* cache) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsConstantBoolean() const;
  virtual bool IsConstantChar() const { return false; }
  virtual bool IsConstantByte() const { return false; }
  virtual bool IsConstantShort() const { return false; }
  virtual bool IsOne() const { return false; }
  virtual bool IsZero() const { return false; }
  virtual bool IsNull() const { return false; }
  bool IsReferenceTypes() const {
    return IsNonZeroReferenceTypes() || IsZero() || IsNull();
  }
  bool IsZeroOrNull() const {
    return IsZero() || IsNull();
  }
  virtual bool IsNonZeroReferenceTypes() const { return false; }
  bool IsCategory1Types() const {
    return IsChar() || IsInteger() || IsFloat() || IsConstant() || IsByte() ||
           IsShort() || IsBoolean();
  }
  bool IsCategory2Types() const {
    return IsLowHalf();  // Don't expect explicit testing of high halves
  }
  bool IsBooleanTypes() const { return IsBoolean() || IsConstantBoolean(); }
  bool IsByteTypes() const {
    return IsConstantByte() || IsByte() || IsBoolean();
  }
  bool IsShortTypes() const {
    return IsShort() || IsByte() || IsBoolean() || IsConstantShort();
  }
  bool IsCharTypes() const {
    return IsChar() || IsBooleanTypes() || IsConstantChar();
  }
  bool IsIntegralTypes() const {
    return IsInteger() || IsConstant() || IsByte() || IsShort() || IsChar() ||
           IsBoolean();
  }
  // Give the constant value encoded, but this shouldn't be called in the
  // general case.
  bool IsArrayIndexTypes() const { return IsIntegralTypes(); }
  // Float type may be derived from any constant type
  bool IsFloatTypes() const { return IsFloat() || IsConstant(); }
  bool IsLongTypes() const { return IsLongLo() || IsLongConstant(); }
  bool IsLongHighTypes() const {
    return (IsLongHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  bool IsDoubleTypes() const { return IsDoubleLo() || IsLongConstant(); }
  bool IsDoubleHighTypes() const {
    return (IsDoubleHi() || IsPreciseConstantHi() || IsImpreciseConstantHi());
  }
  virtual bool IsLong() const { return false; }
  bool HasClass() const {
    bool result = !klass_.IsNull();
    DCHECK_EQ(result, HasClassVirtual());
    return result;
  }
  virtual bool HasClassVirtual() const { return false; }
  bool IsJavaLangObject() const REQUIRES_SHARED(Locks::mutator_lock_);
  virtual bool IsArrayTypes() const REQUIRES_SHARED(Locks::mutator_lock_);
  virtual bool IsObjectArrayTypes() const REQUIRES_SHARED(Locks::mutator_lock_);
  Primitive::Type GetPrimitiveType() const;
  bool IsJavaLangObjectArray() const
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsInstantiableTypes() const REQUIRES_SHARED(Locks::mutator_lock_);
  const std::string_view& GetDescriptor() const {
    DCHECK(HasClass() ||
           (IsUnresolvedTypes() && !IsUnresolvedMergedReference() &&
            !IsUnresolvedSuperClass()));
    return descriptor_;
  }
  ObjPtr<mirror::Class> GetClass() const REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(!IsUnresolvedReference());
    DCHECK(!klass_.IsNull()) << Dump();
    DCHECK(HasClass());
    return klass_.Read();
  }
  uint16_t GetId() const { return cache_id_; }
  const RegType& GetSuperClass(RegTypeCache* cache) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  virtual std::string Dump() const
      REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  // Can this type access other?
  bool CanAccess(const RegType& other) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this type access a member with the given properties?
  bool CanAccessMember(ObjPtr<mirror::Class> klass, uint32_t access_flags) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this type be assigned by src?
  // Note: Object and interface types may always be assigned to one another, see
  // comment on
  // ClassJoin.
  bool IsAssignableFrom(const RegType& src, MethodVerifier* verifier) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this array type potentially be assigned by src.
  // This function is necessary as array types are valid even if their components types are not,
  // e.g., when they component type could not be resolved. The function will return true iff the
  // types are assignable. It will return false otherwise. In case of return=false, soft_error
  // will be set to true iff the assignment test failure should be treated as a soft-error, i.e.,
  // when both array types have the same 'depth' and the 'final' component types may be assignable
  // (both are reference types).
  bool CanAssignArray(const RegType& src,
                      RegTypeCache& reg_types,
                      Handle<mirror::ClassLoader> class_loader,
                      MethodVerifier* verifier,
                      bool* soft_error) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this type be assigned by src? Variant of IsAssignableFrom that doesn't
  // allow assignment to
  // an interface from an Object.
  bool IsStrictlyAssignableFrom(const RegType& src, MethodVerifier* verifier) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Are these RegTypes the same?
  bool Equals(const RegType& other) const { return GetId() == other.GetId(); }

  // Compute the merge of this register from one edge (path) with incoming_type
  // from another.
  const RegType& Merge(const RegType& incoming_type,
                       RegTypeCache* reg_types,
                       MethodVerifier* verifier) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Same as above, but also handles the case where incoming_type == this.
  const RegType& SafeMerge(const RegType& incoming_type,
                           RegTypeCache* reg_types,
                           MethodVerifier* verifier) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (Equals(incoming_type)) {
      return *this;
    }
    return Merge(incoming_type, reg_types, verifier);
  }

  virtual ~RegType() {}

  void VisitRoots(RootVisitor* visitor, const RootInfo& root_info) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void* operator new(size_t size) noexcept {
    return ::operator new(size);
  }

  static void* operator new(size_t size, ArenaAllocator* allocator) = delete;
  static void* operator new(size_t size, ScopedArenaAllocator* allocator);

  enum class AssignmentType {
    kBoolean,
    kByte,
    kShort,
    kChar,
    kInteger,
    kFloat,
    kLongLo,
    kDoubleLo,
    kConflict,
    kReference,
    kNotAssignable,
  };

  ALWAYS_INLINE
  inline AssignmentType GetAssignmentType() const {
    AssignmentType t = GetAssignmentTypeImpl();
    if (kIsDebugBuild) {
      if (IsBoolean()) {
        CHECK(AssignmentType::kBoolean == t);
      } else if (IsByte()) {
        CHECK(AssignmentType::kByte == t);
      } else if (IsShort()) {
        CHECK(AssignmentType::kShort == t);
      } else if (IsChar()) {
        CHECK(AssignmentType::kChar == t);
      } else if (IsInteger()) {
        CHECK(AssignmentType::kInteger == t);
      } else if (IsFloat()) {
        CHECK(AssignmentType::kFloat == t);
      } else if (IsLongLo()) {
        CHECK(AssignmentType::kLongLo == t);
      } else if (IsDoubleLo()) {
        CHECK(AssignmentType::kDoubleLo == t);
      } else if (IsConflict()) {
        CHECK(AssignmentType::kConflict == t);
      } else if (IsReferenceTypes()) {
        CHECK(AssignmentType::kReference == t);
      } else {
        LOG(FATAL) << "Unreachable";
        UNREACHABLE();
      }
    }
    return t;
  }

 protected:
  RegType(ObjPtr<mirror::Class> klass,
          const std::string_view& descriptor,
          uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : descriptor_(descriptor),
        klass_(klass),
        cache_id_(cache_id) {}

  template <typename Class>
  void CheckConstructorInvariants(Class* this_ ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(std::is_final<Class>::value, "Class must be final.");
    if (kIsDebugBuild) {
      CheckInvariants();
    }
  }

  virtual AssignmentType GetAssignmentTypeImpl() const = 0;

  const std::string_view descriptor_;
  mutable GcRoot<mirror::Class> klass_;  // Non-const only due to moving classes.
  const uint16_t cache_id_;

  friend class RegTypeCache;

 private:
  virtual void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_);


  static bool AssignableFrom(const RegType& lhs,
                             const RegType& rhs,
                             bool strict,
                             MethodVerifier* verifier)
      REQUIRES_SHARED(Locks::mutator_lock_);

  DISALLOW_COPY_AND_ASSIGN(RegType);
};

// Bottom type.
class ConflictType final : public RegType {
 public:
  bool IsConflict() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the singleton Conflict instance.
  static const ConflictType* GetInstance() PURE;

  // Create the singleton instance.
  static const ConflictType* CreateInstance(ObjPtr<mirror::Class> klass,
                                            const std::string_view& descriptor,
                                            uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kConflict;
  }

 private:
  ConflictType(ObjPtr<mirror::Class> klass,
               const std::string_view& descriptor,
               uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  static const ConflictType* instance_;
};

// A variant of the bottom type used to specify an undefined value in the
// incoming registers.
// Merging with UndefinedType yields ConflictType which is the true bottom.
class UndefinedType final : public RegType {
 public:
  bool IsUndefined() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the singleton Undefined instance.
  static const UndefinedType* GetInstance() PURE;

  // Create the singleton instance.
  static const UndefinedType* CreateInstance(ObjPtr<mirror::Class> klass,
                                             const std::string_view& descriptor,
                                             uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Destroy the singleton instance.
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }

 private:
  UndefinedType(ObjPtr<mirror::Class> klass,
                const std::string_view& descriptor,
                uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  static const UndefinedType* instance_;
};

class PrimitiveType : public RegType {
 public:
  PrimitiveType(ObjPtr<mirror::Class> klass,
                const std::string_view& descriptor,
                uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_);

  bool HasClassVirtual() const override { return true; }
};

class Cat1Type : public PrimitiveType {
 public:
  Cat1Type(ObjPtr<mirror::Class> klass,
           const std::string_view& descriptor,
           uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_);
};

class IntegerType final : public Cat1Type {
 public:
  bool IsInteger() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const IntegerType* CreateInstance(ObjPtr<mirror::Class> klass,
                                           const std::string_view& descriptor,
                                           uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const IntegerType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kInteger;
  }

 private:
  IntegerType(ObjPtr<mirror::Class> klass,
              const std::string_view& descriptor,
              uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const IntegerType* instance_;
};

class BooleanType final : public Cat1Type {
 public:
  bool IsBoolean() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const BooleanType* CreateInstance(ObjPtr<mirror::Class> klass,
                                           const std::string_view& descriptor,
                                           uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const BooleanType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kBoolean;
  }

 private:
  BooleanType(ObjPtr<mirror::Class> klass,
              const std::string_view& descriptor,
              uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  static const BooleanType* instance_;
};

class ByteType final : public Cat1Type {
 public:
  bool IsByte() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const ByteType* CreateInstance(ObjPtr<mirror::Class> klass,
                                        const std::string_view& descriptor,
                                        uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const ByteType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kByte;
  }

 private:
  ByteType(ObjPtr<mirror::Class> klass,
           const std::string_view& descriptor,
           uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const ByteType* instance_;
};

class ShortType final : public Cat1Type {
 public:
  bool IsShort() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const ShortType* CreateInstance(ObjPtr<mirror::Class> klass,
                                         const std::string_view& descriptor,
                                         uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const ShortType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kShort;
  }

 private:
  ShortType(ObjPtr<mirror::Class> klass, const std::string_view& descriptor,
            uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const ShortType* instance_;
};

class CharType final : public Cat1Type {
 public:
  bool IsChar() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const CharType* CreateInstance(ObjPtr<mirror::Class> klass,
                                        const std::string_view& descriptor,
                                        uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const CharType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kChar;
  }

 private:
  CharType(ObjPtr<mirror::Class> klass,
           const std::string_view& descriptor,
           uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const CharType* instance_;
};

class FloatType final : public Cat1Type {
 public:
  bool IsFloat() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  static const FloatType* CreateInstance(ObjPtr<mirror::Class> klass,
                                         const std::string_view& descriptor,
                                         uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const FloatType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kFloat;
  }

 private:
  FloatType(ObjPtr<mirror::Class> klass,
            const std::string_view& descriptor,
            uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat1Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const FloatType* instance_;
};

class Cat2Type : public PrimitiveType {
 public:
  Cat2Type(ObjPtr<mirror::Class> klass,
           const std::string_view& descriptor,
           uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_);
};

class LongLoType final : public Cat2Type {
 public:
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsLongLo() const override { return true; }
  bool IsLong() const override { return true; }
  static const LongLoType* CreateInstance(ObjPtr<mirror::Class> klass,
                                          const std::string_view& descriptor,
                                          uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const LongLoType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kLongLo;
  }

 private:
  LongLoType(ObjPtr<mirror::Class> klass,
             const std::string_view& descriptor,
             uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const LongLoType* instance_;
};

class LongHiType final : public Cat2Type {
 public:
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsLongHi() const override { return true; }
  static const LongHiType* CreateInstance(ObjPtr<mirror::Class> klass,
                                          const std::string_view& descriptor,
                                          uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const LongHiType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }

 private:
  LongHiType(ObjPtr<mirror::Class> klass,
             const std::string_view& descriptor,
             uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const LongHiType* instance_;
};

class DoubleLoType final : public Cat2Type {
 public:
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsDoubleLo() const override { return true; }
  bool IsDouble() const override { return true; }
  static const DoubleLoType* CreateInstance(ObjPtr<mirror::Class> klass,
                                            const std::string_view& descriptor,
                                            uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const DoubleLoType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kDoubleLo;
  }

 private:
  DoubleLoType(ObjPtr<mirror::Class> klass,
               const std::string_view& descriptor,
               uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const DoubleLoType* instance_;
};

class DoubleHiType final : public Cat2Type {
 public:
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsDoubleHi() const override { return true; }
  static const DoubleHiType* CreateInstance(ObjPtr<mirror::Class> klass,
                                            const std::string_view& descriptor,
                                            uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static const DoubleHiType* GetInstance() PURE;
  static void Destroy();

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }

 private:
  DoubleHiType(ObjPtr<mirror::Class> klass,
               const std::string_view& descriptor,
               uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : Cat2Type(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }
  static const DoubleHiType* instance_;
};

class ConstantType : public RegType {
 public:
  ConstantType(uint32_t constant, uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(nullptr, "", cache_id), constant_(constant) {
  }


  // If this is a 32-bit constant, what is the value? This value may be
  // imprecise in which case
  // the value represents part of the integer range of values that may be held
  // in the register.
  int32_t ConstantValue() const {
    DCHECK(IsConstantTypes());
    return constant_;
  }

  int32_t ConstantValueLo() const {
    DCHECK(IsConstantLo());
    return constant_;
  }

  int32_t ConstantValueHi() const {
    if (IsConstantHi() || IsPreciseConstantHi() || IsImpreciseConstantHi()) {
      return constant_;
    } else {
      DCHECK(false);
      return 0;
    }
  }

  bool IsZero() const override {
    return IsPreciseConstant() && ConstantValue() == 0;
  }
  bool IsOne() const override {
    return IsPreciseConstant() && ConstantValue() == 1;
  }

  bool IsConstantChar() const override {
    return IsConstant() && ConstantValue() >= 0 &&
           ConstantValue() <= std::numeric_limits<uint16_t>::max();
  }
  bool IsConstantByte() const override {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<int8_t>::min() &&
           ConstantValue() <= std::numeric_limits<int8_t>::max();
  }
  bool IsConstantShort() const override {
    return IsConstant() &&
           ConstantValue() >= std::numeric_limits<int16_t>::min() &&
           ConstantValue() <= std::numeric_limits<int16_t>::max();
  }
  bool IsConstantTypes() const override { return true; }

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }

 private:
  const uint32_t constant_;
};

class PreciseConstType final : public ConstantType {
 public:
  PreciseConstType(uint32_t constant, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsPreciseConstant() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

class PreciseConstLoType final : public ConstantType {
 public:
  PreciseConstLoType(uint32_t constant, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {
    CheckConstructorInvariants(this);
  }
  bool IsPreciseConstantLo() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

class PreciseConstHiType final : public ConstantType {
 public:
  PreciseConstHiType(uint32_t constant, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {
    CheckConstructorInvariants(this);
  }
  bool IsPreciseConstantHi() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

class ImpreciseConstType final : public ConstantType {
 public:
  ImpreciseConstType(uint32_t constat, uint16_t cache_id)
       REQUIRES_SHARED(Locks::mutator_lock_)
       : ConstantType(constat, cache_id) {
    CheckConstructorInvariants(this);
  }
  bool IsImpreciseConstant() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

class ImpreciseConstLoType final : public ConstantType {
 public:
  ImpreciseConstLoType(uint32_t constant, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {
    CheckConstructorInvariants(this);
  }
  bool IsImpreciseConstantLo() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

class ImpreciseConstHiType final : public ConstantType {
 public:
  ImpreciseConstHiType(uint32_t constant, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : ConstantType(constant, cache_id) {
    CheckConstructorInvariants(this);
  }
  bool IsImpreciseConstantHi() const override { return true; }
  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kNotAssignable;
  }
};

// Special "null" type that captures the semantics of null / bottom.
class NullType final : public RegType {
 public:
  bool IsNull() const override {
    return true;
  }

  // Get the singleton Null instance.
  static const NullType* GetInstance() PURE;

  // Create the singleton instance.
  static const NullType* CreateInstance(ObjPtr<mirror::Class> klass,
                                        const std::string_view& descriptor,
                                        uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void Destroy();

  std::string Dump() const override {
    return "null";
  }

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kReference;
  }

  bool IsConstantTypes() const override {
    return true;
  }

 private:
  NullType(ObjPtr<mirror::Class> klass, const std::string_view& descriptor, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  static const NullType* instance_;
};

// Common parent of all uninitialized types. Uninitialized types are created by
// "new" dex
// instructions and must be passed to a constructor.
class UninitializedType : public RegType {
 public:
  UninitializedType(ObjPtr<mirror::Class> klass,
                    const std::string_view& descriptor,
                    uint32_t allocation_pc,
                    uint16_t cache_id)
      : RegType(klass, descriptor, cache_id), allocation_pc_(allocation_pc) {}

  bool IsUninitializedTypes() const override;
  bool IsNonZeroReferenceTypes() const override;

  uint32_t GetAllocationPc() const {
    DCHECK(IsUninitializedTypes());
    return allocation_pc_;
  }

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kReference;
  }

 private:
  const uint32_t allocation_pc_;
};

// Similar to ReferenceType but not yet having been passed to a constructor.
class UninitializedReferenceType final : public UninitializedType {
 public:
  UninitializedReferenceType(ObjPtr<mirror::Class> klass,
                             const std::string_view& descriptor,
                             uint32_t allocation_pc,
                             uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, allocation_pc, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsUninitializedReference() const override { return true; }

  bool HasClassVirtual() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);
};

// Similar to UnresolvedReferenceType but not yet having been passed to a
// constructor.
class UnresolvedUninitializedRefType final : public UninitializedType {
 public:
  UnresolvedUninitializedRefType(const std::string_view& descriptor,
                                 uint32_t allocation_pc,
                                 uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UninitializedType(nullptr, descriptor, allocation_pc, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsUnresolvedAndUninitializedReference() const override { return true; }

  bool IsUnresolvedTypes() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;
};

// Similar to UninitializedReferenceType but special case for the this argument
// of a constructor.
class UninitializedThisReferenceType final : public UninitializedType {
 public:
  UninitializedThisReferenceType(ObjPtr<mirror::Class> klass,
                                 const std::string_view& descriptor,
                                 uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UninitializedType(klass, descriptor, 0, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsUninitializedThisReference() const override { return true; }

  bool HasClassVirtual() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;
};

class UnresolvedUninitializedThisRefType final : public UninitializedType {
 public:
  UnresolvedUninitializedThisRefType(const std::string_view& descriptor, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UninitializedType(nullptr, descriptor, 0, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsUnresolvedAndUninitializedThisReference() const override { return true; }

  bool IsUnresolvedTypes() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;
};

// A type of register holding a reference to an Object of type GetClass or a
// sub-class.
class ReferenceType final : public RegType {
 public:
  ReferenceType(ObjPtr<mirror::Class> klass,
                const std::string_view& descriptor,
                uint16_t cache_id) REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(klass, descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsReference() const override { return true; }

  bool IsNonZeroReferenceTypes() const override { return true; }

  bool HasClassVirtual() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kReference;
  }
};

// A type of register holding a reference to an Object of type GetClass and only
// an object of that
// type.
class PreciseReferenceType final : public RegType {
 public:
  PreciseReferenceType(ObjPtr<mirror::Class> klass,
                       const std::string_view& descriptor,
                       uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsPreciseReference() const override { return true; }

  bool IsNonZeroReferenceTypes() const override { return true; }

  bool HasClassVirtual() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kReference;
  }
};

// Common parent of unresolved types.
class UnresolvedType : public RegType {
 public:
  UnresolvedType(const std::string_view& descriptor, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : RegType(nullptr, descriptor, cache_id) {}

  bool IsNonZeroReferenceTypes() const override;

  AssignmentType GetAssignmentTypeImpl() const override {
    return AssignmentType::kReference;
  }
};

// Similar to ReferenceType except the Class couldn't be loaded. Assignability
// and other tests made
// of this type must be conservative.
class UnresolvedReferenceType final : public UnresolvedType {
 public:
  UnresolvedReferenceType(const std::string_view& descriptor, uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UnresolvedType(descriptor, cache_id) {
    CheckConstructorInvariants(this);
  }

  bool IsUnresolvedReference() const override { return true; }

  bool IsUnresolvedTypes() const override { return true; }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;
};

// Type representing the super-class of an unresolved type.
class UnresolvedSuperClass final : public UnresolvedType {
 public:
  UnresolvedSuperClass(uint16_t child_id, RegTypeCache* reg_type_cache,
                       uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : UnresolvedType("", cache_id),
        unresolved_child_id_(child_id),
        reg_type_cache_(reg_type_cache) {
    CheckConstructorInvariants(this);
  }

  bool IsUnresolvedSuperClass() const override { return true; }

  bool IsUnresolvedTypes() const override { return true; }

  uint16_t GetUnresolvedSuperClassChildId() const {
    DCHECK(IsUnresolvedSuperClass());
    return static_cast<uint16_t>(unresolved_child_id_ & 0xFFFF);
  }

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;

  const uint16_t unresolved_child_id_;
  const RegTypeCache* const reg_type_cache_;
};

// A merge of unresolved (and resolved) types. If the types were resolved this may be
// Conflict or another known ReferenceType.
class UnresolvedMergedType final : public UnresolvedType {
 public:
  // Note: the constructor will copy the unresolved BitVector, not use it directly.
  UnresolvedMergedType(const RegType& resolved,
                       const BitVector& unresolved,
                       const RegTypeCache* reg_type_cache,
                       uint16_t cache_id)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The resolved part. See description below.
  const RegType& GetResolvedPart() const {
    return resolved_part_;
  }
  // The unresolved part.
  const BitVector& GetUnresolvedTypes() const {
    return unresolved_types_;
  }

  bool IsUnresolvedMergedReference() const override { return true; }

  bool IsUnresolvedTypes() const override { return true; }

  bool IsArrayTypes() const override REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsObjectArrayTypes() const override REQUIRES_SHARED(Locks::mutator_lock_);

  std::string Dump() const override REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void CheckInvariants() const REQUIRES_SHARED(Locks::mutator_lock_) override;

  const RegTypeCache* const reg_type_cache_;

  // The original implementation of merged types was a binary tree. Collection of the flattened
  // types ("leaves") can be expensive, so we store the expanded list now, as two components:
  // 1) A resolved component. We use Zero when there is no resolved component, as that will be
  //    an identity merge.
  // 2) A bitvector of the unresolved reference types. A bitvector was chosen with the assumption
  //    that there should not be too many types in flight in practice. (We also bias the index
  //    against the index of Zero, which is one of the later default entries in any cache.)
  const RegType& resolved_part_;
  const BitVector unresolved_types_;
};

std::ostream& operator<<(std::ostream& os, const RegType& rhs)
    REQUIRES_SHARED(Locks::mutator_lock_);

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_H_
