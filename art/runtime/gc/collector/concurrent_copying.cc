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

#include "concurrent_copying.h"

#include "art_field-inl.h"
#include "barrier.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/histogram-inl.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "class_root.h"
#include "debugger.h"
#include "gc/accounting/atomic_stack.h"
#include "gc/accounting/heap_bitmap-inl.h"
#include "gc/accounting/mod_union_table-inl.h"
#include "gc/accounting/read_barrier_table.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/gc_pause_listener.h"
#include "gc/reference_processor.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "gc/verification.h"
#include "image-inl.h"
#include "intern_table.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object-refvisitor-inl.h"
#include "mirror/object_reference.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_list.h"
#include "well_known_classes.h"

namespace art {
namespace gc {
namespace collector {

static constexpr size_t kDefaultGcMarkStackSize = 2 * MB;
// If kFilterModUnionCards then we attempt to filter cards that don't need to be dirty in the mod
// union table. Disabled since it does not seem to help the pause much.
static constexpr bool kFilterModUnionCards = kIsDebugBuild;
// If kDisallowReadBarrierDuringScan is true then the GC aborts if there are any read barrier that
// occur during ConcurrentCopying::Scan in GC thread. May be used to diagnose possibly unnecessary
// read barriers. Only enabled for kIsDebugBuild to avoid performance hit.
static constexpr bool kDisallowReadBarrierDuringScan = kIsDebugBuild;
// Slow path mark stack size, increase this if the stack is getting full and it is causing
// performance problems.
static constexpr size_t kReadBarrierMarkStackSize = 512 * KB;
// Size (in the number of objects) of the sweep array free buffer.
static constexpr size_t kSweepArrayChunkFreeSize = 1024;
// Verify that there are no missing card marks.
static constexpr bool kVerifyNoMissingCardMarks = kIsDebugBuild;

ConcurrentCopying::ConcurrentCopying(Heap* heap,
                                     bool young_gen,
                                     bool use_generational_cc,
                                     const std::string& name_prefix,
                                     bool measure_read_barrier_slow_path)
    : GarbageCollector(heap,
                       name_prefix + (name_prefix.empty() ? "" : " ") +
                       "concurrent copying"),
      region_space_(nullptr),
      gc_barrier_(new Barrier(0)),
      gc_mark_stack_(accounting::ObjectStack::Create("concurrent copying gc mark stack",
                                                     kDefaultGcMarkStackSize,
                                                     kDefaultGcMarkStackSize)),
      use_generational_cc_(use_generational_cc),
      young_gen_(young_gen),
      rb_mark_bit_stack_(accounting::ObjectStack::Create("rb copying gc mark stack",
                                                         kReadBarrierMarkStackSize,
                                                         kReadBarrierMarkStackSize)),
      rb_mark_bit_stack_full_(false),
      mark_stack_lock_("concurrent copying mark stack lock", kMarkSweepMarkStackLock),
      thread_running_gc_(nullptr),
      is_marking_(false),
      is_using_read_barrier_entrypoints_(false),
      is_active_(false),
      is_asserting_to_space_invariant_(false),
      region_space_bitmap_(nullptr),
      heap_mark_bitmap_(nullptr),
      live_stack_freeze_size_(0),
      from_space_num_objects_at_first_pause_(0),
      from_space_num_bytes_at_first_pause_(0),
      mark_stack_mode_(kMarkStackModeOff),
      weak_ref_access_enabled_(true),
      copied_live_bytes_ratio_sum_(0.f),
      gc_count_(0),
      reclaimed_bytes_ratio_sum_(0.f),
      skipped_blocks_lock_("concurrent copying bytes blocks lock", kMarkSweepMarkStackLock),
      measure_read_barrier_slow_path_(measure_read_barrier_slow_path),
      mark_from_read_barrier_measurements_(false),
      rb_slow_path_ns_(0),
      rb_slow_path_count_(0),
      rb_slow_path_count_gc_(0),
      rb_slow_path_histogram_lock_("Read barrier histogram lock"),
      rb_slow_path_time_histogram_("Mutator time in read barrier slow path", 500, 32),
      rb_slow_path_count_total_(0),
      rb_slow_path_count_gc_total_(0),
      rb_table_(heap_->GetReadBarrierTable()),
      force_evacuate_all_(false),
      gc_grays_immune_objects_(false),
      immune_gray_stack_lock_("concurrent copying immune gray stack lock",
                              kMarkSweepMarkStackLock),
      num_bytes_allocated_before_gc_(0) {
  static_assert(space::RegionSpace::kRegionSize == accounting::ReadBarrierTable::kRegionSize,
                "The region space size and the read barrier table region size must match");
  CHECK(use_generational_cc_ || !young_gen_);
  Thread* self = Thread::Current();
  {
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // Cache this so that we won't have to lock heap_bitmap_lock_ in
    // Mark() which could cause a nested lock on heap_bitmap_lock_
    // when GC causes a RB while doing GC or a lock order violation
    // (class_linker_lock_ and heap_bitmap_lock_).
    heap_mark_bitmap_ = heap->GetMarkBitmap();
  }
  {
    MutexLock mu(self, mark_stack_lock_);
    for (size_t i = 0; i < kMarkStackPoolSize; ++i) {
      accounting::AtomicStack<mirror::Object>* mark_stack =
          accounting::AtomicStack<mirror::Object>::Create(
              "thread local mark stack", kMarkStackSize, kMarkStackSize);
      pooled_mark_stacks_.push_back(mark_stack);
    }
  }
  if (use_generational_cc_) {
    // Allocate sweep array free buffer.
    std::string error_msg;
    sweep_array_free_buffer_mem_map_ = MemMap::MapAnonymous(
        "concurrent copying sweep array free buffer",
        RoundUp(kSweepArrayChunkFreeSize * sizeof(mirror::Object*), kPageSize),
        PROT_READ | PROT_WRITE,
        /*low_4gb=*/ false,
        &error_msg);
    CHECK(sweep_array_free_buffer_mem_map_.IsValid())
        << "Couldn't allocate sweep array free buffer: " << error_msg;
  }
}

void ConcurrentCopying::MarkHeapReference(mirror::HeapReference<mirror::Object>* field,
                                          bool do_atomic_update) {
  Thread* const self = Thread::Current();
  if (UNLIKELY(do_atomic_update)) {
    // Used to mark the referent in DelayReferenceReferent in transaction mode.
    mirror::Object* from_ref = field->AsMirrorPtr();
    if (from_ref == nullptr) {
      return;
    }
    mirror::Object* to_ref = Mark(self, from_ref);
    if (from_ref != to_ref) {
      do {
        if (field->AsMirrorPtr() != from_ref) {
          // Concurrently overwritten by a mutator.
          break;
        }
      } while (!field->CasWeakRelaxed(from_ref, to_ref));
    }
  } else {
    // Used for preserving soft references, should be OK to not have a CAS here since there should be
    // no other threads which can trigger read barriers on the same referent during reference
    // processing.
    field->Assign(Mark(self, field->AsMirrorPtr()));
  }
}

ConcurrentCopying::~ConcurrentCopying() {
  STLDeleteElements(&pooled_mark_stacks_);
}

void ConcurrentCopying::RunPhases() {
  CHECK(kUseBakerReadBarrier || kUseTableLookupReadBarrier);
  CHECK(!is_active_);
  is_active_ = true;
  Thread* self = Thread::Current();
  thread_running_gc_ = self;
  Locks::mutator_lock_->AssertNotHeld(self);
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    InitializePhase();
    // In case of forced evacuation, all regions are evacuated and hence no
    // need to compute live_bytes.
    if (use_generational_cc_ && !young_gen_ && !force_evacuate_all_) {
      MarkingPhase();
    }
  }
  if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
    // Switch to read barrier mark entrypoints before we gray the objects. This is required in case
    // a mutator sees a gray bit and dispatches on the entrypoint. (b/37876887).
    ActivateReadBarrierEntrypoints();
    // Gray dirty immune objects concurrently to reduce GC pause times. We re-process gray cards in
    // the pause.
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    GrayAllDirtyImmuneObjects();
  }
  FlipThreadRoots();
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    CopyingPhase();
  }
  // Verify no from space refs. This causes a pause.
  if (kEnableNoFromSpaceRefsVerification) {
    TimingLogger::ScopedTiming split("(Paused)VerifyNoFromSpaceReferences", GetTimings());
    ScopedPause pause(this, false);
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "Verifying no from-space refs";
    }
    VerifyNoFromSpaceReferences();
    if (kVerboseMode) {
      LOG(INFO) << "Done verifying no from-space refs";
    }
    CheckEmptyMarkStack();
  }
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    ReclaimPhase();
  }
  FinishPhase();
  CHECK(is_active_);
  is_active_ = false;
  thread_running_gc_ = nullptr;
}

class ConcurrentCopying::ActivateReadBarrierEntrypointsCheckpoint : public Closure {
 public:
  explicit ActivateReadBarrierEntrypointsCheckpoint(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {}

  void Run(Thread* thread) override NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    DCHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Switch to the read barrier entrypoints.
    thread->SetReadBarrierEntrypoints();
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::ActivateReadBarrierEntrypointsCallback : public Closure {
 public:
  explicit ActivateReadBarrierEntrypointsCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {}

  void Run(Thread* self ATTRIBUTE_UNUSED) override REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a race with ThreadList::Register().
    CHECK(!concurrent_copying_->is_using_read_barrier_entrypoints_);
    concurrent_copying_->is_using_read_barrier_entrypoints_ = true;
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::ActivateReadBarrierEntrypoints() {
  Thread* const self = Thread::Current();
  ActivateReadBarrierEntrypointsCheckpoint checkpoint(this);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  ActivateReadBarrierEntrypointsCallback callback(this);
  const size_t barrier_count = thread_list->RunCheckpoint(&checkpoint, &callback);
  // If there are no threads to wait which implies that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
  gc_barrier_->Increment(self, barrier_count);
}

void ConcurrentCopying::CreateInterRegionRefBitmaps() {
  DCHECK(use_generational_cc_);
  DCHECK(!region_space_inter_region_bitmap_.IsValid());
  DCHECK(!non_moving_space_inter_region_bitmap_.IsValid());
  DCHECK(region_space_ != nullptr);
  DCHECK(heap_->non_moving_space_ != nullptr);
  // Region-space
  region_space_inter_region_bitmap_ = accounting::ContinuousSpaceBitmap::Create(
      "region-space inter region ref bitmap",
      reinterpret_cast<uint8_t*>(region_space_->Begin()),
      region_space_->Limit() - region_space_->Begin());
  CHECK(region_space_inter_region_bitmap_.IsValid())
      << "Couldn't allocate region-space inter region ref bitmap";

  // non-moving-space
  non_moving_space_inter_region_bitmap_ = accounting::ContinuousSpaceBitmap::Create(
      "non-moving-space inter region ref bitmap",
      reinterpret_cast<uint8_t*>(heap_->non_moving_space_->Begin()),
      heap_->non_moving_space_->Limit() - heap_->non_moving_space_->Begin());
  CHECK(non_moving_space_inter_region_bitmap_.IsValid())
      << "Couldn't allocate non-moving-space inter region ref bitmap";
}

void ConcurrentCopying::BindBitmaps() {
  Thread* self = Thread::Current();
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  // Mark all of the spaces we never collect as immune.
  for (const auto& space : heap_->GetContinuousSpaces()) {
    if (space->GetGcRetentionPolicy() == space::kGcRetentionPolicyNeverCollect ||
        space->GetGcRetentionPolicy() == space::kGcRetentionPolicyFullCollect) {
      CHECK(space->IsZygoteSpace() || space->IsImageSpace());
      immune_spaces_.AddSpace(space);
    } else {
      CHECK(!space->IsZygoteSpace());
      CHECK(!space->IsImageSpace());
      CHECK(space == region_space_ || space == heap_->non_moving_space_);
      if (use_generational_cc_) {
        if (space == region_space_) {
          region_space_bitmap_ = region_space_->GetMarkBitmap();
        } else if (young_gen_ && space->IsContinuousMemMapAllocSpace()) {
          DCHECK_EQ(space->GetGcRetentionPolicy(), space::kGcRetentionPolicyAlwaysCollect);
          space->AsContinuousMemMapAllocSpace()->BindLiveToMarkBitmap();
        }
        if (young_gen_) {
          // Age all of the cards for the region space so that we know which evac regions to scan.
          heap_->GetCardTable()->ModifyCardsAtomic(space->Begin(),
                                                   space->End(),
                                                   AgeCardVisitor(),
                                                   VoidFunctor());
        } else {
          // In a full-heap GC cycle, the card-table corresponding to region-space and
          // non-moving space can be cleared, because this cycle only needs to
          // capture writes during the marking phase of this cycle to catch
          // objects that skipped marking due to heap mutation. Furthermore,
          // if the next GC is a young-gen cycle, then it only needs writes to
          // be captured after the thread-flip of this GC cycle, as that is when
          // the young-gen for the next GC cycle starts getting populated.
          heap_->GetCardTable()->ClearCardRange(space->Begin(), space->Limit());
        }
      } else {
        if (space == region_space_) {
          // It is OK to clear the bitmap with mutators running since the only place it is read is
          // VisitObjects which has exclusion with CC.
          region_space_bitmap_ = region_space_->GetMarkBitmap();
          region_space_bitmap_->Clear();
        }
      }
    }
  }
  if (use_generational_cc_ && young_gen_) {
    for (const auto& space : GetHeap()->GetDiscontinuousSpaces()) {
      CHECK(space->IsLargeObjectSpace());
      space->AsLargeObjectSpace()->CopyLiveToMarked();
    }
  }
}

void ConcurrentCopying::InitializePhase() {
  TimingLogger::ScopedTiming split("InitializePhase", GetTimings());
  num_bytes_allocated_before_gc_ = static_cast<int64_t>(heap_->GetBytesAllocated());
  if (kVerboseMode) {
    LOG(INFO) << "GC InitializePhase";
    LOG(INFO) << "Region-space : " << reinterpret_cast<void*>(region_space_->Begin()) << "-"
              << reinterpret_cast<void*>(region_space_->Limit());
  }
  CheckEmptyMarkStack();
  rb_mark_bit_stack_full_ = false;
  mark_from_read_barrier_measurements_ = measure_read_barrier_slow_path_;
  if (measure_read_barrier_slow_path_) {
    rb_slow_path_ns_.store(0, std::memory_order_relaxed);
    rb_slow_path_count_.store(0, std::memory_order_relaxed);
    rb_slow_path_count_gc_.store(0, std::memory_order_relaxed);
  }

  immune_spaces_.Reset();
  bytes_moved_.store(0, std::memory_order_relaxed);
  objects_moved_.store(0, std::memory_order_relaxed);
  bytes_moved_gc_thread_ = 0;
  objects_moved_gc_thread_ = 0;
  GcCause gc_cause = GetCurrentIteration()->GetGcCause();

  force_evacuate_all_ = false;
  if (!use_generational_cc_ || !young_gen_) {
    if (gc_cause == kGcCauseExplicit ||
        gc_cause == kGcCauseCollectorTransition ||
        GetCurrentIteration()->GetClearSoftReferences()) {
      force_evacuate_all_ = true;
    }
  }
  if (kUseBakerReadBarrier) {
    updated_all_immune_objects_.store(false, std::memory_order_relaxed);
    // GC may gray immune objects in the thread flip.
    gc_grays_immune_objects_ = true;
    if (kIsDebugBuild) {
      MutexLock mu(Thread::Current(), immune_gray_stack_lock_);
      DCHECK(immune_gray_stack_.empty());
    }
  }
  if (use_generational_cc_) {
    done_scanning_.store(false, std::memory_order_release);
  }
  BindBitmaps();
  if (kVerboseMode) {
    LOG(INFO) << "young_gen=" << std::boolalpha << young_gen_ << std::noboolalpha;
    LOG(INFO) << "force_evacuate_all=" << std::boolalpha << force_evacuate_all_ << std::noboolalpha;
    LOG(INFO) << "Largest immune region: " << immune_spaces_.GetLargestImmuneRegion().Begin()
              << "-" << immune_spaces_.GetLargestImmuneRegion().End();
    for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
      LOG(INFO) << "Immune space: " << *space;
    }
    LOG(INFO) << "GC end of InitializePhase";
  }
  if (use_generational_cc_ && !young_gen_) {
    region_space_bitmap_->Clear();
  }
  mark_stack_mode_.store(ConcurrentCopying::kMarkStackModeThreadLocal, std::memory_order_relaxed);
  // Mark all of the zygote large objects without graying them.
  MarkZygoteLargeObjects();
}

// Used to switch the thread roots of a thread from from-space refs to to-space refs.
class ConcurrentCopying::ThreadFlipVisitor : public Closure, public RootVisitor {
 public:
  ThreadFlipVisitor(ConcurrentCopying* concurrent_copying, bool use_tlab)
      : concurrent_copying_(concurrent_copying), use_tlab_(use_tlab) {
  }

  void Run(Thread* thread) override REQUIRES_SHARED(Locks::mutator_lock_) {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    CHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    thread->SetIsGcMarkingAndUpdateEntrypoints(true);
    if (use_tlab_ && thread->HasTlab()) {
      // We should not reuse the partially utilized TLABs revoked here as they
      // are going to be part of from-space.
      if (ConcurrentCopying::kEnableFromSpaceAccountingCheck) {
        // This must come before the revoke.
        size_t thread_local_objects = thread->GetThreadLocalObjectsAllocated();
        concurrent_copying_->region_space_->RevokeThreadLocalBuffers(thread, /*reuse=*/ false);
        reinterpret_cast<Atomic<size_t>*>(
            &concurrent_copying_->from_space_num_objects_at_first_pause_)->
                fetch_add(thread_local_objects, std::memory_order_relaxed);
      } else {
        concurrent_copying_->region_space_->RevokeThreadLocalBuffers(thread, /*reuse=*/ false);
      }
    }
    if (kUseThreadLocalAllocationStack) {
      thread->RevokeThreadLocalAllocationStack();
    }
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // We can use the non-CAS VisitRoots functions below because we update thread-local GC roots
    // only.
    thread->VisitRoots(this, kVisitRootFlagAllRoots);
    concurrent_copying_->GetBarrier().Pass(self);
  }

  void VisitRoots(mirror::Object*** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Thread* self = Thread::Current();
    for (size_t i = 0; i < count; ++i) {
      mirror::Object** root = roots[i];
      mirror::Object* ref = *root;
      if (ref != nullptr) {
        mirror::Object* to_ref = concurrent_copying_->Mark(self, ref);
        if (to_ref != ref) {
          *root = to_ref;
        }
      }
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Thread* self = Thread::Current();
    for (size_t i = 0; i < count; ++i) {
      mirror::CompressedReference<mirror::Object>* const root = roots[i];
      if (!root->IsNull()) {
        mirror::Object* ref = root->AsMirrorPtr();
        mirror::Object* to_ref = concurrent_copying_->Mark(self, ref);
        if (to_ref != ref) {
          root->Assign(to_ref);
        }
      }
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
  const bool use_tlab_;
};

// Called back from Runtime::FlipThreadRoots() during a pause.
class ConcurrentCopying::FlipCallback : public Closure {
 public:
  explicit FlipCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* thread) override REQUIRES(Locks::mutator_lock_) {
    ConcurrentCopying* cc = concurrent_copying_;
    TimingLogger::ScopedTiming split("(Paused)FlipCallback", cc->GetTimings());
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    if (kVerifyNoMissingCardMarks && cc->young_gen_) {
      cc->VerifyNoMissingCardMarks();
    }
    CHECK_EQ(thread, self);
    Locks::mutator_lock_->AssertExclusiveHeld(self);
    space::RegionSpace::EvacMode evac_mode = space::RegionSpace::kEvacModeLivePercentNewlyAllocated;
    if (cc->young_gen_) {
      CHECK(!cc->force_evacuate_all_);
      evac_mode = space::RegionSpace::kEvacModeNewlyAllocated;
    } else if (cc->force_evacuate_all_) {
      evac_mode = space::RegionSpace::kEvacModeForceAll;
    }
    {
      TimingLogger::ScopedTiming split2("(Paused)SetFromSpace", cc->GetTimings());
      // Only change live bytes for 1-phase full heap CC.
      cc->region_space_->SetFromSpace(
          cc->rb_table_,
          evac_mode,
          /*clear_live_bytes=*/ !cc->use_generational_cc_);
    }
    cc->SwapStacks();
    if (ConcurrentCopying::kEnableFromSpaceAccountingCheck) {
      cc->RecordLiveStackFreezeSize(self);
      cc->from_space_num_objects_at_first_pause_ = cc->region_space_->GetObjectsAllocated();
      cc->from_space_num_bytes_at_first_pause_ = cc->region_space_->GetBytesAllocated();
    }
    cc->is_marking_ = true;
    if (kIsDebugBuild && !cc->use_generational_cc_) {
      cc->region_space_->AssertAllRegionLiveBytesZeroOrCleared();
    }
    if (UNLIKELY(Runtime::Current()->IsActiveTransaction())) {
      CHECK(Runtime::Current()->IsAotCompiler());
      TimingLogger::ScopedTiming split3("(Paused)VisitTransactionRoots", cc->GetTimings());
      Runtime::Current()->VisitTransactionRoots(cc);
    }
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      cc->GrayAllNewlyDirtyImmuneObjects();
      if (kIsDebugBuild) {
        // Check that all non-gray immune objects only reference immune objects.
        cc->VerifyGrayImmuneObjects();
      }
    }
    // May be null during runtime creation, in this case leave java_lang_Object null.
    // This is safe since single threaded behavior should mean FillDummyObject does not
    // happen when java_lang_Object_ is null.
    if (WellKnownClasses::java_lang_Object != nullptr) {
      cc->java_lang_Object_ = down_cast<mirror::Class*>(cc->Mark(thread,
          WellKnownClasses::ToClass(WellKnownClasses::java_lang_Object).Ptr()));
    } else {
      cc->java_lang_Object_ = nullptr;
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::VerifyGrayImmuneObjectsVisitor {
 public:
  explicit VerifyGrayImmuneObjectsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj, MemberOffset offset, bool /* is_static */)
      const ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_) {
    CheckReference(obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset),
                   obj, offset);
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    CheckReference(ref->GetReferent<kWithoutReadBarrier>(),
                   ref,
                   mirror::Reference::ReferentOffset());
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CheckReference(root->AsMirrorPtr(), nullptr, MemberOffset(0));
  }

 private:
  ConcurrentCopying* const collector_;

  void CheckReference(ObjPtr<mirror::Object> ref,
                      ObjPtr<mirror::Object> holder,
                      MemberOffset offset) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (ref != nullptr) {
      if (!collector_->immune_spaces_.ContainsObject(ref.Ptr())) {
        // Not immune, must be a zygote large object.
        space::LargeObjectSpace* large_object_space =
            Runtime::Current()->GetHeap()->GetLargeObjectsSpace();
        CHECK(large_object_space->Contains(ref.Ptr()) &&
              large_object_space->IsZygoteLargeObject(Thread::Current(), ref.Ptr()))
            << "Non gray object references non immune, non zygote large object "<< ref << " "
            << mirror::Object::PrettyTypeOf(ref) << " in holder " << holder << " "
            << mirror::Object::PrettyTypeOf(holder) << " offset=" << offset.Uint32Value();
      } else {
        // Make sure the large object class is immune since we will never scan the large object.
        CHECK(collector_->immune_spaces_.ContainsObject(
            ref->GetClass<kVerifyNone, kWithoutReadBarrier>()));
      }
    }
  }
};

void ConcurrentCopying::VerifyGrayImmuneObjects() {
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  for (auto& space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
    VerifyGrayImmuneObjectsVisitor visitor(this);
    live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(space->Begin()),
                                  reinterpret_cast<uintptr_t>(space->Limit()),
                                  [&visitor](mirror::Object* obj)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      // If an object is not gray, it should only have references to things in the immune spaces.
      if (obj->GetReadBarrierState() != ReadBarrier::GrayState()) {
        obj->VisitReferences</*kVisitNativeRoots=*/true,
                             kDefaultVerifyFlags,
                             kWithoutReadBarrier>(visitor, visitor);
      }
    });
  }
}

