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

/*
    申请dmabuf内存
    is_cma 是否申请CMA heap内存，若为false则申请SYSTEM heap内存
    is_cacheable 申请的内存是否支持cache
    fd 存放返回的dmabuf fd
    成功 返回0
    失败 返回负值
 */
int dmabuf_alloc(uint32_t len, bool is_cma, bool is_cacheable, int *fd);

#ifdef __cplusplus
}
#endif

#endif