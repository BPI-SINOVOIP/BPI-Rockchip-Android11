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

#include "BufferAllocator.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <shared_mutex>
#include <string>
#include <unordered_set>

#include "log.h"

static constexpr char kDmaHeapRoot[] = "/dev/dma_heap/";

int BufferAllocator::OpenDmabufHeap(const std::string& heap_name) {
    std::shared_lock<std::shared_mutex> slock(dmabuf_heap_fd_mutex_);

    /* Check if heap has already been opened. */
    auto it = dmabuf_heap_fds_.find(heap_name);
    if (it != dmabuf_heap_fds_.end())
        return it->second;

    slock.unlock();

    /*
     * Heap device needs to be opened, use a unique_lock since dmabuf_heap_fd_
     * needs to be modified.
     */
    std::unique_lock<std::shared_mutex> ulock(dmabuf_heap_fd_mutex_);

    /*
     * Check if we already opened this heap again to prevent racing threads from
     * opening the heap device multiple times.
     */
    it = dmabuf_heap_fds_.find(heap_name);
    if (it != dmabuf_heap_fds_.end()) return it->second;

    std::string heap_path = kDmaHeapRoot + heap_name;
    int fd = TEMP_FAILURE_RETRY(open(heap_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        ALOGE("Unable to find DMA-BUF heap: %s", heap_name.c_str());
        return -errno;
    }

    ALOGI("Using DMA-BUF heap named: %s", heap_name.c_str());

    auto ret = dmabuf_heap_fds_.insert({heap_name, fd});
    return fd;
}

BufferAllocator::BufferAllocator() {
}

int BufferAllocator::DmabufAlloc(const std::string& heap_name, size_t len) {
    int fd = OpenDmabufHeap(heap_name);
    if (fd < 0) return fd;

    struct dma_heap_allocation_data heap_data{
        .len = len,  // length of data to be allocated in bytes
        .fd_flags = O_RDWR | O_CLOEXEC,  // permissions for the memory to be allocated
    };

    auto ret = TEMP_FAILURE_RETRY(ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &heap_data));
    if (ret < 0) {
        ALOGE("Unable to allocate from DMA-BUF heap: %s", heap_name.c_str());
        return ret;
    }

    return heap_data.fd;
}

int BufferAllocator::Alloc(const std::string& heap_name, size_t len) {
    int fd = DmabufAlloc(heap_name, len);

    return fd;
}

int BufferAllocator::AllocSystem(bool cpu_access_needed, size_t len) {
    if (!cpu_access_needed) {
        /*
         * CPU does not need to access allocated buffer so we try to allocate in
         * the 'system-uncached' heap after querying for its existence.
         */
        static bool uncached_dmabuf_system_heap_support = [this]() -> bool {
            auto dmabuf_heap_list = this->GetDmabufHeapList();
            return (dmabuf_heap_list.find(kDmabufSystemUncachedHeapName) != dmabuf_heap_list.end());
        }();

        if (uncached_dmabuf_system_heap_support)
            return DmabufAlloc(kDmabufSystemUncachedHeapName, len);
    }

    /*
     * Either 1) CPU needs to access allocated buffer OR 2) CPU does not need to
     * access allocated buffer but the "system-uncached" heap is unsupported.
     */
    return Alloc(kDmabufSystemHeapName, len);
}

std::unordered_set<std::string> BufferAllocator::GetDmabufHeapList() {
    std::unordered_set<std::string> heap_list;
    std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(kDmaHeapRoot), closedir);

    if (dir) {
        struct dirent* dent;
        while ((dent = readdir(dir.get()))) {
            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) continue;

            heap_list.insert(dent->d_name);
        }
    }

    return heap_list;
}
