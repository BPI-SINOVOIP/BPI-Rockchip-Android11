/*
 *
 * Copyright 2014 Rockchip Electronics S.LSI Co. LTD
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
/*
   * File:
   * vpu_mem_dmabuf.c
   * Description:
   *
   * Author:
   *     Alpha Lin
   * Date:
   *    2014-1-23
 */

#define ALOG_TAG "vpu_mem_dmabuf"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>
#include <errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vpu_mem.h"
#include "vpu_list.h"
#include <utils/Log.h>
#include <linux/ion.h>
#include "libion_vpu/ionalloc_vpu.h"
#include "vpu_mem_pool/vpu_mem_pool.h"
#include "vpu_mem_pool/vpu_dma_buf.h"
#include "vpu.h"
#define LOG_TAG "vpu_mem_dmabuf"
#ifdef AVS40
#undef ALOGV
#define ALOGV LOGV

#undef ALOGD
#define ALOGD LOGD

#undef ALOGI
#define ALOGI LOGI

#undef ALOGW
#define ALOGW LOGW

#undef ALOGE
#define ALOGE LOGE
#endif

#define VPU_MEM_IOCTL_MAGIC 'p'
#define VPU_MEM_GET_PHYS        _IOW(VPU_MEM_IOCTL_MAGIC, 1, unsigned int)
#define VPU_MEM_GET_TOTAL_SIZE  _IOW(VPU_MEM_IOCTL_MAGIC, 2, unsigned int)
#define VPU_MEM_ALLOCATE        _IOW(VPU_MEM_IOCTL_MAGIC, 3, unsigned int)
#define VPU_MEM_FREE            _IOW(VPU_MEM_IOCTL_MAGIC, 4, unsigned int)
#define VPU_MEM_CACHE_FLUSH     _IOW(VPU_MEM_IOCTL_MAGIC, 5, unsigned int)
#define VPU_MEM_DUPLICATE       _IOW(VPU_MEM_IOCTL_MAGIC, 6, unsigned int)
#define VPU_MEM_LINK            _IOW(VPU_MEM_IOCTL_MAGIC, 7, unsigned int)
#define VPU_MEM_CACHE_CLEAN     _IOW(VPU_MEM_IOCTL_MAGIC, 8, unsigned int)
#define VPU_MEM_CACHE_INVALID   _IOW(VPU_MEM_IOCTL_MAGIC, 9, unsigned int)
#define VPU_MEM_GET_COUNT       _IOW(VPU_MEM_IOCTL_MAGIC, 10, unsigned int)
#define VPU_MEM_GET_FREE_SIZE   _IO(VPU_MEM_IOCTL_MAGIC, 11)

//#define _VPU_MEM_DEUBG
#define _VPU_MEM_ERROR

#ifdef _VPU_MEM_DEUBG
#define VPM_DEBUG(fmt, args...) ALOGD(fmt, ## args)
#else
#define VPM_DEBUG(fmt, args...) /* not debugging: nothing */
#endif

#ifdef _VPU_MEM_ERROR
#define VPM_ERROR(fmt, args...) ALOGE(fmt, ## args)
#else
#define VPM_ERROR(fmt, args...)
#endif

#define ALIGN_SIZE()    (4096)
#define ALIGN_4K(x)     ((x+(4095))&(~4095))
#define SIZE_TO_PFN(x)  ((ALIGN_4K(x))/ALIGN_SIZE())
#define MAX_FD          (0x7FFF)
#define INVAILD_PTR     ((void *)(-1))

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#endif
typedef struct {
    pthread_mutex_t mutex;
    RK_U32 pool_en;
    struct list_head list_pool;

    union {
        struct {
            void *vir_base;
            RK_U32 phy_base;
            RK_U32 size;
            int fd;
        } original;

        struct {
            ion_device_t *  dev;
        } ion;

        struct {
            int res;
            struct vpu_dmabuf_dev *dev;
        } dma_buf;
    } share;
} vpu_mem_file;

typedef struct vpu_mem_pool {
    struct list_head list;
    RK_U32 pfn;
    RK_S32 count;
} vpu_mem_pool;

typedef struct vpu_mem_pool_config {
    RK_U32 size;
    RK_U32 count;
} vpm_pool_config;

