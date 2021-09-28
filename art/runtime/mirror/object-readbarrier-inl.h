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

#ifndef ART_RUNTIME_MIRROR_OBJECT_READBARRIER_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_READBARRIER_INL_H_

#include "object.h"

#include "base/atomic.h"
#include "heap_poisoning.h"
#include "lock_word-inl.h"
#include "object_reference-inl.h"
#include "read_barrier.h"
#include "runtime.h"

namespace art {
namespace mirror {

template<VerifyObjectFlags kVerifyFlags>
inline LockWord Object::GetLockWord(bool as_volatile) {
  if (as_volatile) {
    return LockWord(GetField32Volatile<kVerifyFlags>(MonitorOffset()));
  }
  return LockWord(GetField32<kVerifyFlags>(MonitorOffset()));
}

template<bool kTransactionActive, bool kCheckTransaction, VerifyObjectFlags kVerifyFlags>
inline bool Object::CasField32(MemberOffset field_offset,
                               int32_t old_value,
                               int32_t new_value,
                               CASMode mode,
                               std::memory_order memory_order) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  if (kVerifyFlags & kVerifyThis) {
    VerifyObject(this);
  }
  uint8_t* raw_addr = reinterpret_cast<uint8_t*>(this) + field_offset.Int32Value();
  AtomicInteger* atomic_addr = reinterpret_cast<AtomicInteger*>(raw_addr);

  return atomic_addr->CompareAndSet(old_value, new_value, mode, memory_order);
}

inline bool Object::CasLockWord(LockWord old_val,
                                LockWord new_val,
                                CASMode mode,
                                std::memory_order memory_order) {
  // Force use of non-transactional mode and do not check.
  return CasField32<false, false>(MonitorOffset(),
                                  old_val.GetValue(),
                                  new_val.GetValue(),
                                  mode,
                                  memory_order);
}

inline uint32_t Object::GetReadBarrierState(uintptr_t* fake_address_dependency) {
  if (!kUseBakerReadBarrier) {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
#if defined(__arm__)
  uintptr_t obj = reinterpret_cast<uintptr_t>(this);
  uintptr_t result;
  DCHECK_EQ(OFFSETOF_MEMBER(Object, monitor_), 4U);
  // Use inline assembly to prevent the compiler from optimizing away the false dependency.
  __asm__ __volatile__(
      "ldr %[result], [%[obj], #4]\n\t"
      // This instruction is enough to "fool the compiler and the CPU" by having `fad` always be
      // null, without them being able to assume that fact.
      "eor %[fad], %[result], %[result]\n\t"
      : [result] "+r" (result), [fad] "=r" (*fake_address_dependency)
      : [obj] "r" (obj));
  DCHECK_EQ(*fake_address_dependency, 0U);
  LockWord lw(static_cast<uint32_t>(result));
  uint32_t rb_state = lw.ReadBarrierState();
  return rb_state;
#elif defined(__aarch64__)
  uintptr_t obj = reinterpret_cast<uintptr_t>(this);
  uintptr_t result;
  DCHECK_EQ(OFFSETOF_MEMBER(Object, monitor_), 4U);
  // Use inline assembly to prevent the compiler from optimizing away the false dependency.
  __asm__ __volatile__(
      "ldr %w[result], [%[obj], #4]\n\t"
      // This instruction is enough to "fool the compiler and the CPU" by having `fad` always be
      // null, without them being able to assume that fact.
      "eor %[fad], %[result], %[result]\n\t"
      : [result] "+r" (result), [fad] "=r" (*fake_address_dependency)
      : [obj] "r" (obj));
  DCHECK_EQ(*fake_address_dependency, 0U);
  LockWord lw(static_cast<uint32_t>(result));
  uint32_t rb_state = lw.ReadBarrierState();
  return rb_state;
#elif defined(__i386__) || defined(__x86_64__)
  LockWord lw = GetLockWord(false);
  // i386/x86_64 don't need fake address dependency. Use a compiler fence to avoid compiler
  // reordering.
  *fake_address_dependency = 0;
  std::atomic_signal_fence(std::memory_order_acquire);
  uint32_t rb_state = lw.ReadBarrierState();
  return rb_state;
#else
  UNUSED(fake_address_dependency);
  LOG(FATAL) << "Unsupported architecture.";
  UNREACHABLE();
#endif
}

inline uint32_t Object::GetReadBarrierState() {
  if (!kUseBakerReadBarrier) {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
  DCHECK(kUseBakerReadBarrier);
  LockWord lw(GetFieldPrimitive<uint32_t, /*kIsVolatile=*/false>(MonitorOffset()));
  uint32_t rb_state = lw.ReadBarrierState();
  DCHECK(ReadBarrier::IsValidReadBarrierState(rb_state)) << rb_state;
  return rb_state;
}

inline uint32_t Object::GetReadBarrierStateAcquire() {
  if (!kUseBakerReadBarrier) {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
  LockWord lw(GetFieldAcquire<uint32_t>(MonitorOffset()));
  uint32_t rb_state = lw.ReadBarrierState();
  DCHECK(ReadBarrier::IsValidReadBarrierState(rb_state)) << rb_state;
  return rb_state;
}

template<std::memory_order kMemoryOrder>
inline bool Object::AtomicSetReadBarrierState(uint32_t expected_rb_state, uint32_t rb_state) {
  if (!kUseBakerReadBarrier) {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
  DCHECK(ReadBarrier::IsValidReadBarrierState(expected_rb_state)) << expected_rb_state;
  DCHECK(ReadBarrier::IsValidReadBarrierState(rb_state)) << rb_state;
  LockWord expected_lw;
  LockWord new_lw;
  do {
    LockWord lw = GetLockWord(false);
    if (UNLIKELY(lw.ReadBarrierState() != expected_rb_state)) {
      // Lost the race.
      return false;
    }
    expected_lw = lw;
    expected_lw.SetReadBarrierState(expected_rb_state);
    new_lw = lw;
    new_lw.SetReadBarrierState(rb_state);
    // ConcurrentCopying::ProcessMarkStackRef uses this with
    // `kMemoryOrder` == `std::memory_order_release`.
    // If `kMemoryOrder` == `std::memory_order_release`, use a CAS release so that when GC updates
    // all the fields of an object and then changes the object from gray to black (non-gray), the
    // field updates (stores) will be visible (won't be reordered after this CAS.)
  } while (!CasLockWord(expected_lw, new_lw, CASMode::kWeak, kMemoryOrder));
  return true;
}

inline bool Object::AtomicSetMarkBit(uint32_t expected_mark_bit, uint32_t mark_bit) {
  LockWord expected_lw;
  LockWord new_lw;
  do {
    LockWord lw = GetLockWord(false);
    if (UNLIKELY(lw.MarkBitState() != expected_mark_bit)) {
      // Lost the race.
      return false;
    }
    expected_lw = lw;
    new_lw = lw;
    new_lw.SetMarkBitState(mark_bit);
    // Since this is only set from the mutator, we can use the non-release CAS.
  } while (!CasLockWord(expected_lw, new_lw, CASMode::kWeak, std::memory_order_relaxed));
  return true;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_READBARRIER_INL_H_
