/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_
#define ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_

#include "region_space.h"

#include "base/mutex-inl.h"
#include "mirror/object-inl.h"
#include "region_space.h"
#include "thread-current-inl.h"

namespace art {
namespace gc {
namespace space {

inline mirror::Object* RegionSpace::Alloc(Thread* self ATTRIBUTE_UNUSED,
                                          size_t num_bytes,
                                          /* out */ size_t* bytes_allocated,
                                          /* out */ size_t* usable_size,
                                          /* out */ size_t* bytes_tl_bulk_allocated) {
  num_bytes = RoundUp(num_bytes, kAlignment);
  return AllocNonvirtual<false>(num_bytes, bytes_allocated, usable_size,
                                bytes_tl_bulk_allocated);
}

inline mirror::Object* RegionSpace::AllocThreadUnsafe(Thread* self,
                                                      size_t num_bytes,
                                                      /* out */ size_t* bytes_allocated,
                                                      /* out */ size_t* usable_size,
                                                      /* out */ size_t* bytes_tl_bulk_allocated) {
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  return Alloc(self, num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
}

template<bool kForEvac>
inline mirror::Object* RegionSpace::AllocNonvirtual(size_t num_bytes,
                                                    /* out */ size_t* bytes_allocated,
                                                    /* out */ size_t* usable_size,
                                                    /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK_ALIGNED(num_bytes, kAlignment);
  mirror::Object* obj;
  if (LIKELY(num_bytes <= kRegionSize)) {
    // Non-large object.
    obj = (kForEvac ? evac_region_ : current_region_)->Alloc(num_bytes,
                                                             bytes_allocated,
                                                             usable_size,
                                                             bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    MutexLock mu(Thread::Current(), region_lock_);
    // Retry with current region since another thread may have updated
    // current_region_ or evac_region_.  TODO: fix race.
    obj = (kForEvac ? evac_region_ : current_region_)->Alloc(num_bytes,
                                                             bytes_allocated,
                                                             usable_size,
                                                             bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
    Region* r = AllocateRegion(kForEvac);
    if (LIKELY(r != nullptr)) {
      obj = r->Alloc(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
      CHECK(obj != nullptr);
      // Do our allocation before setting the region, this makes sure no threads race ahead
      // and fill in the region before we allocate the object. b/63153464
      if (kForEvac) {
        evac_region_ = r;
      } else {
        current_region_ = r;
      }
      return obj;
    }
  } else {
    // Large object.
    obj = AllocLarge<kForEvac>(num_bytes, bytes_allocated, usable_size, bytes_tl_bulk_allocated);
    if (LIKELY(obj != nullptr)) {
      return obj;
    }
  }
  return nullptr;
}

inline mirror::Object* RegionSpace::Region::Alloc(size_t num_bytes,
                                                  /* out */ size_t* bytes_allocated,
                                                  /* out */ size_t* usable_size,
                                                  /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK(IsAllocated() && IsInToSpace());
  DCHECK_ALIGNED(num_bytes, kAlignment);
  uint8_t* old_top;
  uint8_t* new_top;
  do {
    old_top = top_.load(std::memory_order_relaxed);
    new_top = old_top + num_bytes;
    if (UNLIKELY(new_top > end_)) {
      return nullptr;
    }
  } while (!top_.CompareAndSetWeakRelaxed(old_top, new_top));
  objects_allocated_.fetch_add(1, std::memory_order_relaxed);
  DCHECK_LE(Top(), end_);
  DCHECK_LT(old_top, end_);
  DCHECK_LE(new_top, end_);
  *bytes_allocated = num_bytes;
  if (usable_size != nullptr) {
    *usable_size = num_bytes;
  }
  *bytes_tl_bulk_allocated = num_bytes;
  return reinterpret_cast<mirror::Object*>(old_top);
}

template<RegionSpace::RegionType kRegionType>
inline uint64_t RegionSpace::GetBytesAllocatedInternal() {
  uint64_t bytes = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree()) {
      continue;
    }
    switch (kRegionType) {
      case RegionType::kRegionTypeAll:
        bytes += r->BytesAllocated();
        break;
      case RegionType::kRegionTypeFromSpace:
        if (r->IsInFromSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      case RegionType::kRegionTypeUnevacFromSpace:
        if (r->IsInUnevacFromSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      case RegionType::kRegionTypeToSpace:
        if (r->IsInToSpace()) {
          bytes += r->BytesAllocated();
        }
        break;
      default:
        LOG(FATAL) << "Unexpected space type : " << kRegionType;
    }
  }
  return bytes;
}

template<RegionSpace::RegionType kRegionType>
inline uint64_t RegionSpace::GetObjectsAllocatedInternal() {
  uint64_t bytes = 0;
  MutexLock mu(Thread::Current(), region_lock_);
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree()) {
      continue;
    }
    switch (kRegionType) {
      case RegionType::kRegionTypeAll:
        bytes += r->ObjectsAllocated();
        break;
      case RegionType::kRegionTypeFromSpace:
        if (r->IsInFromSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      case RegionType::kRegionTypeUnevacFromSpace:
        if (r->IsInUnevacFromSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      case RegionType::kRegionTypeToSpace:
        if (r->IsInToSpace()) {
          bytes += r->ObjectsAllocated();
        }
        break;
      default:
        LOG(FATAL) << "Unexpected space type : " << kRegionType;
    }
  }
  return bytes;
}

template <typename Visitor>
inline void RegionSpace::ScanUnevacFromSpace(accounting::ContinuousSpaceBitmap* bitmap,
                                             Visitor&& visitor) {
  const size_t iter_limit = kUseTableLookupReadBarrier
      ? num_regions_ : std::min(num_regions_, non_free_region_index_limit_);
  // Instead of region-wise scan, find contiguous blocks of un-evac regions and then
  // visit them. Everything before visit_block_begin has been processed, while
  // [visit_block_begin, visit_block_end) still needs to be visited.
  uint8_t* visit_block_begin = nullptr;
  uint8_t* visit_block_end = nullptr;
  for (size_t i = 0; i < iter_limit; ++i) {
    Region* r = &regions_[i];
    if (r->IsInUnevacFromSpace()) {
      // visit_block_begin set to nullptr means a new visit block needs to be stated.
      if (visit_block_begin == nullptr) {
        visit_block_begin = r->Begin();
      }
      visit_block_end = r->End();
    } else if (visit_block_begin != nullptr) {
      // Visit the block range as r is not adjacent to current visit block.
      bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(visit_block_begin),
                               reinterpret_cast<uintptr_t>(visit_block_end),
                               visitor);
      visit_block_begin = nullptr;
    }
  }
  // Visit last block, if not processed yet.
  if (visit_block_begin != nullptr) {
    bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(visit_block_begin),
                             reinterpret_cast<uintptr_t>(visit_block_end),
                             visitor);
  }
}

template<bool kToSpaceOnly, typename Visitor>
inline void RegionSpace::WalkInternal(Visitor&& visitor) {
  // TODO: MutexLock on region_lock_ won't work due to lock order
  // issues (the classloader classes lock and the monitor lock). We
  // call this with threads suspended.
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  for (size_t i = 0; i < num_regions_; ++i) {
    Region* r = &regions_[i];
    if (r->IsFree() || (kToSpaceOnly && !r->IsInToSpace())) {
      continue;
    }
    if (r->IsLarge()) {
      // We may visit a large object with live_bytes = 0 here. However, it is
      // safe as it cannot contain dangling pointers because corresponding regions
      // (and regions corresponding to dead referents) cannot be allocated for new
      // allocations without first clearing regions' live_bytes and state.
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(r->Begin());
      DCHECK(obj->GetClass() != nullptr);
      visitor(obj);
    } else if (r->IsLargeTail()) {
      // Do nothing.
    } else {
      WalkNonLargeRegion(visitor, r);
    }
  }
}

template<typename Visitor>
inline void RegionSpace::WalkNonLargeRegion(Visitor&& visitor, const Region* r) {
  DCHECK(!r->IsLarge() && !r->IsLargeTail());
  // For newly allocated and evacuated regions, live bytes will be -1.
  uint8_t* pos = r->Begin();
  uint8_t* top = r->Top();
  // We need the region space bitmap to iterate over a region's objects
  // if
  // - its live bytes count is invalid (i.e. -1); or
  // - its live bytes count is lower than the allocated bytes count.
  //
  // In both of the previous cases, we do not have the guarantee that
  // all allocated objects are "alive" (i.e. valid), so we depend on
  // the region space bitmap to identify which ones to visit.
  //
  // On the other hand, when all allocated bytes are known to be alive,
  // we know that they form a range of consecutive objects (modulo
  // object alignment constraints) that can be visited iteratively: we
  // can compute the next object's location by using the current
  // object's address and size (and object alignment constraints).
  const bool need_bitmap =
      r->LiveBytes() != static_cast<size_t>(-1) &&
      r->LiveBytes() != static_cast<size_t>(top - pos);
  if (need_bitmap) {
    GetLiveBitmap()->VisitMarkedRange(
        reinterpret_cast<uintptr_t>(pos),
        reinterpret_cast<uintptr_t>(top),
        visitor);
  } else {
    while (pos < top) {
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(pos);
      if (obj->GetClass<kDefaultVerifyFlags, kWithoutReadBarrier>() != nullptr) {
        visitor(obj);
        pos = reinterpret_cast<uint8_t*>(GetNextObject(obj));
      } else {
        break;
      }
    }
  }
}

template <typename Visitor>
inline void RegionSpace::Walk(Visitor&& visitor) {
  WalkInternal</* kToSpaceOnly= */ false>(visitor);
}
template <typename Visitor>
inline void RegionSpace::WalkToSpace(Visitor&& visitor) {
  WalkInternal</* kToSpaceOnly= */ true>(visitor);
}

inline mirror::Object* RegionSpace::GetNextObject(mirror::Object* obj) {
  const uintptr_t position = reinterpret_cast<uintptr_t>(obj) + obj->SizeOf();
  return reinterpret_cast<mirror::Object*>(RoundUp(position, kAlignment));
}

template<bool kForEvac>
inline mirror::Object* RegionSpace::AllocLarge(size_t num_bytes,
                                               /* out */ size_t* bytes_allocated,
                                               /* out */ size_t* usable_size,
                                               /* out */ size_t* bytes_tl_bulk_allocated) {
  DCHECK_ALIGNED(num_bytes, kAlignment);
  DCHECK_GT(num_bytes, kRegionSize);
  size_t num_regs_in_large_region = RoundUp(num_bytes, kRegionSize) / kRegionSize;
  DCHECK_GT(num_regs_in_large_region, 0U);
  DCHECK_LT((num_regs_in_large_region - 1) * kRegionSize, num_bytes);
  DCHECK_LE(num_bytes, num_regs_in_large_region * kRegionSize);
  MutexLock mu(Thread::Current(), region_lock_);
  if (!kForEvac) {
    // Retain sufficient free regions for full evacuation.
    if ((num_non_free_regions_ + num_regs_in_large_region) * 2 > num_regions_) {
      return nullptr;
    }
  }

  mirror::Object* region = nullptr;
  // Find a large enough set of contiguous free regions.
  if (kCyclicRegionAllocation) {
    size_t next_region = -1;
    // Try to find a range of free regions within [cyclic_alloc_region_index_, num_regions_).
    region = AllocLargeInRange<kForEvac>(cyclic_alloc_region_index_,
                                         num_regions_,
                                         num_regs_in_large_region,
                                         bytes_allocated,
                                         usable_size,
                                         bytes_tl_bulk_allocated,
                                         &next_region);

    if (region == nullptr) {
      DCHECK_EQ(next_region, static_cast<size_t>(-1));
      // If the previous attempt failed, try to find a range of free regions within
      // [0, min(cyclic_alloc_region_index_ + num_regs_in_large_region - 1, num_regions_)).
      region = AllocLargeInRange<kForEvac>(
          0,
          std::min(cyclic_alloc_region_index_ + num_regs_in_large_region - 1, num_regions_),
          num_regs_in_large_region,
          bytes_allocated,
          usable_size,
          bytes_tl_bulk_allocated,
          &next_region);
    }

    if (region != nullptr) {
      DCHECK_LT(0u, next_region);
      DCHECK_LE(next_region, num_regions_);
      // Move the cyclic allocation region marker to the region
      // following the large region that was just allocated.
      cyclic_alloc_region_index_ = next_region % num_regions_;
    }
  } else {
    // Try to find a range of free regions within [0, num_regions_).
    region = AllocLargeInRange<kForEvac>(0,
                                         num_regions_,
                                         num_regs_in_large_region,
                                         bytes_allocated,
                                         usable_size,
                                         bytes_tl_bulk_allocated);
  }
  if (kForEvac && region != nullptr) {
    TraceHeapSize();
  }
  return region;
}

template<bool kForEvac>
inline mirror::Object* RegionSpace::AllocLargeInRange(size_t begin,
                                                      size_t end,
                                                      size_t num_regs_in_large_region,
                                                      /* out */ size_t* bytes_allocated,
                                                      /* out */ size_t* usable_size,
                                                      /* out */ size_t* bytes_tl_bulk_allocated,
                                                      /* out */ size_t* next_region) {
  DCHECK_LE(0u, begin);
  DCHECK_LT(begin, end);
  DCHECK_LE(end, num_regions_);
  size_t left = begin;
  while (left + num_regs_in_large_region - 1 < end) {
    bool found = true;
    size_t right = left;
    DCHECK_LT(right, left + num_regs_in_large_region)
        << "The inner loop should iterate at least once";
    while (right < left + num_regs_in_large_region) {
      if (regions_[right].IsFree()) {
        ++right;
        // Ensure `right` is not going beyond the past-the-end index of the region space.
        DCHECK_LE(right, num_regions_);
      } else {
        found = false;
        break;
      }
    }
    if (found) {
      // `right` points to the one region past the last free region.
      DCHECK_EQ(left + num_regs_in_large_region, right);
      Region* first_reg = &regions_[left];
      DCHECK(first_reg->IsFree());
      first_reg->UnfreeLarge(this, time_);
      if (kForEvac) {
        ++num_evac_regions_;
      } else {
        ++num_non_free_regions_;
      }
      size_t allocated = num_regs_in_large_region * kRegionSize;
      // We make 'top' all usable bytes, as the caller of this
      // allocation may use all of 'usable_size' (see mirror::Array::Alloc).
      first_reg->SetTop(first_reg->Begin() + allocated);
      if (!kForEvac) {
        // Evac doesn't count as newly allocated.
        first_reg->SetNewlyAllocated();
      }
      for (size_t p = left + 1; p < right; ++p) {
        DCHECK_LT(p, num_regions_);
        DCHECK(regions_[p].IsFree());
        regions_[p].UnfreeLargeTail(this, time_);
        if (kForEvac) {
          ++num_evac_regions_;
        } else {
          ++num_non_free_regions_;
        }
        if (!kForEvac) {
          // Evac doesn't count as newly allocated.
          regions_[p].SetNewlyAllocated();
        }
      }
      *bytes_allocated = allocated;
      if (usable_size != nullptr) {
        *usable_size = allocated;
      }
      *bytes_tl_bulk_allocated = allocated;
      mirror::Object* large_region = reinterpret_cast<mirror::Object*>(first_reg->Begin());
      DCHECK(large_region != nullptr);
      if (next_region != nullptr) {
        // Return the index to the region next to the allocated large region via `next_region`.
        *next_region = right;
      }
      return large_region;
    } else {
      // `right` points to the non-free region. Start with the one after it.
      left = right + 1;
    }
  }
  return nullptr;
}

template<bool kForEvac>
inline void RegionSpace::FreeLarge(mirror::Object* large_obj, size_t bytes_allocated) {
  DCHECK(Contains(large_obj));
  DCHECK_ALIGNED(large_obj, kRegionSize);
  MutexLock mu(Thread::Current(), region_lock_);
  uint8_t* begin_addr = reinterpret_cast<uint8_t*>(large_obj);
  uint8_t* end_addr = AlignUp(reinterpret_cast<uint8_t*>(large_obj) + bytes_allocated, kRegionSize);
  CHECK_LT(begin_addr, end_addr);
  for (uint8_t* addr = begin_addr; addr < end_addr; addr += kRegionSize) {
    Region* reg = RefToRegionLocked(reinterpret_cast<mirror::Object*>(addr));
    if (addr == begin_addr) {
      DCHECK(reg->IsLarge());
    } else {
      DCHECK(reg->IsLargeTail());
    }
    reg->Clear(/*zero_and_release_pages=*/true);
    if (kForEvac) {
      --num_evac_regions_;
    } else {
      --num_non_free_regions_;
    }
  }
  if (kIsDebugBuild && end_addr < Limit()) {
    // If we aren't at the end of the space, check that the next region is not a large tail.
    Region* following_reg = RefToRegionLocked(reinterpret_cast<mirror::Object*>(end_addr));
    DCHECK(!following_reg->IsLargeTail());
  }
}

inline size_t RegionSpace::Region::BytesAllocated() const {
  if (IsLarge()) {
    DCHECK_LT(begin_ + kRegionSize, Top());
    return static_cast<size_t>(Top() - begin_);
  } else if (IsLargeTail()) {
    DCHECK_EQ(begin_, Top());
    return 0;
  } else {
    DCHECK(IsAllocated()) << "state=" << state_;
    DCHECK_LE(begin_, Top());
    size_t bytes;
    if (is_a_tlab_) {
      bytes = thread_->GetTlabEnd() - begin_;
    } else {
      bytes = static_cast<size_t>(Top() - begin_);
    }
    DCHECK_LE(bytes, kRegionSize);
    return bytes;
  }
}

inline size_t RegionSpace::Region::ObjectsAllocated() const {
  if (IsLarge()) {
    DCHECK_LT(begin_ + kRegionSize, Top());
    DCHECK_EQ(objects_allocated_.load(std::memory_order_relaxed), 0U);
    return 1;
  } else if (IsLargeTail()) {
    DCHECK_EQ(begin_, Top());
    DCHECK_EQ(objects_allocated_.load(std::memory_order_relaxed), 0U);
    return 0;
  } else {
    DCHECK(IsAllocated()) << "state=" << state_;
    return objects_allocated_;
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_INL_H_