class ConcurrentCopying::VerifyNoMissingCardMarkVisitor {
 public:
  VerifyNoMissingCardMarkVisitor(ConcurrentCopying* cc, ObjPtr<mirror::Object> holder)
    : cc_(cc),
      holder_(holder) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    if (offset.Uint32Value() != mirror::Object::ClassOffset().Uint32Value()) {
     CheckReference(obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(
         offset), offset.Uint32Value());
    }
  }
  void operator()(ObjPtr<mirror::Class> klass,
                  ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CheckReference(root->AsMirrorPtr());
  }

  void CheckReference(mirror::Object* ref, int32_t offset = -1) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (ref != nullptr && cc_->region_space_->IsInNewlyAllocatedRegion(ref)) {
      LOG(FATAL_WITHOUT_ABORT)
        << holder_->PrettyTypeOf() << "(" << holder_.Ptr() << ") references object "
        << ref->PrettyTypeOf() << "(" << ref << ") in newly allocated region at offset=" << offset;
      LOG(FATAL_WITHOUT_ABORT) << "time=" << cc_->region_space_->Time();
      constexpr const char* kIndent = "  ";
      LOG(FATAL_WITHOUT_ABORT) << cc_->DumpReferenceInfo(holder_.Ptr(), "holder_", kIndent);
      LOG(FATAL_WITHOUT_ABORT) << cc_->DumpReferenceInfo(ref, "ref", kIndent);
      LOG(FATAL) << "Unexpected reference to newly allocated region.";
    }
  }

 private:
  ConcurrentCopying* const cc_;
  const ObjPtr<mirror::Object> holder_;
};

void ConcurrentCopying::VerifyNoMissingCardMarks() {
  auto visitor = [&](mirror::Object* obj)
      REQUIRES(Locks::mutator_lock_)
      REQUIRES(!mark_stack_lock_) {
    // Objects on clean cards should never have references to newly allocated regions. Note
    // that aged cards are also not clean.
    if (heap_->GetCardTable()->GetCard(obj) == gc::accounting::CardTable::kCardClean) {
      VerifyNoMissingCardMarkVisitor internal_visitor(this, /*holder=*/ obj);
      obj->VisitReferences</*kVisitNativeRoots=*/true, kVerifyNone, kWithoutReadBarrier>(
          internal_visitor, internal_visitor);
    }
  };
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  region_space_->Walk(visitor);
  {
    ReaderMutexLock rmu(Thread::Current(), *Locks::heap_bitmap_lock_);
    heap_->GetLiveBitmap()->Visit(visitor);
  }
}

// Switch threads that from from-space to to-space refs. Forward/mark the thread roots.
void ConcurrentCopying::FlipThreadRoots() {
  TimingLogger::ScopedTiming split("FlipThreadRoots", GetTimings());
  if (kVerboseMode || heap_->dump_region_info_before_gc_) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
  }
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  gc_barrier_->Init(self, 0);
  ThreadFlipVisitor thread_flip_visitor(this, heap_->use_tlab_);
  FlipCallback flip_callback(this);

  size_t barrier_count = Runtime::Current()->GetThreadList()->FlipThreadRoots(
      &thread_flip_visitor, &flip_callback, this, GetHeap()->GetGcPauseListener());

  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  is_asserting_to_space_invariant_ = true;
  QuasiAtomic::ThreadFenceForConstructor();
  if (kVerboseMode) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
    LOG(INFO) << "GC end of FlipThreadRoots";
  }
}

template <bool kConcurrent>
class ConcurrentCopying::GrayImmuneObjectVisitor {
 public:
  explicit GrayImmuneObjectVisitor(Thread* self) : self_(self) {}

  ALWAYS_INLINE void operator()(mirror::Object* obj) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kUseBakerReadBarrier && obj->GetReadBarrierState() == ReadBarrier::NonGrayState()) {
      if (kConcurrent) {
        Locks::mutator_lock_->AssertSharedHeld(self_);
        obj->AtomicSetReadBarrierState(ReadBarrier::NonGrayState(), ReadBarrier::GrayState());
        // Mod union table VisitObjects may visit the same object multiple times so we can't check
        // the result of the atomic set.
      } else {
        Locks::mutator_lock_->AssertExclusiveHeld(self_);
        obj->SetReadBarrierState(ReadBarrier::GrayState());
      }
    }
  }

  static void Callback(mirror::Object* obj, void* arg) REQUIRES_SHARED(Locks::mutator_lock_) {
    reinterpret_cast<GrayImmuneObjectVisitor<kConcurrent>*>(arg)->operator()(obj);
  }

 private:
  Thread* const self_;
};

void ConcurrentCopying::GrayAllDirtyImmuneObjects() {
  TimingLogger::ScopedTiming split("GrayAllDirtyImmuneObjects", GetTimings());
  accounting::CardTable* const card_table = heap_->GetCardTable();
  Thread* const self = Thread::Current();
  using VisitorType = GrayImmuneObjectVisitor</* kIsConcurrent= */ true>;
  VisitorType visitor(self);
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
    // Mark all the objects on dirty cards since these may point to objects in other space.
    // Once these are marked, the GC will eventually clear them later.
    // Table is non null for boot image and zygote spaces. It is only null for application image
    // spaces.
    if (table != nullptr) {
      table->ProcessCards();
      table->VisitObjects(&VisitorType::Callback, &visitor);
      // Don't clear cards here since we need to rescan in the pause. If we cleared the cards here,
      // there would be races with the mutator marking new cards.
    } else {
      // Keep cards aged if we don't have a mod-union table since we may need to scan them in future
      // GCs. This case is for app images.
      card_table->ModifyCardsAtomic(
          space->Begin(),
          space->End(),
          [](uint8_t card) {
            return (card != gc::accounting::CardTable::kCardClean)
                ? gc::accounting::CardTable::kCardAged
                : card;
          },
          /* card modified visitor */ VoidFunctor());
      card_table->Scan</*kClearCard=*/ false>(space->GetMarkBitmap(),
                                              space->Begin(),
                                              space->End(),
                                              visitor,
                                              gc::accounting::CardTable::kCardAged);
    }
  }
}

void ConcurrentCopying::GrayAllNewlyDirtyImmuneObjects() {
  TimingLogger::ScopedTiming split("(Paused)GrayAllNewlyDirtyImmuneObjects", GetTimings());
  accounting::CardTable* const card_table = heap_->GetCardTable();
  using VisitorType = GrayImmuneObjectVisitor</* kIsConcurrent= */ false>;
  Thread* const self = Thread::Current();
  VisitorType visitor(self);
  WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
    DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
    accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);

    // Don't need to scan aged cards since we did these before the pause. Note that scanning cards
    // also handles the mod-union table cards.
    card_table->Scan</*kClearCard=*/ false>(space->GetMarkBitmap(),
                                            space->Begin(),
                                            space->End(),
                                            visitor,
                                            gc::accounting::CardTable::kCardDirty);
    if (table != nullptr) {
      // Add the cards to the mod-union table so that we can clear cards to save RAM.
      table->ProcessCards();
      TimingLogger::ScopedTiming split2("(Paused)ClearCards", GetTimings());
      card_table->ClearCardRange(space->Begin(),
                                 AlignDown(space->End(), accounting::CardTable::kCardSize));
    }
  }
  // Since all of the objects that may point to other spaces are gray, we can avoid all the read
  // barriers in the immune spaces.
  updated_all_immune_objects_.store(true, std::memory_order_relaxed);
}

void ConcurrentCopying::SwapStacks() {
  heap_->SwapStacks();
}

void ConcurrentCopying::RecordLiveStackFreezeSize(Thread* self) {
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  live_stack_freeze_size_ = heap_->GetLiveStack()->Size();
}

// Used to visit objects in the immune spaces.
inline void ConcurrentCopying::ScanImmuneObject(mirror::Object* obj) {
  DCHECK(obj != nullptr);
  DCHECK(immune_spaces_.ContainsObject(obj));
  // Update the fields without graying it or pushing it onto the mark stack.
  if (use_generational_cc_ && young_gen_) {
    // Young GC does not care about references to unevac space. It is safe to not gray these as
    // long as scan immune objects happens after scanning the dirty cards.
    Scan<true>(obj);
  } else {
    Scan<false>(obj);
  }
}

class ConcurrentCopying::ImmuneSpaceScanObjVisitor {
 public:
  explicit ImmuneSpaceScanObjVisitor(ConcurrentCopying* cc)
      : collector_(cc) {}

  ALWAYS_INLINE void operator()(mirror::Object* obj) const REQUIRES_SHARED(Locks::mutator_lock_) {
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      // Only need to scan gray objects.
      if (obj->GetReadBarrierState() == ReadBarrier::GrayState()) {
        collector_->ScanImmuneObject(obj);
        // Done scanning the object, go back to black (non-gray).
        bool success = obj->AtomicSetReadBarrierState(ReadBarrier::GrayState(),
                                                      ReadBarrier::NonGrayState());
        CHECK(success)
            << Runtime::Current()->GetHeap()->GetVerification()->DumpObjectInfo(obj, "failed CAS");
      }
    } else {
      collector_->ScanImmuneObject(obj);
    }
  }

  static void Callback(mirror::Object* obj, void* arg) REQUIRES_SHARED(Locks::mutator_lock_) {
    reinterpret_cast<ImmuneSpaceScanObjVisitor*>(arg)->operator()(obj);
  }

 private:
  ConcurrentCopying* const collector_;
};

template <bool kAtomicTestAndSet>
class ConcurrentCopying::CaptureRootsForMarkingVisitor : public RootVisitor {
 public:
  explicit CaptureRootsForMarkingVisitor(ConcurrentCopying* cc, Thread* self)
      : collector_(cc), self_(self) {}

  void VisitRoots(mirror::Object*** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      mirror::Object** root = roots[i];
      mirror::Object* ref = *root;
      if (ref != nullptr && !collector_->TestAndSetMarkBitForRef<kAtomicTestAndSet>(ref)) {
        collector_->PushOntoMarkStack(self_, ref);
      }
    }
  }

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info ATTRIBUTE_UNUSED) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    for (size_t i = 0; i < count; ++i) {
      mirror::CompressedReference<mirror::Object>* const root = roots[i];
      if (!root->IsNull()) {
        mirror::Object* ref = root->AsMirrorPtr();
        if (!collector_->TestAndSetMarkBitForRef<kAtomicTestAndSet>(ref)) {
          collector_->PushOntoMarkStack(self_, ref);
        }
      }
    }
  }

 private:
  ConcurrentCopying* const collector_;
  Thread* const self_;
};

void ConcurrentCopying::RemoveThreadMarkStackMapping(Thread* thread,
                                                     accounting::ObjectStack* tl_mark_stack) {
  CHECK(tl_mark_stack != nullptr);
  auto it = thread_mark_stack_map_.find(thread);
  CHECK(it != thread_mark_stack_map_.end());
  CHECK(it->second == tl_mark_stack);
  thread_mark_stack_map_.erase(it);
}

void ConcurrentCopying::AssertEmptyThreadMarkStackMap() {
  std::ostringstream oss;
  auto capture_mappings = [this, &oss] () REQUIRES(mark_stack_lock_) {
    for (const auto & iter : thread_mark_stack_map_) {
      oss << "thread:" << iter.first << " mark-stack:" << iter.second << "\n";
    }
    return oss.str();
  };
  CHECK(thread_mark_stack_map_.empty()) << "thread_mark_stack_map not empty. size:"
                                        << thread_mark_stack_map_.size()
                                        << "Mappings:\n"
                                        << capture_mappings()
                                        << "pooled_mark_stacks size:"
                                        << pooled_mark_stacks_.size();
}

void ConcurrentCopying::AssertNoThreadMarkStackMapping(Thread* thread) {
  MutexLock mu(Thread::Current(), mark_stack_lock_);
  CHECK(thread_mark_stack_map_.find(thread) == thread_mark_stack_map_.end());
}

void ConcurrentCopying::AddThreadMarkStackMapping(Thread* thread,
                                                  accounting::ObjectStack* tl_mark_stack) {
  CHECK(tl_mark_stack != nullptr);
  CHECK(thread_mark_stack_map_.find(thread) == thread_mark_stack_map_.end());
  thread_mark_stack_map_.insert({thread, tl_mark_stack});
}

class ConcurrentCopying::RevokeThreadLocalMarkStackCheckpoint : public Closure {
 public:
  RevokeThreadLocalMarkStackCheckpoint(ConcurrentCopying* concurrent_copying,
                                       bool disable_weak_ref_access)
      : concurrent_copying_(concurrent_copying),
        disable_weak_ref_access_(disable_weak_ref_access) {
  }

  void Run(Thread* thread) override NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* const self = Thread::Current();
    CHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Revoke thread local mark stacks.
    {
      MutexLock mu(self, concurrent_copying_->mark_stack_lock_);
      accounting::AtomicStack<mirror::Object>* tl_mark_stack = thread->GetThreadLocalMarkStack();
      if (tl_mark_stack != nullptr) {
        concurrent_copying_->revoked_mark_stacks_.push_back(tl_mark_stack);
        thread->SetThreadLocalMarkStack(nullptr);
        concurrent_copying_->RemoveThreadMarkStackMapping(thread, tl_mark_stack);
      }
    }
    // Disable weak ref access.
    if (disable_weak_ref_access_) {
      thread->SetWeakRefAccessEnabled(false);
    }
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 protected:
  ConcurrentCopying* const concurrent_copying_;

 private:
  const bool disable_weak_ref_access_;
};

class ConcurrentCopying::CaptureThreadRootsForMarkingAndCheckpoint :
  public RevokeThreadLocalMarkStackCheckpoint {
 public:
  explicit CaptureThreadRootsForMarkingAndCheckpoint(ConcurrentCopying* cc) :
    RevokeThreadLocalMarkStackCheckpoint(cc, /* disable_weak_ref_access */ false) {}

  void Run(Thread* thread) override
      REQUIRES_SHARED(Locks::mutator_lock_) {
    Thread* const self = Thread::Current();
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    // We can use the non-CAS VisitRoots functions below because we update thread-local GC roots
    // only.
    CaptureRootsForMarkingVisitor</*kAtomicTestAndSet*/ true> visitor(concurrent_copying_, self);
    thread->VisitRoots(&visitor, kVisitRootFlagAllRoots);
    // If thread_running_gc_ performed the root visit then its thread-local
    // mark-stack should be null as we directly push to gc_mark_stack_.
    CHECK(self == thread || self->GetThreadLocalMarkStack() == nullptr);
    // Barrier handling is done in the base class' Run() below.
    RevokeThreadLocalMarkStackCheckpoint::Run(thread);
  }
};

void ConcurrentCopying::CaptureThreadRootsForMarking() {
  TimingLogger::ScopedTiming split("CaptureThreadRootsForMarking", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
  }
  Thread* const self = Thread::Current();
  CaptureThreadRootsForMarkingAndCheckpoint check_point(this);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  size_t barrier_count = thread_list->RunCheckpoint(&check_point, /* callback */ nullptr);
  // If there are no threads to wait which implys that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  Locks::mutator_lock_->SharedUnlock(self);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  Locks::mutator_lock_->SharedLock(self);
  if (kVerboseMode) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
    LOG(INFO) << "GC end of CaptureThreadRootsForMarking";
  }
}

// Used to scan ref fields of an object.
template <bool kHandleInterRegionRefs>
class ConcurrentCopying::ComputeLiveBytesAndMarkRefFieldsVisitor {
 public:
  explicit ComputeLiveBytesAndMarkRefFieldsVisitor(ConcurrentCopying* collector,
                                                   size_t obj_region_idx)
      : collector_(collector),
      obj_region_idx_(obj_region_idx),
      contains_inter_region_idx_(false) {}

  void operator()(mirror::Object* obj, MemberOffset offset, bool /* is_static */) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_) {
    DCHECK_EQ(collector_->RegionSpace()->RegionIdxForRef(obj), obj_region_idx_);
    DCHECK(kHandleInterRegionRefs || collector_->immune_spaces_.ContainsObject(obj));
    CheckReference(obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(offset));
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    DCHECK(klass->IsTypeOfReferenceClass());
    // If the referent is not null, then we must re-visit the object during
    // copying phase to enqueue it for delayed processing and setting
    // read-barrier state to gray to ensure that call to GetReferent() triggers
    // the read-barrier. We use same data structure that is used to remember
    // objects with inter-region refs for this purpose too.
    if (kHandleInterRegionRefs
        && !contains_inter_region_idx_
        && ref->AsReference()->GetReferent<kWithoutReadBarrier>() != nullptr) {
      contains_inter_region_idx_ = true;
    }
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CheckReference(root->AsMirrorPtr());
  }

  bool ContainsInterRegionRefs() const ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_) {
    return contains_inter_region_idx_;
  }

 private:
  void CheckReference(mirror::Object* ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (ref == nullptr) {
      // Nothing to do.
      return;
    }
    if (!collector_->TestAndSetMarkBitForRef(ref)) {
      collector_->PushOntoLocalMarkStack(ref);
    }
    if (kHandleInterRegionRefs && !contains_inter_region_idx_) {
      size_t ref_region_idx = collector_->RegionSpace()->RegionIdxForRef(ref);
      // If a region-space object refers to an outside object, we will have a
      // mismatch of region idx, but the object need not be re-visited in
      // copying phase.
      if (ref_region_idx != static_cast<size_t>(-1) && obj_region_idx_ != ref_region_idx) {
        contains_inter_region_idx_ = true;
      }
    }
  }

  ConcurrentCopying* const collector_;
  const size_t obj_region_idx_;
  mutable bool contains_inter_region_idx_;
};

