#ifndef __RK_DMABUF_H__
#define __RK_DMABUF_H__

#include <stdint.h>
#include <sys/mman.h>
#include "linux4.19/dma-buf.h"

#ifdef __cplusplus
extern "C" {
#endif

int dmabuf_sync(int fd, uint64_t flags);
int dmabuf_sync_partial(int fd, uint32_t offset, uint32_t len, uint64_t flags);
off_t dmabuf_get_size(int fd);
void* dmabuf_mmap(int fd, off_t offset, size_t len);

#ifdef __cplusplus
}
#endif

#endif