typedef struct VPUMemIon
{
    RK_U32  phy_addr;
    RK_U32  *vir_addr;
    RK_U32  size;
    void    *handle;
} VPUMemLinear_ion;

static vpu_mem_file vpu_mem =
{
    .mutex      = PTHREAD_RECURSIVE_MUTEX_INITIALIZER,
    .pool_en    = 1,
    .list_pool  = LIST_HEAD_INIT(vpu_mem.list_pool),
    .share      = {
        .original   = {
            .vir_base  = NULL,
            .phy_base  = (RK_U32)(-1),
            .fd        = -1,
            .size      = 0,
        },
        .dma_buf    = {
            .res = 0,
            .dev = NULL,
        },
    },
};

static int vpu_mem_status = -1;
#define VPU_MEM_TEST    \
    do { \
        if (vpu_mem_status < 0) { \
            vpu_mem_status = (access("/dev/vpu_mem", F_OK) == 0); \
            vpu_mem_status = 2; \
        } \
    } while (0)

#define vpm_vpu     (vpu_mem.share.original)
#define vpm_ion     (vpu_mem.share.ion)
#define vpm_lock    (vpu_mem.mutex)
#define vpm_pool    (vpu_mem.list_pool)
#define vpm_dmabuf  (vpu_mem.share.dma_buf)

static RK_S32 vpu_mem_link()
{
    int err = 0;
    vpm_vpu.fd = open("/dev/vpu_mem", O_RDWR, 0);
    if (vpm_vpu.fd < 0) {
        fprintf(stderr, "open vpu_mem failed\n");
        err = -errno;
        goto OPEN_ERR;
    }
    VPM_DEBUG("vpu_mem open success\n");

    err = ioctl(vpm_vpu.fd, VPU_MEM_GET_TOTAL_SIZE, &vpm_vpu.size);
    if (err < 0) {
        fprintf(stderr, "VPU_MEM_GET_TOTAL_SIZE failed\n");
        goto IOCTL_ERR;
    }
    VPM_DEBUG("vpu_mem VPU_MEM_GET_TOTAL_SIZE %d\n", vpm_vpu.size);

    err = ioctl(vpm_vpu.fd, VPU_MEM_GET_PHYS, &vpm_vpu.phy_base);
    if (err < 0) {
        fprintf(stderr, "VPU_MEM_GET_PHYS failed\n");
        goto IOCTL_ERR;
    }
    VPM_DEBUG("vpu_mem VPU_MEM_GET_PHYS 0x%x\n", vpm_vpu.phy_base);

    vpm_vpu.vir_base = mmap(0, vpm_vpu.size,
            PROT_READ | PROT_WRITE, MAP_SHARED, vpm_vpu.fd, 0);
    if (MAP_FAILED == vpm_vpu.vir_base) {
        fprintf(stderr, "VPU_MEM_MMAP failed\n");
        goto IOCTL_ERR;
    }
    VPM_DEBUG("vpu_mem VPU_MEM_MMAP 0x%x\n", (RK_U32)vpm_vpu.vir_base);

    return 0;
IOCTL_ERR:
    if (vpm_vpu.fd >= 0)
        close(vpm_vpu.fd);
OPEN_ERR:
    vpm_vpu.fd         = -1;
    vpm_vpu.size       = 0;
    vpm_vpu.vir_base   = NULL;
    vpm_vpu.phy_base   = 0;
    return err;
}

static vpu_mem_pool *find_pool_by_pfn(RK_U32 pfn)
{
    vpu_mem_pool *pos, *n;
    list_for_each_entry_safe(pos, n, &vpm_pool, list) {
        if (pfn == pos->pfn) {
            return pos;
        }
    }

    return NULL;
}

static RK_S32 vpu_mem_pool_check_nolocked(RK_U32 size)
{
    RK_U32 pfn = SIZE_TO_PFN(size);
    vpu_mem_pool * pool = find_pool_by_pfn(pfn);
    if (pool) {
        RK_S32 count = 0;
        if (vpu_mem_status == 2) {
            /// implement later.
        }

        if ((count > 0) && (count >= pool->count)) {
            VPM_DEBUG("%s: ret 1 size %d k count %d pool %d", ((vpu_mem_status)?("vpu_mem"):("vpu_ion")), pfn*4, count, pool->count);
            return 1;
        }
        VPM_DEBUG("%s: ret 0 size %d k count %d pool %d", ((vpu_mem_status)?("vpu_mem"):("vpu_ion")), pfn*4, count, pool->count);
    }
    return 0;
}