void ConcurrentCopying::AddLiveBytesAndScanRef(mirror::Object* ref) {
  DCHECK(ref != nullptr);
  DCHECK(!immune_spaces_.ContainsObject(ref));
  DCHECK(TestMarkBitmapForRef(ref));
  size_t obj_region_idx = static_cast<size_t>(-1);
  if (LIKELY(region_space_->HasAddress(ref))) {
    obj_region_idx = region_space_->RegionIdxForRefUnchecked(ref);
    // Add live bytes to the corresponding region
    if (!region_space_->IsRegionNewlyAllocated(obj_region_idx)) {
      // Newly Allocated regions are always chosen for evacuation. So no need
      // to update live_bytes_.
      size_t obj_size = ref->SizeOf<kDefaultVerifyFlags>();
      size_t alloc_size = RoundUp(obj_size, space::RegionSpace::kAlignment);
      region_space_->AddLiveBytes(ref, alloc_size);
    }
  }
  ComputeLiveBytesAndMarkRefFieldsVisitor</*kHandleInterRegionRefs*/ true>
      visitor(this, obj_region_idx);
  ref->VisitReferences</*kVisitNativeRoots=*/ true, kDefaultVerifyFlags, kWithoutReadBarrier>(
      visitor, visitor);
  // Mark the corresponding card dirty if the object contains any
  // inter-region reference.
  if (visitor.ContainsInterRegionRefs()) {
    if (obj_region_idx == static_cast<size_t>(-1)) {
      // If an inter-region ref has been found in a non-region-space, then it
      // must be non-moving-space. This is because this function cannot be
      // called on a immune-space object, and a large-object-space object has
      // only class object reference, which is either in some immune-space, or
      // in non-moving-space.
      DCHECK(heap_->non_moving_space_->HasAddress(ref));
      non_moving_space_inter_region_bitmap_.Set(ref);
    } else {
      region_space_inter_region_bitmap_.Set(ref);
    }
  }
}

template <bool kAtomic>
bool ConcurrentCopying::TestAndSetMarkBitForRef(mirror::Object* ref) {
  accounting::ContinuousSpaceBitmap* bitmap = nullptr;
  accounting::LargeObjectBitmap* los_bitmap = nullptr;
  if (LIKELY(region_space_->HasAddress(ref))) {
    bitmap = region_space_bitmap_;
  } else if (heap_->GetNonMovingSpace()->HasAddress(ref)) {
    bitmap = heap_->GetNonMovingSpace()->GetMarkBitmap();
  } else if (immune_spaces_.ContainsObject(ref)) {
    // References to immune space objects are always live.
    DCHECK(heap_mark_bitmap_->GetContinuousSpaceBitmap(ref)->Test(ref));
    return true;
  } else {
    // Should be a large object. Must be page aligned and the LOS must exist.
    if (kIsDebugBuild
        && (!IsAligned<kPageSize>(ref) || heap_->GetLargeObjectsSpace() == nullptr)) {
      // It must be heap corruption. Remove memory protection and dump data.
      region_space_->Unprotect();
      heap_->GetVerification()->LogHeapCorruption(/* obj */ nullptr,
                                                  MemberOffset(0),
                                                  ref,
                                                  /* fatal */ true);
    }
    los_bitmap = heap_->GetLargeObjectsSpace()->GetMarkBitmap();
  }
  if (kAtomic) {
    return (bitmap != nullptr) ? bitmap->AtomicTestAndSet(ref) : los_bitmap->AtomicTestAndSet(ref);
  } else {
    return (bitmap != nullptr) ? bitmap->Set(ref) : los_bitmap->Set(ref);
  }
}

bool ConcurrentCopying::TestMarkBitmapForRef(mirror::Object* ref) {
  if (LIKELY(region_space_->HasAddress(ref))) {
    return region_space_bitmap_->Test(ref);
  } else if (heap_->GetNonMovingSpace()->HasAddress(ref)) {
    return heap_->GetNonMovingSpace()->GetMarkBitmap()->Test(ref);
  } else if (immune_spaces_.ContainsObject(ref)) {
    // References to immune space objects are always live.
    DCHECK(heap_mark_bitmap_->GetContinuousSpaceBitmap(ref)->Test(ref));
    return true;
  } else {
    // Should be a large object. Must be page aligned and the LOS must exist.
    if (kIsDebugBuild
        && (!IsAligned<kPageSize>(ref) || heap_->GetLargeObjectsSpace() == nullptr)) {
      // It must be heap corruption. Remove memory protection and dump data.
      region_space_->Unprotect();
      heap_->GetVerification()->LogHeapCorruption(/* obj */ nullptr,
                                                  MemberOffset(0),
                                                  ref,
                                                  /* fatal */ true);
    }
    return heap_->GetLargeObjectsSpace()->GetMarkBitmap()->Test(ref);
  }
}

void ConcurrentCopying::PushOntoLocalMarkStack(mirror::Object* ref) {
  if (kIsDebugBuild) {
    Thread *self = Thread::Current();
    DCHECK_EQ(thread_running_gc_, self);
    DCHECK(self->GetThreadLocalMarkStack() == nullptr);
  }
  DCHECK_EQ(mark_stack_mode_.load(std::memory_order_relaxed), kMarkStackModeThreadLocal);
  if (UNLIKELY(gc_mark_stack_->IsFull())) {
    ExpandGcMarkStack();
  }
  gc_mark_stack_->PushBack(ref);
}

void ConcurrentCopying::ProcessMarkStackForMarkingAndComputeLiveBytes() {
  // Process thread-local mark stack containing thread roots
  ProcessThreadLocalMarkStacks(/* disable_weak_ref_access */ false,
                               /* checkpoint_callback */ nullptr,
                               [this] (mirror::Object* ref)
                                   REQUIRES_SHARED(Locks::mutator_lock_) {
                                 AddLiveBytesAndScanRef(ref);
                               });
  {
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    CHECK(revoked_mark_stacks_.empty());
    AssertEmptyThreadMarkStackMap();
    CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
  }

  while (!gc_mark_stack_->IsEmpty()) {
    mirror::Object* ref = gc_mark_stack_->PopBack();
    AddLiveBytesAndScanRef(ref);
  }
}

class ConcurrentCopying::ImmuneSpaceCaptureRefsVisitor {
 public:
  explicit ImmuneSpaceCaptureRefsVisitor(ConcurrentCopying* cc) : collector_(cc) {}

  ALWAYS_INLINE void operator()(mirror::Object* obj) const REQUIRES_SHARED(Locks::mutator_lock_) {
    ComputeLiveBytesAndMarkRefFieldsVisitor</*kHandleInterRegionRefs*/ false>
        visitor(collector_, /*obj_region_idx*/ static_cast<size_t>(-1));
    obj->VisitReferences</*kVisitNativeRoots=*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
        visitor, visitor);
  }

  static void Callback(mirror::Object* obj, void* arg) REQUIRES_SHARED(Locks::mutator_lock_) {
    reinterpret_cast<ImmuneSpaceCaptureRefsVisitor*>(arg)->operator()(obj);
  }

 private:
  ConcurrentCopying* const collector_;
};

/* Invariants for two-phase CC
 * ===========================
 * A) Definitions
 * ---------------
 * 1) Black: marked in bitmap, rb_state is non-gray, and not in mark stack
 * 2) Black-clean: marked in bitmap, and corresponding card is clean/aged
 * 3) Black-dirty: marked in bitmap, and corresponding card is dirty
 * 4) Gray: marked in bitmap, and exists in mark stack
 * 5) Gray-dirty: marked in bitmap, rb_state is gray, corresponding card is
 *    dirty, and exists in mark stack
 * 6) White: unmarked in bitmap, rb_state is non-gray, and not in mark stack
 *
 * B) Before marking phase
 * -----------------------
 * 1) All objects are white
 * 2) Cards are either clean or aged (cannot be asserted without a STW pause)
 * 3) Mark bitmap is cleared
 * 4) Mark stack is empty
 *
 * C) During marking phase
 * ------------------------
 * 1) If a black object holds an inter-region or white reference, then its
 *    corresponding card is dirty. In other words, it changes from being
 *    black-clean to black-dirty
 * 2) No black-clean object points to a white object
 *
 * D) After marking phase
 * -----------------------
 * 1) There are no gray objects
 * 2) All newly allocated objects are in from space
 * 3) No white object can be reachable, directly or otherwise, from a
 *    black-clean object
 *
 * E) During copying phase
 * ------------------------
 * 1) Mutators cannot observe white and black-dirty objects
 * 2) New allocations are in to-space (newly allocated regions are part of to-space)
 * 3) An object in mark stack must have its rb_state = Gray
 *
 * F) During card table scan
 * --------------------------
 * 1) Referents corresponding to root references are gray or in to-space
 * 2) Every path from an object that is read or written by a mutator during
 *    this period to a dirty black object goes through some gray object.
 *    Mutators preserve this by graying black objects as needed during this
 *    period. Ensures that a mutator never encounters a black dirty object.
 *
 * G) After card table scan
 * ------------------------
 * 1) There are no black-dirty objects
 * 2) Referents corresponding to root references are gray, black-clean or in
 *    to-space
 *
 * H) After copying phase
 * -----------------------
 * 1) Mark stack is empty
 * 2) No references into evacuated from-space
 * 3) No reference to an object which is unmarked and is also not in newly
 *    allocated region. In other words, no reference to white objects.
*/

void ConcurrentCopying::MarkingPhase() {
  TimingLogger::ScopedTiming split("MarkingPhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC MarkingPhase";
  }
  accounting::CardTable* const card_table = heap_->GetCardTable();
  Thread* const self = Thread::Current();
  CHECK_EQ(self, thread_running_gc_);
  // Clear live_bytes_ of every non-free region, except the ones that are newly
  // allocated.
  region_space_->SetAllRegionLiveBytesZero();
  if (kIsDebugBuild) {
    region_space_->AssertAllRegionLiveBytesZeroOrCleared();
  }
  // Scan immune spaces
  {
    TimingLogger::ScopedTiming split2("ScanImmuneSpaces", GetTimings());
    for (auto& space : immune_spaces_.GetSpaces()) {
      DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
      accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
      ImmuneSpaceCaptureRefsVisitor visitor(this);
      if (table != nullptr) {
        table->VisitObjects(ImmuneSpaceCaptureRefsVisitor::Callback, &visitor);
      } else {
        WriterMutexLock rmu(Thread::Current(), *Locks::heap_bitmap_lock_);
        card_table->Scan<false>(
            live_bitmap,
            space->Begin(),
            space->Limit(),
            visitor,
            accounting::CardTable::kCardDirty - 1);
      }
    }
  }
  // Scan runtime roots
  {
    TimingLogger::ScopedTiming split2("VisitConcurrentRoots", GetTimings());
    CaptureRootsForMarkingVisitor visitor(this, self);
    Runtime::Current()->VisitConcurrentRoots(&visitor, kVisitRootFlagAllRoots);
  }
  {
    // TODO: don't visit the transaction roots if it's not active.
    TimingLogger::ScopedTiming split2("VisitNonThreadRoots", GetTimings());
    CaptureRootsForMarkingVisitor visitor(this, self);
    Runtime::Current()->VisitNonThreadRoots(&visitor);
  }
  // Capture thread roots
  CaptureThreadRootsForMarking();
  // Process mark stack
  ProcessMarkStackForMarkingAndComputeLiveBytes();

  if (kVerboseMode) {
    LOG(INFO) << "GC end of MarkingPhase";
  }
}

template <bool kNoUnEvac>
void ConcurrentCopying::ScanDirtyObject(mirror::Object* obj) {
  Scan<kNoUnEvac>(obj);
  // Set the read-barrier state of a reference-type object to gray if its
  // referent is not marked yet. This is to ensure that if GetReferent() is
  // called, it triggers the read-barrier to process the referent before use.
  if (UNLIKELY((obj->GetClass<kVerifyNone, kWithoutReadBarrier>()->IsTypeOfReferenceClass()))) {
    mirror::Object* referent =
        obj->AsReference<kVerifyNone, kWithoutReadBarrier>()->GetReferent<kWithoutReadBarrier>();
    if (referent != nullptr && !IsInToSpace(referent)) {
      obj->AtomicSetReadBarrierState(ReadBarrier::NonGrayState(), ReadBarrier::GrayState());
    }
  }
}

// Concurrently mark roots that are guarded by read barriers and process the mark stack.
void ConcurrentCopying::CopyingPhase() {
  TimingLogger::ScopedTiming split("CopyingPhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC CopyingPhase";
  }
  Thread* self = Thread::Current();
  accounting::CardTable* const card_table = heap_->GetCardTable();
  if (kIsDebugBuild) {
    MutexLock mu(self, *Locks::thread_list_lock_);
    CHECK(weak_ref_access_enabled_);
  }

  // Scan immune spaces.
  // Update all the fields in the immune spaces first without graying the objects so that we
  // minimize dirty pages in the immune spaces. Note mutators can concurrently access and gray some
  // of the objects.
  if (kUseBakerReadBarrier) {
    gc_grays_immune_objects_ = false;
  }
  if (use_generational_cc_) {
    if (kVerboseMode) {
      LOG(INFO) << "GC ScanCardsForSpace";
    }
    TimingLogger::ScopedTiming split2("ScanCardsForSpace", GetTimings());
    WriterMutexLock rmu(Thread::Current(), *Locks::heap_bitmap_lock_);
    CHECK(!done_scanning_.load(std::memory_order_relaxed));
    if (kIsDebugBuild) {
      // Leave some time for mutators to race ahead to try and find races between the GC card
      // scanning and mutators reading references.
      usleep(10 * 1000);
    }
    for (space::ContinuousSpace* space : GetHeap()->GetContinuousSpaces()) {
      if (space->IsImageSpace() || space->IsZygoteSpace()) {
        // Image and zygote spaces are already handled since we gray the objects in the pause.
        continue;
      }
      // Scan all of the objects on dirty cards in unevac from space, and non moving space. These
      // are from previous GCs (or from marking phase of 2-phase full GC) and may reference things
      // in the from space.
      //
      // Note that we do not need to process the large-object space (the only discontinuous space)
      // as it contains only large string objects and large primitive array objects, that have no
      // reference to other objects, except their class. There is no need to scan these large
      // objects, as the String class and the primitive array classes are expected to never move
      // during a collection:
      // - In the case where we run with a boot image, these classes are part of the image space,
      //   which is an immune space.
      // - In the case where we run without a boot image, these classes are allocated in the
      //   non-moving space (see art::ClassLinker::InitWithoutImage).
      card_table->Scan<false>(
          space->GetMarkBitmap(),
          space->Begin(),
          space->End(),
          [this, space](mirror::Object* obj)
              REQUIRES(Locks::heap_bitmap_lock_)
              REQUIRES_SHARED(Locks::mutator_lock_) {
            // TODO: This code may be refactored to avoid scanning object while
            // done_scanning_ is false by setting rb_state to gray, and pushing the
            // object on mark stack. However, it will also require clearing the
            // corresponding mark-bit and, for region space objects,
            // decrementing the object's size from the corresponding region's
            // live_bytes.
            if (young_gen_) {
              // Don't push or gray unevac refs.
              if (kIsDebugBuild && space == region_space_) {
                // We may get unevac large objects.
                if (!region_space_->IsInUnevacFromSpace(obj)) {
                  CHECK(region_space_bitmap_->Test(obj));
                  region_space_->DumpRegionForObject(LOG_STREAM(FATAL_WITHOUT_ABORT), obj);
                  LOG(FATAL) << "Scanning " << obj << " not in unevac space";
                }
              }
              ScanDirtyObject</*kNoUnEvac*/ true>(obj);
            } else if (space != region_space_) {
              DCHECK(space == heap_->non_moving_space_);
              // We need to process un-evac references as they may be unprocessed,
              // if they skipped the marking phase due to heap mutation.
              ScanDirtyObject</*kNoUnEvac*/ false>(obj);
              non_moving_space_inter_region_bitmap_.Clear(obj);
            } else if (region_space_->IsInUnevacFromSpace(obj)) {
              ScanDirtyObject</*kNoUnEvac*/ false>(obj);
              region_space_inter_region_bitmap_.Clear(obj);
            }
          },
          accounting::CardTable::kCardAged);

      if (!young_gen_) {
        auto visitor = [this](mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_) {
                         // We don't need to process un-evac references as any unprocessed
                         // ones will be taken care of in the card-table scan above.
                         ScanDirtyObject</*kNoUnEvac*/ true>(obj);
                       };
        if (space == region_space_) {
          region_space_->ScanUnevacFromSpace(&region_space_inter_region_bitmap_, visitor);
        } else {
          DCHECK(space == heap_->non_moving_space_);
          non_moving_space_inter_region_bitmap_.VisitMarkedRange(
              reinterpret_cast<uintptr_t>(space->Begin()),
              reinterpret_cast<uintptr_t>(space->End()),
              visitor);
        }
      }
    }
    // Done scanning unevac space.
    done_scanning_.store(true, std::memory_order_release);
    // NOTE: inter-region-ref bitmaps can be cleared here to release memory, if needed.
    // Currently we do it in ReclaimPhase().
    if (kVerboseMode) {
      LOG(INFO) << "GC end of ScanCardsForSpace";
    }
  }
  {
    // For a sticky-bit collection, this phase needs to be after the card scanning since the
    // mutator may read an unevac space object out of an image object. If the image object is no
    // longer gray it will trigger a read barrier for the unevac space object.
    TimingLogger::ScopedTiming split2("ScanImmuneSpaces", GetTimings());
    for (auto& space : immune_spaces_.GetSpaces()) {
      DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
      accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
      accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
      ImmuneSpaceScanObjVisitor visitor(this);
      if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects && table != nullptr) {
        table->VisitObjects(ImmuneSpaceScanObjVisitor::Callback, &visitor);
      } else {
        WriterMutexLock rmu(Thread::Current(), *Locks::heap_bitmap_lock_);
        card_table->Scan<false>(
            live_bitmap,
            space->Begin(),
            space->Limit(),
            visitor,
            accounting::CardTable::kCardDirty - 1);
      }
    }
  }
  if (kUseBakerReadBarrier) {
    // This release fence makes the field updates in the above loop visible before allowing mutator
    // getting access to immune objects without graying it first.
    updated_all_immune_objects_.store(true, std::memory_order_release);
    // Now "un-gray" (conceptually blacken) immune objects concurrently accessed and grayed by
    // mutators. We can't do this in the above loop because we would incorrectly disable the read
    // barrier by un-graying (conceptually blackening) an object which may point to an unscanned,
    // white object, breaking the to-space invariant (a mutator shall never observe a from-space
    // (white) object).
    //
    // Make sure no mutators are in the middle of marking an immune object before un-graying
    // (blackening) immune objects.
    IssueEmptyCheckpoint();
    MutexLock mu(Thread::Current(), immune_gray_stack_lock_);
    if (kVerboseMode) {
      LOG(INFO) << "immune gray stack size=" << immune_gray_stack_.size();
    }
    for (mirror::Object* obj : immune_gray_stack_) {
      DCHECK_EQ(obj->GetReadBarrierState(), ReadBarrier::GrayState());
      bool success = obj->AtomicSetReadBarrierState(ReadBarrier::GrayState(),
                                                    ReadBarrier::NonGrayState());
      DCHECK(success);
    }
    immune_gray_stack_.clear();
  }

  {
    TimingLogger::ScopedTiming split2("VisitConcurrentRoots", GetTimings());
    Runtime::Current()->VisitConcurrentRoots(this, kVisitRootFlagAllRoots);
  }
  {
    // TODO: don't visit the transaction roots if it's not active.
    TimingLogger::ScopedTiming split5("VisitNonThreadRoots", GetTimings());
    Runtime::Current()->VisitNonThreadRoots(this);
  }

  {
    TimingLogger::ScopedTiming split7("ProcessMarkStack", GetTimings());
    // We transition through three mark stack modes (thread-local, shared, GC-exclusive). The
    // primary reasons are the fact that we need to use a checkpoint to process thread-local mark
    // stacks, but after we disable weak refs accesses, we can't use a checkpoint due to a deadlock
    // issue because running threads potentially blocking at WaitHoldingLocks, and that once we
    // reach the point where we process weak references, we can avoid using a lock when accessing
    // the GC mark stack, which makes mark stack processing more efficient.

    // Process the mark stack once in the thread local stack mode. This marks most of the live
    // objects, aside from weak ref accesses with read barriers (Reference::GetReferent() and system
    // weaks) that may happen concurrently while we processing the mark stack and newly mark/gray
    // objects and push refs on the mark stack.
    ProcessMarkStack();
    // Switch to the shared mark stack mode. That is, revoke and process thread-local mark stacks
    // for the last time before transitioning to the shared mark stack mode, which would process new
    // refs that may have been concurrently pushed onto the mark stack during the ProcessMarkStack()
    // call above. At the same time, disable weak ref accesses using a per-thread flag. It's
    // important to do these together in a single checkpoint so that we can ensure that mutators
    // won't newly gray objects and push new refs onto the mark stack due to weak ref accesses and
    // mutators safely transition to the shared mark stack mode (without leaving unprocessed refs on
    // the thread-local mark stacks), without a race. This is why we use a thread-local weak ref
    // access flag Thread::tls32_.weak_ref_access_enabled_ instead of the global ones.
    SwitchToSharedMarkStackMode();
    CHECK(!self->GetWeakRefAccessEnabled());
    // Now that weak refs accesses are disabled, once we exhaust the shared mark stack again here
    // (which may be non-empty if there were refs found on thread-local mark stacks during the above
    // SwitchToSharedMarkStackMode() call), we won't have new refs to process, that is, mutators
    // (via read barriers) have no way to produce any more refs to process. Marking converges once
    // before we process weak refs below.
    ProcessMarkStack();
    CheckEmptyMarkStack();
    // Switch to the GC exclusive mark stack mode so that we can process the mark stack without a
    // lock from this point on.
    SwitchToGcExclusiveMarkStackMode();
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "ProcessReferences";
    }
    // Process weak references. This may produce new refs to process and have them processed via
    // ProcessMarkStack (in the GC exclusive mark stack mode).
    ProcessReferences(self);
    CheckEmptyMarkStack();
    if (kVerboseMode) {
      LOG(INFO) << "SweepSystemWeaks";
    }
    SweepSystemWeaks(self);
    if (kVerboseMode) {
      LOG(INFO) << "SweepSystemWeaks done";
    }
    // Process the mark stack here one last time because the above SweepSystemWeaks() call may have
    // marked some objects (strings alive) as hash_set::Erase() can call the hash function for
    // arbitrary elements in the weak intern table in InternTable::Table::SweepWeaks().
    ProcessMarkStack();
    CheckEmptyMarkStack();
    // Re-enable weak ref accesses.
    ReenableWeakRefAccess(self);
    // Free data for class loaders that we unloaded.
    Runtime::Current()->GetClassLinker()->CleanupClassLoaders();
    // Marking is done. Disable marking.
    DisableMarking();
    CheckEmptyMarkStack();
  }

