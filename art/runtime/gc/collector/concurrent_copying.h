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

#ifndef ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
#define ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_

#include "garbage_collector.h"
#include "gc/accounting/space_bitmap.h"
#include "immune_spaces.h"
#include "offsets.h"

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace art {
class Barrier;
class Closure;
class RootInfo;

namespace mirror {
template<class MirrorType> class CompressedReference;
template<class MirrorType> class HeapReference;
class Object;
}  // namespace mirror

namespace gc {

namespace accounting {
template<typename T> class AtomicStack;
typedef AtomicStack<mirror::Object> ObjectStack;
template <size_t kAlignment> class SpaceBitmap;
typedef SpaceBitmap<kObjectAlignment> ContinuousSpaceBitmap;
class HeapBitmap;
class ReadBarrierTable;
}  // namespace accounting

namespace space {
class RegionSpace;
}  // namespace space

namespace collector {

class ConcurrentCopying : public GarbageCollector {
 public:
  // Enable the no-from-space-refs verification at the pause.
  static constexpr bool kEnableNoFromSpaceRefsVerification = kIsDebugBuild;
  // Enable the from-space bytes/objects check.
  static constexpr bool kEnableFromSpaceAccountingCheck = kIsDebugBuild;
  // Enable verbose mode.
  static constexpr bool kVerboseMode = false;
  // If kGrayDirtyImmuneObjects is true then we gray dirty objects in the GC pause to prevent dirty
  // pages.
  static constexpr bool kGrayDirtyImmuneObjects = true;

  ConcurrentCopying(Heap* heap,
                    bool young_gen,
                    bool use_generational_cc,
                    const std::string& name_prefix = "",
                    bool measure_read_barrier_slow_path = false);
  ~ConcurrentCopying();

  void RunPhases() override
      REQUIRES(!immune_gray_stack_lock_,
               !mark_stack_lock_,
               !rb_slow_path_histogram_lock_,
               !skipped_blocks_lock_);
  void InitializePhase() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !immune_gray_stack_lock_);
  void MarkingPhase() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void CopyingPhase() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void ReclaimPhase() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void FinishPhase() REQUIRES(!mark_stack_lock_,
                              !rb_slow_path_histogram_lock_,
                              !skipped_blocks_lock_);

  void CaptureRssAtPeak() REQUIRES(!mark_stack_lock_);
  void BindBitmaps() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::heap_bitmap_lock_);
  GcType GetGcType() const override {
    return (use_generational_cc_ && young_gen_)
        ? kGcTypeSticky
        : kGcTypePartial;
  }
  CollectorType GetCollectorType() const override {
    return kCollectorTypeCC;
  }
  void RevokeAllThreadLocalBuffers() override;
  // Creates inter-region ref bitmaps for region-space and non-moving-space.
  // Gets called in Heap construction after the two spaces are created.
  void CreateInterRegionRefBitmaps();
  void SetRegionSpace(space::RegionSpace* region_space) {
    DCHECK(region_space != nullptr);
    region_space_ = region_space;
  }
  space::RegionSpace* RegionSpace() {
    return region_space_;
  }
  // Assert the to-space invariant for a heap reference `ref` held in `obj` at offset `offset`.
  void AssertToSpaceInvariant(mirror::Object* obj, MemberOffset offset, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Assert the to-space invariant for a GC root reference `ref`.
  void AssertToSpaceInvariant(GcRootSource* gc_root_source, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsInToSpace(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(ref != nullptr);
    return IsMarked(ref) == ref;
  }
  // Mark object `from_ref`, copying it to the to-space if needed.
  template<bool kGrayImmuneObject = true, bool kNoUnEvac = false, bool kFromGCThread = false>
  ALWAYS_INLINE mirror::Object* Mark(Thread* const self,
                                     mirror::Object* from_ref,
                                     mirror::Object* holder = nullptr,
                                     MemberOffset offset = MemberOffset(0))
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  ALWAYS_INLINE mirror::Object* MarkFromReadBarrier(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  bool IsMarking() const {
    return is_marking_;
  }
  // We may want to use read barrier entrypoints before is_marking_ is true since concurrent graying
  // creates a small window where we might dispatch on these entrypoints.
  bool IsUsingReadBarrierEntrypoints() const {
    return is_using_read_barrier_entrypoints_;
  }
  bool IsActive() const {
    return is_active_;
  }
  Barrier& GetBarrier() {
    return *gc_barrier_;
  }
  bool IsWeakRefAccessEnabled() REQUIRES(Locks::thread_list_lock_) {
    return weak_ref_access_enabled_;
  }
  void RevokeThreadLocalMarkStack(Thread* thread) REQUIRES(!mark_stack_lock_);

  mirror::Object* IsMarked(mirror::Object* from_ref) override
      REQUIRES_SHARED(Locks::mutator_lock_);

  void AssertNoThreadMarkStackMapping(Thread* thread) REQUIRES(!mark_stack_lock_);

 private:
  void PushOntoMarkStack(Thread* const self, mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  mirror::Object* Copy(Thread* const self,
                       mirror::Object* from_ref,
                       mirror::Object* holder,
                       MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  // Scan the reference fields of object `to_ref`.
  template <bool kNoUnEvac>
  void Scan(mirror::Object* to_ref) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  // Scan the reference fields of object 'obj' in the dirty cards during
  // card-table scan. In addition to visiting the references, it also sets the
  // read-barrier state to gray for Reference-type objects to ensure that
  // GetReferent() called on these objects calls the read-barrier on the referent.
  template <bool kNoUnEvac>
  void ScanDirtyObject(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  // Process a field.
  template <bool kNoUnEvac>
  void Process(mirror::Object* obj, MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_ , !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  template<bool kGrayImmuneObject>
  void MarkRoot(Thread* const self, mirror::CompressedReference<mirror::Object>* root)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void VerifyNoFromSpaceReferences() REQUIRES(Locks::mutator_lock_);
  accounting::ObjectStack* GetAllocationStack();
  accounting::ObjectStack* GetLiveStack();
  void ProcessMarkStack() override REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  bool ProcessMarkStackOnce() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void ProcessMarkStackRef(mirror::Object* to_ref) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void GrayAllDirtyImmuneObjects()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void GrayAllNewlyDirtyImmuneObjects()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void VerifyGrayImmuneObjects()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void VerifyNoMissingCardMarks()
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  template <typename Processor>
  size_t ProcessThreadLocalMarkStacks(bool disable_weak_ref_access,
                                      Closure* checkpoint_callback,
                                      const Processor& processor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void RevokeThreadLocalMarkStacks(bool disable_weak_ref_access, Closure* checkpoint_callback)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SwitchToSharedMarkStackMode() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);
  void SwitchToGcExclusiveMarkStackMode() REQUIRES_SHARED(Locks::mutator_lock_);
  void DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                              ObjPtr<mirror::Reference> reference) override
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ProcessReferences(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* MarkObject(mirror::Object* from_ref) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void MarkHeapReference(mirror::HeapReference<mirror::Object>* from_ref,
                         bool do_atomic_update) override
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  bool IsMarkedInUnevacFromSpace(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsMarkedInNonMovingSpace(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* field,
                                   bool do_atomic_update) override
      REQUIRES_SHARED(Locks::mutator_lock_);
  void SweepSystemWeaks(Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::heap_bitmap_lock_);
  // Sweep unmarked objects to complete the garbage collection. Full GCs sweep
  // all allocation spaces (except the region space). Sticky-bit GCs just sweep
  // a subset of the heap.
  void Sweep(bool swap_bitmaps)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_, !mark_stack_lock_);
  // Sweep only pointers within an array.
  void SweepArray(accounting::ObjectStack* allocation_stack_, bool swap_bitmaps)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_, !mark_stack_lock_);
  void SweepLargeObjects(bool swap_bitmaps)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::heap_bitmap_lock_);
  void MarkZygoteLargeObjects()
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FillWithDummyObject(Thread* const self, mirror::Object* dummy_obj, size_t byte_size)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* AllocateInSkippedBlock(Thread* const self, size_t alloc_size)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void CheckEmptyMarkStack() REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  void IssueEmptyCheckpoint() REQUIRES_SHARED(Locks::mutator_lock_);
  bool IsOnAllocStack(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* GetFwdPtr(mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void FlipThreadRoots() REQUIRES(!Locks::mutator_lock_);
  void SwapStacks() REQUIRES_SHARED(Locks::mutator_lock_);
  void RecordLiveStackFreezeSize(Thread* self);
  void ComputeUnevacFromSpaceLiveRatio();
  void LogFromSpaceRefHolder(mirror::Object* obj, MemberOffset offset)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Dump information about reference `ref` and return it as a string.
  // Use `ref_name` to name the reference in messages. Each message is prefixed with `indent`.
  std::string DumpReferenceInfo(mirror::Object* ref, const char* ref_name, const char* indent = "")
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Dump information about heap reference `ref`, referenced from object `obj` at offset `offset`,
  // and return it as a string.
  std::string DumpHeapReference(mirror::Object* obj, MemberOffset offset, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Dump information about GC root `ref` and return it as a string.
  std::string DumpGcRoot(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  void AssertToSpaceInvariantInNonMovingSpace(mirror::Object* obj, mirror::Object* ref)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void ReenableWeakRefAccess(Thread* self) REQUIRES_SHARED(Locks::mutator_lock_);
  void DisableMarking() REQUIRES_SHARED(Locks::mutator_lock_);
  void IssueDisableMarkingCheckpoint() REQUIRES_SHARED(Locks::mutator_lock_);
  void ExpandGcMarkStack() REQUIRES_SHARED(Locks::mutator_lock_);
  mirror::Object* MarkNonMoving(Thread* const self,
                                mirror::Object* from_ref,
                                mirror::Object* holder = nullptr,
                                MemberOffset offset = MemberOffset(0))
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  ALWAYS_INLINE mirror::Object* MarkUnevacFromSpaceRegion(Thread* const self,
      mirror::Object* from_ref,
      accounting::SpaceBitmap<kObjectAlignment>* bitmap)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_);
  template<bool kGrayImmuneObject>
  ALWAYS_INLINE mirror::Object* MarkImmuneSpace(Thread* const self,
                                                mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!immune_gray_stack_lock_);
  void ScanImmuneObject(mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!mark_stack_lock_);
  mirror::Object* MarkFromReadBarrierWithMeasurements(Thread* const self,
                                                      mirror::Object* from_ref)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_, !skipped_blocks_lock_, !immune_gray_stack_lock_);
  void DumpPerformanceInfo(std::ostream& os) override REQUIRES(!rb_slow_path_histogram_lock_);
  // Set the read barrier mark entrypoints to non-null.
  void ActivateReadBarrierEntrypoints();

  void CaptureThreadRootsForMarking() REQUIRES_SHARED(Locks::mutator_lock_);
  void AddLiveBytesAndScanRef(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  bool TestMarkBitmapForRef(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  template <bool kAtomic = false>
  bool TestAndSetMarkBitForRef(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  void PushOntoLocalMarkStack(mirror::Object* ref) REQUIRES_SHARED(Locks::mutator_lock_);
  void ProcessMarkStackForMarkingAndComputeLiveBytes() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_);

  void RemoveThreadMarkStackMapping(Thread* thread, accounting::ObjectStack* tl_mark_stack)
      REQUIRES(mark_stack_lock_);
  void AddThreadMarkStackMapping(Thread* thread, accounting::ObjectStack* tl_mark_stack)
      REQUIRES(mark_stack_lock_);
  void AssertEmptyThreadMarkStackMap() REQUIRES(mark_stack_lock_);

  space::RegionSpace* region_space_;      // The underlying region space.
  std::unique_ptr<Barrier> gc_barrier_;
  std::unique_ptr<accounting::ObjectStack> gc_mark_stack_;

  // If true, enable generational collection when using the Concurrent Copying
  // (CC) collector, i.e. use sticky-bit CC for minor collections and (full) CC
  // for major collections. Generational CC collection is currently only
  // compatible with Baker read barriers. Set in Heap constructor.
  const bool use_generational_cc_;

  // Generational "sticky", only trace through dirty objects in region space.
  const bool young_gen_;

  // If true, the GC thread is done scanning marked objects on dirty and aged
  // card (see ConcurrentCopying::CopyingPhase).
  Atomic<bool> done_scanning_;

  // The read-barrier mark-bit stack. Stores object references whose
  // mark bit has been set by ConcurrentCopying::MarkFromReadBarrier,
  // so that this bit can be reset at the end of the collection in
  // ConcurrentCopying::FinishPhase. The mark bit of an object can be
  // used by mutator read barrier code to quickly test whether that
  // object has been already marked.
  std::unique_ptr<accounting::ObjectStack> rb_mark_bit_stack_;
  // Thread-unsafe Boolean value hinting that `rb_mark_bit_stack_` is
  // full. A thread-safe test of whether the read-barrier mark-bit
  // stack is full is implemented by `rb_mark_bit_stack_->AtomicPushBack(ref)`
  // (see use case in ConcurrentCopying::MarkFromReadBarrier).
  bool rb_mark_bit_stack_full_;

  // Guards access to pooled_mark_stacks_ and revoked_mark_stacks_ vectors.
  // Also guards destruction and revocations of thread-local mark-stacks.
  // Clearing thread-local mark-stack (by other threads or during destruction)
  // should be guarded by it.
  Mutex mark_stack_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<accounting::ObjectStack*> revoked_mark_stacks_
      GUARDED_BY(mark_stack_lock_);
  static constexpr size_t kMarkStackSize = kPageSize;
  static constexpr size_t kMarkStackPoolSize = 256;
  std::vector<accounting::ObjectStack*> pooled_mark_stacks_
      GUARDED_BY(mark_stack_lock_);
  // TODO(lokeshgidra b/140119552): remove this after bug fix.
  std::unordered_map<Thread*, accounting::ObjectStack*> thread_mark_stack_map_
      GUARDED_BY(mark_stack_lock_);
  Thread* thread_running_gc_;
  bool is_marking_;                       // True while marking is ongoing.
  // True while we might dispatch on the read barrier entrypoints.
  bool is_using_read_barrier_entrypoints_;
  bool is_active_;                        // True while the collection is ongoing.
  bool is_asserting_to_space_invariant_;  // True while asserting the to-space invariant.
  ImmuneSpaces immune_spaces_;
  accounting::ContinuousSpaceBitmap* region_space_bitmap_;
  // A cache of Heap::GetMarkBitmap().
  accounting::HeapBitmap* heap_mark_bitmap_;
  size_t live_stack_freeze_size_;
  size_t from_space_num_objects_at_first_pause_;  // Computed if kEnableFromSpaceAccountingCheck
  size_t from_space_num_bytes_at_first_pause_;  // Computed if kEnableFromSpaceAccountingCheck
  Atomic<int> is_mark_stack_push_disallowed_;
  enum MarkStackMode {
    kMarkStackModeOff = 0,      // Mark stack is off.
    kMarkStackModeThreadLocal,  // All threads except for the GC-running thread push refs onto
                                // thread-local mark stacks. The GC-running thread pushes onto and
                                // pops off the GC mark stack without a lock.
    kMarkStackModeShared,       // All threads share the GC mark stack with a lock.
    kMarkStackModeGcExclusive   // The GC-running thread pushes onto and pops from the GC mark stack
                                // without a lock. Other threads won't access the mark stack.
  };
  Atomic<MarkStackMode> mark_stack_mode_;
  bool weak_ref_access_enabled_ GUARDED_BY(Locks::thread_list_lock_);

  // How many objects and bytes we moved. The GC thread moves many more objects
  // than mutators.  Therefore, we separate the two to avoid CAS.  Bytes_moved_ and
  // bytes_moved_gc_thread_ are critical for GC triggering; the others are just informative.
  Atomic<size_t> bytes_moved_;  // Used by mutators
  Atomic<size_t> objects_moved_;  // Used by mutators
  size_t bytes_moved_gc_thread_;  // Used by GC
  size_t objects_moved_gc_thread_;  // Used by GC
  Atomic<uint64_t> cumulative_bytes_moved_;
  Atomic<uint64_t> cumulative_objects_moved_;

  // copied_live_bytes_ratio_sum_ is read and written by CC per GC, in
  // ReclaimPhase, and is read by DumpPerformanceInfo (potentially from another
  // thread). However, at present, DumpPerformanceInfo is only called when the
  // runtime shuts down, so no concurrent access. The same reasoning goes for
  // gc_count_ and reclaimed_bytes_ratio_sum_

  // The sum of of all copied live bytes ratio (to_bytes/from_bytes)
  float copied_live_bytes_ratio_sum_;
  // The number of GC counts, used to calculate the average above. (It doesn't
  // include GC where from_bytes is zero, IOW, from-space is empty, which is
  // possible for minor GC if all allocated objects are in non-moving
  // space.)
  size_t gc_count_;
  // Bit is set if the corresponding object has inter-region references that
  // were found during the marking phase of two-phase full-heap GC cycle.
  accounting::ContinuousSpaceBitmap region_space_inter_region_bitmap_;
  accounting::ContinuousSpaceBitmap non_moving_space_inter_region_bitmap_;

  // reclaimed_bytes_ratio = reclaimed_bytes/num_allocated_bytes per GC cycle
  float reclaimed_bytes_ratio_sum_;

  // The skipped blocks are memory blocks/chucks that were copies of
  // objects that were unused due to lost races (cas failures) at
  // object copy/forward pointer install. They may be reused.
  // Skipped blocks are always in region space. Their size is included directly
  // in num_bytes_allocated_, i.e. they are treated as allocated, but may be directly
  // used without going through a GC cycle like other objects. They are reused only
  // if we run out of region space. TODO: Revisit this design.
  Mutex skipped_blocks_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::multimap<size_t, uint8_t*> skipped_blocks_map_ GUARDED_BY(skipped_blocks_lock_);
  Atomic<size_t> to_space_bytes_skipped_;
  Atomic<size_t> to_space_objects_skipped_;

  // If measure_read_barrier_slow_path_ is true, we count how long is spent in MarkFromReadBarrier
  // and also log.
  bool measure_read_barrier_slow_path_;
  // mark_from_read_barrier_measurements_ is true if systrace is enabled or
  // measure_read_barrier_time_ is true.
  bool mark_from_read_barrier_measurements_;
  Atomic<uint64_t> rb_slow_path_ns_;
  Atomic<uint64_t> rb_slow_path_count_;
  Atomic<uint64_t> rb_slow_path_count_gc_;
  mutable Mutex rb_slow_path_histogram_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  Histogram<uint64_t> rb_slow_path_time_histogram_ GUARDED_BY(rb_slow_path_histogram_lock_);
  uint64_t rb_slow_path_count_total_ GUARDED_BY(rb_slow_path_histogram_lock_);
  uint64_t rb_slow_path_count_gc_total_ GUARDED_BY(rb_slow_path_histogram_lock_);

  accounting::ReadBarrierTable* rb_table_;
  bool force_evacuate_all_;  // True if all regions are evacuated.
  Atomic<bool> updated_all_immune_objects_;
  bool gc_grays_immune_objects_;
  Mutex immune_gray_stack_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
  std::vector<mirror::Object*> immune_gray_stack_ GUARDED_BY(immune_gray_stack_lock_);

  // Class of java.lang.Object. Filled in from WellKnownClasses in FlipCallback. Must
  // be filled in before flipping thread roots so that FillDummyObject can run. Not
  // ObjPtr since the GC may transition to suspended and runnable between phases.
  mirror::Class* java_lang_Object_;

  // Sweep array free buffer, used to sweep the spaces based on an array more
  // efficiently, by recording dead objects to be freed in batches (see
  // ConcurrentCopying::SweepArray).
  MemMap sweep_array_free_buffer_mem_map_;

  // Use signed because after_gc may be larger than before_gc.
  int64_t num_bytes_allocated_before_gc_;

  class ActivateReadBarrierEntrypointsCallback;
  class ActivateReadBarrierEntrypointsCheckpoint;
  class AssertToSpaceInvariantFieldVisitor;
  class AssertToSpaceInvariantRefsVisitor;
  class ClearBlackPtrsVisitor;
  class ComputeUnevacFromSpaceLiveRatioVisitor;
  class DisableMarkingCallback;
  class DisableMarkingCheckpoint;
  class DisableWeakRefAccessCallback;
  class FlipCallback;
  template <bool kConcurrent> class GrayImmuneObjectVisitor;
  class ImmuneSpaceScanObjVisitor;
  class LostCopyVisitor;
  template <bool kNoUnEvac> class RefFieldsVisitor;
  class RevokeThreadLocalMarkStackCheckpoint;
  class ScopedGcGraysImmuneObjects;
  class ThreadFlipVisitor;
  class VerifyGrayImmuneObjectsVisitor;
  class VerifyNoFromSpaceRefsFieldVisitor;
  class VerifyNoFromSpaceRefsVisitor;
  class VerifyNoMissingCardMarkVisitor;
  class ImmuneSpaceCaptureRefsVisitor;
  template <bool kAtomicTestAndSet = false> class CaptureRootsForMarkingVisitor;
  class CaptureThreadRootsForMarkingAndCheckpoint;
  template <bool kHandleInterRegionRefs> class ComputeLiveBytesAndMarkRefFieldsVisitor;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ConcurrentCopying);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_CONCURRENT_COPYING_H_
