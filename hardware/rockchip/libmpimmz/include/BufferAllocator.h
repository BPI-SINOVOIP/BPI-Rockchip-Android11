/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static const char kDmabufSystemHeapName[] = "system";
static const char kDmabufSystemUncachedHeapName[] = "system-uncached";
static const char kDmabufCmaHeapName[] = "cma";
static const char kDmabufCmaUncachedHeapName[] = "cma-uncached";

class BufferAllocator {
  public:
    BufferAllocator();
    ~BufferAllocator() {}

    /* Not copyable or movable */
    BufferAllocator(const BufferAllocator&) = delete;
    BufferAllocator& operator=(const BufferAllocator&) = delete;

    /* *
     * Returns a dmabuf fd if the allocation in one of the specified heaps is successful and
     * an error code otherwise. If dmabuf heaps are supported, tries to allocate in the
     * specified dmabuf heap. If allocation fails in the specified dmabuf heap and ion_fd is a
     * valid fd, goes through saved heap data to find a heap ID/mask to match the specified heap
     * names and allocates memory as per the specified parameters.
     * @heap_name: name of the heap to allocate in.
     * @len: size of the allocation.
     */
    int Alloc(const std::string& heap_name, size_t len);

    /* *
     * Returns a dmabuf fd if the allocation in system heap(cached/uncached) is successful and
     * an error code otherwise. Allocates in the 'system' heap if CPU access of
     * the buffer is expected and 'system-uncached' otherwise. If the 'system-uncached'
     * heap is not supported, falls back to the 'system' heap.
     * @cpu_access: indicates if CPU access of the buffer is expected.
     * @len: size of the allocation.
     */
    int AllocSystem(bool cpu_access, size_t len);

    /**
     * Query supported DMA-BUF heaps.
     *
     * @return the list of supported DMA-BUF heap names.
     */
    static std::unordered_set<std::string> GetDmabufHeapList();

  private:
    int OpenDmabufHeap(const std::string& name);
    bool DmabufHeapsSupported() { return !dmabuf_heap_fds_.empty(); }
    int DmabufAlloc(const std::string& heap_name, size_t len);

    /* Stores all open dmabuf_heap handles. */
    std::unordered_map<std::string, int> dmabuf_heap_fds_;
    /* Protects dma_buf_heap_fd_ from concurrent access */
    std::shared_mutex dmabuf_heap_fd_mutex_;
};
