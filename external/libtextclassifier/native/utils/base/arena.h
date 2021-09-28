/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Sometimes it is necessary to allocate a large number of small
// objects.  Doing this the usual way (malloc, new) is slow,
// especially for multithreaded programs.  A BaseArena provides a
// mark/release method of memory management: it asks for a large chunk
// from the operating system and doles it out bit by bit as required.
// Then you free all the memory at once by calling BaseArena::Reset().
//
//
// --Example Uses Of UnsafeArena
//    This is the simplest way.  Just create an arena, and whenever you
//    need a block of memory to put something in, call BaseArena::Alloc().  eg
//        s = arena.Alloc(100);
//        snprintf(s, 100, "%s:%d", host, port);
//        arena.Shrink(strlen(s)+1);     // optional; see below for use
//
//    You'll probably use the convenience routines more often:
//        s = arena.Strdup(host);        // a copy of host lives in the arena
//        s = arena.Strndup(host, 100);  // we guarantee to NUL-terminate!
//        s = arena.Memdup(protobuf, sizeof(protobuf);
//
//    If you go the Alloc() route, you'll probably allocate too-much-space.
//    You can reclaim the extra space by calling Shrink() before the next
//    Alloc() (or Strdup(), or whatever), with the #bytes you actually used.
//       If you use this method, memory management is easy: just call Alloc()
//    and friends a lot, and call Reset() when you're done with the data.
//
// FOR STRINGS: --Uses UnsafeArena
//    This is a special case of STL (below), but is simpler.  Use an
//    astring, which acts like a string but allocates from the passed-in
//    arena:
//       astring s(arena);             // or "sastring" to use a SafeArena
//       s.assign(host);
//       astring s2(host, hostlen, arena);

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_ARENA_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_ARENA_H_

#include <assert.h>
#include <string.h>

#include <vector>
#ifdef ADDRESS_SANITIZER
#include <sanitizer/asan_interface.h>
#endif

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"

namespace libtextclassifier3 {

// This class is "thread-compatible": different threads can access the
// arena at the same time without locking, as long as they use only
// const methods.
class BaseArena {
 protected:  // You can't make an arena directly; only a subclass of one
  BaseArena(char* first_block, const size_t block_size, bool align_to_page);

 public:
  virtual ~BaseArena();

  virtual void Reset();

  // they're "slow" only 'cause they're virtual (subclasses define "fast" ones)
  virtual char* SlowAlloc(size_t size) = 0;
  virtual void SlowFree(void* memory, size_t size) = 0;
  virtual char* SlowRealloc(char* memory, size_t old_size, size_t new_size) = 0;

  class Status {
   private:
    friend class BaseArena;
    size_t bytes_allocated_;

   public:
    Status() : bytes_allocated_(0) {}
    size_t bytes_allocated() const { return bytes_allocated_; }
  };

  // Accessors and stats counters
  // This accessor isn't so useful here, but is included so we can be
  // type-compatible with ArenaAllocator (in arena_allocator.h).  That is,
  // we define arena() because ArenaAllocator does, and that way you
  // can template on either of these and know it's safe to call arena().
  virtual BaseArena* arena() { return this; }
  size_t block_size() const { return block_size_; }
  int block_count() const;
  bool is_empty() const {
    // must check block count in case we allocated a block larger than blksize
    return freestart_ == freestart_when_empty_ && 1 == block_count();
  }

  // The alignment that ArenaAllocator uses except for 1-byte objects.
  static constexpr int kDefaultAlignment = 8;

 protected:
  bool SatisfyAlignment(const size_t alignment);
  void MakeNewBlock(const uint32 alignment);
  void* GetMemoryFallback(const size_t size, const int align);
  void* GetMemory(const size_t size, const int align) {
    assert(remaining_ <= block_size_);                   // an invariant
    if (size > 0 && size <= remaining_ && align == 1) {  // common case
      last_alloc_ = freestart_;
      freestart_ += size;
      remaining_ -= size;
#ifdef ADDRESS_SANITIZER
      ASAN_UNPOISON_MEMORY_REGION(last_alloc_, size);
#endif
      return reinterpret_cast<void*>(last_alloc_);
    }
    return GetMemoryFallback(size, align);
  }

  // This doesn't actually free any memory except for the last piece allocated
  void ReturnMemory(void* memory, const size_t size) {
    if (memory == last_alloc_ &&
        size == static_cast<size_t>(freestart_ - last_alloc_)) {
      remaining_ += size;
      freestart_ = last_alloc_;
    }
#ifdef ADDRESS_SANITIZER
    ASAN_POISON_MEMORY_REGION(memory, size);
#endif
  }

  // This is used by Realloc() -- usually we Realloc just by copying to a
  // bigger space, but for the last alloc we can realloc by growing the region.
  bool AdjustLastAlloc(void* last_alloc, const size_t newsize);

  Status status_;
  size_t remaining_;

 private:
  struct AllocatedBlock {
    char* mem;
    size_t size;
    size_t alignment;
  };

  // Allocate new new block of at least block_size, with the specified
  // alignment.
  // The returned AllocatedBlock* is valid until the next call to AllocNewBlock
  // or Reset (i.e. anything that might affect overflow_blocks_).
  AllocatedBlock* AllocNewBlock(const size_t block_size,
                                const uint32 alignment);

