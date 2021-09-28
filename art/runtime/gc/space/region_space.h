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

#ifndef ART_RUNTIME_GC_SPACE_REGION_SPACE_H_
#define ART_RUNTIME_GC_SPACE_REGION_SPACE_H_

#include "base/macros.h"
#include "base/mutex.h"
#include "space.h"
#include "thread.h"

#include <functional>
#include <map>

namespace art {
namespace gc {

namespace accounting {
class ReadBarrierTable;
}  // namespace accounting

namespace space {

// Cyclic region allocation strategy. If `true`, region allocation
// will not try to allocate a new region from the beginning of the
// region space, but from the last allocated region. This allocation
// strategy reduces region reuse and should help catch some GC bugs
// earlier. However, cyclic region allocation can also create memory
// fragmentation at the region level (see b/33795328); therefore, we
// only enable it in debug mode.
static constexpr bool kCyclicRegionAllocation = kIsDebugBuild;

// A space that consists of equal-sized regions.
class RegionSpace final : public ContinuousMemMapAllocSpace {
 public:
  typedef void(*WalkCallback)(void *start, void *end, size_t num_bytes, void* callback_arg);

  enum EvacMode {
    kEvacModeNewlyAllocated,
    kEvacModeLivePercentNewlyAllocated,
    kEvacModeForceAll,
  };

  SpaceType GetType() const override {
    return kSpaceTypeRegionSpace;
  }

  // Create a region space mem map with the requested sizes. The requested base address is not
  // guaranteed to be granted, if it is required, the caller should call Begin on the returned
  // space to confirm the request was granted.
  static MemMap CreateMemMap(const std::string& name, size_t capacity, uint8_t* requested_begin);
  static RegionSpace* Create(const std::string& name, MemMap&& mem_map, bool use_generational_cc);

  // Allocate `num_bytes`, returns null if the space is full.
  mirror::Object* Alloc(Thread* self,
                        size_t num_bytes,
                        /* out */ size_t* bytes_allocated,
                        /* out */ size_t* usable_size,
                        /* out */ size_t* bytes_tl_bulk_allocated)
      override REQUIRES(!region_lock_);
  // Thread-unsafe allocation for when mutators are suspended, used by the semispace collector.
  mirror::Object* AllocThreadUnsafe(Thread* self,
                                    size_t num_bytes,
                                    /* out */ size_t* bytes_allocated,
                                    /* out */ size_t* usable_size,
                                    /* out */ size_t* bytes_tl_bulk_allocated)
      override REQUIRES(Locks::mutator_lock_) REQUIRES(!region_lock_);
  // The main allocation routine.
  template<bool kForEvac>
  ALWAYS_INLINE mirror::Object* AllocNonvirtual(size_t num_bytes,
                                                /* out */ size_t* bytes_allocated,
                                                /* out */ size_t* usable_size,
                                                /* out */ size_t* bytes_tl_bulk_allocated)
      REQUIRES(!region_lock_);
  // Allocate/free large objects (objects that are larger than the region size).
  template<bool kForEvac>
  mirror::Object* AllocLarge(size_t num_bytes,
                             /* out */ size_t* bytes_allocated,
                             /* out */ size_t* usable_size,
                             /* out */ size_t* bytes_tl_bulk_allocated) REQUIRES(!region_lock_);
  template<bool kForEvac>
  void FreeLarge(mirror::Object* large_obj, size_t bytes_allocated) REQUIRES(!region_lock_);

  // Return the storage space required by obj.
  size_t AllocationSize(mirror::Object* obj, size_t* usable_size) override
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_) {
    return AllocationSizeNonvirtual(obj, usable_size);
  }
  size_t AllocationSizeNonvirtual(mirror::Object* obj, size_t* usable_size)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_);