  if (kIsDebugBuild) {
    MutexLock mu(self, *Locks::thread_list_lock_);
    CHECK(weak_ref_access_enabled_);
  }
  if (kVerboseMode) {
    LOG(INFO) << "GC end of CopyingPhase";
  }
}

void ConcurrentCopying::ReenableWeakRefAccess(Thread* self) {
  if (kVerboseMode) {
    LOG(INFO) << "ReenableWeakRefAccess";
  }
  // Iterate all threads (don't need to or can't use a checkpoint) and re-enable weak ref access.
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    weak_ref_access_enabled_ = true;  // This is for new threads.
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      thread->SetWeakRefAccessEnabled(true);
    }
  }
  // Unblock blocking threads.
  GetHeap()->GetReferenceProcessor()->BroadcastForSlowPath(self);
  Runtime::Current()->BroadcastForNewSystemWeaks();
}

class ConcurrentCopying::DisableMarkingCheckpoint : public Closure {
 public:
  explicit DisableMarkingCheckpoint(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* thread) override NO_THREAD_SAFETY_ANALYSIS {
    // Note: self is not necessarily equal to thread since thread may be suspended.
    Thread* self = Thread::Current();
    DCHECK(thread == self || thread->IsSuspended() || thread->GetState() == kWaitingPerformingGc)
        << thread->GetState() << " thread " << thread << " self " << self;
    // Disable the thread-local is_gc_marking flag.
    // Note a thread that has just started right before this checkpoint may have already this flag
    // set to false, which is ok.
    thread->SetIsGcMarkingAndUpdateEntrypoints(false);
    // If thread is a running mutator, then act on behalf of the garbage collector.
    // See the code in ThreadList::RunCheckpoint.
    concurrent_copying_->GetBarrier().Pass(self);
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

class ConcurrentCopying::DisableMarkingCallback : public Closure {
 public:
  explicit DisableMarkingCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* self ATTRIBUTE_UNUSED) override REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a race with ThreadList::Register().
    CHECK(concurrent_copying_->is_marking_);
    concurrent_copying_->is_marking_ = false;
    if (kUseBakerReadBarrier && kGrayDirtyImmuneObjects) {
      CHECK(concurrent_copying_->is_using_read_barrier_entrypoints_);
      concurrent_copying_->is_using_read_barrier_entrypoints_ = false;
    } else {
      CHECK(!concurrent_copying_->is_using_read_barrier_entrypoints_);
    }
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::IssueDisableMarkingCheckpoint() {
  Thread* self = Thread::Current();
  DisableMarkingCheckpoint check_point(this);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  DisableMarkingCallback dmc(this);
  size_t barrier_count = thread_list->RunCheckpoint(&check_point, &dmc);
  // If there are no threads to wait which implies that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  // Release locks then wait for all mutator threads to pass the barrier.
  Locks::mutator_lock_->SharedUnlock(self);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::DisableMarking() {
  // Use a checkpoint to turn off the global is_marking and the thread-local is_gc_marking flags and
  // to ensure no threads are still in the middle of a read barrier which may have a from-space ref
  // cached in a local variable.
  IssueDisableMarkingCheckpoint();
  if (kUseTableLookupReadBarrier) {
    heap_->rb_table_->ClearAll();
    DCHECK(heap_->rb_table_->IsAllCleared());
  }
  is_mark_stack_push_disallowed_.store(1, std::memory_order_seq_cst);
  mark_stack_mode_.store(kMarkStackModeOff, std::memory_order_seq_cst);
}

void ConcurrentCopying::IssueEmptyCheckpoint() {
  Thread* self = Thread::Current();
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  // Release locks then wait for all mutator threads to pass the barrier.
  Locks::mutator_lock_->SharedUnlock(self);
  thread_list->RunEmptyCheckpoint();
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::ExpandGcMarkStack() {
  DCHECK(gc_mark_stack_->IsFull());
  const size_t new_size = gc_mark_stack_->Capacity() * 2;
  std::vector<StackReference<mirror::Object>> temp(gc_mark_stack_->Begin(),
                                                   gc_mark_stack_->End());
  gc_mark_stack_->Resize(new_size);
  for (auto& ref : temp) {
    gc_mark_stack_->PushBack(ref.AsMirrorPtr());
  }
  DCHECK(!gc_mark_stack_->IsFull());
}

void ConcurrentCopying::PushOntoMarkStack(Thread* const self, mirror::Object* to_ref) {
  CHECK_EQ(is_mark_stack_push_disallowed_.load(std::memory_order_relaxed), 0)
      << " " << to_ref << " " << mirror::Object::PrettyTypeOf(to_ref);
  CHECK(thread_running_gc_ != nullptr);
  MarkStackMode mark_stack_mode = mark_stack_mode_.load(std::memory_order_relaxed);
  if (LIKELY(mark_stack_mode == kMarkStackModeThreadLocal)) {
    if (LIKELY(self == thread_running_gc_)) {
      // If GC-running thread, use the GC mark stack instead of a thread-local mark stack.
      CHECK(self->GetThreadLocalMarkStack() == nullptr);
      if (UNLIKELY(gc_mark_stack_->IsFull())) {
        ExpandGcMarkStack();
      }
      gc_mark_stack_->PushBack(to_ref);
    } else {
      // Otherwise, use a thread-local mark stack.
      accounting::AtomicStack<mirror::Object>* tl_mark_stack = self->GetThreadLocalMarkStack();
      if (UNLIKELY(tl_mark_stack == nullptr || tl_mark_stack->IsFull())) {
        MutexLock mu(self, mark_stack_lock_);
        // Get a new thread local mark stack.
        accounting::AtomicStack<mirror::Object>* new_tl_mark_stack;
        if (!pooled_mark_stacks_.empty()) {
          // Use a pooled mark stack.
          new_tl_mark_stack = pooled_mark_stacks_.back();
          pooled_mark_stacks_.pop_back();
        } else {
          // None pooled. Create a new one.
          new_tl_mark_stack =
              accounting::AtomicStack<mirror::Object>::Create(
                  "thread local mark stack", 4 * KB, 4 * KB);
        }
        DCHECK(new_tl_mark_stack != nullptr);
        DCHECK(new_tl_mark_stack->IsEmpty());
        new_tl_mark_stack->PushBack(to_ref);
        self->SetThreadLocalMarkStack(new_tl_mark_stack);
        if (tl_mark_stack != nullptr) {
          // Store the old full stack into a vector.
          revoked_mark_stacks_.push_back(tl_mark_stack);
          RemoveThreadMarkStackMapping(self, tl_mark_stack);
        }
        AddThreadMarkStackMapping(self, new_tl_mark_stack);
      } else {
        tl_mark_stack->PushBack(to_ref);
      }
    }
  } else if (mark_stack_mode == kMarkStackModeShared) {
    // Access the shared GC mark stack with a lock.
    MutexLock mu(self, mark_stack_lock_);
    if (UNLIKELY(gc_mark_stack_->IsFull())) {
      ExpandGcMarkStack();
    }
    gc_mark_stack_->PushBack(to_ref);
  } else {
    CHECK_EQ(static_cast<uint32_t>(mark_stack_mode),
             static_cast<uint32_t>(kMarkStackModeGcExclusive))
        << "ref=" << to_ref
        << " self->gc_marking=" << self->GetIsGcMarking()
        << " cc->is_marking=" << is_marking_;
    CHECK(self == thread_running_gc_)
        << "Only GC-running thread should access the mark stack "
        << "in the GC exclusive mark stack mode";
    // Access the GC mark stack without a lock.
    if (UNLIKELY(gc_mark_stack_->IsFull())) {
      ExpandGcMarkStack();
    }
    gc_mark_stack_->PushBack(to_ref);
  }
}

accounting::ObjectStack* ConcurrentCopying::GetAllocationStack() {
  return heap_->allocation_stack_.get();
}

accounting::ObjectStack* ConcurrentCopying::GetLiveStack() {
  return heap_->live_stack_.get();
}

// The following visitors are used to verify that there's no references to the from-space left after
// marking.
class ConcurrentCopying::VerifyNoFromSpaceRefsVisitor : public SingleRootVisitor {
 public:
  explicit VerifyNoFromSpaceRefsVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(mirror::Object* ref,
                  MemberOffset offset = MemberOffset(0),
                  mirror::Object* holder = nullptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    if (ref == nullptr) {
      // OK.
      return;
    }
    collector_->AssertToSpaceInvariant(holder, offset, ref);
    if (kUseBakerReadBarrier) {
      CHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::NonGrayState())
          << "Ref " << ref << " " << ref->PrettyTypeOf() << " has gray rb_state";
    }
  }

  void VisitRoot(mirror::Object* root, const RootInfo& info ATTRIBUTE_UNUSED)
      override REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(root != nullptr);
    operator()(root);
  }

 private:
  ConcurrentCopying* const collector_;
};

class ConcurrentCopying::VerifyNoFromSpaceRefsFieldVisitor {
 public:
  explicit VerifyNoFromSpaceRefsFieldVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(offset);
    VerifyNoFromSpaceRefsVisitor visitor(collector_);
    visitor(ref, offset, obj.Ptr());
  }
  void operator()(ObjPtr<mirror::Class> klass,
                  ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    this->operator()(ref, mirror::Reference::ReferentOffset(), false);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    VerifyNoFromSpaceRefsVisitor visitor(collector_);
    visitor(root->AsMirrorPtr());
  }

 private:
  ConcurrentCopying* const collector_;
};

// Verify there's no from-space references left after the marking phase.
void ConcurrentCopying::VerifyNoFromSpaceReferences() {
  Thread* self = Thread::Current();
  DCHECK(Locks::mutator_lock_->IsExclusiveHeld(self));
  // Verify all threads have is_gc_marking to be false
  {
    MutexLock mu(self, *Locks::thread_list_lock_);
    std::list<Thread*> thread_list = Runtime::Current()->GetThreadList()->GetList();
    for (Thread* thread : thread_list) {
      CHECK(!thread->GetIsGcMarking());
    }
  }

  auto verify_no_from_space_refs_visitor = [&](mirror::Object* obj)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    CHECK(obj != nullptr);
    space::RegionSpace* region_space = RegionSpace();
    CHECK(!region_space->IsInFromSpace(obj)) << "Scanning object " << obj << " in from space";
    VerifyNoFromSpaceRefsFieldVisitor visitor(this);
    obj->VisitReferences</*kVisitNativeRoots=*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
        visitor,
        visitor);
    if (kUseBakerReadBarrier) {
      CHECK_EQ(obj->GetReadBarrierState(), ReadBarrier::NonGrayState())
          << "obj=" << obj << " has gray rb_state " << obj->GetReadBarrierState();
    }
  };
  // Roots.
  {
    ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
    VerifyNoFromSpaceRefsVisitor ref_visitor(this);
    Runtime::Current()->VisitRoots(&ref_visitor);
  }
  // The to-space.
  region_space_->WalkToSpace(verify_no_from_space_refs_visitor);
  // Non-moving spaces.
  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    heap_->GetMarkBitmap()->Visit(verify_no_from_space_refs_visitor);
  }
  // The alloc stack.
  {
    VerifyNoFromSpaceRefsVisitor ref_visitor(this);
    for (auto* it = heap_->allocation_stack_->Begin(), *end = heap_->allocation_stack_->End();
        it < end; ++it) {
      mirror::Object* const obj = it->AsMirrorPtr();
      if (obj != nullptr && obj->GetClass() != nullptr) {
        // TODO: need to call this only if obj is alive?
        ref_visitor(obj);
        verify_no_from_space_refs_visitor(obj);
      }
    }
  }
  // TODO: LOS. But only refs in LOS are classes.
}

// The following visitors are used to assert the to-space invariant.
class ConcurrentCopying::AssertToSpaceInvariantFieldVisitor {
 public:
  explicit AssertToSpaceInvariantFieldVisitor(ConcurrentCopying* collector)
      : collector_(collector) {}

  void operator()(ObjPtr<mirror::Object> obj,
                  MemberOffset offset,
                  bool is_static ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    mirror::Object* ref =
        obj->GetFieldObject<mirror::Object, kDefaultVerifyFlags, kWithoutReadBarrier>(offset);
    collector_->AssertToSpaceInvariant(obj.Ptr(), offset, ref);
  }
  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    mirror::Object* ref = root->AsMirrorPtr();
    collector_->AssertToSpaceInvariant(/* obj */ nullptr, MemberOffset(0), ref);
  }

 private:
  ConcurrentCopying* const collector_;
};

void ConcurrentCopying::RevokeThreadLocalMarkStacks(bool disable_weak_ref_access,
                                                    Closure* checkpoint_callback) {
  Thread* self = Thread::Current();
  RevokeThreadLocalMarkStackCheckpoint check_point(this, disable_weak_ref_access);
  ThreadList* thread_list = Runtime::Current()->GetThreadList();
  gc_barrier_->Init(self, 0);
  size_t barrier_count = thread_list->RunCheckpoint(&check_point, checkpoint_callback);
  // If there are no threads to wait which implys that all the checkpoint functions are finished,
  // then no need to release the mutator lock.
  if (barrier_count == 0) {
    return;
  }
  Locks::mutator_lock_->SharedUnlock(self);
  {
    ScopedThreadStateChange tsc(self, kWaitingForCheckPointsToRun);
    gc_barrier_->Increment(self, barrier_count);
  }
  Locks::mutator_lock_->SharedLock(self);
}

void ConcurrentCopying::RevokeThreadLocalMarkStack(Thread* thread) {
  Thread* self = Thread::Current();
  CHECK_EQ(self, thread);
  MutexLock mu(self, mark_stack_lock_);
  accounting::AtomicStack<mirror::Object>* tl_mark_stack = thread->GetThreadLocalMarkStack();
  if (tl_mark_stack != nullptr) {
    CHECK(is_marking_);
    revoked_mark_stacks_.push_back(tl_mark_stack);
    RemoveThreadMarkStackMapping(thread, tl_mark_stack);
    thread->SetThreadLocalMarkStack(nullptr);
  }
}

void ConcurrentCopying::ProcessMarkStack() {
  if (kVerboseMode) {
    LOG(INFO) << "ProcessMarkStack. ";
  }
  bool empty_prev = false;
  while (true) {
    bool empty = ProcessMarkStackOnce();
    if (empty_prev && empty) {
      // Saw empty mark stack for a second time, done.
      break;
    }
    empty_prev = empty;
  }
}

bool ConcurrentCopying::ProcessMarkStackOnce() {
  DCHECK(thread_running_gc_ != nullptr);
  Thread* const self = Thread::Current();
  DCHECK(self == thread_running_gc_);
  DCHECK(thread_running_gc_->GetThreadLocalMarkStack() == nullptr);
  size_t count = 0;
  MarkStackMode mark_stack_mode = mark_stack_mode_.load(std::memory_order_relaxed);
  if (mark_stack_mode == kMarkStackModeThreadLocal) {
    // Process the thread-local mark stacks and the GC mark stack.
    count += ProcessThreadLocalMarkStacks(/* disable_weak_ref_access= */ false,
                                          /* checkpoint_callback= */ nullptr,
                                          [this] (mirror::Object* ref)
                                              REQUIRES_SHARED(Locks::mutator_lock_) {
                                            ProcessMarkStackRef(ref);
                                          });
    while (!gc_mark_stack_->IsEmpty()) {
      mirror::Object* to_ref = gc_mark_stack_->PopBack();
      ProcessMarkStackRef(to_ref);
      ++count;
    }
    gc_mark_stack_->Reset();
  } else if (mark_stack_mode == kMarkStackModeShared) {
    // Do an empty checkpoint to avoid a race with a mutator preempted in the middle of a read
    // barrier but before pushing onto the mark stack. b/32508093. Note the weak ref access is
    // disabled at this point.
    IssueEmptyCheckpoint();
    // Process the shared GC mark stack with a lock.
    {
      MutexLock mu(thread_running_gc_, mark_stack_lock_);
      CHECK(revoked_mark_stacks_.empty());
      AssertEmptyThreadMarkStackMap();
      CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
    }
    while (true) {
      std::vector<mirror::Object*> refs;
      {
        // Copy refs with lock. Note the number of refs should be small.
        MutexLock mu(thread_running_gc_, mark_stack_lock_);
        if (gc_mark_stack_->IsEmpty()) {
          break;
        }
        for (StackReference<mirror::Object>* p = gc_mark_stack_->Begin();
             p != gc_mark_stack_->End(); ++p) {
          refs.push_back(p->AsMirrorPtr());
        }
        gc_mark_stack_->Reset();
      }
      for (mirror::Object* ref : refs) {
        ProcessMarkStackRef(ref);
        ++count;
      }
    }
  } else {
    CHECK_EQ(static_cast<uint32_t>(mark_stack_mode),
             static_cast<uint32_t>(kMarkStackModeGcExclusive));
    {
      MutexLock mu(thread_running_gc_, mark_stack_lock_);
      CHECK(revoked_mark_stacks_.empty());
      AssertEmptyThreadMarkStackMap();
      CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
    }
    // Process the GC mark stack in the exclusive mode. No need to take the lock.
    while (!gc_mark_stack_->IsEmpty()) {
      mirror::Object* to_ref = gc_mark_stack_->PopBack();
      ProcessMarkStackRef(to_ref);
      ++count;
    }
    gc_mark_stack_->Reset();
  }

  // Return true if the stack was empty.
  return count == 0;
}

