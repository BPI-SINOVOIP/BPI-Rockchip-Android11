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

#ifndef ART_LIBARTBASE_BASE_MEMORY_REGION_H_
#define ART_LIBARTBASE_BASE_MEMORY_REGION_H_

#include <stdint.h>
#include <type_traits>

#include <android-base/logging.h>

#include "bit_utils.h"
#include "casts.h"
#include "enums.h"
#include "globals.h"
#include "macros.h"
#include "value_object.h"

namespace art {

// Memory regions are useful for accessing memory with bounds check in
// debug mode. They can be safely passed by value and do not assume ownership
// of the region.
class MemoryRegion final : public ValueObject {
 public:
  struct ContentEquals {
    constexpr bool operator()(const MemoryRegion& lhs, const MemoryRegion& rhs) const {
      return lhs.size() == rhs.size() && memcmp(lhs.begin(), rhs.begin(), lhs.size()) == 0;
    }
  };

  MemoryRegion() : pointer_(nullptr), size_(0) {}
  MemoryRegion(void* pointer_in, uintptr_t size_in) : pointer_(pointer_in), size_(size_in) {}

  void* pointer() const { return pointer_; }
  size_t size() const { return size_; }
  size_t size_in_bits() const { return size_ * kBitsPerByte; }

  static size_t pointer_offset() {
    return OFFSETOF_MEMBER(MemoryRegion, pointer_);
  }

  uint8_t* begin() const { return reinterpret_cast<uint8_t*>(pointer_); }
  uint8_t* end() const { return begin() + size_; }

  // Load value of type `T` at `offset`.  The memory address corresponding
  // to `offset` should be word-aligned (on ARM, this is a requirement).
  template<typename T>
  ALWAYS_INLINE T Load(uintptr_t offset) const {
    T* address = ComputeInternalPointer<T>(offset);
    DCHECK(IsWordAligned(address));
    return *address;
  }

  // Store `value` (of type `T`) at `offset`.  The memory address
  // corresponding to `offset` should be word-aligned (on ARM, this is
  // a requirement).
  template<typename T>
  ALWAYS_INLINE void Store(uintptr_t offset, T value) const {
    T* address = ComputeInternalPointer<T>(offset);
    DCHECK(IsWordAligned(address));
    *address = value;
  }

  // Load value of type `T` at `offset`.  The memory address corresponding
  // to `offset` does not need to be word-aligned.
  template<typename T>
  ALWAYS_INLINE T LoadUnaligned(uintptr_t offset) const {
    // Equivalent unsigned integer type corresponding to T.
    typedef typename std::make_unsigned<T>::type U;
    U equivalent_unsigned_integer_value = 0;
    // Read the value byte by byte in a little-endian fashion.
    for (size_t i = 0; i < sizeof(U); ++i) {
      equivalent_unsigned_integer_value +=
          *ComputeInternalPointer<uint8_t>(offset + i) << (i * kBitsPerByte);
    }
    return bit_cast<T, U>(equivalent_unsigned_integer_value);
  }

  // Store `value` (of type `T`) at `offset`.  The memory address
  // corresponding to `offset` does not need to be word-aligned.
  template<typename T>
  ALWAYS_INLINE void StoreUnaligned(uintptr_t offset, T value) const {
    // Equivalent unsigned integer type corresponding to T.
    typedef typename std::make_unsigned<T>::type U;
    U equivalent_unsigned_integer_value = bit_cast<U, T>(value);
    // Write the value byte by byte in a little-endian fashion.
    for (size_t i = 0; i < sizeof(U); ++i) {
      *ComputeInternalPointer<uint8_t>(offset + i) =
          (equivalent_unsigned_integer_value >> (i * kBitsPerByte)) & 0xFF;
    }
  }

  template<typename T>
  ALWAYS_INLINE T* PointerTo(uintptr_t offset) const {
    return ComputeInternalPointer<T>(offset);
  }

  void CopyFrom(size_t offset, const MemoryRegion& from) const;

  template<class Vector>
  void CopyFromVector(size_t offset, Vector& vector) const {
    if (!vector.empty()) {
      CopyFrom(offset, MemoryRegion(vector.data(), vector.size()));
    }
  }

  // Compute a sub memory region based on an existing one.
  ALWAYS_INLINE MemoryRegion Subregion(uintptr_t offset, uintptr_t size_in) const {
    CHECK_GE(this->size(), size_in);
    CHECK_LE(offset,  this->size() - size_in);
    return MemoryRegion(reinterpret_cast<void*>(begin() + offset), size_in);
  }

  // Compute an extended memory region based on an existing one.
  ALWAYS_INLINE void Extend(const MemoryRegion& region, uintptr_t extra) {
    pointer_ = region.pointer();
    size_ = (region.size() + extra);
  }

 private:
  template<typename T>
  ALWAYS_INLINE T* ComputeInternalPointer(size_t offset) const {
    CHECK_GE(size(), sizeof(T));
    CHECK_LE(offset, size() - sizeof(T));
    return reinterpret_cast<T*>(begin() + offset);
  }

  // Locate the bit with the given offset. Returns a pointer to the byte
  // containing the bit, and sets bit_mask to the bit within that byte.
  ALWAYS_INLINE uint8_t* ComputeBitPointer(uintptr_t bit_offset, uint8_t* bit_mask) const {
    uintptr_t bit_remainder = (bit_offset & (kBitsPerByte - 1));
    *bit_mask = (1U << bit_remainder);
    uintptr_t byte_offset = (bit_offset >> kBitsPerByteLog2);
    return ComputeInternalPointer<uint8_t>(byte_offset);
  }

  // Is `address` aligned on a machine word?
  template<typename T> static constexpr bool IsWordAligned(const T* address) {
    // Word alignment in bytes.  Determined from pointer size.
    return IsAligned<kRuntimePointerSize>(address);
  }

  void* pointer_;
  size_t size_;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_MEMORY_REGION_H_