  const AllocatedBlock* IndexToBlock(int index) const;

  const size_t block_size_;
  char* freestart_;  // beginning of the free space in most recent block
  char* freestart_when_empty_;  // beginning of the free space when we're empty
  char* last_alloc_;            // used to make sure ReturnBytes() is safe
  // if the first_blocks_ aren't enough, expand into overflow_blocks_.
  std::vector<AllocatedBlock>* overflow_blocks_;
  // STL vector isn't as efficient as it could be, so we use an array at first
  const bool first_block_externally_owned_;  // true if they pass in 1st block
  const bool page_aligned_;  // when true, all blocks need to be page aligned
  int8_t blocks_alloced_;  // how many of the first_blocks_ have been allocated
  AllocatedBlock first_blocks_[16];  // the length of this array is arbitrary

  void FreeBlocks();  // Frees all except first block

  BaseArena(const BaseArena&) = delete;
  BaseArena& operator=(const BaseArena&) = delete;
};

class UnsafeArena : public BaseArena {
 public:
  // Allocates a thread-compatible arena with the specified block size.
  explicit UnsafeArena(const size_t block_size)
      : BaseArena(nullptr, block_size, false) {}
  UnsafeArena(const size_t block_size, bool align)
      : BaseArena(nullptr, block_size, align) {}

  // Allocates a thread-compatible arena with the specified block
  // size. "first_block" must have size "block_size". Memory is
  // allocated from "first_block" until it is exhausted; after that
  // memory is allocated by allocating new blocks from the heap.
  UnsafeArena(char* first_block, const size_t block_size)
      : BaseArena(first_block, block_size, false) {}
  UnsafeArena(char* first_block, const size_t block_size, bool align)
      : BaseArena(first_block, block_size, align) {}

  char* Alloc(const size_t size) {
    return reinterpret_cast<char*>(GetMemory(size, 1));
  }
  void* AllocAligned(const size_t size, const int align) {
    return GetMemory(size, align);
  }

  // Allocates and initializes an object on the arena.
  template <typename T, typename... Args>
  T* AllocAndInit(Args... args) {
    return new (reinterpret_cast<T*>(AllocAligned(sizeof(T), alignof(T))))
        T(std::forward<Args>(args)...);
  }

  char* Calloc(const size_t size) {
    void* return_value = Alloc(size);
    memset(return_value, 0, size);
    return reinterpret_cast<char*>(return_value);
  }

  void* CallocAligned(const size_t size, const int align) {
    void* return_value = AllocAligned(size, align);
    memset(return_value, 0, size);
    return return_value;
  }

  // Free does nothing except for the last piece allocated.
  void Free(void* memory, size_t size) { ReturnMemory(memory, size); }
  char* SlowAlloc(size_t size) override {  // "slow" 'cause it's virtual
    return Alloc(size);
  }
  void SlowFree(void* memory,
                size_t size) override {  // "slow" 'cause it's virt
    Free(memory, size);
  }
  char* SlowRealloc(char* memory, size_t old_size, size_t new_size) override {
    return Realloc(memory, old_size, new_size);
  }

  char* Memdup(const char* s, size_t bytes) {
    char* newstr = Alloc(bytes);
    memcpy(newstr, s, bytes);
    return newstr;
  }
  char* MemdupPlusNUL(const char* s, size_t bytes) {  // like "string(s, len)"
    char* newstr = Alloc(bytes + 1);
    memcpy(newstr, s, bytes);
    newstr[bytes] = '\0';
    return newstr;
  }
  char* Strdup(const char* s) { return Memdup(s, strlen(s) + 1); }
  // Unlike libc's strncpy, I always NUL-terminate.  libc's semantics are dumb.
  // This will allocate at most n+1 bytes (+1 is for the nul terminator).
  char* Strndup(const char* s, size_t n) {
    // Use memchr so we don't walk past n.
    // We can't use the one in //strings since this is the base library,
    // so we have to reinterpret_cast from the libc void*.
    const char* eos = reinterpret_cast<const char*>(memchr(s, '\0', n));
    // if no null terminator found, use full n
    const size_t bytes = (eos == nullptr) ? n : eos - s;
    return MemdupPlusNUL(s, bytes);
  }

  // You can realloc a previously-allocated string either bigger or smaller.
  // We can be more efficient if you realloc a string right after you allocate
  // it (eg allocate way-too-much space, fill it, realloc to just-big-enough)
  char* Realloc(char* original, size_t oldsize, size_t newsize);
  // If you know the new size is smaller (or equal), you don't need to know
  // oldsize.  We don't check that newsize is smaller, so you'd better be sure!
  char* Shrink(char* s, size_t newsize) {
    AdjustLastAlloc(s, newsize);  // reclaim space if we can
    return s;                     // never need to move if we go smaller
  }

  // We make a copy so you can keep track of status at a given point in time
  Status status() const { return status_; }

  // Number of bytes remaining before the arena has to allocate another block.
  size_t bytes_until_next_allocation() const { return remaining_; }

 private:
  UnsafeArena(const UnsafeArena&) = delete;
  UnsafeArena& operator=(const UnsafeArena&) = delete;

  virtual void UnusedKeyMethod();  // Dummy key method to avoid weak vtable.
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_ARENA_H_
