#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <list>
#include <mutex>

#include "rk_mpi_mmz.h"
#include "log.h"
#include "rk_ion.h"
#include "rk_dmabuf.h"
#include "version.h"

struct BufferInfo {
    int32_t     fd;
    uint32_t    len;
    uint32_t    flags;
    void*       vaddr;
    uint64_t    paddr;
    uint32_t    hor_stride;
    uint32_t    ver_stride;
    void*       priv;
};

typedef std::list<MB_BLK> MB_LIST;
static MB_LIST mb_list;
static std::mutex mb_list_mutex;

// you can query version by 'strings' command: strings libmpimmz.so | grep git
const char* mpi_mmz_version = MPI_MMZ_BUILT_VERSION;

static MB_BLK create_blk_from_fd(int fd, RK_U32 len, uint32_t flags)
{
    // get buffer length
    if (len == 0) {
        off_t _size = dmabuf_get_size(fd);
        if (_size == (off_t) -1) {
            ALOGE("get buffer length failed: %s", strerror(errno));
            return (MB_BLK)NULL;
        }
        len = _size;
    }

    // get physic addr
    uint64_t paddr = (uint64_t)-1;
    if (ion_check_support()) {
        if (ion_get_phys(fd, &paddr) < 0) {
            //ALOGW("get physical address fail");
        }
    }

    // get virt addr
    void* vaddr = dmabuf_mmap(fd, 0, len);
    if (vaddr == NULL) {
        ALOGE("mmap failed: %s", strerror(errno));
        return (MB_BLK)NULL;
    }

    // setup pBlk
    struct BufferInfo *pBI = (struct BufferInfo *)malloc(sizeof(struct BufferInfo));
    if (pBI == NULL) {
        ALOGE("out of memory: %s", strerror(errno));
        return (MB_BLK)NULL;
    }

    memset(pBI, 0, sizeof(struct BufferInfo));
    pBI->fd = fd;
    pBI->len = len;
    pBI->paddr = paddr;
    pBI->vaddr = vaddr;
    pBI->flags = flags;

    return (MB_BLK)pBI;
}

/*
    申请内存并放在mb中返回
    成功 返回0
    失败 返回负值
 */
RK_S32 RK_MPI_MMZ_Alloc(MB_BLK *pBlk, RK_U32 u32Len, RK_U32 u32Flags)
{
    if (u32Len <= 0)
        return -1;

    bool is_cma = !!(u32Flags & RK_MMZ_ALLOC_TYPE_CMA);
    bool is_cacheable = !(u32Flags & RK_MMZ_ALLOC_UNCACHEABLE);

    // alloc
    int fd = -1;
    if (ion_check_support()) {
        if (ion_alloc(u32Len, is_cma, is_cacheable, &fd) < 0) {
            return -1;
        }
    } else {
        if (dmabuf_alloc(u32Len, is_cma, is_cacheable, &fd) < 0) {
            return -1;
        }
    }

    MB_BLK mb = create_blk_from_fd(fd, u32Len, u32Flags);

    if (mb == (MB_BLK)NULL) {
        close(fd);
        return -1;
    }

    {
    std::lock_guard<std::mutex> lck (mb_list_mutex);
    mb_list.push_back(mb);
    }

    *pBlk = mb;

    return 0;
}

/*
    释放mb所指向的内存
    返回0
 */
RK_S32 RK_MPI_MMZ_Free(MB_BLK mb)
{
    if (mb == NULL) {
        return 0;
    }

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    if (pBI->vaddr) {
        munmap(pBI->vaddr, pBI->len);
    }
    if (pBI->fd >= 0) {
        close(pBI->fd);
    }
    free(pBI);

    {
    std::lock_guard<std::mutex> lck (mb_list_mutex);
    mb_list.remove(mb);
    }

    return 0;
}

RK_VOID *RK_MPI_MMZ_Handle2VirAddr(MB_BLK mb)
{
    if (mb == NULL)
        return NULL;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    return pBI->vaddr;
}

RK_U64 RK_MPI_MMZ_Handle2PhysAddr(MB_BLK mb)
{
    if (mb == NULL)
        return (RK_U64)-1;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    return pBI->paddr;
}

RK_S32 RK_MPI_MMZ_Handle2Fd(MB_BLK mb)
{
    if (mb == NULL)
        return -1;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    return pBI->fd;
}

