#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "rk_mpi_mmz.h"

#define vaddr_to_fd_offset(vaddr, fd, offset) do {\
    MB_BLK __blk = RK_MPI_MMZ_VirAddr2Handle(vaddr); \
    if (__blk != (MB_BLK)NULL) { \
        void* __vaddr = RK_MPI_MMZ_Handle2VirAddr(__blk); \
        offset = (unsigned long)vaddr - (unsigned long)__vaddr; \
        fd = RK_MPI_MMZ_Handle2Fd(__blk); \
    } else { \
        offset = -1; \
        fd = -1; \
    } \
} while (0);

#define paddr_to_fd_offset(paddr, fd, offset) do {\
    MB_BLK __blk = RK_MPI_MMZ_PhyAddr2Handle(paddr); \
    if (__blk != (MB_BLK)NULL) { \
        uint64_t __paddr = RK_MPI_MMZ_Handle2PhysAddr(__blk); \
        offset = (unsigned long)paddr - (unsigned long)__paddr; \
        fd = RK_MPI_MMZ_Handle2Fd(__blk); \
    } else { \
        offset = -1; \
        fd = -1; \
    } \
} while (0);

/*
static inline int time_diff_ms(struct timespec* before, struct timespec* after)
{
    return (after->tv_sec - before->tv_sec) * 1000 + (after->tv_nsec - before->tv_nsec) / 1000000;
}
*/

int main(int argc, const char** argv)
{
    int len = 128*1024;
    int ret;
    MB_BLK mb;
    int flags = RK_MMZ_ALLOC_TYPE_IOMMU | RK_MMZ_ALLOC_CACHEABLE;
    printf("Usage: %s [--cma] [--uncache] len\n", argv[0]);

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--cma"))
            flags |= RK_MMZ_ALLOC_TYPE_CMA;
        else if (!strcmp(argv[i], "--uncache"))
            flags |= RK_MMZ_ALLOC_UNCACHEABLE;
        else
            len = atoi(argv[i]);
    }

    while(1) {
        ret = RK_MPI_MMZ_Alloc(&mb, len, flags);
        if (ret < 0) {
            return ret;
        }

        void* vaddr = RK_MPI_MMZ_Handle2VirAddr(mb);
        RK_U64 paddr = RK_MPI_MMZ_Handle2PhysAddr(mb);
        int fd = RK_MPI_MMZ_Handle2Fd(mb);
        len = RK_MPI_MMZ_GetSize(mb);
        int is_cacheable = RK_MPI_MMZ_IsCacheable(mb);

        printf("alloc buffer: fd=%d, len=%d, paddr=%"PRIx64", vaddr=%p, cacheable=%d\n", fd, len, paddr, vaddr, is_cacheable);

        MB_BLK mb_by_fd = RK_MPI_MMZ_Fd2Handle(fd);
        MB_BLK mb_by_vaddr = RK_MPI_MMZ_VirAddr2Handle(vaddr);
        printf("MB: %p %p %p\n", mb, mb_by_fd, mb_by_vaddr);

        void* vaddr_temp = (char*)vaddr + 1;
        printf("vaddr+1: %p, mb: %p\n", vaddr_temp, RK_MPI_MMZ_VirAddr2Handle(vaddr_temp));
        vaddr_temp = (char*)vaddr - 1;
        printf("vaddr-1: %p, mb: %p\n", vaddr_temp, RK_MPI_MMZ_VirAddr2Handle(vaddr_temp));
        vaddr_temp = (char*)vaddr + len;
        printf("vaddr+len: %p, mb: %p\n", vaddr_temp, RK_MPI_MMZ_VirAddr2Handle(vaddr_temp));
        vaddr_temp = (char*)vaddr + len - 1;
        printf("vaddr+len-1: %p, mb: %p\n", vaddr_temp, RK_MPI_MMZ_VirAddr2Handle(vaddr_temp));

        RK_U64 paddr_temp = paddr+1;
        printf("paddr+1: 0x%"PRIx64", mb: %p\n", paddr_temp, RK_MPI_MMZ_PhyAddr2Handle(paddr_temp));
        paddr_temp = paddr-1;
        printf("paddr-1: 0x%"PRIx64", mb: %p\n", paddr_temp, RK_MPI_MMZ_PhyAddr2Handle(paddr_temp));
        paddr_temp = paddr+len;
        printf("paddr+len: 0x%"PRIx64", mb: %p\n", paddr_temp, RK_MPI_MMZ_PhyAddr2Handle(paddr_temp));
        paddr_temp = paddr+len-1;
        printf("paddr+len-1: 0x%"PRIx64", mb: %p\n", paddr_temp, RK_MPI_MMZ_PhyAddr2Handle(paddr_temp));

        uint32_t offset;
        vaddr_temp = (char*)vaddr + len - 10;
        vaddr_to_fd_offset(vaddr_temp, fd, offset);
        printf("vaddr+len-10: %p, fd: %d, offset: %d\n", vaddr_temp, fd, offset);

        paddr_temp = paddr+len-10;
        paddr_to_fd_offset(paddr_temp, fd, offset);
        printf("paddr+len-10: 0x%"PRIx64", fd: %d, offset: %d\n", paddr_temp, fd, offset);

        ret = RK_MPI_MMZ_FlushCacheStart(mb, 0, 0, RK_MMZ_SYNC_WRITEONLY);
        memset(vaddr, 0x5A, len);
        ret = RK_MPI_MMZ_FlushCacheEnd(mb, 0, 0, RK_MMZ_SYNC_WRITEONLY);

        ret = RK_MPI_MMZ_FlushCacheStart(mb, 4096, 4096, RK_MMZ_SYNC_RW);
        memset(vaddr, 0x5A, len);
        ret = RK_MPI_MMZ_FlushCacheEnd(mb, 4096, 4096, RK_MMZ_SYNC_RW);

        vaddr_temp = (char*)vaddr + len - 10;
        ret = RK_MPI_MMZ_FlushCacheVaddrStart(vaddr_temp, 4096, RK_MMZ_SYNC_WRITEONLY);
        memset(vaddr_temp, 0x5A, 10);
        ret = RK_MPI_MMZ_FlushCacheVaddrEnd(vaddr_temp, 4096, RK_MMZ_SYNC_WRITEONLY);

        paddr_temp = paddr_temp+len-10;
        ret = RK_MPI_MMZ_FlushCachePaddrStart(paddr_temp, 4096, RK_MMZ_SYNC_WRITEONLY);
        vaddr_temp = (char*)vaddr + len - 10;
        memset(vaddr_temp, 0x5A, 10);
        ret = RK_MPI_MMZ_FlushCachePaddrEnd(paddr_temp, 4096, RK_MMZ_SYNC_WRITEONLY);

        usleep(100000);
        RK_MPI_MMZ_Free(mb);
    }

    return 0;
}