  size_t Free(Thread*, mirror::Object*) override {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  size_t FreeList(Thread*, size_t, mirror::Object**) override {
    UNIMPLEMENTED(FATAL);
    return 0;
  }
  accounting::ContinuousSpaceBitmap* GetLiveBitmap() override {
    return &mark_bitmap_;
  }
  accounting::ContinuousSpaceBitmap* GetMarkBitmap() override {
    return &mark_bitmap_;
  }

  void Clear() override REQUIRES(!region_lock_);

  // Remove read and write memory protection from the whole region space,
  // i.e. make memory pages backing the region area not readable and not
  // writable.
  void Protect();

  // Remove memory protection from the whole region space, i.e. make memory
  // pages backing the region area readable and writable. This method is useful
  // to avoid page protection faults when dumping information about an invalid
  // reference.
  void Unprotect();

  // Change the non growth limit capacity to new capacity by shrinking or expanding the map.
  // Currently, only shrinking is supported.
  // Unlike implementations of this function in other spaces, we need to pass
  // new capacity as argument here as region space doesn't have any notion of
  // growth limit.
  void ClampGrowthLimit(size_t new_capacity) REQUIRES(!region_lock_);

  void Dump(std::ostream& os) const override;
  void DumpRegions(std::ostream& os) REQUIRES(!region_lock_);
  // Dump region containing object `obj`. Precondition: `obj` is in the region space.
  void DumpRegionForObject(std::ostream& os, mirror::Object* obj) REQUIRES(!region_lock_);
  void DumpNonFreeRegions(std::ostream& os) REQUIRES(!region_lock_);

  size_t RevokeThreadLocalBuffers(Thread* thread) override REQUIRES(!region_lock_);
  size_t RevokeThreadLocalBuffers(Thread* thread, const bool reuse) REQUIRES(!region_lock_);
  size_t RevokeAllThreadLocalBuffers() override
      REQUIRES(!Locks::runtime_shutdown_lock_, !Locks::thread_list_lock_, !region_lock_);
  void AssertThreadLocalBuffersAreRevoked(Thread* thread) REQUIRES(!region_lock_);
  void AssertAllThreadLocalBuffersAreRevoked()
      REQUIRES(!Locks::runtime_shutdown_lock_, !Locks::thread_list_lock_, !region_lock_);

  enum class RegionType : uint8_t {
    kRegionTypeAll,              // All types.
    kRegionTypeFromSpace,        // From-space. To be evacuated.
    kRegionTypeUnevacFromSpace,  // Unevacuated from-space. Not to be evacuated.
    kRegionTypeToSpace,          // To-space.
    kRegionTypeNone,             // None.
  };

  enum class RegionState : uint8_t {
    kRegionStateFree,            // Free region.
    kRegionStateAllocated,       // Allocated region.
    kRegionStateLarge,           // Large allocated (allocation larger than the region size).
    kRegionStateLargeTail,       // Large tail (non-first regions of a large allocation).
  };

  template<RegionType kRegionType> uint64_t GetBytesAllocatedInternal() REQUIRES(!region_lock_);
  template<RegionType kRegionType> uint64_t GetObjectsAllocatedInternal() REQUIRES(!region_lock_);
  uint64_t GetBytesAllocated() override REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeAll>();
  }
  uint64_t GetObjectsAllocated() override REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeAll>();
  }
  uint64_t GetBytesAllocatedInFromSpace() REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeFromSpace>();
  }
  uint64_t GetObjectsAllocatedInFromSpace() REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeFromSpace>();
  }
  uint64_t GetBytesAllocatedInUnevacFromSpace() REQUIRES(!region_lock_) {
    return GetBytesAllocatedInternal<RegionType::kRegionTypeUnevacFromSpace>();
  }
  uint64_t GetObjectsAllocatedInUnevacFromSpace() REQUIRES(!region_lock_) {
    return GetObjectsAllocatedInternal<RegionType::kRegionTypeUnevacFromSpace>();
  }
  size_t GetMaxPeakNumNonFreeRegions() const {
    return max_peak_num_non_free_regions_;
  }
  size_t GetNumRegions() const {
    return num_regions_;
  }
  size_t GetNumNonFreeRegions() const NO_THREAD_SAFETY_ANALYSIS {
    return num_non_free_regions_;
  }

  bool CanMoveObjects() const override {
    return true;
  }

  bool Contains(const mirror::Object* obj) const override {
    const uint8_t* byte_obj = reinterpret_cast<const uint8_t*>(obj);
    return byte_obj >= Begin() && byte_obj < Limit();
  }

  RegionSpace* AsRegionSpace() override {
    return this;
  }

  // Go through all of the blocks and visit the continuous objects.
  template <typename Visitor>
  ALWAYS_INLINE void Walk(Visitor&& visitor) REQUIRES(Locks::mutator_lock_);
  template <typename Visitor>
  ALWAYS_INLINE void WalkToSpace(Visitor&& visitor) REQUIRES(Locks::mutator_lock_);

  // Scans regions and calls visitor for objects in unevac-space corresponding
  // to the bits set in 'bitmap'.
  // Cannot acquire region_lock_ as visitor may need to acquire it for allocation.
  // Should not be called concurrently with functions (like SetFromSpace()) which
  // change regions' type.
  template <typename Visitor>
  ALWAYS_INLINE void ScanUnevacFromSpace(accounting::ContinuousSpaceBitmap* bitmap,
                                         Visitor&& visitor) NO_THREAD_SAFETY_ANALYSIS;

  accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() override {
    return nullptr;
  }
  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) override
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!region_lock_);

  // Object alignment within the space.
  static constexpr size_t kAlignment = kObjectAlignment;
  // The region size.
  static constexpr size_t kRegionSize = 256 * KB;

  bool IsInFromSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInFromSpace();
    }
    return false;
  }

  bool IsRegionNewlyAllocated(size_t idx) const NO_THREAD_SAFETY_ANALYSIS {
    DCHECK_LT(idx, num_regions_);
    return regions_[idx].IsNewlyAllocated();
  }

  bool IsInNewlyAllocatedRegion(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsNewlyAllocated();
    }
    return false;
  }

  bool IsInUnevacFromSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInUnevacFromSpace();
    }
    return false;
  }

  bool IsLargeObject(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsLarge();
    }
    return false;
  }

  bool IsInToSpace(mirror::Object* ref) {
    if (HasAddress(ref)) {
      Region* r = RefToRegionUnlocked(ref);
      return r->IsInToSpace();
    }
    return false;
  }

  // If `ref` is in the region space, return the type of its region;
  // otherwise, return `RegionType::kRegionTypeNone`.
  RegionType GetRegionType(mirror::Object* ref) {
    if (HasAddress(ref)) {
      return GetRegionTypeUnsafe(ref);
    }
    return RegionType::kRegionTypeNone;
  }

  // Unsafe version of RegionSpace::GetRegionType.
  // Precondition: `ref` is in the region space.
  RegionType GetRegionTypeUnsafe(mirror::Object* ref) {
    DCHECK(HasAddress(ref)) << ref;
    Region* r = RefToRegionUnlocked(ref);
    return r->Type();
  }

  // Zero live bytes for a large object, used by young gen CC for marking newly allocated large
  // objects.
  void ZeroLiveBytesForLargeObject(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  // Determine which regions to evacuate and tag them as
  // from-space. Tag the rest as unevacuated from-space.
  void SetFromSpace(accounting::ReadBarrierTable* rb_table,
                    EvacMode evac_mode,
                    bool clear_live_bytes)
      REQUIRES(!region_lock_);

  size_t FromSpaceSize() REQUIRES(!region_lock_);
  size_t UnevacFromSpaceSize() REQUIRES(!region_lock_);
  size_t ToSpaceSize() REQUIRES(!region_lock_);
  void ClearFromSpace(/* out */ uint64_t* cleared_bytes,
                      /* out */ uint64_t* cleared_objects,
                      const bool clear_bitmap)
      REQUIRES(!region_lock_);

  void AddLiveBytes(mirror::Object* ref, size_t alloc_size) {
    Region* reg = RefToRegionUnlocked(ref);
    reg->AddLiveBytes(alloc_size);
  }

  void AssertAllRegionLiveBytesZeroOrCleared() REQUIRES(!region_lock_) {
    if (kIsDebugBuild) {
      MutexLock mu(Thread::Current(), region_lock_);
      for (size_t i = 0; i < num_regions_; ++i) {
        Region* r = &regions_[i];
        size_t live_bytes = r->LiveBytes();
        CHECK(live_bytes == 0U || live_bytes == static_cast<size_t>(-1)) << live_bytes;
      }
    }
  }

  void SetAllRegionLiveBytesZero() REQUIRES(!region_lock_) {
    MutexLock mu(Thread::Current(), region_lock_);
    const size_t iter_limit = kUseTableLookupReadBarrier
        ? num_regions_
        : std::min(num_regions_, non_free_region_index_limit_);
    for (size_t i = 0; i < iter_limit; ++i) {
      Region* r = &regions_[i];
      // Newly allocated regions don't need up-to-date live_bytes_ for deciding
      // whether to be evacuated or not. See Region::ShouldBeEvacuated().
      if (!r->IsFree() && !r->IsNewlyAllocated()) {
        r->ZeroLiveBytes();
      }
    }
  }

  size_t RegionIdxForRefUnchecked(mirror::Object* ref) const NO_THREAD_SAFETY_ANALYSIS {
    DCHECK(HasAddress(ref));
    uintptr_t offset = reinterpret_cast<uintptr_t>(ref) - reinterpret_cast<uintptr_t>(Begin());
    size_t reg_idx = offset / kRegionSize;
    DCHECK_LT(reg_idx, num_regions_);
    Region* reg = &regions_[reg_idx];
    DCHECK_EQ(reg->Idx(), reg_idx);
    DCHECK(reg->Contains(ref));
    return reg_idx;
  }
  // Return -1 as region index for references outside this region space.
  size_t RegionIdxForRef(mirror::Object* ref) const NO_THREAD_SAFETY_ANALYSIS {
    if (HasAddress(ref)) {
      return RegionIdxForRefUnchecked(ref);
    } else {
      return static_cast<size_t>(-1);
    }
  }

  // Increment object allocation count for region containing ref.
  void RecordAlloc(mirror::Object* ref) REQUIRES(!region_lock_);

  bool AllocNewTlab(Thread* self, const size_t tlab_size, size_t* bytes_tl_bulk_allocated)
      REQUIRES(!region_lock_);

  uint32_t Time() {
    return time_;
  }

  size_t EvacBytes() const NO_THREAD_SAFETY_ANALYSIS {
    return num_evac_regions_ * kRegionSize;
  }

 private:
  RegionSpace(const std::string& name, MemMap&& mem_map, bool use_generational_cc);

  class Region {
   public:
    Region()
        : idx_(static_cast<size_t>(-1)),
          live_bytes_(static_cast<size_t>(-1)),
          begin_(nullptr),
          thread_(nullptr),
          top_(nullptr),
          end_(nullptr),
          objects_allocated_(0),
          alloc_time_(0),
          is_newly_allocated_(false),
          is_a_tlab_(false),
          state_(RegionState::kRegionStateAllocated),
          type_(RegionType::kRegionTypeToSpace) {}

    void Init(size_t idx, uint8_t* begin, uint8_t* end) {
      idx_ = idx;
      begin_ = begin;
      top_.store(begin, std::memory_order_relaxed);
      end_ = end;
      state_ = RegionState::kRegionStateFree;
      type_ = RegionType::kRegionTypeNone;
      objects_allocated_.store(0, std::memory_order_relaxed);
      alloc_time_ = 0;
      live_bytes_ = static_cast<size_t>(-1);
      is_newly_allocated_ = false;
      is_a_tlab_ = false;
      thread_ = nullptr;
      DCHECK_LT(begin, end);
      DCHECK_EQ(static_cast<size_t>(end - begin), kRegionSize);
    }

    RegionState State() const {
      return state_;
    }

    RegionType Type() const {
      return type_;
    }

    void Clear(bool zero_and_release_pages);

    ALWAYS_INLINE mirror::Object* Alloc(size_t num_bytes,
                                        /* out */ size_t* bytes_allocated,
                                        /* out */ size_t* usable_size,
                                        /* out */ size_t* bytes_tl_bulk_allocated);

    bool IsFree() const {
      bool is_free = (state_ == RegionState::kRegionStateFree);
      if (is_free) {
        DCHECK(IsInNoSpace());
        DCHECK_EQ(begin_, Top());
        DCHECK_EQ(objects_allocated_.load(std::memory_order_relaxed), 0U);
      }
      return is_free;
    }

    // Given a free region, declare it non-free (allocated).
    void Unfree(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    // Given a free region, declare it non-free (allocated) and large.
    void UnfreeLarge(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    // Given a free region, declare it non-free (allocated) and large tail.
    void UnfreeLargeTail(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    void MarkAsAllocated(RegionSpace* region_space, uint32_t alloc_time)
        REQUIRES(region_space->region_lock_);

    void SetNewlyAllocated() {
      is_newly_allocated_ = true;
    }

    // Non-large, non-large-tail allocated.
    bool IsAllocated() const {
      return state_ == RegionState::kRegionStateAllocated;
    }

    // Large allocated.
    bool IsLarge() const {
      bool is_large = (state_ == RegionState::kRegionStateLarge);
      if (is_large) {
        DCHECK_LT(begin_ + kRegionSize, Top());
      }
      return is_large;
    }

    void ZeroLiveBytes() {
      live_bytes_ = 0;
    }

    // Large-tail allocated.
    bool IsLargeTail() const {
      bool is_large_tail = (state_ == RegionState::kRegionStateLargeTail);
      if (is_large_tail) {
        DCHECK_EQ(begin_, Top());
      }
      return is_large_tail;
    }

    size_t Idx() const {
      return idx_;
    }

    bool IsNewlyAllocated() const {
      return is_newly_allocated_;
    }

    bool IsTlab() const {
      return is_a_tlab_;
    }

    bool IsInFromSpace() const {
      return type_ == RegionType::kRegionTypeFromSpace;
    }

    bool IsInToSpace() const {
      return type_ == RegionType::kRegionTypeToSpace;
    }

    bool IsInUnevacFromSpace() const {
      return type_ == RegionType::kRegionTypeUnevacFromSpace;
    }

    bool IsInNoSpace() const {
      return type_ == RegionType::kRegionTypeNone;
    }

    // Set this region as evacuated from-space. At the end of the
    // collection, RegionSpace::ClearFromSpace will clear and reclaim
    // the space used by this region, and tag it as unallocated/free.
    void SetAsFromSpace() {
      DCHECK(!IsFree() && IsInToSpace());
      type_ = RegionType::kRegionTypeFromSpace;
      if (IsNewlyAllocated()) {
        // Clear the "newly allocated" status here, as we do not want the
        // GC to see it when encountering references in the from-space.
        //
        // Invariant: There should be no newly-allocated region in the
        // from-space (when the from-space exists, which is between the calls
        // to RegionSpace::SetFromSpace and RegionSpace::ClearFromSpace).
        is_newly_allocated_ = false;
      }
      // Set live bytes to an invalid value, as we have made an
      // evacuation decision (possibly based on the percentage of live
      // bytes).
      live_bytes_ = static_cast<size_t>(-1);
    }

    // Set this region as unevacuated from-space. At the end of the
    // collection, RegionSpace::ClearFromSpace will preserve the space
    // used by this region, and tag it as to-space (see
    // Region::SetUnevacFromSpaceAsToSpace below).
    void SetAsUnevacFromSpace(bool clear_live_bytes);

    // Set this region as to-space. Used by RegionSpace::ClearFromSpace.
    // This is only valid if it is currently an unevac from-space region.
    void SetUnevacFromSpaceAsToSpace() {
      DCHECK(!IsFree() && IsInUnevacFromSpace());
      type_ = RegionType::kRegionTypeToSpace;
    }

    // Return whether this region should be evacuated. Used by RegionSpace::SetFromSpace.
    ALWAYS_INLINE bool ShouldBeEvacuated(EvacMode evac_mode);

    void AddLiveBytes(size_t live_bytes) {
      DCHECK(GetUseGenerationalCC() || IsInUnevacFromSpace());
      DCHECK(!IsLargeTail());
      DCHECK_NE(live_bytes_, static_cast<size_t>(-1));
      // For large allocations, we always consider all bytes in the regions live.
      live_bytes_ += IsLarge() ? Top() - begin_ : live_bytes;
      DCHECK_LE(live_bytes_, BytesAllocated());
    }

    bool AllAllocatedBytesAreLive() const {
      return LiveBytes() == static_cast<size_t>(Top() - Begin());
    }

    size_t LiveBytes() const {
      return live_bytes_;
    }

    // Returns the number of allocated bytes.  "Bulk allocated" bytes in active TLABs are excluded.
    size_t BytesAllocated() const;

    size_t ObjectsAllocated() const;

    uint8_t* Begin() const {
      return begin_;
    }

    ALWAYS_INLINE uint8_t* Top() const {
      return top_.load(std::memory_order_relaxed);
    }

    void SetTop(uint8_t* new_top) {
      top_.store(new_top, std::memory_order_relaxed);
    }

    uint8_t* End() const {
      return end_;
    }

    bool Contains(mirror::Object* ref) const {
      return begin_ <= reinterpret_cast<uint8_t*>(ref) && reinterpret_cast<uint8_t*>(ref) < end_;
    }

    void Dump(std::ostream& os) const;

    void RecordThreadLocalAllocations(size_t num_objects, size_t num_bytes) {
      DCHECK(IsAllocated());
      DCHECK_EQ(Top(), end_);
      objects_allocated_.fetch_add(num_objects, std::memory_order_relaxed);
      top_.store(begin_ + num_bytes, std::memory_order_relaxed);
      DCHECK_LE(Top(), end_);
    }

    uint64_t GetLongestConsecutiveFreeBytes() const;

   private:
    static bool GetUseGenerationalCC();

    size_t idx_;                        // The region's index in the region space.
    size_t live_bytes_;                 // The live bytes. Used to compute the live percent.
    uint8_t* begin_;                    // The begin address of the region.
    Thread* thread_;                    // The owning thread if it's a tlab.
    // Note that `top_` can be higher than `end_` in the case of a
    // large region, where an allocated object spans multiple regions
    // (large region + one or more large tail regions).
    Atomic<uint8_t*> top_;              // The current position of the allocation.
    uint8_t* end_;                      // The end address of the region.
    // objects_allocated_ is accessed using memory_order_relaxed. Treat as approximate when there
    // are concurrent updates.
    Atomic<size_t> objects_allocated_;  // The number of objects allocated.
    uint32_t alloc_time_;               // The allocation time of the region.
    // Note that newly allocated and evacuated regions use -1 as
    // special value for `live_bytes_`.
    bool is_newly_allocated_;           // True if it's allocated after the last collection.
    bool is_a_tlab_;                    // True if it's a tlab.
    RegionState state_;                 // The region state (see RegionState).
    RegionType type_;                   // The region type (see RegionType).

    friend class RegionSpace;
  };

  template<bool kToSpaceOnly, typename Visitor>
  ALWAYS_INLINE void WalkInternal(Visitor&& visitor) NO_THREAD_SAFETY_ANALYSIS;

  // Visitor will be iterating on objects in increasing address order.
  template<typename Visitor>
  ALWAYS_INLINE void WalkNonLargeRegion(Visitor&& visitor, const Region* r)
      NO_THREAD_SAFETY_ANALYSIS;

  Region* RefToRegion(mirror::Object* ref) REQUIRES(!region_lock_) {
    MutexLock mu(Thread::Current(), region_lock_);
    return RefToRegionLocked(ref);
  }

  void TraceHeapSize() REQUIRES(region_lock_);

  Region* RefToRegionUnlocked(mirror::Object* ref) NO_THREAD_SAFETY_ANALYSIS {
    // For a performance reason (this is frequently called via
    // RegionSpace::IsInFromSpace, etc.) we avoid taking a lock here.
    // Note that since we only change a region from to-space to (evac)
    // from-space during a pause (in RegionSpace::SetFromSpace) and
    // from (evac) from-space to free (after GC is done), as long as
    // `ref` is a valid reference into an allocated region, it's safe
    // to access the region state without the lock.
    return RefToRegionLocked(ref);
  }

  Region* RefToRegionLocked(mirror::Object* ref) REQUIRES(region_lock_) {
    DCHECK(HasAddress(ref));
    uintptr_t offset = reinterpret_cast<uintptr_t>(ref) - reinterpret_cast<uintptr_t>(Begin());
    size_t reg_idx = offset / kRegionSize;
    DCHECK_LT(reg_idx, num_regions_);
    Region* reg = &regions_[reg_idx];
    DCHECK_EQ(reg->Idx(), reg_idx);
    DCHECK(reg->Contains(ref));
    return reg;
  }

  // Return the object location following `obj` in the region space
  // (i.e., the object location at `obj + obj->SizeOf()`).
  //
  // Note that unless
  // - the region containing `obj` is fully used; and
  // - `obj` is not the last object of that region;
  // the returned location is not guaranteed to be a valid object.
  static mirror::Object* GetNextObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AdjustNonFreeRegionLimit(size_t new_non_free_region_index) REQUIRES(region_lock_) {
    DCHECK_LT(new_non_free_region_index, num_regions_);
    non_free_region_index_limit_ = std::max(non_free_region_index_limit_,
                                            new_non_free_region_index + 1);
    VerifyNonFreeRegionLimit();
  }

  void SetNonFreeRegionLimit(size_t new_non_free_region_index_limit) REQUIRES(region_lock_) {
    DCHECK_LE(new_non_free_region_index_limit, num_regions_);
    non_free_region_index_limit_ = new_non_free_region_index_limit;
    VerifyNonFreeRegionLimit();
  }

  // Implementation of this invariant:
  // for all `i >= non_free_region_index_limit_`, `regions_[i].IsFree()` is true.
  void VerifyNonFreeRegionLimit() REQUIRES(region_lock_) {
    if (kIsDebugBuild && non_free_region_index_limit_ < num_regions_) {
      for (size_t i = non_free_region_index_limit_; i < num_regions_; ++i) {
        CHECK(regions_[i].IsFree());
      }
    }
  }

  Region* AllocateRegion(bool for_evac) REQUIRES(region_lock_);
  void RevokeThreadLocalBuffersLocked(Thread* thread, bool reuse) REQUIRES(region_lock_);

  // Scan region range [`begin`, `end`) in increasing order to try to
  // allocate a large region having a size of `num_regs_in_large_region`
  // regions. If there is no space in the region space to allocate this
  // large region, return null.
  //
  // If argument `next_region` is not null, use `*next_region` to
  // return the index to the region next to the allocated large region
  // returned by this method.
  template<bool kForEvac>
  mirror::Object* AllocLargeInRange(size_t begin,
                                    size_t end,
                                    size_t num_regs_in_large_region,
                                    /* out */ size_t* bytes_allocated,
                                    /* out */ size_t* usable_size,
                                    /* out */ size_t* bytes_tl_bulk_allocated,
                                    /* out */ size_t* next_region = nullptr) REQUIRES(region_lock_);

  // Check that the value of `r->LiveBytes()` matches the number of
  // (allocated) bytes used by live objects according to the live bits
  // in the region space bitmap range corresponding to region `r`.
  void CheckLiveBytesAgainstRegionBitmap(Region* r);

  // Poison memory areas used by dead objects within unevacuated
  // region `r`. This is meant to detect dangling references to dead
  // objects earlier in debug mode.
  void PoisonDeadObjectsInUnevacuatedRegion(Region* r);

  Mutex region_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  // Cached version of Heap::use_generational_cc_.
  const bool use_generational_cc_;
  uint32_t time_;                  // The time as the number of collections since the startup.
  size_t num_regions_;             // The number of regions in this space.
  // The number of non-free regions in this space.
  size_t num_non_free_regions_ GUARDED_BY(region_lock_);

  // The number of evac regions allocated during collection. 0 when GC not running.
  size_t num_evac_regions_ GUARDED_BY(region_lock_);

  // Maintain the maximum of number of non-free regions collected just before
  // reclaim in each GC cycle. At this moment in cycle, highest number of
  // regions are in non-free.
  size_t max_peak_num_non_free_regions_;

  // The pointer to the region array.
  std::unique_ptr<Region[]> regions_ GUARDED_BY(region_lock_);

  // To hold partially used TLABs which can be reassigned to threads later for
  // utilizing the un-used portion.
  std::multimap<size_t, Region*, std::greater<size_t>> partial_tlabs_ GUARDED_BY(region_lock_);
  // The upper-bound index of the non-free regions. Used to avoid scanning all regions in
  // RegionSpace::SetFromSpace and RegionSpace::ClearFromSpace.
  //
  // Invariant (verified by RegionSpace::VerifyNonFreeRegionLimit):
  //   for all `i >= non_free_region_index_limit_`, `regions_[i].IsFree()` is true.
  size_t non_free_region_index_limit_ GUARDED_BY(region_lock_);

  Region* current_region_;         // The region currently used for allocation.
  Region* evac_region_;            // The region currently used for evacuation.
  Region full_region_;             // The dummy/sentinel region that looks full.

  // Index into the region array pointing to the starting region when
  // trying to allocate a new region. Only used when
  // `kCyclicRegionAllocation` is true.
  size_t cyclic_alloc_region_index_ GUARDED_BY(region_lock_);

  // Mark bitmap used by the GC.
  accounting::ContinuousSpaceBitmap mark_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(RegionSpace);
};

std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionState& value);
std::ostream& operator<<(std::ostream& os, const RegionSpace::RegionType& value);

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_REGION_SPACE_H_
