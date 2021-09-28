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

#ifndef ART_RUNTIME_MIRROR_CLASS_H_
#define ART_RUNTIME_MIRROR_CLASS_H_

#include <string_view>

#include "base/bit_utils.h"
#include "base/casts.h"
#include "class_flags.h"
#include "class_status.h"
#include "dex/dex_file_types.h"
#include "dex/modifiers.h"
#include "dex/primitive.h"
#include "object.h"
#include "object_array.h"
#include "read_barrier_option.h"

namespace art {

namespace dex {
struct ClassDef;
class TypeList;
}  // namespace dex

namespace gc {
enum AllocatorType : char;
}  // namespace gc

namespace hiddenapi {
class AccessContext;
}  // namespace hiddenapi

namespace linker {
class ImageWriter;
}  // namespace linker

template<typename T> class ArraySlice;
class ArtField;
class ArtMethod;
struct ClassOffsets;
class DexFile;
template<class T> class Handle;
class ImTable;
enum InvokeType : uint32_t;
template <typename Iter> class IterationRange;
template<typename T> class LengthPrefixedArray;
enum class PointerSize : size_t;
class Signature;
template<typename T> class StrideIterator;
template<size_t kNumReferences> class PACKED(4) StackHandleScope;
class Thread;

namespace mirror {

class ClassExt;
class ClassLoader;
class Constructor;
class DexCache;
class IfTable;
class Method;
template <typename T> struct PACKED(8) DexCachePair;

using StringDexCachePair = DexCachePair<String>;
using StringDexCacheType = std::atomic<StringDexCachePair>;

// C++ mirror of java.lang.Class
class MANAGED Class final : public Object {
 public:
  // A magic value for reference_instance_offsets_. Ignore the bits and walk the super chain when
  // this is the value.
  // [This is an unlikely "natural" value, since it would be 30 non-ref instance fields followed by
  // 2 ref instance fields.]
  static constexpr uint32_t kClassWalkSuper = 0xC0000000;

  // Shift primitive type by kPrimitiveTypeSizeShiftShift to get the component type size shift
  // Used for computing array size as follows:
  // array_bytes = header_size + (elements << (primitive_type >> kPrimitiveTypeSizeShiftShift))
  static constexpr uint32_t kPrimitiveTypeSizeShiftShift = 16;
  static constexpr uint32_t kPrimitiveTypeMask = (1u << kPrimitiveTypeSizeShiftShift) - 1;

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           bool kWithSynchronizationBarrier = true>
  ClassStatus GetStatus() REQUIRES_SHARED(Locks::mutator_lock_) {
    // Reading the field without barrier is used exclusively for IsVisiblyInitialized().
    int32_t field_value = kWithSynchronizationBarrier
        ? GetField32Volatile<kVerifyFlags>(StatusOffset())
        : GetField32<kVerifyFlags>(StatusOffset());
    // Avoid including "subtype_check_bits_and_status.h" to get the field.
    // The ClassStatus is always in the 4 most-significant bits of status_.
    return enum_cast<ClassStatus>(static_cast<uint32_t>(field_value) >> (32 - 4));
  }

  // This is static because 'this' may be moved by GC.
  static void SetStatus(Handle<Class> h_this, ClassStatus new_status, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Used for structural redefinition to directly set the class-status while
  // holding a strong mutator-lock.
  void SetStatusLocked(ClassStatus new_status) REQUIRES(Locks::mutator_lock_);

  void SetStatusForPrimitiveOrArray(ClassStatus new_status) REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset StatusOffset() {
    return MemberOffset(OFFSET_OF_OBJECT_MEMBER(Class, status_));
  }

  // Returns true if the class has been retired.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsRetired() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == ClassStatus::kRetired;
  }

  // Returns true if the class has failed to link.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsErroneousUnresolved() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == ClassStatus::kErrorUnresolved;
  }

  // Returns true if the class has failed to initialize.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsErroneousResolved() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == ClassStatus::kErrorResolved;
  }

  // Returns true if the class status indicets that the class has failed to link or initialize.
  static bool IsErroneous(ClassStatus status) {
    return status == ClassStatus::kErrorUnresolved || status == ClassStatus::kErrorResolved;
  }