static RK_S32 vpu_ion_link()
{
    ALOGE("%s %d\n", __func__, __LINE__);
    int err = 0;// ion_open_vpu(ALIGN_SIZE(), ION_MODULE_VPU, &vpm_ion.dev);
    if (err) {
        VPM_ERROR("open ion_mem failed\n");
        goto OPEN_ERR;
    }
    VPM_DEBUG("vpu_ion open success dev %p\n", vpm_ion.dev);

    return 0;
OPEN_ERR:
    vpm_ion.dev = NULL;
    return err;
}

RK_S32 VPUMemJudgeIommu(){
    int ret = 0;
    if(VPUClientGetIOMMUStatus() > 0){
        ALOGV("media.used.iommu");
        ret = 1;
    }
    return ret;
}
static RK_S32 vpu_dma_buf_link()
{
    int err = vpu_dmabuf_open(ALIGN_SIZE(), &vpm_dmabuf.dev, "vpudmabuf");
    if (err) {
        VPM_ERROR("open ion client failed\n");
        vpm_dmabuf.dev = NULL;
        return err;
    }

    return 0;
}

static int is_renderbuf(VPUMemLinear_t *p)
{
    VPUMemLinear_t *p_dmabuf;
    struct vpu_dmabuf_dev *dev;
    int err;

    if (NULL == vpm_dmabuf.dev) {
        err = vpu_dma_buf_link();
        if (err) {
            VPM_DEBUG("IONMem: VPUMemLink to vpu_mem fail\n");
            return 0;
        }
    }

    dev = vpm_dmabuf.dev;

    assert(p);
    p_dmabuf = (VPUMemLinear_t*)p->offset;

    if (p_dmabuf == INVAILD_PTR) {
        VPM_DEBUG("Invalidate private data point, when deintermine render buffer\n");
        return 0;
    }

    // check the dmabuf private data validation for renderer buffer.
    if (dev->get_fd(p_dmabuf) > 0 && dev->get_priv(p_dmabuf) != NULL) {
        VPM_DEBUG("--------------->, origin fd = %d\n", dev->get_fd(p_dmabuf));
        return 1;
    } else {
        return 0;
    }
}

RK_S32 VPUMemGetFreeSize()
{
    pthread_mutex_lock(&vpm_lock);
    RK_S32 size = 0;
    VPU_MEM_TEST;
    if (vpu_mem_status == 2) {
        /// implement later.
    }
    pthread_mutex_unlock(&vpm_lock);
    return size;
}

RK_S32 VPUMemPoolSet(RK_U32 size, RK_U32 count)
{
    RK_S32 err = 0;
    if (0 == count)
        return err;
    pthread_mutex_lock(&vpm_lock);
    do {
        RK_U32 pfn = SIZE_TO_PFN(size);
        vpu_mem_pool * pool = find_pool_by_pfn(pfn);
        if (pool) {
            pool->count += count;
        } else {
            pool = malloc(sizeof(vpu_mem_pool));
            if (pool) {
                pool->pfn   = pfn;
                pool->count = count;
                list_add_tail(&pool->list, &vpm_pool);
                ALOGD("VPUMemPoolSet: add pfn %d count %d", pfn, count);
            } else {
                VPM_ERROR("VPUMemPoolSet: malloc pool return NULL");
                err = -1;
            }
        }
    } while(0);
    pthread_mutex_unlock(&vpm_lock);
    return err;
}

RK_S32 VPUMemPoolUnset(RK_U32 size, RK_U32 count)
{
    RK_S32 err = 0;
    if (0 == count)
        return err;
    pthread_mutex_lock(&vpm_lock);
    do {
        RK_U32 pfn = SIZE_TO_PFN(size);
        vpu_mem_pool * pool = find_pool_by_pfn(pfn);
        if (pool) {
            ALOGD("VPUMemPoolUnset: del pfn %d count %d", pfn, count);
            pool->count -= count;
            if (pool->count <= 0) {
                list_del_init(&pool->list);
                free(pool);
                pool = NULL;
            }
        } else {
            VPM_ERROR("VPUMemPoolUnset: could not found pool of pfn %d", pfn);
            err = -1;
        }
    } while(0);
    pthread_mutex_unlock(&vpm_lock);
    return err;
}

