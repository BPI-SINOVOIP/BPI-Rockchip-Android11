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

#ifndef ART_RUNTIME_MIRROR_ARRAY_H_
#define ART_RUNTIME_MIRROR_ARRAY_H_

#include "base/bit_utils.h"
#include "base/enums.h"
#include "obj_ptr.h"
#include "object.h"

namespace art {

namespace gc {
enum AllocatorType : char;
}  // namespace gc

template<class T> class Handle;
class Thread;

namespace mirror {

class MANAGED Array : public Object {
 public:
  static constexpr size_t kFirstElementOffset = 12u;

  // The size of a java.lang.Class representing an array.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Allocates an array with the given properties, if kFillUsable is true the array will be of at
  // least component_count size, however, if there's usable space at the end of the allocation the
  // array will fill it.
  template <bool kIsInstrumented = true, bool kFillUsable = false>
  ALWAYS_INLINE static ObjPtr<Array> Alloc(Thread* self,
                                           ObjPtr<Class> array_class,
                                           int32_t component_count,
                                           size_t component_size_shift,
                                           gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<Array> CreateMultiArray(Thread* self,
                                        Handle<Class> element_class,
                                        Handle<IntArray> dimensions)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t SizeOf() REQUIRES_SHARED(Locks::mutator_lock_);
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE int32_t GetLength() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(Array, length_));
  }

  void SetLength(int32_t length) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_GE(length, 0);
    // We use non transactional version since we can't undo this write. We also disable checking
    // since it would fail during a transaction.
    SetField32<false, false, kVerifyNone>(OFFSET_OF_OBJECT_MEMBER(Array, length_), length);
  }

  static constexpr MemberOffset LengthOffset() {
    return OFFSET_OF_OBJECT_MEMBER(Array, length_);
  }

  static constexpr MemberOffset DataOffset(size_t component_size) {
    DCHECK(IsPowerOfTwo(component_size)) << component_size;
    size_t data_offset = RoundUp(OFFSETOF_MEMBER(Array, first_element_), component_size);
    DCHECK_EQ(RoundUp(data_offset, component_size), data_offset)
        << "Array data offset isn't aligned with component size";
    return MemberOffset(data_offset);
  }
  template <size_t kComponentSize>
  static constexpr MemberOffset DataOffset() {
    static_assert(IsPowerOfTwo(kComponentSize), "Invalid component size");
    constexpr size_t data_offset = RoundUp(kFirstElementOffset, kComponentSize);
    static_assert(RoundUp(data_offset, kComponentSize) == data_offset, "RoundUp fail");
    return MemberOffset(data_offset);
  }

  static constexpr size_t FirstElementOffset() {
    return OFFSETOF_MEMBER(Array, first_element_);
  }

  void* GetRawData(size_t component_size, int32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value() +
        + (index * component_size);
    return reinterpret_cast<void*>(data);
  }
  template <size_t kComponentSize>
  void* GetRawData(int32_t index) REQUIRES_SHARED(Locks::mutator_lock_) {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset<kComponentSize>().Int32Value() +
        + (index * kComponentSize);
    return reinterpret_cast<void*>(data);
  }

  const void* GetRawData(size_t component_size, int32_t index) const {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset(component_size).Int32Value() +
        + (index * component_size);
    return reinterpret_cast<void*>(data);
  }
  template <size_t kComponentSize>
  const void* GetRawData(int32_t index) const {
    intptr_t data = reinterpret_cast<intptr_t>(this) + DataOffset<kComponentSize>().Int32Value() +
        + (index * kComponentSize);
    return reinterpret_cast<void*>(data);
  }

  // Returns true if the index is valid. If not, throws an ArrayIndexOutOfBoundsException and
  // returns false.
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  ALWAYS_INLINE bool CheckIsValidIndex(int32_t index) REQUIRES_SHARED(Locks::mutator_lock_);

  static ObjPtr<Array> CopyOf(Handle<Array> h_this, Thread* self, int32_t new_length)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

 protected:
  void ThrowArrayStoreException(ObjPtr<Object> object) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

 private:
  void ThrowArrayIndexOutOfBoundsException(int32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // The number of array elements.
  // We only use the field indirectly using the LengthOffset() method.
  int32_t length_ ATTRIBUTE_UNUSED;
  // Marker for the data (used by generated code)
  // We only use the field indirectly using the DataOffset() method.
  uint32_t first_element_[0] ATTRIBUTE_UNUSED;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Array);
};

template<typename T>
class MANAGED PrimitiveArray : public Array {
 public:
  typedef T ElementType;

