#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <vector>
#include <mutex>

#include "log.h"
#include "linux4.19/ion.h"
#include "rk_ion.h"

static int ion_fd = -1;
static std::vector<struct ion_heap_data> ion_heaps;
static std::mutex ion_heaps_mutex;

static int ion_open()
{
    if (ion_fd < 0) {
        ion_fd = open("/dev/ion", O_RDONLY | O_CLOEXEC);
        if (ion_fd < 0) {
            ALOGE("open /dev/ion failed: %s", strerror(errno));
        }
    }

    return ion_fd;
}

static void ion_close()
{
    if (ion_fd >= 0) {
        close(ion_fd);
        ion_fd = -1;
    }
}

static int ion_ioctl(int req, void* arg) {
    if (ion_open() < 0) {
        return -1;
    }

    int ret = ioctl(ion_fd, req, arg);
    if (ret < 0) {
        ALOGE("ioctl %x failed with code %d: %s", req, ret, strerror(errno));
    }

    return ret;
}

static int ion_query_heaps()
{
    int ret;
    struct ion_heap_query query;
    memset(&query, 0, sizeof(query));

    // query heap count
    ret = ion_ioctl(ION_IOC_HEAP_QUERY, &query);
    if (ret < 0) return ret;

    // query heap info
    ion_heaps.resize(query.cnt, {});
    query.heaps = (uintptr_t)ion_heaps.data();
    ret = ion_ioctl(ION_IOC_HEAP_QUERY, &query);
    if (ret < 0) {
        ion_heaps.resize(0, {});
    }

    return ret;
}

static uint32_t ion_get_heap_id_mask(uint32_t heap_type)
{
    {
    std::lock_guard<std::mutex> lck (ion_heaps_mutex);
    if (ion_heaps.empty()) {
        ion_query_heaps();
        if (ion_heaps.empty()) {
            return (uint32_t)-1;
        }
    }
    }

    for (const auto& heap : ion_heaps) {
        if (heap.type == heap_type)
            return 1<<heap.heap_id;
    }

    return (uint32_t)-1;
}

int ion_get_phys(int fd, uint64_t *paddr)
{
    struct ion_phys_data phys = {
        .fd = (uint32_t)fd,
        .padding = 0,
        .paddr = (uint64_t)-1,
    };

    int ret = ion_ioctl(ION_IOC_GET_PHYS, &phys);
    if (ret < 0) {
        // ignore
        return ret;
    }

    *paddr = phys.paddr;

    return 0;
}

int ion_alloc(uint32_t len, bool is_cma, bool is_cacheable, int *fd)
{
    // ion alloc
    struct ion_allocation_data data = {
        .len = len,
        .heap_id_mask = (uint32_t)-1,
    };

    if (is_cma) {
        data.heap_id_mask = ion_get_heap_id_mask(ION_HEAP_TYPE_DMA);
    } else {
        data.heap_id_mask = ion_get_heap_id_mask(ION_HEAP_TYPE_SYSTEM);
    }
    if (data.heap_id_mask == (uint32_t)-1) {
        ALOGE("Can not find heap %s\n", is_cma?"CMA":"SYSTEM");
        return -1;
    }

    if (is_cacheable)
        data.flags = ION_FLAG_CACHED;
    else
        data.flags = 0;

    int ret = ion_ioctl(ION_IOC_ALLOC, &data);
    if (ret < 0) {
        return -1;
    }

    *fd = data.fd;

    return 0;
}