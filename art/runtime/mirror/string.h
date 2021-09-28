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

#ifndef ART_RUNTIME_MIRROR_STRING_H_
#define ART_RUNTIME_MIRROR_STRING_H_

#include "base/bit_utils.h"
#include "class.h"
#include "object.h"
#include "runtime_globals.h"

namespace art {

namespace gc {
enum AllocatorType : char;
}  // namespace gc

template<class T> class Handle;
template<class MirrorType> class ObjPtr;
class StringBuilderAppend;
struct StringOffsets;
class StubTest_ReadBarrierForRoot_Test;

namespace mirror {

// String Compression
static constexpr bool kUseStringCompression = true;
enum class StringCompressionFlag : uint32_t {
    kCompressed = 0u,
    kUncompressed = 1u
};

// C++ mirror of java.lang.String
class MANAGED String final : public Object {
 public:
  // Size of java.lang.String.class.
  static uint32_t ClassSize(PointerSize pointer_size);

  // Size of an instance of java.lang.String not including its value array.
  static constexpr uint32_t InstanceSize() {
    return sizeof(String);
  }

  static constexpr MemberOffset CountOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, count_);
  }

  static constexpr MemberOffset ValueOffset() {
    return OFFSET_OF_OBJECT_MEMBER(String, value_);
  }

  uint16_t* GetValue() REQUIRES_SHARED(Locks::mutator_lock_) {
    return &value_[0];
  }

