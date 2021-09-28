// Copyright 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "goldfish_dma.h"

#include <qemu_pipe_bp.h>

#if PLATFORM_SDK_VERSION < 26
#include <cutils/log.h>
#else
#include <log/log.h>
#endif
#include <errno.h>
#ifdef __ANDROID__
#include <linux/ioctl.h>
#include <linux/types.h>
#include <sys/cdefs.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* There is an ioctl associated with goldfish dma driver.
 * Make it conflict with ioctls that are not likely to be used
 * in the emulator.
 * 'G'	00-3F	drivers/misc/sgi-gru/grulib.h	conflict!
 * 'G'	00-0F	linux/gigaset_dev.h	conflict!
 */
#define GOLDFISH_DMA_IOC_MAGIC	'G'

#define GOLDFISH_DMA_IOC_LOCK			_IOWR(GOLDFISH_DMA_IOC_MAGIC, 0, struct goldfish_dma_ioctl_info)
#define GOLDFISH_DMA_IOC_UNLOCK			_IOWR(GOLDFISH_DMA_IOC_MAGIC, 1, struct goldfish_dma_ioctl_info)
#define GOLDFISH_DMA_IOC_GETOFF			_IOWR(GOLDFISH_DMA_IOC_MAGIC, 2, struct goldfish_dma_ioctl_info)
#define GOLDFISH_DMA_IOC_CREATE_REGION	_IOWR(GOLDFISH_DMA_IOC_MAGIC, 3, struct goldfish_dma_ioctl_info)

struct goldfish_dma_ioctl_info {
    uint64_t phys_begin;
    uint64_t size;
};

int goldfish_dma_create_region(uint32_t sz, struct goldfish_dma_context* res) {

    res->fd = qemu_pipe_open("opengles");
    res->mapped_addr = 0;
    res->size = 0;

    if (res->fd > 0) {
        // now alloc
        struct goldfish_dma_ioctl_info info;
        info.size = sz;
        int alloc_res = ioctl(res->fd, GOLDFISH_DMA_IOC_CREATE_REGION, &info);

        if (alloc_res) {
            ALOGE("%s: failed to allocate DMA region. errno=%d",
                  __FUNCTION__, errno);
            close(res->fd);
            res->fd = -1;
            return alloc_res;
        }

        res->size = sz;
        ALOGV("%s: successfully allocated goldfish DMA region with size %u cxt=%p fd=%d",
              __FUNCTION__, sz, res, res->fd);
        return 0;
    } else {
        ALOGE("%s: could not obtain fd to device! fd %d errno=%d\n",
              __FUNCTION__, res->fd, errno);
        return ENODEV;
    }
}

void* goldfish_dma_map(struct goldfish_dma_context* cxt) {
    ALOGV("%s: on fd %d errno=%d", __FUNCTION__, cxt->fd, errno);
    void *mapped = mmap(0, cxt->size, PROT_WRITE, MAP_SHARED, cxt->fd, 0);
    ALOGV("%s: cxt=%p mapped=%p size=%u errno=%d",
        __FUNCTION__, cxt, mapped, cxt->size, errno);

    if (mapped == MAP_FAILED) {
        mapped = NULL;
    }

    cxt->mapped_addr = reinterpret_cast<uint64_t>(mapped);
    return mapped;
}

int goldfish_dma_unmap(struct goldfish_dma_context* cxt) {
    ALOGV("%s: cxt=%p mapped=0x%" PRIu64, __FUNCTION__, cxt, cxt->mapped_addr);
    munmap(reinterpret_cast<void *>(cxt->mapped_addr), cxt->size);
    cxt->mapped_addr = 0;
    cxt->size = 0;
    return 0;
}

void goldfish_dma_write(struct goldfish_dma_context* cxt,
                               const void* to_write,
                               uint32_t sz) {
    ALOGV("%s: cxt=%p mapped=0x%" PRIu64 " to_write=%p size=%u",
        __FUNCTION__, cxt, cxt->mapped_addr, to_write, sz);
    memcpy(reinterpret_cast<void *>(cxt->mapped_addr), to_write, sz);
}

void goldfish_dma_free(goldfish_dma_context* cxt) {
    close(cxt->fd);
}

uint64_t goldfish_dma_guest_paddr(const struct goldfish_dma_context* cxt) {
    struct goldfish_dma_ioctl_info info;
    ioctl(cxt->fd, GOLDFISH_DMA_IOC_GETOFF, &info);
    return info.phys_begin;
}