  // Returns true if the class has failed to link or initialize.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsErroneous() REQUIRES_SHARED(Locks::mutator_lock_) {
    return IsErroneous(GetStatus<kVerifyFlags>());
  }

  // Returns true if the class has been loaded.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsIdxLoaded() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kIdx;
  }

  // Returns true if the class has been loaded.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsLoaded() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kLoaded;
  }

  // Returns true if the class has been linked.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsResolved() REQUIRES_SHARED(Locks::mutator_lock_) {
    ClassStatus status = GetStatus<kVerifyFlags>();
    return status >= ClassStatus::kResolved || status == ClassStatus::kErrorResolved;
  }

  // Returns true if the class should be verified at runtime.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool ShouldVerifyAtRuntime() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() == ClassStatus::kRetryVerificationAtRuntime;
  }

  // Returns true if the class has been verified at compile time, but should be
  // executed with access checks.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsVerifiedNeedsAccessChecks() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kVerifiedNeedsAccessChecks;
  }

  // Returns true if the class has been verified.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsVerified() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kVerified;
  }

  // Returns true if the class is initializing.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsInitializing() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kInitializing;
  }

  // Returns true if the class is initialized.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsInitialized() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetStatus<kVerifyFlags>() >= ClassStatus::kInitialized;
  }

  // Returns true if the class is visibly initialized.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsVisiblyInitialized() REQUIRES_SHARED(Locks::mutator_lock_) {
    // Note: Avoiding the synchronization barrier for the visibly initialized check.
    ClassStatus status = GetStatus<kVerifyFlags, /*kWithSynchronizationBarrier=*/ false>();
    return status == ClassStatus::kVisiblyInitialized;
  }

  // Returns true if this class is ever accessed through a C++ mirror.
  bool IsMirrored() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint32_t GetAccessFlags() REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kIsDebugBuild) {
      GetAccessFlagsDCheck<kVerifyFlags>();
    }
    return GetField32<kVerifyFlags>(AccessFlagsOffset());
  }

  static constexpr MemberOffset AccessFlagsOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, access_flags_);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE uint32_t GetClassFlags() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, class_flags_));
  }
  void SetClassFlags(uint32_t new_flags) REQUIRES_SHARED(Locks::mutator_lock_);

  // Set access flags during linking, these cannot be rolled back by a Transaction.
  void SetAccessFlagsDuringLinking(uint32_t new_access_flags) REQUIRES_SHARED(Locks::mutator_lock_);

  // Set access flags, recording the change if running inside a Transaction.
  void SetAccessFlags(uint32_t new_access_flags) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if the class is an enum.
  ALWAYS_INLINE bool IsEnum() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccEnum) != 0;
  }

  // Returns true if the class is an interface.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsInterface() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccInterface) != 0;
  }

  // Returns true if the class is declared public.
  ALWAYS_INLINE bool IsPublic() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccPublic) != 0;
  }

  // Returns true if the class is declared final.
  ALWAYS_INLINE bool IsFinal() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccFinal) != 0;
  }

  ALWAYS_INLINE bool IsFinalizable() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccClassIsFinalizable) != 0;
  }

  ALWAYS_INLINE bool ShouldSkipHiddenApiChecks() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccSkipHiddenapiChecks) != 0;
  }

  ALWAYS_INLINE void SetSkipHiddenApiChecks() REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t flags = GetAccessFlags();
    SetAccessFlags(flags | kAccSkipHiddenapiChecks);
  }

  ALWAYS_INLINE void SetRecursivelyInitialized() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetHasDefaultMethods() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetFinalizable() REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    SetAccessFlagsDuringLinking(flags | kAccClassIsFinalizable);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsStringClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetClassFlags<kVerifyFlags>() & kClassFlagString) != 0;
  }

  ALWAYS_INLINE void SetStringClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    SetClassFlags(kClassFlagString | kClassFlagNoReferenceFields);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsClassLoaderClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetClassFlags<kVerifyFlags>() == kClassFlagClassLoader;
  }

  ALWAYS_INLINE void SetClassLoaderClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    SetClassFlags(kClassFlagClassLoader);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsDexCacheClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetClassFlags<kVerifyFlags>() & kClassFlagDexCache) != 0;
  }

  ALWAYS_INLINE void SetDexCacheClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    SetClassFlags(GetClassFlags() | kClassFlagDexCache);
  }

  // Returns true if the class is abstract.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsAbstract() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags<kVerifyFlags>() & kAccAbstract) != 0;
  }

  // Returns true if the class is an annotation.
  ALWAYS_INLINE bool IsAnnotation() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccAnnotation) != 0;
  }

  // Returns true if the class is synthetic.
  ALWAYS_INLINE bool IsSynthetic() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccSynthetic) != 0;
  }

  // Return whether the class had run the verifier at least once.
  // This does not necessarily mean that access checks are avoidable,
  // since the class methods might still need to be run with access checks.
  bool WasVerificationAttempted() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccVerificationAttempted) != 0;
  }

  // Mark the class as having gone through a verification attempt.
  // Mutually exclusive from whether or not each method is allowed to skip access checks.
  void SetVerificationAttempted() REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    if ((flags & kAccVerificationAttempted) == 0) {
      SetAccessFlags(flags | kAccVerificationAttempted);
    }
  }

  bool IsObsoleteObject() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccObsoleteObject) != 0;
  }

  void SetObsoleteObject() REQUIRES_SHARED(Locks::mutator_lock_) {
    uint32_t flags = GetField32(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    if ((flags & kAccObsoleteObject) == 0) {
      SetAccessFlags(flags | kAccObsoleteObject);
    }
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsTypeOfReferenceClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetClassFlags<kVerifyFlags>() & kClassFlagReference) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsWeakReferenceClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetClassFlags<kVerifyFlags>() == kClassFlagWeakReference;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsSoftReferenceClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetClassFlags<kVerifyFlags>() == kClassFlagSoftReference;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsFinalizerReferenceClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetClassFlags<kVerifyFlags>() == kClassFlagFinalizerReference;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPhantomReferenceClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetClassFlags<kVerifyFlags>() == kClassFlagPhantomReference;
  }

  // Can references of this type be assigned to by things of another type? For non-array types
  // this is a matter of whether sub-classes may exist - which they can't if the type is final.
  // For array classes, where all the classes are final due to there being no sub-classes, an
  // Object[] may be assigned to by a String[] but a String[] may not be assigned to by other
  // types as the component is final.
  bool CannotBeAssignedFromOtherTypes() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if this class is the placeholder and should retire and
  // be replaced with a class with the right size for embedded imt/vtable.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsTemp() REQUIRES_SHARED(Locks::mutator_lock_) {
    ClassStatus s = GetStatus<kVerifyFlags>();
    return s < ClassStatus::kResolving &&
           s != ClassStatus::kErrorResolved &&
           ShouldHaveEmbeddedVTable<kVerifyFlags>();
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<String> GetName() REQUIRES_SHARED(Locks::mutator_lock_);  // Returns the cached name.
  void SetName(ObjPtr<String> name) REQUIRES_SHARED(Locks::mutator_lock_);  // Sets the cached name.
  // Computes the name, then sets the cached value.
  static ObjPtr<String> ComputeName(Handle<Class> h_this) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsProxyClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    // Read access flags without using getter as whether something is a proxy can be check in
    // any loaded state
    // TODO: switch to a check if the super class is java.lang.reflect.Proxy?
    uint32_t access_flags = GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, access_flags_));
    return (access_flags & kAccClassIsProxy) != 0;
  }

  static constexpr MemberOffset PrimitiveTypeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  Primitive::Type GetPrimitiveType() ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  void SetPrimitiveType(Primitive::Type new_type) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(sizeof(Primitive::Type), sizeof(int32_t));
    uint32_t v32 = static_cast<uint32_t>(new_type);
    DCHECK_EQ(v32 & kPrimitiveTypeMask, v32) << "upper 16 bits aren't zero";
    // Store the component size shift in the upper 16 bits.
    v32 |= Primitive::ComponentSizeShift(new_type) << kPrimitiveTypeSizeShiftShift;
    SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
        OFFSET_OF_OBJECT_MEMBER(Class, primitive_type_), v32);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t GetPrimitiveTypeSizeShift() ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if the class is a primitive type.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitive() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() != Primitive::kPrimNot;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveBoolean() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimBoolean;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveByte() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimByte;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveChar() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimChar;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveShort() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimShort;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveInt() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType() == Primitive::kPrimInt;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveLong() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimLong;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveFloat() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimFloat;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveDouble() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimDouble;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveVoid() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPrimitiveType<kVerifyFlags>() == Primitive::kPrimVoid;
  }

  // Depth of class from java.lang.Object
  uint32_t Depth() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsArrayClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsClassClass() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsThrowableClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset ComponentTypeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, component_type_);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<Class> GetComponentType() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetComponentType(ObjPtr<Class> new_component_type) REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetComponentSize() REQUIRES_SHARED(Locks::mutator_lock_);

  size_t GetComponentSizeShift() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsObjectClass() REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsInstantiableNonArray() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsInstantiable() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsObjectArrayClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsPrimitiveArray() REQUIRES_SHARED(Locks::mutator_lock_);

  // Enum used to control whether we try to add a finalizer-reference for object alloc or not.
  enum class AddFinalizer {
    // Don't create a finalizer reference regardless of what the class-flags say.
    kNoAddFinalizer,
    // Use the class-flags to figure out if we should make a finalizer reference.
    kUseClassTag,
  };

  // Creates a raw object instance but does not invoke the default constructor.
  // kCheckAddFinalizer controls whether we use a DCHECK to sanity check that we create a
  // finalizer-reference if needed. This should only be disabled when doing structural class
  // redefinition.
  template <bool kIsInstrumented = true,
            AddFinalizer kAddFinalizer = AddFinalizer::kUseClassTag,
            bool kCheckAddFinalizer = true>
  ALWAYS_INLINE ObjPtr<Object> Alloc(Thread* self, gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  ObjPtr<Object> AllocObject(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);
  ObjPtr<Object> AllocNonMovableObject(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool IsVariableSize() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t SizeOf() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, class_size_));
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetClassSize() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, class_size_));
  }

  void SetClassSize(uint32_t new_class_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Compute how many bytes would be used a class with the given elements.
  static uint32_t ComputeClassSize(bool has_embedded_vtable,
                                   uint32_t num_vtable_entries,
                                   uint32_t num_8bit_static_fields,
                                   uint32_t num_16bit_static_fields,
                                   uint32_t num_32bit_static_fields,
                                   uint32_t num_64bit_static_fields,
                                   uint32_t num_ref_static_fields,
                                   PointerSize pointer_size);

  // The size of java.lang.Class.class.
  static uint32_t ClassClassSize(PointerSize pointer_size) {
    // The number of vtable entries in java.lang.Class.
    uint32_t vtable_entries = Object::kVTableLength + 67;
    return ComputeClassSize(true, vtable_entries, 0, 0, 4, 1, 0, pointer_size);
  }

  // The size of a java.lang.Class representing a primitive such as int.class.
  static uint32_t PrimitiveClassSize(PointerSize pointer_size) {
    return ComputeClassSize(false, 0, 0, 0, 0, 0, 0, pointer_size);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetObjectSize() REQUIRES_SHARED(Locks::mutator_lock_);
  static constexpr MemberOffset ObjectSizeOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, object_size_);
  }
  static constexpr MemberOffset ObjectSizeAllocFastPathOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, object_size_alloc_fast_path_);
  }

  ALWAYS_INLINE void SetObjectSize(uint32_t new_object_size) REQUIRES_SHARED(Locks::mutator_lock_);

  void SetObjectSizeAllocFastPath(uint32_t new_object_size) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetObjectSizeAllocFastPath() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetObjectSizeWithoutChecks(uint32_t new_object_size)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Not called within a transaction.
    return SetField32<false, false, kVerifyNone>(
        OFFSET_OF_OBJECT_MEMBER(Class, object_size_), new_object_size);
  }

  // Returns true if this class is in the same packages as that class.
  bool IsInSamePackage(ObjPtr<Class> that) REQUIRES_SHARED(Locks::mutator_lock_);

  static bool IsInSamePackage(std::string_view descriptor1, std::string_view descriptor2);

  // Returns true if this class can access that class.
  bool CanAccess(ObjPtr<Class> that) REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this class access a member in the provided class with the provided member access flags?
  // Note that access to the class isn't checked in case the declaring class is protected and the
  // method has been exposed by a public sub-class
  bool CanAccessMember(ObjPtr<Class> access_to, uint32_t member_flags)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this class access a resolved field?
  // Note that access to field's class is checked and this may require looking up the class
  // referenced by the FieldId in the DexFile in case the declaring class is inaccessible.
  bool CanAccessResolvedField(ObjPtr<Class> access_to,
                              ArtField* field,
                              ObjPtr<DexCache> dex_cache,
                              uint32_t field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool CheckResolvedFieldAccess(ObjPtr<Class> access_to,
                                ArtField* field,
                                ObjPtr<DexCache> dex_cache,
                                uint32_t field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Can this class access a resolved method?
  // Note that access to methods's class is checked and this may require looking up the class
  // referenced by the MethodId in the DexFile in case the declaring class is inaccessible.
  bool CanAccessResolvedMethod(ObjPtr<Class> access_to,
                               ArtMethod* resolved_method,
                               ObjPtr<DexCache> dex_cache,
                               uint32_t method_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool CheckResolvedMethodAccess(ObjPtr<Class> access_to,
                                 ArtMethod* resolved_method,
                                 ObjPtr<DexCache> dex_cache,
                                 uint32_t method_idx,
                                 InvokeType throw_invoke_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool IsSubClass(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  // Can src be assigned to this class? For example, String can be assigned to Object (by an
  // upcast), however, an Object cannot be assigned to a String as a potentially exception throwing
  // downcast would be necessary. Similarly for interfaces, a class that implements (or an interface
  // that extends) another can be assigned to its parent, but not vice-versa. All Classes may assign
  // to themselves. Classes for primitive types may not assign to each other.
  ALWAYS_INLINE bool IsAssignableFrom(ObjPtr<Class> src) REQUIRES_SHARED(Locks::mutator_lock_);

  // Checks if 'klass' is a redefined version of this.
  bool IsObsoleteVersionOf(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<Class> GetObsoleteClass() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE ObjPtr<Class> GetSuperClass() REQUIRES_SHARED(Locks::mutator_lock_);

  // Get first common super class. It will never return null.
  // `This` and `klass` must be classes.
  ObjPtr<Class> GetCommonSuperClass(Handle<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  void SetSuperClass(ObjPtr<Class> new_super_class) REQUIRES_SHARED(Locks::mutator_lock_);

  bool HasSuperClass() REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset SuperClassOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, super_class_));
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<ClassLoader> GetClassLoader() ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  void SetClassLoader(ObjPtr<ClassLoader> new_cl) REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset DexCacheOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, dex_cache_));
  }

  static constexpr MemberOffset IfTableOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, iftable_));
  }

  enum {
    kDumpClassFullDetail = 1,
    kDumpClassClassLoader = (1 << 1),
    kDumpClassInitialized = (1 << 2),
  };

  void DumpClass(std::ostream& os, int flags) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<DexCache> GetDexCache() REQUIRES_SHARED(Locks::mutator_lock_);

  // Also updates the dex_cache_strings_ variable from new_dex_cache.
  void SetDexCache(ObjPtr<DexCache> new_dex_cache) REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDirectMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE LengthPrefixedArray<ArtMethod>* GetMethodsPtr()
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset MethodsOffset() {
    return MemberOffset(OFFSETOF_MEMBER(Class, methods_));
  }

  ALWAYS_INLINE ArraySlice<ArtMethod> GetMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetMethodsPtr(LengthPrefixedArray<ArtMethod>* new_methods,
                     uint32_t num_direct,
                     uint32_t num_virtual)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Used by image writer.
  void SetMethodsPtrUnchecked(LengthPrefixedArray<ArtMethod>* new_methods,
                              uint32_t num_direct,
                              uint32_t num_virtual)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetDirectMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArtMethod* GetDirectMethod(size_t i, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Use only when we are allocating populating the method arrays.
  ALWAYS_INLINE ArtMethod* GetDirectMethodUnchecked(size_t i, PointerSize pointer_size)
        REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE ArtMethod* GetVirtualMethodUnchecked(size_t i, PointerSize pointer_size)
        REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of static, private, and constructor methods.
  ALWAYS_INLINE uint32_t NumDirectMethods() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredMethods(
        PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <PointerSize kPointerSize, bool kTransactionActive>
  static ObjPtr<Method> GetDeclaredMethodInternal(
      Thread* self,
      ObjPtr<Class> klass,
      ObjPtr<String> name,
      ObjPtr<ObjectArray<Class>> args,
      const std::function<hiddenapi::AccessContext()>& fn_get_access_context)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <PointerSize kPointerSize, bool kTransactionActive>
  static ObjPtr<Constructor> GetDeclaredConstructorInternal(Thread* self,
                                                            ObjPtr<Class> klass,
                                                            ObjPtr<ObjectArray<Class>> args)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredVirtualMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredVirtualMethods(
        PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetCopiedMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetCopiedMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE ArraySlice<ArtMethod> GetVirtualMethodsSlice(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetVirtualMethods(
      PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of non-inherited virtual methods (sum of declared and copied methods).
  ALWAYS_INLINE uint32_t NumVirtualMethods() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of copied virtual methods.
  ALWAYS_INLINE uint32_t NumCopiedVirtualMethods() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of declared virtual methods.
  ALWAYS_INLINE uint32_t NumDeclaredVirtualMethods() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE uint32_t NumMethods() REQUIRES_SHARED(Locks::mutator_lock_);
  static ALWAYS_INLINE uint32_t NumMethods(LengthPrefixedArray<ArtMethod>* methods)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ArtMethod* GetVirtualMethod(size_t i, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* GetVirtualMethodDuringLinking(size_t i, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE ObjPtr<PointerArray> GetVTable() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ObjPtr<PointerArray> GetVTableDuringLinking() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetVTable(ObjPtr<PointerArray> new_vtable) REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr MemberOffset VTableOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Class, vtable_);
  }

  static constexpr MemberOffset EmbeddedVTableLengthOffset() {
    return MemberOffset(sizeof(Class));
  }

  static constexpr MemberOffset ImtPtrOffset(PointerSize pointer_size) {
    return MemberOffset(
        RoundUp(EmbeddedVTableLengthOffset().Uint32Value() + sizeof(uint32_t),
                static_cast<size_t>(pointer_size)));
  }

  static constexpr MemberOffset EmbeddedVTableOffset(PointerSize pointer_size) {
    return MemberOffset(
        ImtPtrOffset(pointer_size).Uint32Value() + static_cast<size_t>(pointer_size));
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool ShouldHaveImt() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool ShouldHaveEmbeddedVTable() REQUIRES_SHARED(Locks::mutator_lock_);

  bool HasVTable() REQUIRES_SHARED(Locks::mutator_lock_);

  static MemberOffset EmbeddedVTableEntryOffset(uint32_t i, PointerSize pointer_size);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  int32_t GetVTableLength() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ArtMethod* GetVTableEntry(uint32_t i, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  int32_t GetEmbeddedVTableLength() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetEmbeddedVTableLength(int32_t len) REQUIRES_SHARED(Locks::mutator_lock_);

  ImTable* GetImt(PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);

  void SetImt(ImTable* imt, PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* GetEmbeddedVTableEntry(uint32_t i, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetEmbeddedVTableEntry(uint32_t i, ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  inline void SetEmbeddedVTableEntryUnchecked(uint32_t i,
                                              ArtMethod* method,
                                              PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void PopulateEmbeddedVTable(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given a method implemented by this class but potentially from a super class, return the
  // specific implementation method for this class.
  ArtMethod* FindVirtualMethodForVirtual(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given a method implemented by this class' super class, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForSuper(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given a method from some implementor of this interface, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForInterfaceSuper(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Given a method implemented by this class, but potentially from a
  // super class or interface, return the specific implementation
  // method for this class.
  ArtMethod* FindVirtualMethodForInterface(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE;

  ArtMethod* FindVirtualMethodForVirtualOrInterface(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Find a method with the given name and signature in an interface class.
  //
  // Search for the method declared in the class, then search for a method declared in any
  // superinterface, then search the superclass java.lang.Object (implicitly declared methods
  // in an interface without superinterfaces, see JLS 9.2, can be inherited, see JLS 9.4.1).
  // TODO: Implement search for a unique maximally-specific non-abstract superinterface method.

  ArtMethod* FindInterfaceMethod(std::string_view name,
                                 std::string_view signature,
                                 PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(std::string_view name,
                                 const Signature& signature,
                                 PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindInterfaceMethod(ObjPtr<DexCache> dex_cache,
                                 uint32_t dex_method_idx,
                                 PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Find a method with the given name and signature in a non-interface class.
  //
  // Search for the method in the class, following the JLS rules which conflict with the RI
  // in some cases. The JLS says that inherited methods are searched (JLS 15.12.2.1) and
  // these can come from a superclass or a superinterface (JLS 8.4.8). We perform the
  // following search:
  //   1. Search the methods declared directly in the class. If we find a method with the
  //      given name and signature, return that method.
  //   2. Search the methods declared in superclasses until we find a method with the given
  //      signature or complete the search in java.lang.Object. If we find a method with the
  //      given name and signature, check if it's been inherited by the class where we're
  //      performing the lookup (qualifying type). If it's inherited, return it. Otherwise,
  //      just remember the method and its declaring class and proceed to step 3.
  //   3. Search "copied" methods (containing methods inherited from interfaces) in the class
  //      and its superclass chain. If we found a method in step 2 (which was not inherited,
  //      otherwise we would not be performing step 3), end the search when we reach its
  //      declaring class, otherwise search the entire superclass chain. If we find a method
  //      with the given name and signature, return that method.
  //   4. Return the method found in step 2 if any (not inherited), or null.
  //
  // It's the responsibility of the caller to throw exceptions if the returned method (or null)
  // does not satisfy the request. Special consideration should be given to the case where this
  // function returns a method that's not inherited (found in step 2, returned in step 4).

  ArtMethod* FindClassMethod(std::string_view name,
                             std::string_view signature,
                             PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindClassMethod(std::string_view name,
                             const Signature& signature,
                             PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindClassMethod(ObjPtr<DexCache> dex_cache,
                             uint32_t dex_method_idx,
                             PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindConstructor(std::string_view signature, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredVirtualMethodByName(std::string_view name, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindDeclaredDirectMethodByName(std::string_view name, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtMethod* FindClassInitializer(PointerSize pointer_size) REQUIRES_SHARED(Locks::mutator_lock_);

  bool HasDefaultMethods() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccHasDefaultMethod) != 0;
  }

  bool HasBeenRecursivelyInitialized() REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessFlags() & kAccRecursivelyInitialized) != 0;
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int32_t GetIfTableCount() REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE ObjPtr<IfTable> GetIfTable() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetIfTable(ObjPtr<IfTable> new_iftable)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get instance fields of the class (See also GetSFields).
  LengthPrefixedArray<ArtField>* GetIFieldsPtr() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE IterationRange<StrideIterator<ArtField>> GetIFields()
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetIFieldsPtr(LengthPrefixedArray<ArtField>* new_ifields)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Unchecked edition has no verification flags.
  void SetIFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_sfields)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint32_t NumInstanceFields() REQUIRES_SHARED(Locks::mutator_lock_);
  ArtField* GetInstanceField(uint32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of instance fields containing reference types. Does not count fields in any
  // super classes.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t NumReferenceInstanceFields() REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsResolved<kVerifyFlags>());
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_));
  }

  uint32_t NumReferenceInstanceFieldsDuringLinking() REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsLoaded() || IsErroneous());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_));
  }

  void SetNumReferenceInstanceFields(uint32_t new_num) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_instance_fields_), new_num);
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t GetReferenceInstanceOffsets() ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  void SetReferenceInstanceOffsets(uint32_t new_reference_offsets)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the offset of the first reference instance field. Other reference instance fields follow.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  MemberOffset GetFirstReferenceInstanceFieldOffset()
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the number of static fields containing reference types.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  uint32_t NumReferenceStaticFields() REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsResolved<kVerifyFlags>());
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_));
  }

  uint32_t NumReferenceStaticFieldsDuringLinking() REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsLoaded() || IsErroneous() || IsRetired());
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_));
  }

  void SetNumReferenceStaticFields(uint32_t new_num) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Not called within a transaction.
    SetField32<false>(OFFSET_OF_OBJECT_MEMBER(Class, num_reference_static_fields_), new_num);
  }

  // Get the offset of the first reference static field. Other reference static fields follow.
  template <VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  MemberOffset GetFirstReferenceStaticFieldOffset(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the offset of the first reference static field. Other reference static fields follow.
  MemberOffset GetFirstReferenceStaticFieldOffsetDuringLinking(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Gets the static fields of the class.
  LengthPrefixedArray<ArtField>* GetSFieldsPtr() REQUIRES_SHARED(Locks::mutator_lock_);
  ALWAYS_INLINE IterationRange<StrideIterator<ArtField>> GetSFields()
      REQUIRES_SHARED(Locks::mutator_lock_);

  void SetSFieldsPtr(LengthPrefixedArray<ArtField>* new_sfields)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Unchecked edition has no verification flags.
  void SetSFieldsPtrUnchecked(LengthPrefixedArray<ArtField>* new_sfields)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint32_t NumStaticFields() REQUIRES_SHARED(Locks::mutator_lock_);

  // TODO: uint16_t
  ArtField* GetStaticField(uint32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  // Find a static or instance field using the JLS resolution order
  static ArtField* FindField(Thread* self,
                             ObjPtr<Class> klass,
                             std::string_view name,
                             std::string_view type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass.
  ArtField* FindInstanceField(std::string_view name, std::string_view type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Finds the given instance field in this class or a superclass, only searches classes that
  // have the same dex cache.
  ArtField* FindInstanceField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(std::string_view name, std::string_view type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* FindDeclaredInstanceField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Finds the given static field in this class or a superclass.
  static ArtField* FindStaticField(Thread* self,
                                   ObjPtr<Class> klass,
                                   std::string_view name,
                                   std::string_view type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Finds the given static field in this class or superclass, only searches classes that
  // have the same dex cache.
  static ArtField* FindStaticField(Thread* self,
                                   ObjPtr<Class> klass,
                                   ObjPtr<DexCache> dex_cache,
                                   uint32_t dex_field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(std::string_view name, std::string_view type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ArtField* FindDeclaredStaticField(ObjPtr<DexCache> dex_cache, uint32_t dex_field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  pid_t GetClinitThreadId() REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(IsIdxLoaded() || IsErroneous()) << PrettyClass();
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, clinit_thread_id_));
  }

  void SetClinitThreadId(pid_t new_clinit_thread_id) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<ClassExt> GetExtData() REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the ExtData for this class, allocating one if necessary. This should be the only way
  // to force ext_data_ to be set. No functions are available for changing an already set ext_data_
  // since doing so is not allowed.
  static ObjPtr<ClassExt> EnsureExtDataPresent(Handle<Class> h_this, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  uint16_t GetDexClassDefIndex() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_));
  }

  void SetDexClassDefIndex(uint16_t class_def_idx) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
        OFFSET_OF_OBJECT_MEMBER(Class, dex_class_def_idx_), class_def_idx);
  }

  dex::TypeIndex GetDexTypeIndex() REQUIRES_SHARED(Locks::mutator_lock_) {
    return dex::TypeIndex(
        static_cast<uint16_t>(GetField32(OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_))));
  }

  void SetDexTypeIndex(dex::TypeIndex type_idx) REQUIRES_SHARED(Locks::mutator_lock_) {
    SetField32</*kTransactionActive=*/ false, /*kCheckTransaction=*/ false>(
        OFFSET_OF_OBJECT_MEMBER(Class, dex_type_idx_), type_idx.index_);
  }

  dex::TypeIndex FindTypeIndexInOtherDexFile(const DexFile& dex_file)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit native roots visits roots which are keyed off the native pointers such as ArtFields and
  // ArtMethods.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier, class Visitor>
  void VisitNativeRoots(Visitor& visitor, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit ArtMethods directly owned by this class.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier, class Visitor>
  void VisitMethods(Visitor visitor, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Visit ArtFields directly owned by this class.
  template<ReadBarrierOption kReadBarrierOption = kWithReadBarrier, class Visitor>
  void VisitFields(Visitor visitor) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get one of the primitive classes.
  static ObjPtr<mirror::Class> GetPrimitiveClass(ObjPtr<mirror::String> name)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Clear the kAccMustCountLocks flag on each method, for class redefinition.
  void ClearMustCountLocksFlagOnAllMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Clear the kAccCompileDontBother flag on each method, for class redefinition.
  void ClearDontCompileFlagOnAllMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Clear the kAccSkipAccessChecks flag on each method, for class redefinition.
  void ClearSkipAccessChecksFlagOnAllMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // When class is verified, set the kAccSkipAccessChecks flag on each method.
  void SetSkipAccessChecksFlagOnAllMethods(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the descriptor of the class. In a few cases a std::string is required, rather than
  // always create one the storage argument is populated and its internal c_str() returned. We do
  // this to avoid memory allocation in the common case.
  const char* GetDescriptor(std::string* storage) REQUIRES_SHARED(Locks::mutator_lock_);

  bool DescriptorEquals(const char* match) REQUIRES_SHARED(Locks::mutator_lock_);

  const dex::ClassDef* GetClassDef() REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE uint32_t NumDirectInterfaces() REQUIRES_SHARED(Locks::mutator_lock_);

  dex::TypeIndex GetDirectInterfaceTypeIdx(uint32_t idx) REQUIRES_SHARED(Locks::mutator_lock_);

  // Get the direct interface of the `klass` at index `idx` if resolved, otherwise return null.
  // If the caller expects the interface to be resolved, for example for a resolved `klass`,
  // that assumption should be checked by `DCHECK(result != nullptr)`.
  static ObjPtr<Class> GetDirectInterface(Thread* self, ObjPtr<Class> klass, uint32_t idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Resolve and get the direct interface of the `klass` at index `idx`.
  // Returns null with a pending exception if the resolution fails.
  static ObjPtr<Class> ResolveDirectInterface(Thread* self, Handle<Class> klass, uint32_t idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const char* GetSourceFile() REQUIRES_SHARED(Locks::mutator_lock_);

  std::string GetLocation() REQUIRES_SHARED(Locks::mutator_lock_);

  const DexFile& GetDexFile() REQUIRES_SHARED(Locks::mutator_lock_);

  const dex::TypeList* GetInterfaceTypeList() REQUIRES_SHARED(Locks::mutator_lock_);

  // Asserts we are initialized or initializing in the given thread.
  void AssertInitializedOrInitializingInThread(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static ObjPtr<Class> CopyOf(Handle<Class> h_this,
                              Thread* self,
                              int32_t new_length,
                              ImTable* imt,
                              PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // For proxy class only.
  ObjPtr<ObjectArray<Class>> GetProxyInterfaces() REQUIRES_SHARED(Locks::mutator_lock_);

  // For proxy class only.
  ObjPtr<ObjectArray<ObjectArray<Class>>> GetProxyThrows() REQUIRES_SHARED(Locks::mutator_lock_);

  // May cause thread suspension due to EqualParameters.
  ArtMethod* GetDeclaredConstructor(Thread* self,
                                    Handle<ObjectArray<Class>> args,
                                    PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static int32_t GetInnerClassFlags(Handle<Class> h_this, int32_t default_value)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Used to initialize a class in the allocation code path to ensure it is guarded by a StoreStore
  // fence.
  class InitializeClassVisitor {
   public:
    explicit InitializeClassVisitor(uint32_t class_size) : class_size_(class_size) {
    }

    void operator()(ObjPtr<Object> obj, size_t usable_size) const
        REQUIRES_SHARED(Locks::mutator_lock_);

   private:
    const uint32_t class_size_;

    DISALLOW_COPY_AND_ASSIGN(InitializeClassVisitor);
  };

  // Returns true if the class loader is null, ie the class loader is the boot strap class loader.
  bool IsBootStrapClassLoaded() REQUIRES_SHARED(Locks::mutator_lock_);

  static size_t ImTableEntrySize(PointerSize pointer_size) {
    return static_cast<size_t>(pointer_size);
  }

  static size_t VTableEntrySize(PointerSize pointer_size) {
    return static_cast<size_t>(pointer_size);
  }

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDirectMethodsSliceUnchecked(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetVirtualMethodsSliceUnchecked(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredMethodsSliceUnchecked(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetDeclaredVirtualMethodsSliceUnchecked(
      PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE ArraySlice<ArtMethod> GetCopiedMethodsSliceUnchecked(PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static std::string PrettyDescriptor(ObjPtr<mirror::Class> klass)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyDescriptor()
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns a human-readable form of the name of the given class.
  // Given String.class, the output would be "java.lang.Class<java.lang.String>".
  static std::string PrettyClass(ObjPtr<mirror::Class> c)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyClass()
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Returns a human-readable form of the name of the given class with its class loader.
  static std::string PrettyClassAndClassLoader(ObjPtr<mirror::Class> c)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyClassAndClassLoader()
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Fix up all of the native pointers in the class by running them through the visitor. Only sets
  // the corresponding entry in dest if visitor(obj) != obj to prevent dirty memory. Dest should be
  // initialized to a copy of *this to prevent issues. Does not visit the ArtMethod and ArtField
  // roots.
  template <VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags, typename Visitor>
  void FixupNativePointers(Class* dest, PointerSize pointer_size, const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Get or create the various jni id arrays in a lock-less thread safe manner.
  static bool EnsureMethodIds(Handle<Class> h_this)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<Object> GetMethodIds() REQUIRES_SHARED(Locks::mutator_lock_);
  static bool EnsureStaticFieldIds(Handle<Class> h_this)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<Object> GetStaticFieldIds() REQUIRES_SHARED(Locks::mutator_lock_);
  static bool EnsureInstanceFieldIds(Handle<Class> h_this)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<Object> GetInstanceFieldIds() REQUIRES_SHARED(Locks::mutator_lock_);

  // Calculate the index in the ifields_, methods_ or sfields_ arrays a method is located at. This
  // is to be used with the above Get{,OrCreate}...Ids functions.
  size_t GetStaticFieldIdOffset(ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_);
  size_t GetInstanceFieldIdOffset(ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_);
  size_t GetMethodIdOffset(ArtMethod* method, PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  template <typename T, VerifyObjectFlags kVerifyFlags, typename Visitor>
  void FixupNativePointer(
      Class* dest, PointerSize pointer_size, const Visitor& visitor, MemberOffset member_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE static ArraySlice<ArtMethod> GetMethodsSliceRangeUnchecked(
      LengthPrefixedArray<ArtMethod>* methods,
      PointerSize pointer_size,
      uint32_t start_offset,
      uint32_t end_offset)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool throw_on_failure>
  bool ResolvedFieldAccessTest(ObjPtr<Class> access_to,
                               ArtField* field,
                               ObjPtr<DexCache> dex_cache,
                               uint32_t field_idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool throw_on_failure>
  bool ResolvedMethodAccessTest(ObjPtr<Class> access_to,
                                ArtMethod* resolved_method,
                                ObjPtr<DexCache> dex_cache,
                                uint32_t method_idx,
                                InvokeType throw_invoke_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  bool Implements(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsArrayAssignableFromArray(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsAssignableFromArray(ObjPtr<Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

  void CheckObjectAlloc() REQUIRES_SHARED(Locks::mutator_lock_);

  // Unchecked editions is for root visiting.
  LengthPrefixedArray<ArtField>* GetSFieldsPtrUnchecked() REQUIRES_SHARED(Locks::mutator_lock_);
  IterationRange<StrideIterator<ArtField>> GetSFieldsUnchecked()
      REQUIRES_SHARED(Locks::mutator_lock_);
  LengthPrefixedArray<ArtField>* GetIFieldsPtrUnchecked() REQUIRES_SHARED(Locks::mutator_lock_);
  IterationRange<StrideIterator<ArtField>> GetIFieldsUnchecked()
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The index in the methods_ array where the first declared virtual method is.
  ALWAYS_INLINE uint32_t GetVirtualMethodsStartOffset() REQUIRES_SHARED(Locks::mutator_lock_);

  // The index in the methods_ array where the first direct method is.
  ALWAYS_INLINE uint32_t GetDirectMethodsStartOffset() REQUIRES_SHARED(Locks::mutator_lock_);

  // The index in the methods_ array where the first copied method is.
  ALWAYS_INLINE uint32_t GetCopiedMethodsStartOffset() REQUIRES_SHARED(Locks::mutator_lock_);

  bool ProxyDescriptorEquals(const char* match) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags>
  void GetAccessFlagsDCheck() REQUIRES_SHARED(Locks::mutator_lock_);

  void SetAccessFlagsDCheck(uint32_t new_access_flags) REQUIRES_SHARED(Locks::mutator_lock_);

  // Check that the pointer size matches the one in the class linker.
  ALWAYS_INLINE static void CheckPointerSize(PointerSize pointer_size);

  template <bool kVisitNativeRoots,
            VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
            ReadBarrierOption kReadBarrierOption = kWithReadBarrier,
            typename Visitor>
  void VisitReferences(ObjPtr<Class> klass, const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Helper to set the status without any validity cheks.
  void SetStatusInternal(ClassStatus new_status)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // 'Class' Object Fields
  // Order governed by java field ordering. See art::ClassLinker::LinkFields.

  // Defining class loader, or null for the "bootstrap" system loader.
  HeapReference<ClassLoader> class_loader_;

  // For array classes, the component class object for instanceof/checkcast
  // (for String[][][], this will be String[][]). null for non-array classes.
  HeapReference<Class> component_type_;

  // DexCache of resolved constant pool entries (will be null for classes generated by the
  // runtime such as arrays and primitive classes).
  HeapReference<DexCache> dex_cache_;

  // Extraneous class data that is not always needed. This field is allocated lazily and may
  // only be set with 'this' locked. This is synchronized on 'this'.
  // TODO(allight) We should probably synchronize it on something external or handle allocation in
  // some other (safe) way to prevent possible deadlocks.
  HeapReference<ClassExt> ext_data_;

  // The interface table (iftable_) contains pairs of a interface class and an array of the
  // interface methods. There is one pair per interface supported by this class.  That means one
  // pair for each interface we support directly, indirectly via superclass, or indirectly via a
  // superinterface.  This will be null if neither we nor our superclass implement any interfaces.
  //
  // Why we need this: given "class Foo implements Face", declare "Face faceObj = new Foo()".
  // Invoke faceObj.blah(), where "blah" is part of the Face interface.  We can't easily use a
  // single vtable.
  //
  // For every interface a concrete class implements, we create an array of the concrete vtable_
  // methods for the methods in the interface.
  HeapReference<IfTable> iftable_;

  // Descriptor for the class such as "java.lang.Class" or "[C". Lazily initialized by ComputeName
  HeapReference<String> name_;

  // The superclass, or null if this is java.lang.Object or a primitive type.
  //
  // Note that interfaces have java.lang.Object as their
  // superclass. This doesn't match the expectations in JNI
  // GetSuperClass or java.lang.Class.getSuperClass() which need to
  // check for interfaces and return null.
  HeapReference<Class> super_class_;

  // Virtual method table (vtable), for use by "invoke-virtual".  The vtable from the superclass is
  // copied in, and virtual methods from our class either replace those from the super or are
  // appended. For abstract classes, methods may be created in the vtable that aren't in
  // virtual_ methods_ for miranda methods.
  HeapReference<PointerArray> vtable_;

  // instance fields
  //
  // These describe the layout of the contents of an Object.
  // Note that only the fields directly declared by this class are
  // listed in ifields; fields declared by a superclass are listed in
  // the superclass's Class.ifields.
  //
  // ArtFields are allocated as a length prefixed ArtField array, and not an array of pointers to
  // ArtFields.
  uint64_t ifields_;

  // Pointer to an ArtMethod length-prefixed array. All the methods where this class is the place
  // where they are logically defined. This includes all private, static, final and virtual methods
  // as well as inherited default methods and miranda methods.
  //
  // The slice methods_ [0, virtual_methods_offset_) are the direct (static, private, init) methods
  // declared by this class.
  //
  // The slice methods_ [virtual_methods_offset_, copied_methods_offset_) are the virtual methods
  // declared by this class.
  //
  // The slice methods_ [copied_methods_offset_, |methods_|) are the methods that are copied from
  // interfaces such as miranda or default methods. These are copied for resolution purposes as this
  // class is where they are (logically) declared as far as the virtual dispatch is concerned.
  //
  // Note that this field is used by the native debugger as the unique identifier for the type.
  uint64_t methods_;

  // Static fields length-prefixed array.
  uint64_t sfields_;

  // Access flags; low 16 bits are defined by VM spec.
  uint32_t access_flags_;

  // Class flags to help speed up visiting object references.
  uint32_t class_flags_;

  // Total size of the Class instance; used when allocating storage on gc heap.
  // See also object_size_.
  uint32_t class_size_;

  // Tid used to check for recursive <clinit> invocation.
  pid_t clinit_thread_id_;
  static_assert(sizeof(pid_t) == sizeof(int32_t), "java.lang.Class.clinitThreadId size check");

  // ClassDef index in dex file, -1 if no class definition such as an array.
  // TODO: really 16bits
  int32_t dex_class_def_idx_;

  // Type index in dex file.
  // TODO: really 16bits
  int32_t dex_type_idx_;

  // Number of instance fields that are object refs.
  uint32_t num_reference_instance_fields_;

  // Number of static fields that are object refs,
  uint32_t num_reference_static_fields_;

  // Total object size; used when allocating storage on gc heap.
  // (For interfaces and abstract classes this will be zero.)
  // See also class_size_.
  uint32_t object_size_;

  // Aligned object size for allocation fast path. The value is max uint32_t if the object is
  // uninitialized or finalizable. Not currently used for variable sized objects.
  uint32_t object_size_alloc_fast_path_;

  // The lower 16 bits contains a Primitive::Type value. The upper 16
  // bits contains the size shift of the primitive type.
  uint32_t primitive_type_;

  // Bitmap of offsets of ifields.
  uint32_t reference_instance_offsets_;

  // See the real definition in subtype_check_bits_and_status.h
  // typeof(status_) is actually SubtypeCheckBitsAndStatus.
  uint32_t status_;

  // The offset of the first virtual method that is copied from an interface. This includes miranda,
  // default, and default-conflict methods. Having a hard limit of ((2 << 16) - 1) for methods
  // defined on a single class is well established in Java so we will use only uint16_t's here.
  uint16_t copied_methods_offset_;

  // The offset of the first declared virtual methods in the methods_ array.
  uint16_t virtual_methods_offset_;

  // TODO: ?
  // initiating class loader list
  // NOTE: for classes with low serialNumber, these are unused, and the
  // values are kept in a table in gDvm.
  // InitiatingLoaderList initiating_loader_list_;

  // The following data exist in real class objects.
  // Embedded Imtable, for class object that's not an interface, fixed size.
  // ImTableEntry embedded_imtable_[0];
  // Embedded Vtable, for class object that's not an interface, variable size.
  // VTableEntry embedded_vtable_[0];
  // Static fields, variable size.
  // uint32_t fields_[0];

  ART_FRIEND_TEST(DexCacheTest, TestResolvedFieldAccess);  // For ResolvedFieldAccessTest
  friend struct art::ClassOffsets;  // for verifying offset information
  friend class Object;  // For VisitReferences
  friend class linker::ImageWriter;  // For SetStatusInternal
  DISALLOW_IMPLICIT_CONSTRUCTORS(Class);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_H_