template <typename Processor>
size_t ConcurrentCopying::ProcessThreadLocalMarkStacks(bool disable_weak_ref_access,
                                                       Closure* checkpoint_callback,
                                                       const Processor& processor) {
  // Run a checkpoint to collect all thread local mark stacks and iterate over them all.
  RevokeThreadLocalMarkStacks(disable_weak_ref_access, checkpoint_callback);
  if (disable_weak_ref_access) {
    CHECK_EQ(static_cast<uint32_t>(mark_stack_mode_.load(std::memory_order_relaxed)),
             static_cast<uint32_t>(kMarkStackModeShared));
    // From this point onwards no mutator should require a thread-local mark
    // stack.
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    AssertEmptyThreadMarkStackMap();
  }
  size_t count = 0;
  std::vector<accounting::AtomicStack<mirror::Object>*> mark_stacks;
  {
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    // Make a copy of the mark stack vector.
    mark_stacks = revoked_mark_stacks_;
    revoked_mark_stacks_.clear();
  }
  for (accounting::AtomicStack<mirror::Object>* mark_stack : mark_stacks) {
    for (StackReference<mirror::Object>* p = mark_stack->Begin(); p != mark_stack->End(); ++p) {
      mirror::Object* to_ref = p->AsMirrorPtr();
      processor(to_ref);
      ++count;
    }
    {
      MutexLock mu(thread_running_gc_, mark_stack_lock_);
      if (pooled_mark_stacks_.size() >= kMarkStackPoolSize) {
        // The pool has enough. Delete it.
        delete mark_stack;
      } else {
        // Otherwise, put it into the pool for later reuse.
        mark_stack->Reset();
        pooled_mark_stacks_.push_back(mark_stack);
      }
    }
  }
  if (disable_weak_ref_access) {
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    CHECK(revoked_mark_stacks_.empty());
    CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
  }
  return count;
}

inline void ConcurrentCopying::ProcessMarkStackRef(mirror::Object* to_ref) {
  DCHECK(!region_space_->IsInFromSpace(to_ref));
  space::RegionSpace::RegionType rtype = region_space_->GetRegionType(to_ref);
  if (kUseBakerReadBarrier) {
    DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState())
        << " to_ref=" << to_ref
        << " rb_state=" << to_ref->GetReadBarrierState()
        << " is_marked=" << IsMarked(to_ref)
        << " type=" << to_ref->PrettyTypeOf()
        << " young_gen=" << std::boolalpha << young_gen_ << std::noboolalpha
        << " space=" << heap_->DumpSpaceNameFromAddress(to_ref)
        << " region_type=" << rtype
        // TODO: Temporary; remove this when this is no longer needed (b/116087961).
        << " runtime->sentinel=" << Runtime::Current()->GetSentinel().Read<kWithoutReadBarrier>();
  }
  bool add_to_live_bytes = false;
  // Invariant: There should be no object from a newly-allocated
  // region (either large or non-large) on the mark stack.
  DCHECK(!region_space_->IsInNewlyAllocatedRegion(to_ref)) << to_ref;
  bool perform_scan = false;
  switch (rtype) {
    case space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace:
      // Mark the bitmap only in the GC thread here so that we don't need a CAS.
      if (!kUseBakerReadBarrier || !region_space_bitmap_->Set(to_ref)) {
        // It may be already marked if we accidentally pushed the same object twice due to the racy
        // bitmap read in MarkUnevacFromSpaceRegion.
        if (use_generational_cc_ && young_gen_) {
          CHECK(region_space_->IsLargeObject(to_ref));
          region_space_->ZeroLiveBytesForLargeObject(to_ref);
        }
        perform_scan = true;
        // Only add to the live bytes if the object was not already marked and we are not the young
        // GC.
        // Why add live bytes even after 2-phase GC?
        // We need to ensure that if there is a unevac region with any live
        // objects, then its live_bytes must be non-zero. Otherwise,
        // ClearFromSpace() will clear the region. Considering, that we may skip
        // live objects during marking phase of 2-phase GC, we have to take care
        // of such objects here.
        add_to_live_bytes = true;
      }
      break;
    case space::RegionSpace::RegionType::kRegionTypeToSpace:
      if (use_generational_cc_) {
        // Copied to to-space, set the bit so that the next GC can scan objects.
        region_space_bitmap_->Set(to_ref);
      }
      perform_scan = true;
      break;
    default:
      DCHECK(!region_space_->HasAddress(to_ref)) << to_ref;
      DCHECK(!immune_spaces_.ContainsObject(to_ref));
      // Non-moving or large-object space.
      if (kUseBakerReadBarrier) {
        accounting::ContinuousSpaceBitmap* mark_bitmap =
            heap_->GetNonMovingSpace()->GetMarkBitmap();
        const bool is_los = !mark_bitmap->HasAddress(to_ref);
        if (is_los) {
          if (!IsAligned<kPageSize>(to_ref)) {
            // Ref is a large object that is not aligned, it must be heap
            // corruption. Remove memory protection and dump data before
            // AtomicSetReadBarrierState since it will fault if the address is not
            // valid.
            region_space_->Unprotect();
            heap_->GetVerification()->LogHeapCorruption(/* obj */ nullptr,
                                                        MemberOffset(0),
                                                        to_ref,
                                                        /* fatal */ true);
          }
          DCHECK(heap_->GetLargeObjectsSpace())
              << "ref=" << to_ref
              << " doesn't belong to non-moving space and large object space doesn't exist";
          accounting::LargeObjectBitmap* los_bitmap =
              heap_->GetLargeObjectsSpace()->GetMarkBitmap();
          DCHECK(los_bitmap->HasAddress(to_ref));
          // Only the GC thread could be setting the LOS bit map hence doesn't
          // need to be atomically done.
          perform_scan = !los_bitmap->Set(to_ref);
        } else {
          // Only the GC thread could be setting the non-moving space bit map
          // hence doesn't need to be atomically done.
          perform_scan = !mark_bitmap->Set(to_ref);
        }
      } else {
        perform_scan = true;
      }
  }
  if (perform_scan) {
    if (use_generational_cc_ && young_gen_) {
      Scan<true>(to_ref);
    } else {
      Scan<false>(to_ref);
    }
  }
  if (kUseBakerReadBarrier) {
    DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState())
        << " to_ref=" << to_ref
        << " rb_state=" << to_ref->GetReadBarrierState()
        << " is_marked=" << IsMarked(to_ref)
        << " type=" << to_ref->PrettyTypeOf()
        << " young_gen=" << std::boolalpha << young_gen_ << std::noboolalpha
        << " space=" << heap_->DumpSpaceNameFromAddress(to_ref)
        << " region_type=" << rtype
        // TODO: Temporary; remove this when this is no longer needed (b/116087961).
        << " runtime->sentinel=" << Runtime::Current()->GetSentinel().Read<kWithoutReadBarrier>();
  }
#ifdef USE_BAKER_OR_BROOKS_READ_BARRIER
  mirror::Object* referent = nullptr;
  if (UNLIKELY((to_ref->GetClass<kVerifyNone, kWithoutReadBarrier>()->IsTypeOfReferenceClass() &&
                (referent = to_ref->AsReference()->GetReferent<kWithoutReadBarrier>()) != nullptr &&
                !IsInToSpace(referent)))) {
    // Leave this reference gray in the queue so that GetReferent() will trigger a read barrier. We
    // will change it to non-gray later in ReferenceQueue::DisableReadBarrierForReference.
    DCHECK(to_ref->AsReference()->GetPendingNext() != nullptr)
        << "Left unenqueued ref gray " << to_ref;
  } else {
    // We may occasionally leave a reference non-gray in the queue if its referent happens to be
    // concurrently marked after the Scan() call above has enqueued the Reference, in which case the
    // above IsInToSpace() evaluates to true and we change the color from gray to non-gray here in
    // this else block.
    if (kUseBakerReadBarrier) {
      bool success = to_ref->AtomicSetReadBarrierState<std::memory_order_release>(
          ReadBarrier::GrayState(),
          ReadBarrier::NonGrayState());
      DCHECK(success) << "Must succeed as we won the race.";
    }
  }
#else
  DCHECK(!kUseBakerReadBarrier);
#endif

  if (add_to_live_bytes) {
    // Add to the live bytes per unevacuated from-space. Note this code is always run by the
    // GC-running thread (no synchronization required).
    DCHECK(region_space_bitmap_->Test(to_ref));
    size_t obj_size = to_ref->SizeOf<kDefaultVerifyFlags>();
    size_t alloc_size = RoundUp(obj_size, space::RegionSpace::kAlignment);
    region_space_->AddLiveBytes(to_ref, alloc_size);
  }
  if (ReadBarrier::kEnableToSpaceInvariantChecks) {
    CHECK(to_ref != nullptr);
    space::RegionSpace* region_space = RegionSpace();
    CHECK(!region_space->IsInFromSpace(to_ref)) << "Scanning object " << to_ref << " in from space";
    AssertToSpaceInvariant(nullptr, MemberOffset(0), to_ref);
    AssertToSpaceInvariantFieldVisitor visitor(this);
    to_ref->VisitReferences</*kVisitNativeRoots=*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
        visitor,
        visitor);
  }
}

class ConcurrentCopying::DisableWeakRefAccessCallback : public Closure {
 public:
  explicit DisableWeakRefAccessCallback(ConcurrentCopying* concurrent_copying)
      : concurrent_copying_(concurrent_copying) {
  }

  void Run(Thread* self ATTRIBUTE_UNUSED) override REQUIRES(Locks::thread_list_lock_) {
    // This needs to run under the thread_list_lock_ critical section in ThreadList::RunCheckpoint()
    // to avoid a deadlock b/31500969.
    CHECK(concurrent_copying_->weak_ref_access_enabled_);
    concurrent_copying_->weak_ref_access_enabled_ = false;
  }

 private:
  ConcurrentCopying* const concurrent_copying_;
};

void ConcurrentCopying::SwitchToSharedMarkStackMode() {
  Thread* self = Thread::Current();
  DCHECK(thread_running_gc_ != nullptr);
  DCHECK(self == thread_running_gc_);
  DCHECK(thread_running_gc_->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode before_mark_stack_mode = mark_stack_mode_.load(std::memory_order_relaxed);
  CHECK_EQ(static_cast<uint32_t>(before_mark_stack_mode),
           static_cast<uint32_t>(kMarkStackModeThreadLocal));
  mark_stack_mode_.store(kMarkStackModeShared, std::memory_order_relaxed);
  DisableWeakRefAccessCallback dwrac(this);
  // Process the thread local mark stacks one last time after switching to the shared mark stack
  // mode and disable weak ref accesses.
  ProcessThreadLocalMarkStacks(/* disable_weak_ref_access= */ true,
                               &dwrac,
                               [this] (mirror::Object* ref)
                                   REQUIRES_SHARED(Locks::mutator_lock_) {
                                 ProcessMarkStackRef(ref);
                               });
  if (kVerboseMode) {
    LOG(INFO) << "Switched to shared mark stack mode and disabled weak ref access";
  }
}

void ConcurrentCopying::SwitchToGcExclusiveMarkStackMode() {
  Thread* self = Thread::Current();
  DCHECK(thread_running_gc_ != nullptr);
  DCHECK(self == thread_running_gc_);
  DCHECK(thread_running_gc_->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode before_mark_stack_mode = mark_stack_mode_.load(std::memory_order_relaxed);
  CHECK_EQ(static_cast<uint32_t>(before_mark_stack_mode),
           static_cast<uint32_t>(kMarkStackModeShared));
  mark_stack_mode_.store(kMarkStackModeGcExclusive, std::memory_order_relaxed);
  QuasiAtomic::ThreadFenceForConstructor();
  if (kVerboseMode) {
    LOG(INFO) << "Switched to GC exclusive mark stack mode";
  }
}

void ConcurrentCopying::CheckEmptyMarkStack() {
  Thread* self = Thread::Current();
  DCHECK(thread_running_gc_ != nullptr);
  DCHECK(self == thread_running_gc_);
  DCHECK(thread_running_gc_->GetThreadLocalMarkStack() == nullptr);
  MarkStackMode mark_stack_mode = mark_stack_mode_.load(std::memory_order_relaxed);
  if (mark_stack_mode == kMarkStackModeThreadLocal) {
    // Thread-local mark stack mode.
    RevokeThreadLocalMarkStacks(false, nullptr);
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    if (!revoked_mark_stacks_.empty()) {
      for (accounting::AtomicStack<mirror::Object>* mark_stack : revoked_mark_stacks_) {
        while (!mark_stack->IsEmpty()) {
          mirror::Object* obj = mark_stack->PopBack();
          if (kUseBakerReadBarrier) {
            uint32_t rb_state = obj->GetReadBarrierState();
            LOG(INFO) << "On mark queue : " << obj << " " << obj->PrettyTypeOf() << " rb_state="
                      << rb_state << " is_marked=" << IsMarked(obj);
          } else {
            LOG(INFO) << "On mark queue : " << obj << " " << obj->PrettyTypeOf()
                      << " is_marked=" << IsMarked(obj);
          }
        }
      }
      LOG(FATAL) << "mark stack is not empty";
    }
  } else {
    // Shared, GC-exclusive, or off.
    MutexLock mu(thread_running_gc_, mark_stack_lock_);
    CHECK(gc_mark_stack_->IsEmpty());
    CHECK(revoked_mark_stacks_.empty());
    AssertEmptyThreadMarkStackMap();
    CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
  }
}

void ConcurrentCopying::SweepSystemWeaks(Thread* self) {
  TimingLogger::ScopedTiming split("SweepSystemWeaks", GetTimings());
  ReaderMutexLock mu(self, *Locks::heap_bitmap_lock_);
  Runtime::Current()->SweepSystemWeaks(this);
}

void ConcurrentCopying::Sweep(bool swap_bitmaps) {
  if (use_generational_cc_ && young_gen_) {
    // Only sweep objects on the live stack.
    SweepArray(heap_->GetLiveStack(), /* swap_bitmaps= */ false);
  } else {
    {
      TimingLogger::ScopedTiming t("MarkStackAsLive", GetTimings());
      accounting::ObjectStack* live_stack = heap_->GetLiveStack();
      if (kEnableFromSpaceAccountingCheck) {
        // Ensure that nobody inserted items in the live stack after we swapped the stacks.
        CHECK_GE(live_stack_freeze_size_, live_stack->Size());
      }
      heap_->MarkAllocStackAsLive(live_stack);
      live_stack->Reset();
    }
    CheckEmptyMarkStack();
    TimingLogger::ScopedTiming split("Sweep", GetTimings());
    for (const auto& space : GetHeap()->GetContinuousSpaces()) {
      if (space->IsContinuousMemMapAllocSpace() && space != region_space_
          && !immune_spaces_.ContainsSpace(space)) {
        space::ContinuousMemMapAllocSpace* alloc_space = space->AsContinuousMemMapAllocSpace();
        TimingLogger::ScopedTiming split2(
            alloc_space->IsZygoteSpace() ? "SweepZygoteSpace" : "SweepAllocSpace", GetTimings());
        RecordFree(alloc_space->Sweep(swap_bitmaps));
      }
    }
    SweepLargeObjects(swap_bitmaps);
  }
}

// Copied and adapted from MarkSweep::SweepArray.
void ConcurrentCopying::SweepArray(accounting::ObjectStack* allocations, bool swap_bitmaps) {
  // This method is only used when Generational CC collection is enabled.
  DCHECK(use_generational_cc_);
  CheckEmptyMarkStack();
  TimingLogger::ScopedTiming t("SweepArray", GetTimings());
  Thread* self = Thread::Current();
  mirror::Object** chunk_free_buffer = reinterpret_cast<mirror::Object**>(
      sweep_array_free_buffer_mem_map_.BaseBegin());
  size_t chunk_free_pos = 0;
  ObjectBytePair freed;
  ObjectBytePair freed_los;
  // How many objects are left in the array, modified after each space is swept.
  StackReference<mirror::Object>* objects = allocations->Begin();
  size_t count = allocations->Size();
  // Start by sweeping the continuous spaces.
  for (space::ContinuousSpace* space : heap_->GetContinuousSpaces()) {
    if (!space->IsAllocSpace() ||
        space == region_space_ ||
        immune_spaces_.ContainsSpace(space) ||
        space->GetLiveBitmap() == nullptr) {
      continue;
    }
    space::AllocSpace* alloc_space = space->AsAllocSpace();
    accounting::ContinuousSpaceBitmap* live_bitmap = space->GetLiveBitmap();
    accounting::ContinuousSpaceBitmap* mark_bitmap = space->GetMarkBitmap();
    if (swap_bitmaps) {
      std::swap(live_bitmap, mark_bitmap);
    }
    StackReference<mirror::Object>* out = objects;
    for (size_t i = 0; i < count; ++i) {
      mirror::Object* const obj = objects[i].AsMirrorPtr();
      if (kUseThreadLocalAllocationStack && obj == nullptr) {
        continue;
      }
      if (space->HasAddress(obj)) {
        // This object is in the space, remove it from the array and add it to the sweep buffer
        // if needed.
        if (!mark_bitmap->Test(obj)) {
          if (chunk_free_pos >= kSweepArrayChunkFreeSize) {
            TimingLogger::ScopedTiming t2("FreeList", GetTimings());
            freed.objects += chunk_free_pos;
            freed.bytes += alloc_space->FreeList(self, chunk_free_pos, chunk_free_buffer);
            chunk_free_pos = 0;
          }
          chunk_free_buffer[chunk_free_pos++] = obj;
        }
      } else {
        (out++)->Assign(obj);
      }
    }
    if (chunk_free_pos > 0) {
      TimingLogger::ScopedTiming t2("FreeList", GetTimings());
      freed.objects += chunk_free_pos;
      freed.bytes += alloc_space->FreeList(self, chunk_free_pos, chunk_free_buffer);
      chunk_free_pos = 0;
    }
    // All of the references which space contained are no longer in the allocation stack, update
    // the count.
    count = out - objects;
  }
  // Handle the large object space.
  space::LargeObjectSpace* large_object_space = GetHeap()->GetLargeObjectsSpace();
  if (large_object_space != nullptr) {
    accounting::LargeObjectBitmap* large_live_objects = large_object_space->GetLiveBitmap();
    accounting::LargeObjectBitmap* large_mark_objects = large_object_space->GetMarkBitmap();
    if (swap_bitmaps) {
      std::swap(large_live_objects, large_mark_objects);
    }
    for (size_t i = 0; i < count; ++i) {
      mirror::Object* const obj = objects[i].AsMirrorPtr();
      // Handle large objects.
      if (kUseThreadLocalAllocationStack && obj == nullptr) {
        continue;
      }
      if (!large_mark_objects->Test(obj)) {
        ++freed_los.objects;
        freed_los.bytes += large_object_space->Free(self, obj);
      }
    }
  }
  {
    TimingLogger::ScopedTiming t2("RecordFree", GetTimings());
    RecordFree(freed);
    RecordFreeLOS(freed_los);
    t2.NewTiming("ResetStack");
    allocations->Reset();
  }
  sweep_array_free_buffer_mem_map_.MadviseDontNeedAndZero();
}

void ConcurrentCopying::MarkZygoteLargeObjects() {
  TimingLogger::ScopedTiming split(__FUNCTION__, GetTimings());
  Thread* const self = Thread::Current();
  WriterMutexLock rmu(self, *Locks::heap_bitmap_lock_);
  space::LargeObjectSpace* const los = heap_->GetLargeObjectsSpace();
  if (los != nullptr) {
    // Pick the current live bitmap (mark bitmap if swapped).
    accounting::LargeObjectBitmap* const live_bitmap = los->GetLiveBitmap();
    accounting::LargeObjectBitmap* const mark_bitmap = los->GetMarkBitmap();
    // Walk through all of the objects and explicitly mark the zygote ones so they don't get swept.
    std::pair<uint8_t*, uint8_t*> range = los->GetBeginEndAtomic();
    live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(range.first),
                                  reinterpret_cast<uintptr_t>(range.second),
                                  [mark_bitmap, los, self](mirror::Object* obj)
        REQUIRES(Locks::heap_bitmap_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (los->IsZygoteLargeObject(self, obj)) {
        mark_bitmap->Set(obj);
      }
    });
  }
}

