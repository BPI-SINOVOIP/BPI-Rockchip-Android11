#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "rk_dmabuf.h"
#include "log.h"

static int dmabuf_ioctl(int fd, int req, void* arg) {
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
        return -errno;
    }
    return ret;
}

int dmabuf_sync(int fd, uint64_t flags)
{
    struct dma_buf_sync sync;

    sync.flags = flags;
    return dmabuf_ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
}

int dmabuf_sync_partial(int fd, uint32_t offset, uint32_t len, uint64_t flags)
{
    struct dma_buf_sync_partial sync_p;

    sync_p.flags = flags;
    sync_p.offset = offset;
    sync_p.len = len;

    return dmabuf_ioctl(fd, DMA_BUF_IOCTL_SYNC_PARTIAL, &sync_p);
}

off_t dmabuf_get_size(int fd)
{
    off_t len = lseek(fd, 0, SEEK_END);

    lseek(fd, 0, SEEK_SET);
    return len;
}

void* dmabuf_mmap(int fd, off_t offset, size_t len)
{
    void* vaddr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);

    return (vaddr==MAP_FAILED) ? NULL : vaddr;
}