  static ObjPtr<PrimitiveArray<T>> Alloc(Thread* self, size_t length)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<PrimitiveArray<T>> AllocateAndFill(Thread* self, const T* data, size_t length)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);


  const T* GetData() const ALWAYS_INLINE  REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<const T*>(GetRawData<sizeof(T)>(0));
  }

  T* GetData() ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    return reinterpret_cast<T*>(GetRawData<sizeof(T)>(0));
  }

  T Get(int32_t i) ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  T GetWithoutChecks(int32_t i) ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(CheckIsValidIndex(i)) << "i=" << i << " length=" << GetLength();
    return GetData()[i];
  }

  void Set(int32_t i, T value) ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_);

  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template<bool kTransactionActive, bool kCheckTransaction = true>
  void Set(int32_t i, T value) ALWAYS_INLINE NO_THREAD_SAFETY_ANALYSIS;

  // TODO fix thread safety analysis broken by the use of template. This should be
  // REQUIRES_SHARED(Locks::mutator_lock_).
  template<bool kTransactionActive,
           bool kCheckTransaction = true,
           VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  void SetWithoutChecks(int32_t i, T value) ALWAYS_INLINE NO_THREAD_SAFETY_ANALYSIS;

  /*
   * Works like memmove(), except we guarantee not to allow tearing of array values (ie using
   * smaller than element size copies). Arguments are assumed to be within the bounds of the array
   * and the arrays non-null.
   */
  void Memmove(int32_t dst_pos, ObjPtr<PrimitiveArray<T>> src, int32_t src_pos, int32_t count)
      REQUIRES_SHARED(Locks::mutator_lock_);

  /*
   * Works like memcpy(), except we guarantee not to allow tearing of array values (ie using
   * smaller than element size copies). Arguments are assumed to be within the bounds of the array
   * and the arrays non-null.
   */
  void Memcpy(int32_t dst_pos, ObjPtr<PrimitiveArray<T>> src, int32_t src_pos, int32_t count)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PrimitiveArray);
};

// Declare the different primitive arrays. Instantiations will be in array.cc.
extern template class PrimitiveArray<uint8_t>;   // BooleanArray
extern template class PrimitiveArray<int8_t>;    // ByteArray
extern template class PrimitiveArray<uint16_t>;  // CharArray
extern template class PrimitiveArray<double>;    // DoubleArray
extern template class PrimitiveArray<float>;     // FloatArray
extern template class PrimitiveArray<int32_t>;   // IntArray
extern template class PrimitiveArray<int64_t>;   // LongArray
extern template class PrimitiveArray<int16_t>;   // ShortArray

// Either an IntArray or a LongArray.
class PointerArray : public Array {
 public:
  template<typename T, VerifyObjectFlags kVerifyFlags = kVerifyNone>
  T GetElementPtrSize(uint32_t idx, PointerSize ptr_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<typename T, PointerSize kPtrSize, VerifyObjectFlags kVerifyFlags = kVerifyNone>
  T GetElementPtrSize(uint32_t idx)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Same as GetElementPtrSize, but uses unchecked version of array conversion. It is thus not
  // checked whether kPtrSize matches the underlying array. Only use after at least one invocation
  // of GetElementPtrSize!
  template<typename T, PointerSize kPtrSize, VerifyObjectFlags kVerifyFlags = kVerifyNone>
  T GetElementPtrSizeUnchecked(uint32_t idx)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kVerifyNone>
  void** ElementAddress(size_t index, PointerSize ptr_size) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_LT(index, static_cast<size_t>(GetLength<kVerifyFlags>()));
    return reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(this) +
                                    Array::DataOffset(static_cast<size_t>(ptr_size)).Uint32Value() +
                                    static_cast<size_t>(ptr_size) * index);
  }

  template<bool kTransactionActive = false, bool kUnchecked = false>
  void SetElementPtrSize(uint32_t idx, uint64_t element, PointerSize ptr_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
  template<bool kTransactionActive = false, bool kUnchecked = false, typename T>
  void SetElementPtrSize(uint32_t idx, T* element, PointerSize ptr_size)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Fixup the pointers in the dest arrays by passing our pointers through the visitor. Only copies
  // to dest if visitor(source_ptr) != source_ptr.
  template <VerifyObjectFlags kVerifyFlags = kVerifyNone, typename Visitor>
  void Fixup(ObjPtr<mirror::PointerArray> dest, PointerSize pointer_size, const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Works like memcpy(), except we guarantee not to allow tearing of array values (ie using smaller
  // than element size copies). Arguments are assumed to be within the bounds of the array and the
  // arrays non-null. Cannot be called in an active transaction.
  template<bool kUnchecked = false>
  void Memcpy(int32_t dst_pos,
              ObjPtr<PointerArray> src,
              int32_t src_pos,
              int32_t count,
              PointerSize pointer_size)
      REQUIRES_SHARED(Locks::mutator_lock_);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ARRAY_H_