void ConcurrentCopying::SweepLargeObjects(bool swap_bitmaps) {
  TimingLogger::ScopedTiming split("SweepLargeObjects", GetTimings());
  if (heap_->GetLargeObjectsSpace() != nullptr) {
    RecordFreeLOS(heap_->GetLargeObjectsSpace()->Sweep(swap_bitmaps));
  }
}

void ConcurrentCopying::CaptureRssAtPeak() {
  using range_t = std::pair<void*, void*>;
  // This operation is expensive as several calls to mincore() are performed.
  // Also, this must be called before clearing regions in ReclaimPhase().
  // Therefore, we make it conditional on the flag that enables dumping GC
  // performance info on shutdown.
  if (Runtime::Current()->GetDumpGCPerformanceOnShutdown()) {
    std::list<range_t> gc_ranges;
    auto add_gc_range = [&gc_ranges](void* start, size_t size) {
      void* end = static_cast<char*>(start) + RoundUp(size, kPageSize);
      gc_ranges.emplace_back(range_t(start, end));
    };

    // region space
    DCHECK(IsAligned<kPageSize>(region_space_->Limit()));
    gc_ranges.emplace_back(range_t(region_space_->Begin(), region_space_->Limit()));
    // mark bitmap
    add_gc_range(region_space_bitmap_->Begin(), region_space_bitmap_->Size());

    // non-moving space
    {
      DCHECK(IsAligned<kPageSize>(heap_->non_moving_space_->Limit()));
      gc_ranges.emplace_back(range_t(heap_->non_moving_space_->Begin(),
                                     heap_->non_moving_space_->Limit()));
      // mark bitmap
      accounting::ContinuousSpaceBitmap *bitmap = heap_->non_moving_space_->GetMarkBitmap();
      add_gc_range(bitmap->Begin(), bitmap->Size());
      // live bitmap. Deal with bound bitmaps.
      ReaderMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
      if (heap_->non_moving_space_->HasBoundBitmaps()) {
        DCHECK_EQ(bitmap, heap_->non_moving_space_->GetLiveBitmap());
        bitmap = heap_->non_moving_space_->GetTempBitmap();
      } else {
        bitmap = heap_->non_moving_space_->GetLiveBitmap();
      }
      add_gc_range(bitmap->Begin(), bitmap->Size());
    }
    // large-object space
    if (heap_->GetLargeObjectsSpace()) {
      heap_->GetLargeObjectsSpace()->ForEachMemMap([&add_gc_range](const MemMap& map) {
        DCHECK(IsAligned<kPageSize>(map.BaseSize()));
        add_gc_range(map.BaseBegin(), map.BaseSize());
      });
      // mark bitmap
      accounting::LargeObjectBitmap* bitmap = heap_->GetLargeObjectsSpace()->GetMarkBitmap();
      add_gc_range(bitmap->Begin(), bitmap->Size());
      // live bitmap
      bitmap = heap_->GetLargeObjectsSpace()->GetLiveBitmap();
      add_gc_range(bitmap->Begin(), bitmap->Size());
    }
    // card table
    add_gc_range(heap_->GetCardTable()->MemMapBegin(), heap_->GetCardTable()->MemMapSize());
    // inter-region refs
    if (use_generational_cc_ && !young_gen_) {
      // region space
      add_gc_range(region_space_inter_region_bitmap_.Begin(),
                   region_space_inter_region_bitmap_.Size());
      // non-moving space
      add_gc_range(non_moving_space_inter_region_bitmap_.Begin(),
                   non_moving_space_inter_region_bitmap_.Size());
    }
    // Extract RSS using mincore(). Updates the cummulative RSS counter.
    ExtractRssFromMincore(&gc_ranges);
  }
}

void ConcurrentCopying::ReclaimPhase() {
  TimingLogger::ScopedTiming split("ReclaimPhase", GetTimings());
  if (kVerboseMode) {
    LOG(INFO) << "GC ReclaimPhase";
  }
  Thread* self = Thread::Current();

  {
    // Double-check that the mark stack is empty.
    // Note: need to set this after VerifyNoFromSpaceRef().
    is_asserting_to_space_invariant_ = false;
    QuasiAtomic::ThreadFenceForConstructor();
    if (kVerboseMode) {
      LOG(INFO) << "Issue an empty check point. ";
    }
    IssueEmptyCheckpoint();
    // Disable the check.
    is_mark_stack_push_disallowed_.store(0, std::memory_order_seq_cst);
    if (kUseBakerReadBarrier) {
      updated_all_immune_objects_.store(false, std::memory_order_seq_cst);
    }
    CheckEmptyMarkStack();
  }

  // Capture RSS at the time when memory usage is at its peak. All GC related
  // memory ranges like java heap, card table, bitmap etc. are taken into
  // account.
  // TODO: We can fetch resident memory for region space directly by going
  // through list of allocated regions. This way we can avoid calling mincore on
  // the biggest memory range, thereby reducing the cost of this function.
  CaptureRssAtPeak();

  // Sweep the malloc spaces before clearing the from space since the memory tool mode might
  // access the object classes in the from space for dead objects.
  {
    WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
    Sweep(/* swap_bitmaps= */ false);
    SwapBitmaps();
    heap_->UnBindBitmaps();

    // The bitmap was cleared at the start of the GC, there is nothing we need to do here.
    DCHECK(region_space_bitmap_ != nullptr);
    region_space_bitmap_ = nullptr;
  }


  {
    // Record freed objects.
    TimingLogger::ScopedTiming split2("RecordFree", GetTimings());
    // Don't include thread-locals that are in the to-space.
    const uint64_t from_bytes = region_space_->GetBytesAllocatedInFromSpace();
    const uint64_t from_objects = region_space_->GetObjectsAllocatedInFromSpace();
    const uint64_t unevac_from_bytes = region_space_->GetBytesAllocatedInUnevacFromSpace();
    const uint64_t unevac_from_objects = region_space_->GetObjectsAllocatedInUnevacFromSpace();
    uint64_t to_bytes = bytes_moved_.load(std::memory_order_relaxed) + bytes_moved_gc_thread_;
    cumulative_bytes_moved_.fetch_add(to_bytes, std::memory_order_relaxed);
    uint64_t to_objects = objects_moved_.load(std::memory_order_relaxed) + objects_moved_gc_thread_;
    cumulative_objects_moved_.fetch_add(to_objects, std::memory_order_relaxed);
    if (kEnableFromSpaceAccountingCheck) {
      CHECK_EQ(from_space_num_objects_at_first_pause_, from_objects + unevac_from_objects);
      CHECK_EQ(from_space_num_bytes_at_first_pause_, from_bytes + unevac_from_bytes);
    }
    CHECK_LE(to_objects, from_objects);
    // to_bytes <= from_bytes is only approximately true, because objects expand a little when
    // copying to non-moving space in near-OOM situations.
    if (from_bytes > 0) {
      copied_live_bytes_ratio_sum_ += static_cast<float>(to_bytes) / from_bytes;
      gc_count_++;
    }

    // Cleared bytes and objects, populated by the call to RegionSpace::ClearFromSpace below.
    uint64_t cleared_bytes;
    uint64_t cleared_objects;
    {
      TimingLogger::ScopedTiming split4("ClearFromSpace", GetTimings());
      region_space_->ClearFromSpace(&cleared_bytes, &cleared_objects, /*clear_bitmap*/ !young_gen_);
      // `cleared_bytes` and `cleared_objects` may be greater than the from space equivalents since
      // RegionSpace::ClearFromSpace may clear empty unevac regions.
      CHECK_GE(cleared_bytes, from_bytes);
      CHECK_GE(cleared_objects, from_objects);
    }
    // freed_bytes could conceivably be negative if we fall back to nonmoving space and have to
    // pad to a larger size.
    int64_t freed_bytes = (int64_t)cleared_bytes - (int64_t)to_bytes;
    uint64_t freed_objects = cleared_objects - to_objects;
    if (kVerboseMode) {
      LOG(INFO) << "RecordFree:"
                << " from_bytes=" << from_bytes << " from_objects=" << from_objects
                << " unevac_from_bytes=" << unevac_from_bytes
                << " unevac_from_objects=" << unevac_from_objects
                << " to_bytes=" << to_bytes << " to_objects=" << to_objects
                << " freed_bytes=" << freed_bytes << " freed_objects=" << freed_objects
                << " from_space size=" << region_space_->FromSpaceSize()
                << " unevac_from_space size=" << region_space_->UnevacFromSpaceSize()
                << " to_space size=" << region_space_->ToSpaceSize();
      LOG(INFO) << "(before) num_bytes_allocated="
                << heap_->num_bytes_allocated_.load();
    }
    RecordFree(ObjectBytePair(freed_objects, freed_bytes));
    if (kVerboseMode) {
      LOG(INFO) << "(after) num_bytes_allocated="
                << heap_->num_bytes_allocated_.load();
    }

    float reclaimed_bytes_ratio = static_cast<float>(freed_bytes) / num_bytes_allocated_before_gc_;
    reclaimed_bytes_ratio_sum_ += reclaimed_bytes_ratio;
  }

  CheckEmptyMarkStack();

  if (heap_->dump_region_info_after_gc_) {
    LOG(INFO) << "time=" << region_space_->Time();
    region_space_->DumpNonFreeRegions(LOG_STREAM(INFO));
  }

  if (kVerboseMode) {
    LOG(INFO) << "GC end of ReclaimPhase";
  }
}

std::string ConcurrentCopying::DumpReferenceInfo(mirror::Object* ref,
                                                 const char* ref_name,
                                                 const char* indent) {
  std::ostringstream oss;
  oss << indent << heap_->GetVerification()->DumpObjectInfo(ref, ref_name) << '\n';
  if (ref != nullptr) {
    if (kUseBakerReadBarrier) {
      oss << indent << ref_name << "->GetMarkBit()=" << ref->GetMarkBit() << '\n';
      oss << indent << ref_name << "->GetReadBarrierState()=" << ref->GetReadBarrierState() << '\n';
    }
  }
  if (region_space_->HasAddress(ref)) {
    oss << indent << "Region containing " << ref_name << ":" << '\n';
    region_space_->DumpRegionForObject(oss, ref);
    if (region_space_bitmap_ != nullptr) {
      oss << indent << "region_space_bitmap_->Test(" << ref_name << ")="
          << std::boolalpha << region_space_bitmap_->Test(ref) << std::noboolalpha;
    }
  }
  return oss.str();
}

std::string ConcurrentCopying::DumpHeapReference(mirror::Object* obj,
                                                 MemberOffset offset,
                                                 mirror::Object* ref) {
  std::ostringstream oss;
  constexpr const char* kIndent = "  ";
  oss << kIndent << "Invalid reference: ref=" << ref
      << " referenced from: object=" << obj << " offset= " << offset << '\n';
  // Information about `obj`.
  oss << DumpReferenceInfo(obj, "obj", kIndent) << '\n';
  // Information about `ref`.
  oss << DumpReferenceInfo(ref, "ref", kIndent);
  return oss.str();
}

void ConcurrentCopying::AssertToSpaceInvariant(mirror::Object* obj,
                                               MemberOffset offset,
                                               mirror::Object* ref) {
  CHECK_EQ(heap_->collector_type_, kCollectorTypeCC) << static_cast<size_t>(heap_->collector_type_);
  if (is_asserting_to_space_invariant_) {
    if (ref == nullptr) {
      // OK.
      return;
    } else if (region_space_->HasAddress(ref)) {
      // Check to-space invariant in region space (moving space).
      using RegionType = space::RegionSpace::RegionType;
      space::RegionSpace::RegionType type = region_space_->GetRegionTypeUnsafe(ref);
      if (type == RegionType::kRegionTypeToSpace) {
        // OK.
        return;
      } else if (type == RegionType::kRegionTypeUnevacFromSpace) {
        if (!IsMarkedInUnevacFromSpace(ref)) {
          LOG(FATAL_WITHOUT_ABORT) << "Found unmarked reference in unevac from-space:";
          // Remove memory protection from the region space and log debugging information.
          region_space_->Unprotect();
          LOG(FATAL_WITHOUT_ABORT) << DumpHeapReference(obj, offset, ref);
          Thread::Current()->DumpJavaStack(LOG_STREAM(FATAL_WITHOUT_ABORT));
        }
        CHECK(IsMarkedInUnevacFromSpace(ref)) << ref;
     } else {
        // Not OK: either a from-space ref or a reference in an unused region.
        if (type == RegionType::kRegionTypeFromSpace) {
          LOG(FATAL_WITHOUT_ABORT) << "Found from-space reference:";
        } else {
          LOG(FATAL_WITHOUT_ABORT) << "Found reference in region with type " << type << ":";
        }
        // Remove memory protection from the region space and log debugging information.
        region_space_->Unprotect();
        LOG(FATAL_WITHOUT_ABORT) << DumpHeapReference(obj, offset, ref);
        if (obj != nullptr) {
          LogFromSpaceRefHolder(obj, offset);
          LOG(FATAL_WITHOUT_ABORT) << "UNEVAC " << region_space_->IsInUnevacFromSpace(obj) << " "
                                   << obj << " " << obj->GetMarkBit();
          if (region_space_->HasAddress(obj)) {
            region_space_->DumpRegionForObject(LOG_STREAM(FATAL_WITHOUT_ABORT), obj);
          }
          LOG(FATAL_WITHOUT_ABORT) << "CARD " << static_cast<size_t>(
              *Runtime::Current()->GetHeap()->GetCardTable()->CardFromAddr(
                  reinterpret_cast<uint8_t*>(obj)));
          if (region_space_->HasAddress(obj)) {
            LOG(FATAL_WITHOUT_ABORT) << "BITMAP " << region_space_bitmap_->Test(obj);
          } else {
            accounting::ContinuousSpaceBitmap* mark_bitmap =
                heap_mark_bitmap_->GetContinuousSpaceBitmap(obj);
            if (mark_bitmap != nullptr) {
              LOG(FATAL_WITHOUT_ABORT) << "BITMAP " << mark_bitmap->Test(obj);
            } else {
              accounting::LargeObjectBitmap* los_bitmap =
                  heap_mark_bitmap_->GetLargeObjectBitmap(obj);
              LOG(FATAL_WITHOUT_ABORT) << "BITMAP " << los_bitmap->Test(obj);
            }
          }
        }
        ref->GetLockWord(false).Dump(LOG_STREAM(FATAL_WITHOUT_ABORT));
        LOG(FATAL_WITHOUT_ABORT) << "Non-free regions:";
        region_space_->DumpNonFreeRegions(LOG_STREAM(FATAL_WITHOUT_ABORT));
        PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
        MemMap::DumpMaps(LOG_STREAM(FATAL_WITHOUT_ABORT), /* terse= */ true);
        LOG(FATAL) << "Invalid reference " << ref
                   << " referenced from object " << obj << " at offset " << offset;
      }
    } else {
      // Check to-space invariant in non-moving space.
      AssertToSpaceInvariantInNonMovingSpace(obj, ref);
    }
  }
}

class RootPrinter {
 public:
  RootPrinter() { }

  template <class MirrorType>
  ALWAYS_INLINE void VisitRootIfNonNull(mirror::CompressedReference<MirrorType>* root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  template <class MirrorType>
  void VisitRoot(mirror::Object** root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL_WITHOUT_ABORT) << "root=" << root << " ref=" << *root;
  }

  template <class MirrorType>
  void VisitRoot(mirror::CompressedReference<MirrorType>* root)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    LOG(FATAL_WITHOUT_ABORT) << "root=" << root << " ref=" << root->AsMirrorPtr();
  }
};

std::string ConcurrentCopying::DumpGcRoot(mirror::Object* ref) {
  std::ostringstream oss;
  constexpr const char* kIndent = "  ";
  oss << kIndent << "Invalid GC root: ref=" << ref << '\n';
  // Information about `ref`.
  oss << DumpReferenceInfo(ref, "ref", kIndent);
  return oss.str();
}

void ConcurrentCopying::AssertToSpaceInvariant(GcRootSource* gc_root_source,
                                               mirror::Object* ref) {
  CHECK_EQ(heap_->collector_type_, kCollectorTypeCC) << static_cast<size_t>(heap_->collector_type_);
  if (is_asserting_to_space_invariant_) {
    if (ref == nullptr) {
      // OK.
      return;
    } else if (region_space_->HasAddress(ref)) {
      // Check to-space invariant in region space (moving space).
      using RegionType = space::RegionSpace::RegionType;
      space::RegionSpace::RegionType type = region_space_->GetRegionTypeUnsafe(ref);
      if (type == RegionType::kRegionTypeToSpace) {
        // OK.
        return;
      } else if (type == RegionType::kRegionTypeUnevacFromSpace) {
        if (!IsMarkedInUnevacFromSpace(ref)) {
          LOG(FATAL_WITHOUT_ABORT) << "Found unmarked reference in unevac from-space:";
          // Remove memory protection from the region space and log debugging information.
          region_space_->Unprotect();
          LOG(FATAL_WITHOUT_ABORT) << DumpGcRoot(ref);
        }
        CHECK(IsMarkedInUnevacFromSpace(ref)) << ref;
      } else {
        // Not OK: either a from-space ref or a reference in an unused region.
        if (type == RegionType::kRegionTypeFromSpace) {
          LOG(FATAL_WITHOUT_ABORT) << "Found from-space reference:";
        } else {
          LOG(FATAL_WITHOUT_ABORT) << "Found reference in region with type " << type << ":";
        }
        // Remove memory protection from the region space and log debugging information.
        region_space_->Unprotect();
        LOG(FATAL_WITHOUT_ABORT) << DumpGcRoot(ref);
        if (gc_root_source == nullptr) {
          // No info.
        } else if (gc_root_source->HasArtField()) {
          ArtField* field = gc_root_source->GetArtField();
          LOG(FATAL_WITHOUT_ABORT) << "gc root in field " << field << " "
                                   << ArtField::PrettyField(field);
          RootPrinter root_printer;
          field->VisitRoots(root_printer);
        } else if (gc_root_source->HasArtMethod()) {
          ArtMethod* method = gc_root_source->GetArtMethod();
          LOG(FATAL_WITHOUT_ABORT) << "gc root in method " << method << " "
                                   << ArtMethod::PrettyMethod(method);
          RootPrinter root_printer;
          method->VisitRoots(root_printer, kRuntimePointerSize);
        }
        ref->GetLockWord(false).Dump(LOG_STREAM(FATAL_WITHOUT_ABORT));
        LOG(FATAL_WITHOUT_ABORT) << "Non-free regions:";
        region_space_->DumpNonFreeRegions(LOG_STREAM(FATAL_WITHOUT_ABORT));
        PrintFileToLog("/proc/self/maps", LogSeverity::FATAL_WITHOUT_ABORT);
        MemMap::DumpMaps(LOG_STREAM(FATAL_WITHOUT_ABORT), /* terse= */ true);
        LOG(FATAL) << "Invalid reference " << ref;
      }
    } else {
      // Check to-space invariant in non-moving space.
      AssertToSpaceInvariantInNonMovingSpace(/* obj= */ nullptr, ref);
    }
  }
}

