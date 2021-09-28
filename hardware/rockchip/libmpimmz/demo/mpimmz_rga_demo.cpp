#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <sys/mman.h>

#include "im2d_api/im2d.h"

#include "rk_mpi_mmz.h"

static const int width = 1920;
static const int height = 1088;

static int do_sync = 0;
static struct timespec tik, tok;

static inline int time_diff_us(struct timespec* begin, struct timespec* end)
{
    return (end->tv_sec - begin->tv_sec) * 1000000 + (end->tv_nsec - begin->tv_nsec) / 1000;
}

// 数据相同返回0，否则返回1
int check_data(const void* dst, const void* src, int len)
{
    int ret = 0;
    uint8_t* ch_src = (uint8_t*)src;
    uint8_t* ch_dst = (uint8_t*)dst;

    for(int i=0; i<len; i++) {
        if (ch_dst[i] != ch_src[i]) {
            printf("unmatch in %d src[0x%x] dst[0x%x]\n", i, ch_src[i], ch_dst[i]);
            ret = 1;
            break;
        }
    }

    return ret;
}

int rga_copy(MB_BLK mb_dst, MB_BLK mb_src, int len)
{
    rga_buffer_t src, dst;
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    src = wrapbuffer_physicaladdr((void *)RK_MPI_MMZ_Handle2PhysAddr(mb_src), width, height, 0x15);
    dst = wrapbuffer_physicaladdr((void *)RK_MPI_MMZ_Handle2PhysAddr(mb_dst), width, height, 0x15);

    // 使用rga把src地址的数据拷贝到dst地址
    clock_gettime(CLOCK_MONOTONIC, &tik);
    imcopy(src, dst);
    clock_gettime(CLOCK_MONOTONIC, &tok);

    // 检查数据拷贝是否成功
    void* vaddr_src = RK_MPI_MMZ_Handle2VirAddr(mb_src);
    void* vaddr_dst = RK_MPI_MMZ_Handle2VirAddr(mb_dst);

    if (do_sync) RK_MPI_MMZ_FlushCacheStart(mb_dst, 0, 0, RK_MMZ_SYNC_READONLY);
    int ret = check_data(vaddr_dst, vaddr_src, len);
    if (do_sync) RK_MPI_MMZ_FlushCacheEnd(mb_dst, 0, 0, RK_MMZ_SYNC_READONLY);
    return ret;
}

int cpu_copy(MB_BLK mb_dst, MB_BLK mb_src, int len)
{
    void* vaddr_src = RK_MPI_MMZ_Handle2VirAddr(mb_src);
    void* vaddr_dst = RK_MPI_MMZ_Handle2VirAddr(mb_dst);

    clock_gettime(CLOCK_MONOTONIC, &tik);
    memcpy(vaddr_dst, vaddr_src, len);
    clock_gettime(CLOCK_MONOTONIC, &tok);

    // 检查数据拷贝是否成功
    return check_data(vaddr_dst, vaddr_src, len);
}

int main(int argc, const char** argv)
{
    int ret;
    int len = 3133440;//width*height*3/2;//
    int flags = RK_MMZ_ALLOC_TYPE_IOMMU | RK_MMZ_ALLOC_CACHEABLE;
    int test_rga = 0;

    // 绑定到CPU2
    cpu_set_t sCPUSet;
    CPU_ZERO(&sCPUSet);
    CPU_SET(2, &sCPUSet);
    sched_setaffinity(0, sizeof(sCPUSet), &sCPUSet);

    // 定到最高频
    system("echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
    system("echo performance > /sys/devices/system/cpu/cpufreq/policy4/scaling_governor");

    do_sync = 0;
    printf("Usage: %s [--cma] [--uncache] [--rga] [--sync]\n\n", argv[0]);

    // 参数解析
    for (int i=1; i<argc; i++)
    {
        if (!strcmp(argv[i], "--cma"))
            flags |= RK_MMZ_ALLOC_TYPE_CMA;
        else if (!strcmp(argv[i], "--uncache"))
            flags |= RK_MMZ_ALLOC_UNCACHEABLE;
        else if (!strcmp(argv[i], "--rga"))
            test_rga = 1;
        else if (!strcmp(argv[i], "--sync"))
            do_sync = 1;
    }

    printf("Memory: %s %s Cache, use %s copy, %s sync\n\n",
        (flags&RK_MMZ_ALLOC_TYPE_CMA)?"CMA":"Non-CMA",
        (flags&RK_MMZ_ALLOC_UNCACHEABLE)?"without":"with",
        test_rga?"RGA":"CPU",
        do_sync?"do":"not");

    if (!(flags&RK_MMZ_ALLOC_TYPE_CMA) && test_rga) {
        printf("unsupport rga copy for Non-CMA memory!\n");
        return -1;
    }

    for (int i=0; i<10; i++)
    {
        MB_BLK mb_src, mb_dst;
        void* vaddr_src;
        void* vaddr_dst;

        // 分配内存
        ret = RK_MPI_MMZ_Alloc(&mb_src, len, flags);
        if (ret < 0) {
            printf("alloc src fail\n");
            return ret;
        }

        ret = RK_MPI_MMZ_Alloc(&mb_dst, len, flags);
        if (ret < 0) {
            printf("alloc dst fail\n");
            return ret;
        }

        // 填充数据
        vaddr_src = RK_MPI_MMZ_Handle2VirAddr(mb_src);
        if (do_sync) RK_MPI_MMZ_FlushCacheStart(mb_src, 0, 0, RK_MMZ_SYNC_WRITEONLY);
        memset(vaddr_src, 0x5A, len);
        if (do_sync) RK_MPI_MMZ_FlushCacheEnd(mb_src, 0, 0, RK_MMZ_SYNC_WRITEONLY);

        vaddr_dst = RK_MPI_MMZ_Handle2VirAddr(mb_dst);
        if (do_sync) RK_MPI_MMZ_FlushCacheStart(mb_dst, 0, 0, RK_MMZ_SYNC_WRITEONLY);
        memset(vaddr_dst, 0, len);
        if (do_sync) RK_MPI_MMZ_FlushCacheEnd(mb_dst, 0, 0, RK_MMZ_SYNC_WRITEONLY);

        // 复制数据
        if (test_rga == 1) {
            if (rga_copy(mb_dst, mb_src, len) == 0) {
                printf("%d: RGA copy okay, size: %d, use time: %d us\n", i, len, time_diff_us(&tik, &tok));
            } else {
                printf("%d: RGA copy fail.\n", i);
            }
        } else {
            if (cpu_copy(mb_dst, mb_src, len) == 0) {
                printf("%d: CPU copy okay, size: %d, use time: %d us\n", i, len, time_diff_us(&tik, &tok));
            } else {
                printf("%d: CPU copy fail.\n", i);
            }
        }

        // 释放内存
        RK_MPI_MMZ_Free(mb_src);
        RK_MPI_MMZ_Free(mb_dst);

        sleep(1);
    }

    return 0;
}