RK_U64 RK_MPI_MMZ_GetSize(MB_BLK mb)
{
    if (mb == NULL)
        return (RK_U64)-1;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    return pBI->len;
}

RK_S32 RK_MPI_MMZ_IsCacheable(MB_BLK mb)
{
    if (mb == NULL)
        return -1;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;

    if (pBI->flags == (uint32_t)-1)
        return -1;

    return (pBI->flags & RK_MMZ_ALLOC_UNCACHEABLE) ? 0 : 1;
}

MB_BLK RK_MPI_MMZ_Fd2Handle(RK_S32 fd)
{
    if (fd < 0)
        return (MB_BLK)NULL;

    // 查表
    std::lock_guard<std::mutex> lck (mb_list_mutex);
    for (MB_LIST::iterator iter = mb_list.begin(); iter != mb_list.end(); iter++)
    {
        struct BufferInfo *pBI = (struct BufferInfo *)*iter;
        if (pBI->fd == fd)
            return (MB_BLK)pBI;
    }

    return (MB_BLK)NULL;
}

MB_BLK RK_MPI_MMZ_VirAddr2Handle(RK_VOID *pstVirAddr)
{
    if (pstVirAddr == NULL)
        return (MB_BLK)NULL;

    std::lock_guard<std::mutex> lck (mb_list_mutex);
    for (MB_LIST::iterator iter = mb_list.begin(); iter != mb_list.end(); iter++)
    {
        struct BufferInfo *pBI = (struct BufferInfo *)*iter;
        if (pstVirAddr >= pBI->vaddr && pstVirAddr < (void*)((uint8_t*)(pBI->vaddr)+pBI->len)) {
            return (MB_BLK)pBI;
        }
    }

    return (MB_BLK)NULL;
}

MB_BLK RK_MPI_MMZ_PhyAddr2Handle(RK_U64 paddr)
{
    if (paddr == (uint64_t)-1)
        return (MB_BLK)NULL;

    std::lock_guard<std::mutex> lck (mb_list_mutex);
    for (MB_LIST::iterator iter = mb_list.begin(); iter != mb_list.end(); iter++)
    {
        struct BufferInfo *pBI = (struct BufferInfo *)*iter;
        if (paddr >= pBI->paddr && paddr < (pBI->paddr+pBI->len)) {
            return (MB_BLK)pBI;
        }
    }

    return (MB_BLK)NULL;
}

MB_BLK RK_MPI_MMZ_ImportFD(RK_S32 fd, RK_U32 len)
{
    if (fd < 0)
        return (MB_BLK)NULL;

    // 若已在表中，则返回失败
    if (RK_MPI_MMZ_Fd2Handle(fd) != (MB_BLK)NULL)
        return (MB_BLK)NULL;

    // 不在表中，则创建新的MB_BLK
    MB_BLK mb = create_blk_from_fd(fd, len, (uint32_t)-1);

    if (mb != (MB_BLK)NULL)
    {
        std::lock_guard<std::mutex> lck (mb_list_mutex);
        mb_list.push_back(mb);
    }

    return mb;
}

static RK_S32 RK_MPI_MMZ_FlushCache(MB_BLK mb, RK_U32 offset, RK_U32 length, RK_U32 flags, int is_start)
{
    if (mb == NULL)
        return -1;

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    int ret = 0;
    uint64_t dma_flags = (!!is_start)?DMA_BUF_SYNC_START:DMA_BUF_SYNC_END;

    if (offset >= pBI->len || pBI->fd <= 0)
        return -1;

    if ((length+offset) > pBI->len)
        length = pBI->len - offset;

    if (flags == RK_MMZ_SYNC_READONLY)
        dma_flags |= DMA_BUF_SYNC_READ;
    else if (flags == RK_MMZ_SYNC_WRITEONLY)
        dma_flags |= DMA_BUF_SYNC_WRITE;
    else
        dma_flags |= DMA_BUF_SYNC_READ|DMA_BUF_SYNC_WRITE;

    if (offset==0 && length==0) { // full sync
        ret = dmabuf_sync(pBI->fd, dma_flags);
    } else {
        ret = dmabuf_sync_partial(pBI->fd, offset, length, dma_flags);
    }

    return ret;
}