void ConcurrentCopying::LogFromSpaceRefHolder(mirror::Object* obj, MemberOffset offset) {
  if (kUseBakerReadBarrier) {
    LOG(INFO) << "holder=" << obj << " " << obj->PrettyTypeOf()
              << " holder rb_state=" << obj->GetReadBarrierState();
  } else {
    LOG(INFO) << "holder=" << obj << " " << obj->PrettyTypeOf();
  }
  if (region_space_->IsInFromSpace(obj)) {
    LOG(INFO) << "holder is in the from-space.";
  } else if (region_space_->IsInToSpace(obj)) {
    LOG(INFO) << "holder is in the to-space.";
  } else if (region_space_->IsInUnevacFromSpace(obj)) {
    LOG(INFO) << "holder is in the unevac from-space.";
    if (IsMarkedInUnevacFromSpace(obj)) {
      LOG(INFO) << "holder is marked in the region space bitmap.";
    } else {
      LOG(INFO) << "holder is not marked in the region space bitmap.";
    }
  } else {
    // In a non-moving space.
    if (immune_spaces_.ContainsObject(obj)) {
      LOG(INFO) << "holder is in an immune image or the zygote space.";
    } else {
      LOG(INFO) << "holder is in a non-immune, non-moving (or main) space.";
      accounting::ContinuousSpaceBitmap* mark_bitmap = heap_->GetNonMovingSpace()->GetMarkBitmap();
      accounting::LargeObjectBitmap* los_bitmap = nullptr;
      const bool is_los = !mark_bitmap->HasAddress(obj);
      if (is_los) {
        DCHECK(heap_->GetLargeObjectsSpace() && heap_->GetLargeObjectsSpace()->Contains(obj))
            << "obj=" << obj
            << " LOS bit map covers the entire lower 4GB address range";
        los_bitmap = heap_->GetLargeObjectsSpace()->GetMarkBitmap();
      }
      if (!is_los && mark_bitmap->Test(obj)) {
        LOG(INFO) << "holder is marked in the non-moving space mark bit map.";
      } else if (is_los && los_bitmap->Test(obj)) {
        LOG(INFO) << "holder is marked in the los bit map.";
      } else {
        // If ref is on the allocation stack, then it is considered
        // mark/alive (but not necessarily on the live stack.)
        if (IsOnAllocStack(obj)) {
          LOG(INFO) << "holder is on the alloc stack.";
        } else {
          LOG(INFO) << "holder is not marked or on the alloc stack.";
        }
      }
    }
  }
  LOG(INFO) << "offset=" << offset.SizeValue();
}

bool ConcurrentCopying::IsMarkedInNonMovingSpace(mirror::Object* from_ref) {
  DCHECK(!region_space_->HasAddress(from_ref)) << "ref=" << from_ref;
  DCHECK(!immune_spaces_.ContainsObject(from_ref)) << "ref=" << from_ref;
  if (kUseBakerReadBarrier && from_ref->GetReadBarrierStateAcquire() == ReadBarrier::GrayState()) {
    return true;
  } else if (!use_generational_cc_ || done_scanning_.load(std::memory_order_acquire)) {
    // Read the comment in IsMarkedInUnevacFromSpace()
    accounting::ContinuousSpaceBitmap* mark_bitmap = heap_->GetNonMovingSpace()->GetMarkBitmap();
    accounting::LargeObjectBitmap* los_bitmap = nullptr;
    const bool is_los = !mark_bitmap->HasAddress(from_ref);
    if (is_los) {
      DCHECK(heap_->GetLargeObjectsSpace() && heap_->GetLargeObjectsSpace()->Contains(from_ref))
          << "ref=" << from_ref
          << " doesn't belong to non-moving space and large object space doesn't exist";
      los_bitmap = heap_->GetLargeObjectsSpace()->GetMarkBitmap();
    }
    if (is_los ? los_bitmap->Test(from_ref) : mark_bitmap->Test(from_ref)) {
      return true;
    }
  }
  return IsOnAllocStack(from_ref);
}

void ConcurrentCopying::AssertToSpaceInvariantInNonMovingSpace(mirror::Object* obj,
                                                               mirror::Object* ref) {
  CHECK(ref != nullptr);
  CHECK(!region_space_->HasAddress(ref)) << "obj=" << obj << " ref=" << ref;
  // In a non-moving space. Check that the ref is marked.
  if (immune_spaces_.ContainsObject(ref)) {
    // Immune space case.
    if (kUseBakerReadBarrier) {
      // Immune object may not be gray if called from the GC.
      if (Thread::Current() == thread_running_gc_ && !gc_grays_immune_objects_) {
        return;
      }
      bool updated_all_immune_objects = updated_all_immune_objects_.load(std::memory_order_seq_cst);
      CHECK(updated_all_immune_objects || ref->GetReadBarrierState() == ReadBarrier::GrayState())
          << "Unmarked immune space ref. obj=" << obj << " rb_state="
          << (obj != nullptr ? obj->GetReadBarrierState() : 0U)
          << " ref=" << ref << " ref rb_state=" << ref->GetReadBarrierState()
          << " updated_all_immune_objects=" << updated_all_immune_objects;
    }
  } else {
    // Non-moving space and large-object space (LOS) cases.
    // If `ref` is on the allocation stack, then it may not be
    // marked live, but considered marked/alive (but not
    // necessarily on the live stack).
    CHECK(IsMarkedInNonMovingSpace(ref))
        << "Unmarked ref that's not on the allocation stack."
        << " obj=" << obj
        << " ref=" << ref
        << " rb_state=" << ref->GetReadBarrierState()
        << " is_marking=" << std::boolalpha << is_marking_ << std::noboolalpha
        << " young_gen=" << std::boolalpha << young_gen_ << std::noboolalpha
        << " done_scanning="
        << std::boolalpha << done_scanning_.load(std::memory_order_acquire) << std::noboolalpha
        << " self=" << Thread::Current();
  }
}

// Used to scan ref fields of an object.
template <bool kNoUnEvac>
class ConcurrentCopying::RefFieldsVisitor {
 public:
  explicit RefFieldsVisitor(ConcurrentCopying* collector, Thread* const thread)
      : collector_(collector), thread_(thread) {
    // Cannot have `kNoUnEvac` when Generational CC collection is disabled.
    DCHECK(!kNoUnEvac || collector_->use_generational_cc_);
  }

  void operator()(mirror::Object* obj, MemberOffset offset, bool /* is_static */)
      const ALWAYS_INLINE REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_) {
    collector_->Process<kNoUnEvac>(obj, offset);
  }

  void operator()(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> ref) const
      REQUIRES_SHARED(Locks::mutator_lock_) ALWAYS_INLINE {
    CHECK(klass->IsTypeOfReferenceClass());
    collector_->DelayReferenceReferent(klass, ref);
  }

  void VisitRootIfNonNull(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (!root->IsNull()) {
      VisitRoot(root);
    }
  }

  void VisitRoot(mirror::CompressedReference<mirror::Object>* root) const
      ALWAYS_INLINE
      REQUIRES_SHARED(Locks::mutator_lock_) {
    collector_->MarkRoot</*kGrayImmuneObject=*/false>(thread_, root);
  }

 private:
  ConcurrentCopying* const collector_;
  Thread* const thread_;
};

template <bool kNoUnEvac>
inline void ConcurrentCopying::Scan(mirror::Object* to_ref) {
  // Cannot have `kNoUnEvac` when Generational CC collection is disabled.
  DCHECK(!kNoUnEvac || use_generational_cc_);
  if (kDisallowReadBarrierDuringScan && !Runtime::Current()->IsActiveTransaction()) {
    // Avoid all read barriers during visit references to help performance.
    // Don't do this in transaction mode because we may read the old value of an field which may
    // trigger read barriers.
    Thread::Current()->ModifyDebugDisallowReadBarrier(1);
  }
  DCHECK(!region_space_->IsInFromSpace(to_ref));
  DCHECK_EQ(Thread::Current(), thread_running_gc_);
  RefFieldsVisitor<kNoUnEvac> visitor(this, thread_running_gc_);
  // Disable the read barrier for a performance reason.
  to_ref->VisitReferences</*kVisitNativeRoots=*/true, kDefaultVerifyFlags, kWithoutReadBarrier>(
      visitor, visitor);
  if (kDisallowReadBarrierDuringScan && !Runtime::Current()->IsActiveTransaction()) {
    thread_running_gc_->ModifyDebugDisallowReadBarrier(-1);
  }
}

template <bool kNoUnEvac>
inline void ConcurrentCopying::Process(mirror::Object* obj, MemberOffset offset) {
  // Cannot have `kNoUnEvac` when Generational CC collection is disabled.
  DCHECK(!kNoUnEvac || use_generational_cc_);
  DCHECK_EQ(Thread::Current(), thread_running_gc_);
  mirror::Object* ref = obj->GetFieldObject<
      mirror::Object, kVerifyNone, kWithoutReadBarrier, false>(offset);
  mirror::Object* to_ref = Mark</*kGrayImmuneObject=*/false, kNoUnEvac, /*kFromGCThread=*/true>(
      thread_running_gc_,
      ref,
      /*holder=*/ obj,
      offset);
  if (to_ref == ref) {
    return;
  }
  // This may fail if the mutator writes to the field at the same time. But it's ok.
  mirror::Object* expected_ref = ref;
  mirror::Object* new_ref = to_ref;
  do {
    if (expected_ref !=
        obj->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier, false>(offset)) {
      // It was updated by the mutator.
      break;
    }
    // Use release CAS to make sure threads reading the reference see contents of copied objects.
  } while (!obj->CasFieldObjectWithoutWriteBarrier<false, false, kVerifyNone>(
      offset,
      expected_ref,
      new_ref,
      CASMode::kWeak,
      std::memory_order_release));
}

// Process some roots.
inline void ConcurrentCopying::VisitRoots(
    mirror::Object*** roots, size_t count, const RootInfo& info ATTRIBUTE_UNUSED) {
  Thread* const self = Thread::Current();
  for (size_t i = 0; i < count; ++i) {
    mirror::Object** root = roots[i];
    mirror::Object* ref = *root;
    mirror::Object* to_ref = Mark(self, ref);
    if (to_ref == ref) {
      continue;
    }
    Atomic<mirror::Object*>* addr = reinterpret_cast<Atomic<mirror::Object*>*>(root);
    mirror::Object* expected_ref = ref;
    mirror::Object* new_ref = to_ref;
    do {
      if (expected_ref != addr->load(std::memory_order_relaxed)) {
        // It was updated by the mutator.
        break;
      }
    } while (!addr->CompareAndSetWeakRelaxed(expected_ref, new_ref));
  }
}

template<bool kGrayImmuneObject>
inline void ConcurrentCopying::MarkRoot(Thread* const self,
                                        mirror::CompressedReference<mirror::Object>* root) {
  DCHECK(!root->IsNull());
  mirror::Object* const ref = root->AsMirrorPtr();
  mirror::Object* to_ref = Mark<kGrayImmuneObject>(self, ref);
  if (to_ref != ref) {
    auto* addr = reinterpret_cast<Atomic<mirror::CompressedReference<mirror::Object>>*>(root);
    auto expected_ref = mirror::CompressedReference<mirror::Object>::FromMirrorPtr(ref);
    auto new_ref = mirror::CompressedReference<mirror::Object>::FromMirrorPtr(to_ref);
    // If the cas fails, then it was updated by the mutator.
    do {
      if (ref != addr->load(std::memory_order_relaxed).AsMirrorPtr()) {
        // It was updated by the mutator.
        break;
      }
    } while (!addr->CompareAndSetWeakRelaxed(expected_ref, new_ref));
  }
}

inline void ConcurrentCopying::VisitRoots(
    mirror::CompressedReference<mirror::Object>** roots, size_t count,
    const RootInfo& info ATTRIBUTE_UNUSED) {
  Thread* const self = Thread::Current();
  for (size_t i = 0; i < count; ++i) {
    mirror::CompressedReference<mirror::Object>* const root = roots[i];
    if (!root->IsNull()) {
      // kGrayImmuneObject is true because this is used for the thread flip.
      MarkRoot</*kGrayImmuneObject=*/true>(self, root);
    }
  }
}

// Temporary set gc_grays_immune_objects_ to true in a scope if the current thread is GC.
class ConcurrentCopying::ScopedGcGraysImmuneObjects {
 public:
  explicit ScopedGcGraysImmuneObjects(ConcurrentCopying* collector)
      : collector_(collector), enabled_(false) {
    if (kUseBakerReadBarrier &&
        collector_->thread_running_gc_ == Thread::Current() &&
        !collector_->gc_grays_immune_objects_) {
      collector_->gc_grays_immune_objects_ = true;
      enabled_ = true;
    }
  }

  ~ScopedGcGraysImmuneObjects() {
    if (kUseBakerReadBarrier &&
        collector_->thread_running_gc_ == Thread::Current() &&
        enabled_) {
      DCHECK(collector_->gc_grays_immune_objects_);
      collector_->gc_grays_immune_objects_ = false;
    }
  }

 private:
  ConcurrentCopying* const collector_;
  bool enabled_;
};

// Fill the given memory block with a dummy object. Used to fill in a
// copy of objects that was lost in race.
void ConcurrentCopying::FillWithDummyObject(Thread* const self,
                                            mirror::Object* dummy_obj,
                                            size_t byte_size) {
  // GC doesn't gray immune objects while scanning immune objects. But we need to trigger the read
  // barriers here because we need the updated reference to the int array class, etc. Temporary set
  // gc_grays_immune_objects_ to true so that we won't cause a DCHECK failure in MarkImmuneSpace().
  ScopedGcGraysImmuneObjects scoped_gc_gray_immune_objects(this);
  CHECK_ALIGNED(byte_size, kObjectAlignment);
  memset(dummy_obj, 0, byte_size);
  // Avoid going through read barrier for since kDisallowReadBarrierDuringScan may be enabled.
  // Explicitly mark to make sure to get an object in the to-space.
  mirror::Class* int_array_class = down_cast<mirror::Class*>(
      Mark(self, GetClassRoot<mirror::IntArray, kWithoutReadBarrier>().Ptr()));
  CHECK(int_array_class != nullptr);
  if (ReadBarrier::kEnableToSpaceInvariantChecks) {
    AssertToSpaceInvariant(nullptr, MemberOffset(0), int_array_class);
  }
  size_t component_size = int_array_class->GetComponentSize();
  CHECK_EQ(component_size, sizeof(int32_t));
  size_t data_offset = mirror::Array::DataOffset(component_size).SizeValue();
  if (data_offset > byte_size) {
    // An int array is too big. Use java.lang.Object.
    CHECK(java_lang_Object_ != nullptr);
    if (ReadBarrier::kEnableToSpaceInvariantChecks) {
      AssertToSpaceInvariant(nullptr, MemberOffset(0), java_lang_Object_);
    }
    CHECK_EQ(byte_size, java_lang_Object_->GetObjectSize<kVerifyNone>());
    dummy_obj->SetClass(java_lang_Object_);
    CHECK_EQ(byte_size, (dummy_obj->SizeOf<kVerifyNone>()));
  } else {
    // Use an int array.
    dummy_obj->SetClass(int_array_class);
    CHECK(dummy_obj->IsArrayInstance<kVerifyNone>());
    int32_t length = (byte_size - data_offset) / component_size;
    ObjPtr<mirror::Array> dummy_arr = dummy_obj->AsArray<kVerifyNone>();
    dummy_arr->SetLength(length);
    CHECK_EQ(dummy_arr->GetLength(), length)
        << "byte_size=" << byte_size << " length=" << length
        << " component_size=" << component_size << " data_offset=" << data_offset;
    CHECK_EQ(byte_size, (dummy_obj->SizeOf<kVerifyNone>()))
        << "byte_size=" << byte_size << " length=" << length
        << " component_size=" << component_size << " data_offset=" << data_offset;
  }
}

// Reuse the memory blocks that were copy of objects that were lost in race.
mirror::Object* ConcurrentCopying::AllocateInSkippedBlock(Thread* const self, size_t alloc_size) {
  // Try to reuse the blocks that were unused due to CAS failures.
  CHECK_ALIGNED(alloc_size, space::RegionSpace::kAlignment);
  size_t min_object_size = RoundUp(sizeof(mirror::Object), space::RegionSpace::kAlignment);
  size_t byte_size;
  uint8_t* addr;
  {
    MutexLock mu(self, skipped_blocks_lock_);
    auto it = skipped_blocks_map_.lower_bound(alloc_size);
    if (it == skipped_blocks_map_.end()) {
      // Not found.
      return nullptr;
    }
    byte_size = it->first;
    CHECK_GE(byte_size, alloc_size);
    if (byte_size > alloc_size && byte_size - alloc_size < min_object_size) {
      // If remainder would be too small for a dummy object, retry with a larger request size.
      it = skipped_blocks_map_.lower_bound(alloc_size + min_object_size);
      if (it == skipped_blocks_map_.end()) {
        // Not found.
        return nullptr;
      }
      CHECK_ALIGNED(it->first - alloc_size, space::RegionSpace::kAlignment);
      CHECK_GE(it->first - alloc_size, min_object_size)
          << "byte_size=" << byte_size << " it->first=" << it->first << " alloc_size=" << alloc_size;
    }
    // Found a block.
    CHECK(it != skipped_blocks_map_.end());
    byte_size = it->first;
    addr = it->second;
    CHECK_GE(byte_size, alloc_size);
    CHECK(region_space_->IsInToSpace(reinterpret_cast<mirror::Object*>(addr)));
    CHECK_ALIGNED(byte_size, space::RegionSpace::kAlignment);
    if (kVerboseMode) {
      LOG(INFO) << "Reusing skipped bytes : " << reinterpret_cast<void*>(addr) << ", " << byte_size;
    }
    skipped_blocks_map_.erase(it);
  }
  memset(addr, 0, byte_size);
  if (byte_size > alloc_size) {
    // Return the remainder to the map.
    CHECK_ALIGNED(byte_size - alloc_size, space::RegionSpace::kAlignment);
    CHECK_GE(byte_size - alloc_size, min_object_size);
    // FillWithDummyObject may mark an object, avoid holding skipped_blocks_lock_ to prevent lock
    // violation and possible deadlock. The deadlock case is a recursive case:
    // FillWithDummyObject -> Mark(IntArray.class) -> Copy -> AllocateInSkippedBlock.
    FillWithDummyObject(self,
                        reinterpret_cast<mirror::Object*>(addr + alloc_size),
                        byte_size - alloc_size);
    CHECK(region_space_->IsInToSpace(reinterpret_cast<mirror::Object*>(addr + alloc_size)));
    {
      MutexLock mu(self, skipped_blocks_lock_);
      skipped_blocks_map_.insert(std::make_pair(byte_size - alloc_size, addr + alloc_size));
    }
  }
  return reinterpret_cast<mirror::Object*>(addr);
}