RK_S32 VPUMallocLinearFromRender(VPUMemLinear_t *p, RK_U32 size, void *ctx)
{
    int err = 0;
    vpu_display_mem_pool *pool;

    assert(p != NULL);
    assert(size != 0);

    if (0 == size) {
        memset(p, 0, sizeof(VPUMemLinear_t));
        return -1;
    }
    if (ctx == NULL) {
        return VPUMallocLinear(p, size);
    } else {
        pool = (vpu_display_mem_pool*)ctx;
    }

    if(pool->buff_size == -1){
        pool->buff_size = size;
    }

    if((size != pool->buff_size)){
        if(pool->version == 1){
            if(pool->buff_size != 0){
                return VPUMallocLinear(p, size);
            }else{
                pool->buff_size = size;
            }
        }else if(size > pool->buff_size){
            pool->buff_size = size;
        }
    }
    p->offset   = NULL;
    p->phy_addr = 0;
    p->vir_addr = NULL;

    //pthread_mutex_lock(&vpm_lock);

    do {
        VPUMemLinear_t *p_dmabuf = NULL, *dmabuf_from_pool;
        struct vpu_dmabuf_dev *dev;
        int share_fd;

        if (NULL == vpm_dmabuf.dev) {
            err = vpu_dma_buf_link();
            if (err) {
                VPM_DEBUG("IONMem: VPUMem Link to vpu_mem fail\n");
                break;
            }
        }

        dev = vpm_dmabuf.dev;

#if !ENABLE_VPU_MEMORY_POOL_ALLOCATOR
        share_fd = pool->get_free(pool);
        if (share_fd < 0) {
            break;
        }

        err = dev->map(dev, share_fd, size, &p_dmabuf);
        if (err) {
            VPM_ERROR("DMABUF: Import fd %d ret %d\n", share_fd, err);
            p_dmabuf = INVAILD_PTR;
            pool->put_used(pool, share_fd);
            break;
        }

        dev->reserve(p_dmabuf, share_fd, (void*)pool);
#else
        dmabuf_from_pool = pool->get_free(pool);
        if (dmabuf_from_pool == NULL) {
            VPM_DEBUG("DMABUF: get free one from pool failed used alloc directly \n");
            return VPUMallocLinear(p, size);
        }

      if(size > dmabuf_from_pool->size){
            ALOGE("mem pool realsize is small than decoder need");
            int dmabuf_fd = dev->get_fd(dmabuf_from_pool);
            pool->put_used(pool, dmabuf_fd);
            if(pool->version == 1){
                pool->buff_size = -1;
            }
            return VPUMallocLinear(p, size);
        }

        if (0 > dev->share(dev, dmabuf_from_pool, &p_dmabuf)) {
            VPM_ERROR("DMABUF: Share failed\n");
            break;
        }

        if (0 > dev->map(dev, -1, size, &p_dmabuf)) {
            VPM_ERROR("DMABUF: Map failed\n");
            break;
        }

        dev->reserve(p_dmabuf, -1, (void*)pool);
#endif

        memcpy(p, p_dmabuf, sizeof(VPUMemLinear_t));
        p->offset = (RK_S32*)p_dmabuf;

        VPM_DEBUG("DMABUF: map size %x phy_addr 0x%x", p->size, p->phy_addr);

        return 0;
    }
    while (0);
    //pthread_mutex_unlock(&vpm_lock);

    return -1;
}

RK_S32 VPUMallocLinear(VPUMemLinear_t *p, RK_U32 size)
{
    assert(p != NULL);
    assert(size != 0);

    if (0 == size) {
        memset(p, 0, sizeof(VPUMemLinear_t));
        return -1;
    }

    p->offset   = (uint32_t*) -1;
    p->phy_addr = 0;
    p->vir_addr = NULL;

    VPU_MEM_TEST;
    int err = 0;

    //pthread_mutex_lock(&vpm_lock);
    do {
        VPUMemLinear_t *p_dmabuf = NULL;
        struct vpu_dmabuf_dev *dev;
        int share_fd;

        if (NULL == vpm_dmabuf.dev) {
            err = vpu_dma_buf_link();
            if (err) {
                VPM_DEBUG("IONMem: VPUMemLink to vpu_mem fail\n");
                break;
            }
        }

        dev = vpm_dmabuf.dev;

        err = dev->alloc(dev, size, &p_dmabuf);
        if (err) {
            VPM_ERROR("VPU Allocate failed\n");
            break;
        } else {
            VPM_DEBUG("VPU Allocate success\n");
        }

        memcpy(p, p_dmabuf, sizeof(VPUMemLinear_t));
        p->offset = (RK_S32*)p_dmabuf;
    } while (0);
    //pthread_mutex_unlock(&vpm_lock);
    return err;
}

