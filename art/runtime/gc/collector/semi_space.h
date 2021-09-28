/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_H_
#define ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_H_

#include <memory>

#include "base/atomic.h"
#include "base/locks.h"
#include "base/macros.h"
#include "garbage_collector.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc_root.h"
#include "immune_spaces.h"
#include "mirror/object_reference.h"
#include "offsets.h"

namespace art {

class Thread;

namespace mirror {
class Class;
class Object;
}  // namespace mirror

namespace gc {

class Heap;

namespace accounting {
template <typename T> class AtomicStack;
typedef AtomicStack<mirror::Object> ObjectStack;
}  // namespace accounting

namespace space {
class ContinuousMemMapAllocSpace;
class ContinuousSpace;
}  // namespace space

namespace collector {

class SemiSpace : public GarbageCollector {
 public:
  // If true, use remembered sets in the generational mode.
  static constexpr bool kUseRememberedSet = true;

  explicit SemiSpace(Heap* heap, const std::string& name_prefix = "");

  ~SemiSpace() {}

  void RunPhases() override NO_THREAD_SAFETY_ANALYSIS;
  virtual void InitializePhase();
  virtual void MarkingPhase() REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::heap_bitmap_lock_);
  virtual void ReclaimPhase() REQUIRES(Locks::mutator_lock_)
      REQUIRES(!Locks::heap_bitmap_lock_);
  virtual void FinishPhase() REQUIRES(Locks::mutator_lock_);
  void MarkReachableObjects()
      REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);
  GcType GetGcType() const override {
    return kGcTypePartial;
  }
  CollectorType GetCollectorType() const override {
    return kCollectorTypeSS;
  }

  // Sets which space we will be copying objects to.
  void SetToSpace(space::ContinuousMemMapAllocSpace* to_space);

  // Set the space where we copy objects from.
  void SetFromSpace(space::ContinuousMemMapAllocSpace* from_space);

  // Set whether or not we swap the semi spaces in the heap. This needs to be done with mutators
  // suspended.
  void SetSwapSemiSpaces(bool swap_semi_spaces) {
    swap_semi_spaces_ = swap_semi_spaces;
  }

  // Initializes internal structures.
  void Init();

  // Find the default mark bitmap.
  void FindDefaultMarkBitmap();

  // Updates obj_ptr if the object has moved. Takes either an ObjectReference or a HeapReference.
  template<typename CompressedReferenceType>
  void MarkObject(CompressedReferenceType* obj_ptr)
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  template<typename CompressedReferenceType>
  void MarkObjectIfNotInToSpace(CompressedReferenceType* obj_ptr)
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  mirror::Object* MarkObject(mirror::Object* root) override
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void MarkHeapReference(mirror::HeapReference<mirror::Object>* obj_ptr,
                         bool do_atomic_update) override
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void ScanObject(mirror::Object* obj)
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void VerifyNoFromSpaceReferences(mirror::Object* obj)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Marks the root set at the start of a garbage collection.
  void MarkRoots()
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Bind the live bits to the mark bits of bitmaps for spaces that are never collected, ie
  // the image. Mark that portion of the heap as immune.
  virtual void BindBitmaps() REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::heap_bitmap_lock_);

  void UnBindBitmaps()
      REQUIRES(Locks::heap_bitmap_lock_);

  void ProcessReferences(Thread* self) REQUIRES(Locks::mutator_lock_)
      REQUIRES(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  virtual void Sweep(bool swap_bitmaps)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Sweeps unmarked objects to complete the garbage collection.
  void SweepLargeObjects(bool swap_bitmaps) REQUIRES(Locks::heap_bitmap_lock_);

  void SweepSystemWeaks()
      REQUIRES_SHARED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  void VisitRoots(mirror::Object*** roots, size_t count, const RootInfo& info) override
      REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  void VisitRoots(mirror::CompressedReference<mirror::Object>** roots,
                  size_t count,
                  const RootInfo& info) override
      REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  virtual mirror::Object* MarkNonForwardedObject(mirror::Object* obj)
      REQUIRES(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

  // Schedules an unmarked object for reference processing.
  void DelayReferenceReferent(ObjPtr<mirror::Class> klass, ObjPtr<mirror::Reference> reference)
      override REQUIRES_SHARED(Locks::heap_bitmap_lock_, Locks::mutator_lock_);

 protected:
  // Returns null if the object is not marked, otherwise returns the forwarding address (same as
  // object for non movable things).
  mirror::Object* IsMarked(mirror::Object* object) override
      REQUIRES(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_);

  bool IsNullOrMarkedHeapReference(mirror::HeapReference<mirror::Object>* object,
                                   bool do_atomic_update) override
      REQUIRES(Locks::mutator_lock_)
      REQUIRES_SHARED(Locks::heap_bitmap_lock_);

  // Marks or unmarks a large object based on whether or not set is true. If set is true, then we
  // mark, otherwise we unmark.
  bool MarkLargeObject(const mirror::Object* obj)
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Expand mark stack to 2x its current size.
  void ResizeMarkStack(size_t new_size) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true if we should sweep the space.
  virtual bool ShouldSweepSpace(space::ContinuousSpace* space) const;

  // Push an object onto the mark stack.
  void MarkStackPush(mirror::Object* obj) REQUIRES_SHARED(Locks::mutator_lock_);

  void UpdateAndMarkModUnion()
      REQUIRES(Locks::heap_bitmap_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Recursively blackens objects on the mark stack.
  void ProcessMarkStack() override
      REQUIRES(Locks::mutator_lock_, Locks::heap_bitmap_lock_);

  inline mirror::Object* GetForwardingAddressInFromSpace(mirror::Object* obj) const
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Revoke all the thread-local buffers.
  void RevokeAllThreadLocalBuffers() override;

  // Current space, we check this space first to avoid searching for the appropriate space for an
  // object.
  accounting::ObjectStack* mark_stack_;

  // Every object inside the immune spaces is assumed to be marked.
  ImmuneSpaces immune_spaces_;

  // Destination and source spaces (can be any type of ContinuousMemMapAllocSpace which either has
  // a live bitmap or doesn't).
  space::ContinuousMemMapAllocSpace* to_space_;
  // Cached live bitmap as an optimization.
  accounting::ContinuousSpaceBitmap* to_space_live_bitmap_;
  space::ContinuousMemMapAllocSpace* from_space_;
  // Cached mark bitmap as an optimization.
  accounting::HeapBitmap* mark_bitmap_;

  Thread* self_;

  // The space which we copy to if the to_space_ is full.
  space::ContinuousMemMapAllocSpace* fallback_space_;

  // How many objects and bytes we moved, used so that we don't need to Get the size of the
  // to_space_ when calculating how many objects and bytes we freed.
  size_t bytes_moved_;
  size_t objects_moved_;

  // How many bytes we avoided dirtying.
  size_t saved_bytes_;

  // The name of the collector.
  std::string collector_name_;

  // Used for the generational mode. The default interval of the whole
  // heap collection. If N, the whole heap collection occurs every N
  // collections.
  static constexpr int kDefaultWholeHeapCollectionInterval = 5;

  // Whether or not we swap the semi spaces in the heap during the marking phase.
  bool swap_semi_spaces_;

 private:
  class BitmapSetSlowPathVisitor;
  class MarkObjectVisitor;
  class VerifyNoFromSpaceReferencesVisitor;
  DISALLOW_IMPLICIT_CONSTRUCTORS(SemiSpace);
};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_COLLECTOR_SEMI_SPACE_H_