RK_S32 RK_MPI_MMZ_FlushCacheStart(MB_BLK mb, RK_U32 offset, RK_U32 length, RK_U32 flags)
{
    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 1);
}

RK_S32 RK_MPI_MMZ_FlushCacheEnd(MB_BLK mb, RK_U32 offset, RK_U32 length, RK_U32 flags)
{
    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 0);
}

RK_S32 RK_MPI_MMZ_FlushCacheVaddrStart(RK_VOID* vaddr, RK_U32 length, RK_U32 flags)
{
    MB_BLK mb = RK_MPI_MMZ_VirAddr2Handle(vaddr);

    if (mb == NULL || length == 0) {
        return -1;
    }

    uint32_t offset = (unsigned long)vaddr - (unsigned long)RK_MPI_MMZ_Handle2VirAddr(mb);

    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 1);
}

RK_S32 RK_MPI_MMZ_FlushCacheVaddrEnd(RK_VOID* vaddr, RK_U32 length, RK_U32 flags)
{
    MB_BLK mb = RK_MPI_MMZ_VirAddr2Handle(vaddr);

    if (mb == NULL || length == 0) {
        return -1;
    }

    uint32_t offset = (unsigned long)vaddr - (unsigned long)RK_MPI_MMZ_Handle2VirAddr(mb);

    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 0);
}

RK_S32 RK_MPI_MMZ_FlushCachePaddrStart(RK_U64 paddr, RK_U32 length, RK_U32 flags)
{
    MB_BLK mb = RK_MPI_MMZ_PhyAddr2Handle(paddr);

    if (mb == NULL || length == 0) {
        return -1;
    }

    uint32_t offset = paddr - RK_MPI_MMZ_Handle2PhysAddr(mb);

    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 1);
}

RK_S32 RK_MPI_MMZ_FlushCachePaddrEnd(RK_U64 paddr, RK_U32 length, RK_U32 flags)
{
    MB_BLK mb = RK_MPI_MMZ_PhyAddr2Handle(paddr);

    if (mb == NULL || length == 0) {
        return -1;
    }

    uint32_t offset = paddr - RK_MPI_MMZ_Handle2PhysAddr(mb);

    return RK_MPI_MMZ_FlushCache(mb, offset, length, flags, 0);
}

RK_S32 RK_MPI_SYS_CreateMB(MB_BLK *pBlk, MB_EXT_CONFIG_S *pstMbExtConfig)
{
    if (pstMbExtConfig==NULL || pstMbExtConfig->len==0 ||
        (pstMbExtConfig->paddr==0 && pstMbExtConfig->vaddr==NULL && pstMbExtConfig->fd<=0)) {
        return -1;
    }

    // setup pBlk
    struct BufferInfo *pBI = (struct BufferInfo *)malloc(sizeof(struct BufferInfo));
    if (pBI == NULL) {
        ALOGE("out of memory: %s", strerror(errno));
        return -2;
    }

    memset(pBI, 0, sizeof(struct BufferInfo));
    pBI->fd = pstMbExtConfig->fd;
    pBI->len = pstMbExtConfig->len;
    pBI->paddr = pstMbExtConfig->paddr;
    pBI->vaddr = pstMbExtConfig->vaddr;
    pBI->flags = (uint32_t)-1;

    {
    std::lock_guard<std::mutex> lck (mb_list_mutex);
    mb_list.push_back(pBI);
    }

    *pBlk = (MB_BLK)pBI;

    return 0;
}

RK_S32 RK_MPI_MB_SetBufferStride(MB_BLK mb, RK_U32 u32HorStride, RK_U32 u32VerStride)
{
    if (mb == NULL) {
        return -1;
    }

    struct BufferInfo *pBI = (struct BufferInfo *) mb;
    pBI->hor_stride = u32HorStride;
    pBI->ver_stride = u32VerStride;

    return 0;
}

RK_S32 RK_MPI_MB_GetBufferStride(MB_BLK mb, RK_U32 *pu32HorStride, RK_U32 *pu32VerStride)
{
    if (mb == NULL) {
        return -1;
    }

    struct BufferInfo *pBI = (struct BufferInfo *) mb;

    if (pu32HorStride)
        *pu32HorStride = pBI->hor_stride;

    if (pu32VerStride)
        *pu32VerStride = pBI->ver_stride;

    return 0;
}