RK_S32 VPUMemClose()
{
    struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
    if(NULL == dev){
        VPM_DEBUG("%s:%d vpu_dmabuf_dev already closed");
        return -1;
    }else{
        return vpu_dmabuf_close(dev);
    }
}


RK_S32 VPUFreeLinear(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    int err = 0;
    struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
    VPUMemLinear_t *p_dmabuf = (VPUMemLinear_t*)p->offset;

    if (NULL == dev || INVAILD_PTR == p_dmabuf) {
        VPM_DEBUG("IONMem: VPUFreeLinear non-linked handle %p\n", p_dmabuf);
        return -1;
    }

    //pthread_mutex_lock(&vpm_lock);

    vpu_display_mem_pool *pool = (vpu_display_mem_pool*)dev->get_priv(p_dmabuf);
    int render_flag = is_renderbuf(p);
    int dmabuf_fd = dev->get_fd(p_dmabuf);

    err = dev->free(dev, p_dmabuf);
    if (err) {
        VPM_ERROR("DMABUF: VPUFreeLinear unmap failed\n");
    }

    if (render_flag) {
        pool->put_used(pool, dmabuf_fd);
    }

    p->phy_addr = 0;
    p->vir_addr = NULL;
    p->size = 0;
    p->offset = (uint32_t*)-1;

    //pthread_mutex_unlock(&vpm_lock);

    return err;
}

RK_S32 VPUMemDuplicate(VPUMemLinear_t *dst, VPUMemLinear_t *src)
{
    VPU_MEM_TEST;
    int err = 0;

    if (NULL == vpm_dmabuf.dev) {
        VPM_DEBUG("IONMem: VPUMemDuplicate non-linked NULL dev\n");
        return -1;
    }

    //pthread_mutex_lock(&vpm_lock);

    do {
        struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
        VPUMemLinear_t *p_dmabuf = (VPUMemLinear_t*)src->offset;
        int share_fd;
        VPUMemLinear_t *p_outbuf = NULL;

        if (p_dmabuf == INVAILD_PTR) {
            VPM_ERROR("DMABUF, Invalid dmabuf handle\n");
            break;
        }

        err = dev->share(dev, p_dmabuf, &p_outbuf);
        if (err) {
            VPM_ERROR("DMABUF, share failed\n");
            break;
        }

        if (is_renderbuf(src)) {
            vpu_display_mem_pool *pool = (vpu_display_mem_pool*)dev->get_priv(p_dmabuf);
            err = pool->inc_used(pool, dev->get_fd(p_dmabuf));
            if (err) {
                VPM_ERROR("DMABUF: inc ref %d failed\n", dev->get_fd(p_dmabuf));
                break;
            }
        }

        memcpy(dst, p_outbuf, sizeof(VPUMemLinear_t));
        dst->offset = (RK_U32*)p_outbuf;
    }
    while (0);

    //pthread_mutex_unlock(&vpm_lock);
    return err;

}

RK_S32 VPUMemLink(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    int err = 0;
    //pthread_mutex_lock(&vpm_lock);

    do {
        struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
        VPUMemLinear_t *p_dmabuf = (VPUMemLinear_t*)p->offset;

        err = dev->map(dev, 0, p_dmabuf->size, &p_dmabuf);
        if (err) {
            VPM_ERROR("DMABUF: map failed\n");
            break;
        }

        memcpy(p, p_dmabuf, sizeof(VPUMemLinear_t));

        p->offset = (RK_U32*)p_dmabuf;
    }
    while (0);

    //pthread_mutex_unlock(&vpm_lock);
    return err;
}