mirror::Object* ConcurrentCopying::Copy(Thread* const self,
                                        mirror::Object* from_ref,
                                        mirror::Object* holder,
                                        MemberOffset offset) {
  DCHECK(region_space_->IsInFromSpace(from_ref));
  // If the class pointer is null, the object is invalid. This could occur for a dangling pointer
  // from a previous GC that is either inside or outside the allocated region.
  mirror::Class* klass = from_ref->GetClass<kVerifyNone, kWithoutReadBarrier>();
  if (UNLIKELY(klass == nullptr)) {
    // Remove memory protection from the region space and log debugging information.
    region_space_->Unprotect();
    heap_->GetVerification()->LogHeapCorruption(holder, offset, from_ref, /* fatal= */ true);
  }
  // There must not be a read barrier to avoid nested RB that might violate the to-space invariant.
  // Note that from_ref is a from space ref so the SizeOf() call will access the from-space meta
  // objects, but it's ok and necessary.
  size_t obj_size = from_ref->SizeOf<kDefaultVerifyFlags>();
  size_t region_space_alloc_size = (obj_size <= space::RegionSpace::kRegionSize)
      ? RoundUp(obj_size, space::RegionSpace::kAlignment)
      : RoundUp(obj_size, space::RegionSpace::kRegionSize);
  size_t region_space_bytes_allocated = 0U;
  size_t non_moving_space_bytes_allocated = 0U;
  size_t bytes_allocated = 0U;
  size_t dummy;
  bool fall_back_to_non_moving = false;
  mirror::Object* to_ref = region_space_->AllocNonvirtual</*kForEvac=*/ true>(
      region_space_alloc_size, &region_space_bytes_allocated, nullptr, &dummy);
  bytes_allocated = region_space_bytes_allocated;
  if (LIKELY(to_ref != nullptr)) {
    DCHECK_EQ(region_space_alloc_size, region_space_bytes_allocated);
  } else {
    // Failed to allocate in the region space. Try the skipped blocks.
    to_ref = AllocateInSkippedBlock(self, region_space_alloc_size);
    if (to_ref != nullptr) {
      // Succeeded to allocate in a skipped block.
      if (heap_->use_tlab_) {
        // This is necessary for the tlab case as it's not accounted in the space.
        region_space_->RecordAlloc(to_ref);
      }
      bytes_allocated = region_space_alloc_size;
      heap_->num_bytes_allocated_.fetch_sub(bytes_allocated, std::memory_order_relaxed);
      to_space_bytes_skipped_.fetch_sub(bytes_allocated, std::memory_order_relaxed);
      to_space_objects_skipped_.fetch_sub(1, std::memory_order_relaxed);
    } else {
      // Fall back to the non-moving space.
      fall_back_to_non_moving = true;
      if (kVerboseMode) {
        LOG(INFO) << "Out of memory in the to-space. Fall back to non-moving. skipped_bytes="
                  << to_space_bytes_skipped_.load(std::memory_order_relaxed)
                  << " skipped_objects="
                  << to_space_objects_skipped_.load(std::memory_order_relaxed);
      }
      to_ref = heap_->non_moving_space_->Alloc(self, obj_size,
                                               &non_moving_space_bytes_allocated, nullptr, &dummy);
      if (UNLIKELY(to_ref == nullptr)) {
        LOG(FATAL_WITHOUT_ABORT) << "Fall-back non-moving space allocation failed for a "
                                 << obj_size << " byte object in region type "
                                 << region_space_->GetRegionType(from_ref);
        LOG(FATAL) << "Object address=" << from_ref << " type=" << from_ref->PrettyTypeOf();
      }
      bytes_allocated = non_moving_space_bytes_allocated;
    }
  }
  DCHECK(to_ref != nullptr);

  // Copy the object excluding the lock word since that is handled in the loop.
  to_ref->SetClass(klass);
  const size_t kObjectHeaderSize = sizeof(mirror::Object);
  DCHECK_GE(obj_size, kObjectHeaderSize);
  static_assert(kObjectHeaderSize == sizeof(mirror::HeapReference<mirror::Class>) +
                    sizeof(LockWord),
                "Object header size does not match");
  // Memcpy can tear for words since it may do byte copy. It is only safe to do this since the
  // object in the from space is immutable other than the lock word. b/31423258
  memcpy(reinterpret_cast<uint8_t*>(to_ref) + kObjectHeaderSize,
         reinterpret_cast<const uint8_t*>(from_ref) + kObjectHeaderSize,
         obj_size - kObjectHeaderSize);

  // Attempt to install the forward pointer. This is in a loop as the
  // lock word atomic write can fail.
  while (true) {
    LockWord old_lock_word = from_ref->GetLockWord(false);

    if (old_lock_word.GetState() == LockWord::kForwardingAddress) {
      // Lost the race. Another thread (either GC or mutator) stored
      // the forwarding pointer first. Make the lost copy (to_ref)
      // look like a valid but dead (dummy) object and keep it for
      // future reuse.
      FillWithDummyObject(self, to_ref, bytes_allocated);
      if (!fall_back_to_non_moving) {
        DCHECK(region_space_->IsInToSpace(to_ref));
        if (bytes_allocated > space::RegionSpace::kRegionSize) {
          // Free the large alloc.
          region_space_->FreeLarge</*kForEvac=*/ true>(to_ref, bytes_allocated);
        } else {
          // Record the lost copy for later reuse.
          heap_->num_bytes_allocated_.fetch_add(bytes_allocated, std::memory_order_relaxed);
          to_space_bytes_skipped_.fetch_add(bytes_allocated, std::memory_order_relaxed);
          to_space_objects_skipped_.fetch_add(1, std::memory_order_relaxed);
          MutexLock mu(self, skipped_blocks_lock_);
          skipped_blocks_map_.insert(std::make_pair(bytes_allocated,
                                                    reinterpret_cast<uint8_t*>(to_ref)));
        }
      } else {
        DCHECK(heap_->non_moving_space_->HasAddress(to_ref));
        DCHECK_EQ(bytes_allocated, non_moving_space_bytes_allocated);
        // Free the non-moving-space chunk.
        heap_->non_moving_space_->Free(self, to_ref);
      }

      // Get the winner's forward ptr.
      mirror::Object* lost_fwd_ptr = to_ref;
      to_ref = reinterpret_cast<mirror::Object*>(old_lock_word.ForwardingAddress());
      CHECK(to_ref != nullptr);
      CHECK_NE(to_ref, lost_fwd_ptr);
      CHECK(region_space_->IsInToSpace(to_ref) || heap_->non_moving_space_->HasAddress(to_ref))
          << "to_ref=" << to_ref << " " << heap_->DumpSpaces();
      CHECK_NE(to_ref->GetLockWord(false).GetState(), LockWord::kForwardingAddress);
      return to_ref;
    }

    // Copy the old lock word over since we did not copy it yet.
    to_ref->SetLockWord(old_lock_word, false);
    // Set the gray ptr.
    if (kUseBakerReadBarrier) {
      to_ref->SetReadBarrierState(ReadBarrier::GrayState());
    }

    // Do a fence to prevent the field CAS in ConcurrentCopying::Process from possibly reordering
    // before the object copy.
    std::atomic_thread_fence(std::memory_order_release);

    LockWord new_lock_word = LockWord::FromForwardingAddress(reinterpret_cast<size_t>(to_ref));

    // Try to atomically write the fwd ptr.
    bool success = from_ref->CasLockWord(old_lock_word,
                                         new_lock_word,
                                         CASMode::kWeak,
                                         std::memory_order_relaxed);
    if (LIKELY(success)) {
      // The CAS succeeded.
      DCHECK(thread_running_gc_ != nullptr);
      if (LIKELY(self == thread_running_gc_)) {
        objects_moved_gc_thread_ += 1;
        bytes_moved_gc_thread_ += bytes_allocated;
      } else {
        objects_moved_.fetch_add(1, std::memory_order_relaxed);
        bytes_moved_.fetch_add(bytes_allocated, std::memory_order_relaxed);
      }

      if (LIKELY(!fall_back_to_non_moving)) {
        DCHECK(region_space_->IsInToSpace(to_ref));
      } else {
        DCHECK(heap_->non_moving_space_->HasAddress(to_ref));
        DCHECK_EQ(bytes_allocated, non_moving_space_bytes_allocated);
        if (!use_generational_cc_ || !young_gen_) {
          // Mark it in the live bitmap.
          CHECK(!heap_->non_moving_space_->GetLiveBitmap()->AtomicTestAndSet(to_ref));
        }
        if (!kUseBakerReadBarrier) {
          // Mark it in the mark bitmap.
          CHECK(!heap_->non_moving_space_->GetMarkBitmap()->AtomicTestAndSet(to_ref));
        }
      }
      if (kUseBakerReadBarrier) {
        DCHECK(to_ref->GetReadBarrierState() == ReadBarrier::GrayState());
      }
      DCHECK(GetFwdPtr(from_ref) == to_ref);
      CHECK_NE(to_ref->GetLockWord(false).GetState(), LockWord::kForwardingAddress);
      PushOntoMarkStack(self, to_ref);
      return to_ref;
    } else {
      // The CAS failed. It may have lost the race or may have failed
      // due to monitor/hashcode ops. Either way, retry.
    }
  }
}

mirror::Object* ConcurrentCopying::IsMarked(mirror::Object* from_ref) {
  DCHECK(from_ref != nullptr);
  space::RegionSpace::RegionType rtype = region_space_->GetRegionType(from_ref);
  if (rtype == space::RegionSpace::RegionType::kRegionTypeToSpace) {
    // It's already marked.
    return from_ref;
  }
  mirror::Object* to_ref;
  if (rtype == space::RegionSpace::RegionType::kRegionTypeFromSpace) {
    to_ref = GetFwdPtr(from_ref);
    DCHECK(to_ref == nullptr || region_space_->IsInToSpace(to_ref) ||
           heap_->non_moving_space_->HasAddress(to_ref))
        << "from_ref=" << from_ref << " to_ref=" << to_ref;
  } else if (rtype == space::RegionSpace::RegionType::kRegionTypeUnevacFromSpace) {
    if (IsMarkedInUnevacFromSpace(from_ref)) {
      to_ref = from_ref;
    } else {
      to_ref = nullptr;
    }
  } else {
    // At this point, `from_ref` should not be in the region space
    // (i.e. within an "unused" region).
    DCHECK(!region_space_->HasAddress(from_ref)) << from_ref;
    // from_ref is in a non-moving space.
    if (immune_spaces_.ContainsObject(from_ref)) {
      // An immune object is alive.
      to_ref = from_ref;
    } else {
      // Non-immune non-moving space. Use the mark bitmap.
      if (IsMarkedInNonMovingSpace(from_ref)) {
        // Already marked.
        to_ref = from_ref;
      } else {
        to_ref = nullptr;
      }
    }
  }
  return to_ref;
}

bool ConcurrentCopying::IsOnAllocStack(mirror::Object* ref) {
  // TODO: Explain why this is here. What release operation does it pair with?
  std::atomic_thread_fence(std::memory_order_acquire);
  accounting::ObjectStack* alloc_stack = GetAllocationStack();
  return alloc_stack->Contains(ref);
}

mirror::Object* ConcurrentCopying::MarkNonMoving(Thread* const self,
                                                 mirror::Object* ref,
                                                 mirror::Object* holder,
                                                 MemberOffset offset) {
  // ref is in a non-moving space (from_ref == to_ref).
  DCHECK(!region_space_->HasAddress(ref)) << ref;
  DCHECK(!immune_spaces_.ContainsObject(ref));
  // Use the mark bitmap.
  accounting::ContinuousSpaceBitmap* mark_bitmap = heap_->GetNonMovingSpace()->GetMarkBitmap();
  accounting::LargeObjectBitmap* los_bitmap = nullptr;
  const bool is_los = !mark_bitmap->HasAddress(ref);
  if (is_los) {
    if (!IsAligned<kPageSize>(ref)) {
      // Ref is a large object that is not aligned, it must be heap
      // corruption. Remove memory protection and dump data before
      // AtomicSetReadBarrierState since it will fault if the address is not
      // valid.
      region_space_->Unprotect();
      heap_->GetVerification()->LogHeapCorruption(holder, offset, ref, /* fatal= */ true);
    }
    DCHECK(heap_->GetLargeObjectsSpace())
        << "ref=" << ref
        << " doesn't belong to non-moving space and large object space doesn't exist";
    los_bitmap = heap_->GetLargeObjectsSpace()->GetMarkBitmap();
    DCHECK(los_bitmap->HasAddress(ref));
  }
  if (use_generational_cc_) {
    // The sticky-bit CC collector is only compatible with Baker-style read barriers.
    DCHECK(kUseBakerReadBarrier);
    // Not done scanning, use AtomicSetReadBarrierPointer.
    if (!done_scanning_.load(std::memory_order_acquire)) {
      // Since the mark bitmap is still filled in from last GC, we can not use that or else the
      // mutator may see references to the from space. Instead, use the Baker pointer itself as
      // the mark bit.
      //
      // We need to avoid marking objects that are on allocation stack as that will lead to a
      // situation (after this GC cycle is finished) where some object(s) are on both allocation
      // stack and live bitmap. This leads to visiting the same object(s) twice during a heapdump
      // (b/117426281).
      if (!IsOnAllocStack(ref) &&
          ref->AtomicSetReadBarrierState(ReadBarrier::NonGrayState(), ReadBarrier::GrayState())) {
        // TODO: We don't actually need to scan this object later, we just need to clear the gray
        // bit.
        // We don't need to mark newly allocated objects (those in allocation stack) as they can
        // only point to to-space objects. Also, they are considered live till the next GC cycle.
        PushOntoMarkStack(self, ref);
      }
      return ref;
    }
  }
  if (!is_los && mark_bitmap->Test(ref)) {
    // Already marked.
  } else if (is_los && los_bitmap->Test(ref)) {
    // Already marked in LOS.
  } else if (IsOnAllocStack(ref)) {
    // If it's on the allocation stack, it's considered marked. Keep it white (non-gray).
    // Objects on the allocation stack need not be marked.
    if (!is_los) {
      DCHECK(!mark_bitmap->Test(ref));
    } else {
      DCHECK(!los_bitmap->Test(ref));
    }
    if (kUseBakerReadBarrier) {
      DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::NonGrayState());
    }
  } else {
    // Not marked nor on the allocation stack. Try to mark it.
    // This may or may not succeed, which is ok.
    bool success = false;
    if (kUseBakerReadBarrier) {
      success = ref->AtomicSetReadBarrierState(ReadBarrier::NonGrayState(),
                                               ReadBarrier::GrayState());
    } else {
      success = is_los ?
          !los_bitmap->AtomicTestAndSet(ref) :
          !mark_bitmap->AtomicTestAndSet(ref);
    }
    if (success) {
      if (kUseBakerReadBarrier) {
        DCHECK_EQ(ref->GetReadBarrierState(), ReadBarrier::GrayState());
      }
      PushOntoMarkStack(self, ref);
    }
  }
  return ref;
}

void ConcurrentCopying::FinishPhase() {
  Thread* const self = Thread::Current();
  {
    MutexLock mu(self, mark_stack_lock_);
    CHECK(revoked_mark_stacks_.empty());
    AssertEmptyThreadMarkStackMap();
    CHECK_EQ(pooled_mark_stacks_.size(), kMarkStackPoolSize);
  }
  // kVerifyNoMissingCardMarks relies on the region space cards not being cleared to avoid false
  // positives.
  if (!kVerifyNoMissingCardMarks && !use_generational_cc_) {
    TimingLogger::ScopedTiming split("ClearRegionSpaceCards", GetTimings());
    // We do not currently use the region space cards at all, madvise them away to save ram.
    heap_->GetCardTable()->ClearCardRange(region_space_->Begin(), region_space_->Limit());
  } else if (use_generational_cc_ && !young_gen_) {
    region_space_inter_region_bitmap_.Clear();
    non_moving_space_inter_region_bitmap_.Clear();
  }
  {
    MutexLock mu(self, skipped_blocks_lock_);
    skipped_blocks_map_.clear();
  }
  {
    ReaderMutexLock mu(self, *Locks::mutator_lock_);
    {
      WriterMutexLock mu2(self, *Locks::heap_bitmap_lock_);
      heap_->ClearMarkedObjects();
    }
    if (kUseBakerReadBarrier && kFilterModUnionCards) {
      TimingLogger::ScopedTiming split("FilterModUnionCards", GetTimings());
      ReaderMutexLock mu2(self, *Locks::heap_bitmap_lock_);
      for (space::ContinuousSpace* space : immune_spaces_.GetSpaces()) {
        DCHECK(space->IsImageSpace() || space->IsZygoteSpace());
        accounting::ModUnionTable* table = heap_->FindModUnionTableFromSpace(space);
        // Filter out cards that don't need to be set.
        if (table != nullptr) {
          table->FilterCards();
        }
      }
    }
    if (kUseBakerReadBarrier) {
      TimingLogger::ScopedTiming split("EmptyRBMarkBitStack", GetTimings());
      DCHECK(rb_mark_bit_stack_ != nullptr);
      const auto* limit = rb_mark_bit_stack_->End();
      for (StackReference<mirror::Object>* it = rb_mark_bit_stack_->Begin(); it != limit; ++it) {
        CHECK(it->AsMirrorPtr()->AtomicSetMarkBit(1, 0))
            << "rb_mark_bit_stack_->Begin()" << rb_mark_bit_stack_->Begin() << '\n'
            << "rb_mark_bit_stack_->End()" << rb_mark_bit_stack_->End() << '\n'
            << "rb_mark_bit_stack_->IsFull()"
            << std::boolalpha << rb_mark_bit_stack_->IsFull() << std::noboolalpha << '\n'
            << DumpReferenceInfo(it->AsMirrorPtr(), "*it");
      }
      rb_mark_bit_stack_->Reset();
    }
  }
  if (measure_read_barrier_slow_path_) {
    MutexLock mu(self, rb_slow_path_histogram_lock_);
    rb_slow_path_time_histogram_.AdjustAndAddValue(
        rb_slow_path_ns_.load(std::memory_order_relaxed));
    rb_slow_path_count_total_ += rb_slow_path_count_.load(std::memory_order_relaxed);
    rb_slow_path_count_gc_total_ += rb_slow_path_count_gc_.load(std::memory_order_relaxed);
  }
}

bool ConcurrentCopying::IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* field,
                                                    bool do_atomic_update) {
  mirror::Object* from_ref = field->AsMirrorPtr();
  if (from_ref == nullptr) {
    return true;
  }
  mirror::Object* to_ref = IsMarked(from_ref);
  if (to_ref == nullptr) {
    return false;
  }
  if (from_ref != to_ref) {
    if (do_atomic_update) {
      do {
        if (field->AsMirrorPtr() != from_ref) {
          // Concurrently overwritten by a mutator.
          break;
        }
      } while (!field->CasWeakRelaxed(from_ref, to_ref));
    } else {
      // TODO: Why is this seq_cst when the above is relaxed? Document memory ordering.
      field->Assign</* kIsVolatile= */ true>(to_ref);
    }
  }
  return true;
}

mirror::Object* ConcurrentCopying::MarkObject(mirror::Object* from_ref) {
  return Mark(Thread::Current(), from_ref);
}

void ConcurrentCopying::DelayReferenceReferent(ObjPtr<mirror::Class> klass,
                                               ObjPtr<mirror::Reference> reference) {
  heap_->GetReferenceProcessor()->DelayReferenceReferent(klass, reference, this);
}

void ConcurrentCopying::ProcessReferences(Thread* self) {
  TimingLogger::ScopedTiming split("ProcessReferences", GetTimings());
  // We don't really need to lock the heap bitmap lock as we use CAS to mark in bitmaps.
  WriterMutexLock mu(self, *Locks::heap_bitmap_lock_);
  GetHeap()->GetReferenceProcessor()->ProcessReferences(
      /*concurrent=*/ true, GetTimings(), GetCurrentIteration()->GetClearSoftReferences(), this);
}

void ConcurrentCopying::RevokeAllThreadLocalBuffers() {
  TimingLogger::ScopedTiming t(__FUNCTION__, GetTimings());
  region_space_->RevokeAllThreadLocalBuffers();
}

mirror::Object* ConcurrentCopying::MarkFromReadBarrierWithMeasurements(Thread* const self,
                                                                       mirror::Object* from_ref) {
  if (self != thread_running_gc_) {
    rb_slow_path_count_.fetch_add(1u, std::memory_order_relaxed);
  } else {
    rb_slow_path_count_gc_.fetch_add(1u, std::memory_order_relaxed);
  }
  ScopedTrace tr(__FUNCTION__);
  const uint64_t start_time = measure_read_barrier_slow_path_ ? NanoTime() : 0u;
  mirror::Object* ret =
      Mark</*kGrayImmuneObject=*/true, /*kNoUnEvac=*/false, /*kFromGCThread=*/false>(self,
                                                                                     from_ref);
  if (measure_read_barrier_slow_path_) {
    rb_slow_path_ns_.fetch_add(NanoTime() - start_time, std::memory_order_relaxed);
  }
  return ret;
}

void ConcurrentCopying::DumpPerformanceInfo(std::ostream& os) {
  GarbageCollector::DumpPerformanceInfo(os);
  size_t num_gc_cycles = GetCumulativeTimings().GetIterations();
  MutexLock mu(Thread::Current(), rb_slow_path_histogram_lock_);
  if (rb_slow_path_time_histogram_.SampleSize() > 0) {
    Histogram<uint64_t>::CumulativeData cumulative_data;
    rb_slow_path_time_histogram_.CreateHistogram(&cumulative_data);
    rb_slow_path_time_histogram_.PrintConfidenceIntervals(os, 0.99, cumulative_data);
  }
  if (rb_slow_path_count_total_ > 0) {
    os << "Slow path count " << rb_slow_path_count_total_ << "\n";
  }
  if (rb_slow_path_count_gc_total_ > 0) {
    os << "GC slow path count " << rb_slow_path_count_gc_total_ << "\n";
  }

  os << "Average " << (young_gen_ ? "minor" : "major") << " GC reclaim bytes ratio "
     << (reclaimed_bytes_ratio_sum_ / num_gc_cycles) << " over " << num_gc_cycles
     << " GC cycles\n";

  os << "Average " << (young_gen_ ? "minor" : "major") << " GC copied live bytes ratio "
     << (copied_live_bytes_ratio_sum_ / gc_count_) << " over " << gc_count_
     << " " << (young_gen_ ? "minor" : "major") << " GCs\n";

  os << "Cumulative bytes moved "
     << cumulative_bytes_moved_.load(std::memory_order_relaxed) << "\n";
  os << "Cumulative objects moved "
     << cumulative_objects_moved_.load(std::memory_order_relaxed) << "\n";

  os << "Peak regions allocated "
     << region_space_->GetMaxPeakNumNonFreeRegions() << " ("
     << PrettySize(region_space_->GetMaxPeakNumNonFreeRegions() * space::RegionSpace::kRegionSize)
     << ") / " << region_space_->GetNumRegions() / 2 << " ("
     << PrettySize(region_space_->GetNumRegions() * space::RegionSpace::kRegionSize / 2)
     << ")\n";
}

}  // namespace collector
}  // namespace gc
}  // namespace art