  uint8_t* GetValueCompressed() REQUIRES_SHARED(Locks::mutator_lock_) {
    return &value_compressed_[0];
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  size_t SizeOf() REQUIRES_SHARED(Locks::mutator_lock_) {
    size_t size = sizeof(String);
    if (IsCompressed()) {
      size += (sizeof(uint8_t) * GetLength<kVerifyFlags>());
    } else {
      size += (sizeof(uint16_t) * GetLength<kVerifyFlags>());
    }
    // String.equals() intrinsics assume zero-padding up to kObjectAlignment,
    // so make sure the zero-padding is actually copied around if GC compaction
    // chooses to copy only SizeOf() bytes.
    // http://b/23528461
    return RoundUp(size, kObjectAlignment);
  }

  // Taking out the first/uppermost bit because it is not part of actual length value
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  int32_t GetLength() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetLengthFromCount(GetCount<kVerifyFlags>());
  }

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  int32_t GetCount() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetField32<kVerifyFlags>(OFFSET_OF_OBJECT_MEMBER(String, count_));
  }

  void SetCount(int32_t new_count) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Count is invariant so use non-transactional mode. Also disable check as we may run inside
    // a transaction.
    SetField32<false, false>(OFFSET_OF_OBJECT_MEMBER(String, count_), new_count);
  }

  int32_t GetHashCode() REQUIRES_SHARED(Locks::mutator_lock_);

  // Computes, stores, and returns the hash code.
  int32_t ComputeHashCode() REQUIRES_SHARED(Locks::mutator_lock_);

  int32_t GetUtfLength() REQUIRES_SHARED(Locks::mutator_lock_);

  uint16_t CharAt(int32_t index) REQUIRES_SHARED(Locks::mutator_lock_);

  // Create a new string where all occurences of `old_c` are replaced with `new_c`.
  // String.doReplace(char, char) is called from String.replace(char, char) when there is a match.
  static ObjPtr<String> DoReplace(Thread* self, Handle<String> src, uint16_t old_c, uint16_t new_c)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ObjPtr<String> Intern() REQUIRES_SHARED(Locks::mutator_lock_);

  template <bool kIsInstrumented = true>
  ALWAYS_INLINE static ObjPtr<String> AllocFromByteArray(Thread* self,
                                                         int32_t byte_length,
                                                         Handle<ByteArray> array,
                                                         int32_t offset,
                                                         int32_t high_byte,
                                                         gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template <bool kIsInstrumented = true>
  ALWAYS_INLINE static ObjPtr<String> AllocFromCharArray(Thread* self,
                                                         int32_t count,
                                                         Handle<CharArray> array,
                                                         int32_t offset,
                                                         gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template <bool kIsInstrumented = true>
  ALWAYS_INLINE static ObjPtr<String> AllocFromString(Thread* self,
                                                      int32_t string_length,
                                                      Handle<String> string,
                                                      int32_t offset,
                                                      gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  template <bool kIsInstrumented = true>
  ALWAYS_INLINE static ObjPtr<String> AllocEmptyString(Thread* self,
                                                       gc::AllocatorType allocator_type)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<String> AllocFromStrings(Thread* self,
                                         Handle<String> string,
                                         Handle<String> string2)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<String> AllocFromUtf16(Thread* self,
                                       int32_t utf16_length,
                                       const uint16_t* utf16_data_in)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<String> AllocFromModifiedUtf8(Thread* self, const char* utf)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<String> AllocFromModifiedUtf8(Thread* self,
                                              int32_t utf16_length,
                                              const char* utf8_data_in,
                                              int32_t utf8_length)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static ObjPtr<String> AllocFromModifiedUtf8(Thread* self,
                                              int32_t utf16_length,
                                              const char* utf8_data_in)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  bool Equals(const char* modified_utf8) REQUIRES_SHARED(Locks::mutator_lock_);

  bool Equals(ObjPtr<String> that) REQUIRES_SHARED(Locks::mutator_lock_);

  // Create a modified UTF-8 encoded std::string from a java/lang/String object.
  std::string ToModifiedUtf8() REQUIRES_SHARED(Locks::mutator_lock_);

  int32_t FastIndexOf(int32_t ch, int32_t start) REQUIRES_SHARED(Locks::mutator_lock_);

  template <typename MemoryType>
  int32_t FastIndexOf(MemoryType* chars, int32_t ch, int32_t start)
      REQUIRES_SHARED(Locks::mutator_lock_);

  int32_t CompareTo(ObjPtr<String> other) REQUIRES_SHARED(Locks::mutator_lock_);

  static ObjPtr<CharArray> ToCharArray(Handle<String> h_this, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  void GetChars(int32_t start, int32_t end, Handle<CharArray> array, int32_t index)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags>
  bool IsCompressed() REQUIRES_SHARED(Locks::mutator_lock_) {
    return kUseStringCompression && IsCompressed(GetCount());
  }

  bool IsValueNull() REQUIRES_SHARED(Locks::mutator_lock_);

  template<typename MemoryType>
  static bool AllASCII(const MemoryType* chars, const int length);

  static bool DexFileStringAllASCII(const char* chars, const int length);

  ALWAYS_INLINE static bool IsCompressed(int32_t count) {
    return GetCompressionFlagFromCount(count) == StringCompressionFlag::kCompressed;
  }

  ALWAYS_INLINE static StringCompressionFlag GetCompressionFlagFromCount(int32_t count) {
    return kUseStringCompression
        ? static_cast<StringCompressionFlag>(static_cast<uint32_t>(count) & 1u)
        : StringCompressionFlag::kUncompressed;
  }

  ALWAYS_INLINE static int32_t GetLengthFromCount(int32_t count) {
    return kUseStringCompression ? static_cast<int32_t>(static_cast<uint32_t>(count) >> 1) : count;
  }

  ALWAYS_INLINE static int32_t GetFlaggedCount(int32_t length, bool compressible) {
    return kUseStringCompression
        ? static_cast<int32_t>((static_cast<uint32_t>(length) << 1) |
                               (static_cast<uint32_t>(compressible
                                                          ? StringCompressionFlag::kCompressed
                                                          : StringCompressionFlag::kUncompressed)))
        : length;
  }

  // Returns a human-readable equivalent of 'descriptor'. So "I" would be "int",
  // "[[I" would be "int[][]", "[Ljava/lang/String;" would be
  // "java.lang.String[]", and so forth.
  static std::string PrettyStringDescriptor(ObjPtr<mirror::String> descriptor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  std::string PrettyStringDescriptor()
      REQUIRES_SHARED(Locks::mutator_lock_);

  static constexpr bool IsASCII(uint16_t c) {
    // Valid ASCII characters are in range 1..0x7f. Zero is not considered ASCII
    // because it would complicate the detection of ASCII strings in Modified-UTF8.
    return (c - 1u) < 0x7fu;
  }

 private:
  static bool AllASCIIExcept(const uint16_t* chars, int32_t length, uint16_t non_ascii);

  void SetHashCode(int32_t new_hash_code) REQUIRES_SHARED(Locks::mutator_lock_) {
    // Hash code is invariant so use non-transactional mode. Also disable check as we may run inside
    // a transaction.
    DCHECK_EQ(0, GetField32(OFFSET_OF_OBJECT_MEMBER(String, hash_code_)));
    SetField32<false, false>(OFFSET_OF_OBJECT_MEMBER(String, hash_code_), new_hash_code);
  }

  template <bool kIsInstrumented = true, typename PreFenceVisitor>
  ALWAYS_INLINE static ObjPtr<String> Alloc(Thread* self,
                                            int32_t utf16_length_with_flag,
                                            gc::AllocatorType allocator_type,
                                            const PreFenceVisitor& pre_fence_visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Field order required by test "ValidateFieldOrderOfJavaCppUnionClasses".

  // If string compression is enabled, count_ holds the StringCompressionFlag in the
  // least significant bit and the length in the remaining bits, length = count_ >> 1.
  int32_t count_;

  uint32_t hash_code_;

  // Compression of all-ASCII into 8-bit memory leads to usage one of these fields
  union {
    uint16_t value_[0];
    uint8_t value_compressed_[0];
  };

  friend class art::StringBuilderAppend;
  friend struct art::StringOffsets;  // for verifying offset information

  DISALLOW_IMPLICIT_CONSTRUCTORS(String);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STRING_H_