RK_S32 VPUMemGetFD(VPUMemLinear_t *p)
{
    struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
    VPUMemLinear_t *p_dmabuf = (VPUMemLinear_t*)p->offset;

    return dev->get_fd(p_dmabuf);
}

RK_S32 VPUMemGetREF(VPUMemLinear_t *p)
{
    struct vpu_dmabuf_dev *dev = vpm_dmabuf.dev;
    VPUMemLinear_t *p_dmabuf = (VPUMemLinear_t*)p->offset;

    return dev->get_ref(p_dmabuf);
}

vpu_dmabuf_dev* VPUMemGetDev()
{
    return vpu_mem.share.dma_buf.dev;
}

RK_S32 VPUMemFlush(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    /// implement later.
    return 0;
}

RK_S32 VPUMemClean(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    return 0;
}

RK_S32 VPUMemInvalidate(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    return 0;
}

RK_S32 VPUMemImport_phyaddr(int share_fd,RK_U32 *phy_addr){
    struct vpu_dmabuf_dev *dev = NULL;
    int err = vpu_dmabuf_open(ALIGN_SIZE(), &dev, "vpudmabuf");
    if (err) {
        VPM_ERROR("open ion client failed\n");
        return err;
    }
    dev->get_phyaddr(dev,share_fd,phy_addr);
    vpu_dmabuf_close(dev);
    return 0;
}

#if BUILD_VPU_MEM_TEST
#define MAX_MEM    100
#define LOOP_MEM   5000

volatile int err = 0;
void *mem_test_loop_0(void *pdata)
{
    int i;
    VPUMemLinear_t m[MAX_MEM];
    VPUMemLinear_t *p = &m[0];
    VPUMemLinear_t *end = &m[99];

    ALOGV("vpu_mem test 0 loop start\n");
    memset(m, 0, sizeof(m));
    for (i = 0; i < LOOP_MEM; i++) {
    //for (;;) {
        if (0 == p->phy_addr) {
            err |= VPUMallocLinear(p, 100);
        }
        if ((p+1)->phy_addr) {
            err |= VPUFreeLinear(p+1);
        }
        if ((p+2)->phy_addr) {
            err |= VPUFreeLinear(p+2);
        }
        err |= VPUMemDuplicate(p+1, p);
        err |= VPUFreeLinear(p);
        err |= VPUMemLink(p+1);
        err |= VPUMemDuplicate(p+2, p+1);
        err |= VPUFreeLinear(p+1);
        err |= VPUMemLink(p+2);
        err |= VPUFreeLinear(p+2);
        //printf("thread: loop %5d err %d\n", i, err);
        if (err) {
            ALOGV("thread: err %d\n", err);
            break;
        }
        if (p+2 == end) {
            p = &m[0];
        } else {
            p++;
        }
    }
    if (err) {
        ALOGV("thread: found vpu mem operation err %d\n", err);
    } else {
        ALOGV("thread: test done and found no err\n");
    }
    return NULL;
}

int test_0()
{
    int i;
    ALOGV("vpu_mem test 0 start\n");
    VPUMemLinear_t m[MAX_MEM];
    VPUMemLinear_t *p = &m[0];
    VPUMemLinear_t *end = &m[99];

    memset(m, 0, sizeof(m));

    pthread_t mThread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&mThread, &attr, mem_test_loop_0, NULL);
    pthread_attr_destroy(&attr);

    for (i = 0; i < LOOP_MEM; i++) {
    //for (;;) {
        if (0 == p->phy_addr) {
            err |= VPUMallocLinear(p, 100);
        }
        if ((p+1)->phy_addr) {
            err |= VPUFreeLinear(p+1);
        }
        if ((p+2)->phy_addr) {
            err |= VPUFreeLinear(p+2);
        }
        err |= VPUMemDuplicate(p+1, p);
        err |= VPUFreeLinear(p);
        err |= VPUMemLink(p+1);
        err |= VPUMemDuplicate(p+2, p+1);
        err |= VPUFreeLinear(p+1);
        err |= VPUMemLink(p+2);
        err |= VPUFreeLinear(p+2);
        //printf("main  : loop %5d err %d\n", i, err);
        if (err) {
            ALOGV("main  : err %d\n", err);
            break;
        }
        if (p+2 == end) {
            p = &m[0];
        } else {
            p++;
        }
    }
    if (err) {
        ALOGV("main  : found vpu mem operation err %d\n", err);
    } else {
        ALOGV("main  : test done and found no err\n");
    }

    void *dummy;
    pthread_join(mThread, &dummy);

    ALOGV("vpu_mem test 0 end\n");
    return err;
}

void *mem_test_loop_1(void *pdata)
{
    int i;
    VPUMemLinear_t m[MAX_MEM];
    VPUMemLinear_t *p = &m[0];
    VPUMemLinear_t *end = &m[99];

    ALOGV("vpu_mem test 1 loop start\n");
    memset(m, 0, sizeof(m));
    //for (i = 0; i < LOOP_MEM; i++) {
    for (i = 0; ; i++) {
        err |= VPUMallocLinear(p+0, 0x5000);
        err |= VPUMemClean(p);
        err |= VPUMallocLinear(p+1, 0x2b000);
        err |= VPUMallocLinear(p+2, 0x1000);
        err |= VPUMemFlush(p+2);
        usleep(5);
        err |= VPUMemInvalidate(p+1);
        err |= VPUFreeLinear(p);
        err |= VPUFreeLinear(p+1);
        err |= VPUFreeLinear(p+2);
        //printf("thread: loop %5d err %d\n", i, err);
        if (err) {
            ALOGV("thread: err %d\n", err);
            break;
        }
        if (p+2 == end) {
            p = &m[0];
        } else {
            p++;
        }
    }
    if (err) {
        ALOGV("thread: found vpu mem operation err %d\n", err);
    } else {
        ALOGV("thread: test done and found no err\n");
    }
    return NULL;
}

int test_1()
{
    int i;
    ALOGV("vpu_mem test 1 start\n");
    VPUMemLinear_t m[MAX_MEM];
    VPUMemLinear_t *p = &m[0];
    VPUMemLinear_t *end = &m[99];

    memset(m, 0, sizeof(m));

    pthread_t thread0, thread1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&thread0, &attr, mem_test_loop_1, NULL);
    pthread_create(&thread1, &attr, mem_test_loop_1, NULL);
    pthread_attr_destroy(&attr);

    //for (i = 0; i < LOOP_MEM; i++) {
    for (i = 0; ; i++) {
        err |= VPUMallocLinear(p+0, 0x5000);
        err |= VPUMemClean(p);
        err |= VPUMallocLinear(p+1, 0x2b000);
        err |= VPUMallocLinear(p+2, 0x1000);
        err |= VPUMemFlush(p+2);
        usleep(5);
        err |= VPUMemInvalidate(p+1);
        err |= VPUFreeLinear(p);
        err |= VPUFreeLinear(p+1);
        err |= VPUFreeLinear(p+2);
        //printf("main  : loop %5d err %d\n", i, err);
        if (err) {
            ALOGV("main  : err %d\n", err);
            break;
        }
        if (p+2 == end) {
            p = &m[0];
        } else {
            p++;
        }
    }
    if (err) {
        ALOGV("main  : found vpu mem operation err %d\n", err);
    } else {
        ALOGV("main  : test done and found no err\n");
    }

    void *dummy;
    pthread_join(thread0, &dummy);
    pthread_join(thread1, &dummy);

    ALOGV("vpu_mem test 1 end\n");
    return err;
}

typedef int (*TEST_FUNC)(void);
TEST_FUNC func[2] = {
    test_0,
    test_1,
};

#define TOTAL_TEST_COUNT    2

int main(int argc, char *argv[])
{
    int i, start = 0, end = 0;
    if (argc < 2) {
        end = TOTAL_TEST_COUNT;
    } else if (argc == 2) {
        start = atoi(argv[1]);
        end   = start + 1;
    } else if (argc == 3) {
        start = atoi(argv[1]);
        end   = atoi(argv[2]);
    } else {
        ALOGV("too many argc %d\n", argc);
        return -1;
    }
    if (start < 0 || start > TOTAL_TEST_COUNT || end < 0 || end > TOTAL_TEST_COUNT) {
        ALOGV("invalid input: start %d end %d\n", start, end);
        return -1;
    }
    for (i = start; i < end; i++) {
        int err = func[i]();
        if (err) {
            ALOGV("test case %d return err %d\n", i, err);
            break;
        }
    }
    return 0;
}
#endif
