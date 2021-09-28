/*
 *
 * Copyright 2010 Rockchip Electronics S.LSI Co. LTD
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
   * vpu_mem.c
   * Description:
   *
   * Author:
   *     Chm
   * Date:
   *    2010-11-23
 */

#define ALOG_TAG "vpu_mem"

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
#include <linux/android_pmem.h>
#include <utils/Log.h>
#include <linux/ion.h>
#include <ion/ionalloc.h>
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
#define ALIGN_4K(x)     ((x+(4096))&(~4095))
#define SIZE_TO_PFN(x)  ((ALIGN_4K(x))/ALIGN_SIZE())
#define MAX_FD          (0x7FFF)
#define INVAILD_PTR     ((void *)(-1))

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
        }
    },
};

static int vpu_mem_status = -1;
#define VPU_MEM_TEST    \
    do { \
        if (vpu_mem_status < 0) { \
            vpu_mem_status = (access("/dev/vpu_mem", F_OK) == 0); \
        } \
    } while (0)

#define vpm_vpu     (vpu_mem.share.original)
#define vpm_ion     (vpu_mem.share.ion)
#define vpm_lock    (vpu_mem.mutex)
#define vpm_pool    (vpu_mem.list_pool)

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
        if (vpu_mem_status) {
            count = ioctl(vpm_vpu.fd, VPU_MEM_GET_COUNT, &pfn);
        } else {
            ion_device_t *dev = vpm_ion.dev;
            int err = dev->perform(dev, ION_MODULE_PERFORM_QUERY_BUFCOUNT, ALIGN_4K(size), &count);
            if (err) {
                VPM_ERROR("perform ION_MODULE_PERFORM_QUERY_BUFCOUNT err %d", err);
            }
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
    int err = ion_open(ALIGN_SIZE(), ION_MODULE_VPU, &vpm_ion.dev);
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

RK_S32 VPUMemGetFreeSize()
{
    pthread_mutex_lock(&vpm_lock);
    RK_S32 size = 0;
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        do {
            if (vpm_vpu.fd < 0) {
                if (vpu_mem_link()) {
                    VPM_ERROR("VPUMem: vpu_mem_link in VPUMemGetFreeSize err");
                    break;
                }
            }

            size = ioctl(vpm_vpu.fd, VPU_MEM_GET_FREE_SIZE);
            size *= ALIGN_SIZE();
        } while (0);
        VPM_DEBUG("VPUMemGetFreeSize %d", size);
    }else {
        RK_U32 total_size = 0, allocated_size = 0;
        do {
            if (NULL == vpm_ion.dev) {
                if (vpu_ion_link()) {
                    VPM_ERROR("VPUIon: vpu_ion_link in VPUMemGetFreeSize err");
                    break;
                }
            }

            ion_device_t *dev = vpm_ion.dev;
            int err = dev->perform(dev, ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED, &allocated_size);
            err |= dev->perform(dev, ION_MODULE_PERFORM_QUERY_HEAP_SIZE, &total_size);
            if (err) {
                VPM_ERROR("VPUIon: ION_MODULE_PERFORM err %d", err);
                break;
            }
            size = total_size - allocated_size;
        } while (0);
        VPM_DEBUG("VPUMemGetFreeSize total %d allocated %d free %d", total_size, allocated_size, size);
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

RK_S32 VPUMallocLinear(VPUMemLinear_t *p, RK_U32 size)
{
    assert(p != NULL);
    assert(size != 0);

    if (0 == size) {
        memset(p, 0, sizeof(VPUMemLinear_t));
        return -1;
    }

    p->offset   = -1;
    p->phy_addr = 0;
    p->vir_addr = NULL;

    VPU_MEM_TEST;
    if (vpu_mem_status) {
        int err = 0;
        int index = -1;

        pthread_mutex_lock(&vpm_lock);

        if (vpm_vpu.fd == -1) {
            err = vpu_mem_link();
            if (err)
                goto VPU_MEM_RET;
        }

        if (vpu_mem.pool_en) {
            err = vpu_mem_pool_check_nolocked(size);
            if (err) {
                goto VPU_MEM_RET;
            }
        }
        index = ioctl(vpm_vpu.fd, VPU_MEM_ALLOCATE, &size);
        if (index < 0) {
            err = -2;
            p->offset   = index;
        } else {
            p->offset   = index;
            p->phy_addr = (RK_U32)vpm_vpu.phy_base + index * PAGE_SIZE;
            p->vir_addr = (RK_U32 *)((RK_U32)vpm_vpu.vir_base + index * PAGE_SIZE);
        }
        p->size = size;
        VPM_DEBUG("VPUMem: VPUMallocLinear size 0x%x at index %d\n", size, index);

VPU_MEM_RET:
        pthread_mutex_unlock(&vpm_lock);
        return err;
    } else {
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        p_ion->phy_addr = 0;
        p_ion->vir_addr = NULL;
        p_ion->handle   = INVAILD_PTR;

        pthread_mutex_lock(&vpm_lock);
        int err;
        if (NULL == vpm_ion.dev) {
            err = vpu_ion_link();
            if (err)
                goto VPU_ION_RET;
        }

        if (vpu_mem.pool_en) {
            err = vpu_mem_pool_check_nolocked(size);
            if (err) {
                goto VPU_ION_RET;
            }
        }

        ion_device_t *dev = vpm_ion.dev;
        ion_buffer_t *buffer = NULL;
        err = dev->alloc(dev, ALIGN_4K(size), _ION_HEAP_RESERVE, &buffer);
        if (err) {
            VPM_DEBUG("IONMem: ION_IOC_ALLOC size 0x%x failed ret %d\n", size, err);
            goto VPU_ION_RET;
        } else {
            p_ion->phy_addr = buffer->phys;
            p_ion->vir_addr = buffer->virt;
            p_ion->size     = size;
            p_ion->handle   = (void *)buffer;
            VPM_DEBUG("Ion alloc phy: 0x%x hnd: %p 0x%x", p_ion->phy_addr, buffer, ((RK_U32 *)buffer)[6]);
        }
        VPM_DEBUG("IONMem: ION_IOC_ALLOC size %x handle %p_ion", size, p_ion->handle);

VPU_ION_RET:
        pthread_mutex_unlock(&vpm_lock);
        return err;
    }
}

RK_S32 VPUFreeLinear(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        int err;
        int offset;

        if ((vpm_vpu.fd < 0) || (p->offset < 0))
        {
            VPM_DEBUG("VPUMem: VPUFreeLinear non-linked index %d\n", p->offset);
            return -1;
        }

        pthread_mutex_lock(&vpm_lock);

        offset = p->offset;
        err = ioctl(vpm_vpu.fd, VPU_MEM_FREE, &p->offset);

        VPM_DEBUG("VPUMem: VPUFreeLinear index %d err %d\n", offset, err);

        p->phy_addr = 0;
        p->vir_addr = NULL;
        p->offset   = -1;

        pthread_mutex_unlock(&vpm_lock);
        return err;
    } else {
        ion_device_t *dev = vpm_ion.dev;
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        if (NULL == dev || INVAILD_PTR == p_ion->handle) {
            VPM_DEBUG("IONMem: VPUFreeLinear non-linked handle %p_ion\n", p_ion->handle);
            return -1;
        }

        pthread_mutex_lock(&vpm_lock);
        VPM_DEBUG("Ion free  phy: 0x%x hnd: %p 0x%x", p_ion->phy_addr, p_ion->handle, ((RK_U32 *)p_ion->handle)[6]);
        int err = dev->free(dev, (ion_buffer_t *)p_ion->handle);
        if (err) {
            VPM_ERROR("IONMem: free handle %p_ion phy_addr 0x%x ret %d\n", p_ion->handle, p_ion->phy_addr, err);
        } else {
            VPM_DEBUG("IONMem: free handle %p_ion phy_addr 0x%x ret %d\n", p_ion->handle, p_ion->phy_addr, err);
        }
        p_ion->phy_addr = 0;
        p_ion->vir_addr = NULL;
        p_ion->handle   = INVAILD_PTR;
        pthread_mutex_unlock(&vpm_lock);
        return err;
    }
}

RK_S32 VPUMemDuplicate(VPUMemLinear_t *dst, VPUMemLinear_t *src)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        int err;

        if (vpm_vpu.fd < 0) {
            VPM_DEBUG("VPUMem: VPUMemDuplicate non-linked index %d\n", src->offset);
            return -1;
        }

        pthread_mutex_lock(&vpm_lock);
        err = ioctl(vpm_vpu.fd, VPU_MEM_DUPLICATE, &src->offset);
        if (!err) {
            dst->offset   = src->offset;
            dst->phy_addr = src->phy_addr;
            dst->size     = src->size;
            VPM_DEBUG("VPUMem: VPUMemDuplicate index %d phy_addr 0x%x ret %d\n", src->offset, src->phy_addr, err);
        } else {
            VPM_ERROR("VPUMem: VPUMemDuplicate index %d phy_addr 0x%x ret %d\n", src->offset, src->phy_addr, err);
        }
        pthread_mutex_unlock(&vpm_lock);
        return err;
    } else {
        if (NULL == vpm_ion.dev) {
            VPM_DEBUG("IONMem: VPUMemDuplicate non-linked NULL dev\n");
            return -1;
        }
        VPUMemLinear_ion *dst_ion = (VPUMemLinear_ion *)dst;
        VPUMemLinear_ion *src_ion = (VPUMemLinear_ion *)src;
        if (NULL == src_ion) {
            if (src_ion) {
                VPM_ERROR("invalid src_ion handle %p in VPUMemDuplicate", src_ion->handle);
            } else {
                VPM_ERROR("NULL src_ion in VPUMemDuplicate");
            }
            return -2;
        }

        pthread_mutex_lock(&vpm_lock);
        int fd = -1;
        int err = 0;
        ion_device_t *dev = vpm_ion.dev;
        do {
            dst_ion->phy_addr = src_ion->phy_addr;
            dst_ion->size     = src_ion->size;
            err = dev->share(dev, (ion_buffer_t *)src_ion->handle, &fd);
            dst_ion->vir_addr = NULL;
            if (err) {
                VPM_ERROR("IONMem: VPUMemDuplicate ION_IOC_SHARE handle %p phy_addr 0x%x ret %d\n", src_ion->handle, src_ion->phy_addr, err);
                dst_ion->handle   = (void *)INVAILD_PTR;
            } else {
                dst_ion->handle   = (void *)fd;
                VPM_DEBUG("IONMem: VPUMemDuplicate handle %p phy_addr 0x%x ret %d\n", src_ion->handle, src_ion->phy_addr, fd);
                VPM_DEBUG("Ion share phy: 0x%x hnd: %p 0x%x fd %d", src_ion->phy_addr, src_ion->handle, ((RK_U32 *)src_ion->handle)[6], fd);
            }
        }while (0);
        pthread_mutex_unlock(&vpm_lock);
        return err;
    }
}

RK_S32 VPUMemLink(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    int err = -1;
    if (vpu_mem_status) {
        pthread_mutex_lock(&vpm_lock);
        if (vpm_vpu.fd < 0) {
            err = vpu_mem_link();
            if (err) {
                VPM_DEBUG("VPUMem: VPUMemLink to vpu_mem fail\n");
                goto VPU_MEM_RET;
            }
        }

        err = ioctl(vpm_vpu.fd, VPU_MEM_LINK, &p->offset);
        if (!err) {
            p->vir_addr = (RK_U32 *)((RK_U32)vpm_vpu.vir_base + p->offset * PAGE_SIZE);
            VPM_DEBUG("VPUMem: VPUMemLink index %d ret %d\n", p->offset, err);
        } else {
            p->vir_addr = NULL;
            VPM_ERROR("VPUMem: VPUMemLink index %d ret %d\n", p->offset, err);
        }
VPU_MEM_RET:
        pthread_mutex_unlock(&vpm_lock);
        return err;
    } else {
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        p_ion->vir_addr = NULL;
        p_ion->phy_addr = 0;
        pthread_mutex_lock(&vpm_lock);
        if (NULL == vpm_ion.dev) {
            err = vpu_ion_link();
            if (err) {
                VPM_DEBUG("IONMem: VPUMemLink to vpu_mem fail\n");
                goto VPU_ION_RET;
            }
        }

        if ((void *)MAX_FD > p_ion->handle) {
            ion_device_t *dev = vpm_ion.dev;
            ion_buffer_t *buffer = NULL;
            err = dev->map(dev, (int)p_ion->handle, &buffer);
            if (err) {
                VPM_ERROR("IONMem: VPUMemLink fd %d phy_addr 0x%x ret %d\n", (int)p_ion->handle, p_ion->phy_addr, err);
                p_ion->handle   = INVAILD_PTR;
                goto VPU_ION_RET;
            }
            p_ion->vir_addr = buffer->virt;
            p_ion->phy_addr = buffer->phys;
            p_ion->size     = buffer->size;
            VPM_DEBUG("Ion imprt phy: 0x%x hnd: %p 0x%x fd %d", p_ion->phy_addr, buffer, ((RK_U32 *)buffer)[6], (int)p_ion->handle);
            p_ion->handle   = (void *)buffer;
        } else {
            VPM_ERROR("IONMem: handle %p has nothing to link", p_ion->handle);
            p_ion->handle   = INVAILD_PTR;
            err = -1;
        }
        VPM_DEBUG("IONMem: map size %x handle %p phy_addr 0x%x", p_ion->size, p_ion->handle, p_ion->phy_addr);
VPU_ION_RET:
        pthread_mutex_unlock(&vpm_lock);
        return err;
    }
}

RK_S32 VPUMemFlush(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        if (vpm_vpu.fd < 0)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        int err = ioctl(vpm_vpu.fd, VPU_MEM_CACHE_FLUSH, &p->offset);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("VPUMem: VPU_MEM_CACHE_FLUSH index %d ret %d\n", p->offset, err);
        } else {
            VPM_DEBUG("VPUMem: VPU_MEM_CACHE_FLUSH index %d ret %d\n", p->offset, err);
        }
        return err;
    } else {
        if (NULL == vpm_ion.dev)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        int err = vpm_ion.dev->cache_op(vpm_ion.dev, (ion_buffer_t *)p_ion->handle, ION_FLUSH_CACHE);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("IONMem: ION_FLUSH_CACHE handle %p ret %d\n", p_ion->handle, err);
        } else {
            VPM_DEBUG("IONMem: ION_FLUSH_CACHE handle %p ret %d\n", p_ion->handle, err);
        }
        return err;
    }
}

RK_S32 VPUMemClean(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        if (vpm_vpu.fd < 0)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        int err = ioctl(vpm_vpu.fd, VPU_MEM_CACHE_CLEAN, &p->offset);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("VPUMem: VPU_MEM_CACHE_CLEAN index %d ret %d\n", p->offset, err);
        } else {
            VPM_DEBUG("VPUMem: VPU_MEM_CACHE_CLEAN index %d ret %d\n", p->offset, err);
        }
        return err;
    } else {
        if (NULL == vpm_ion.dev)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        int err = vpm_ion.dev->cache_op(vpm_ion.dev, (ion_buffer_t *)p_ion->handle, ION_CLEAN_CACHE);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("IONMem: ION_CLEAN_CACHE handle %p ret %d\n", p_ion->handle, err);
        } else {
            VPM_DEBUG("IONMem: ION_CLEAN_CACHE handle %p ret %d\n", p_ion->handle, err);
        }
        return err;
    }
}

RK_S32 VPUMemInvalidate(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        if (vpm_vpu.fd < 0)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        int err = ioctl(vpm_vpu.fd, VPU_MEM_CACHE_INVALID, &p->offset);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("VPUMem: VPU_MEM_CACHE_INVALID index %d ret %d\n", p->offset, err);
        } else {
            VPM_DEBUG("VPUMem: VPU_MEM_CACHE_INVALID index %d ret %d\n", p->offset, err);
        }
        return err;
    } else {
        if (NULL == vpm_ion.dev)
            return -1;

        pthread_mutex_lock(&vpm_lock);
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        int err = vpm_ion.dev->cache_op(vpm_ion.dev, (ion_buffer_t *)p_ion->handle, ION_INVALID_CACHE);
        pthread_mutex_unlock(&vpm_lock);
        if (err) {
            VPM_ERROR("IONMem: ION_INVALID_CACHE handle %p ret %d\n", p_ion->handle, err);
        } else {
            VPM_DEBUG("IONMem: ION_INVALID_CACHE handle %p ret %d\n", p_ion->handle, err);
        }
        return err;
    }
}

RK_U32 VPUMemPhysical(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        if (vpm_vpu.fd < 0)
            return 0;

        return vpm_vpu.phy_base + p->offset * PAGE_SIZE;
    } else {
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        return p_ion->phy_addr;
    }
}

RK_U32 *VPUMemVirtual(VPUMemLinear_t *p)
{
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        if (vpm_vpu.fd < 0)
            return NULL;

        return (RK_U32 *)((RK_U32)vpm_vpu.vir_base + p->offset * PAGE_SIZE);
    } else {
        VPUMemLinear_ion *p_ion = (VPUMemLinear_ion *)p;
        return p_ion->vir_addr;
    }
}

typedef struct {
    RK_U32  check;
    RK_HANDLE hnd;
    pthread_mutex_t mutex;
    RK_U32 maxSize;
} VPUMemAllocator;

typedef struct {
    VPUMemHnd   hnd;
    RK_U32      check;
    RK_HANDLE   allocator;
    RK_U32      reserv[10];
} VPUMemSlot;

static RK_S32 check_allocator(RK_HANDLE *p)
{
    RK_S32 ret = 1;
    if (p && ((*((RK_U32*)p)) == (RK_U32)p))
        ret = 0;
    return ret;
}

static RK_S32 check_mem_hnd(VPUMemHnd *p)
{
    RK_S32 ret = 1;
    if (p && (((VPUMemSlot*)p)->check == (RK_U32)p))
        ret = 0;
    return ret;
}

static RK_S32 vpu_lock_init(pthread_mutex_t *mutex)
{
    RK_S32 ret = 1;
    pthread_mutexattr_t attribute;
    if (0 == pthread_mutexattr_init(&attribute)) {
        if (0 == pthread_mutexattr_settype(&attribute, PTHREAD_MUTEX_RECURSIVE)) {
            /* Initialize the mutex. */
            if (0 == pthread_mutex_init(mutex, &attribute)) {
                ret = 0;
            }
        }
        pthread_mutexattr_destroy(&attribute);
    }
    return ret;
}

RK_HANDLE VPUMemAllocatorCreate(RK_U32 maxSize)
{
    VPUMemAllocator *allocator = (VPUMemAllocator *)malloc(sizeof(VPUMemAllocator));

    if (NULL == allocator || vpu_lock_init(&allocator->mutex))
        return NULL;

    do {
        VPU_MEM_TEST;
        if (vpu_mem_status) {
            int fd = open("/dev/vpu_mem", O_RDWR, 0);
            if (fd < 0) {
                ALOGE("open vpu_mem failed\n");
                break;
            } else {
                allocator->hnd = (RK_HANDLE)fd;
            }
        } else {
            ion_device_t *dev;
            int err = ion_open(ALIGN_SIZE(), ION_MODULE_VPU, &dev);
            if (err) {
                VPM_ERROR("open ion_mem failed ret %d\n", err);
                break;
            } else {
                allocator->hnd = (RK_HANDLE)dev;
            }
        }
        allocator->maxSize = maxSize;
        allocator->check   = (RK_U32)allocator;
        return (RK_HANDLE)allocator;
    }while (0);

    if (allocator) {
        free(allocator);
        allocator = NULL;
    }
    return NULL;
}

void VPUMemAllocatorDestory(RK_HANDLE allocator)
{
    if (check_allocator(allocator))
        return ;

    VPUMemAllocator *p = allocator;
    pthread_mutex_lock(&p->mutex);
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        close((int)p->hnd);
    } else {
        ion_close((ion_device_t *)p->hnd);
    }
    pthread_mutex_unlock(&p->mutex);
    free(allocator);
}

RK_S32 VPUMemMalloc(RK_HANDLE allocator, RK_U32 size, RK_S32 timeout, VPUMemHnd **p)
{
    if (NULL == p) {
        ALOGE("VPUMemMalloc: NULL ptr for handle");
        return INVALID_NULL_PTR;
    }
    *p = NULL;
    if (check_allocator(allocator)) {
        ALOGE("VPUMemMalloc: invalid allocator %p", allocator);
        return INVALID_ALLOCATOR;
    }

    int err = 0;
    VPUMemAllocator *pAlloc = allocator;
    VPUMemSlot *pSlot = (VPUMemSlot*)malloc(sizeof(VPUMemSlot));
    if (NULL == pSlot) {
        return -ENOMEM;
    }
    memset(pSlot, 0, sizeof(VPUMemHnd));
    pSlot->check        = (RK_U32)pSlot;
    pSlot->allocator    = allocator;
    VPUMemHnd *pHnd     = &pSlot->hnd;
    VPU_MEM_TEST;
    pthread_mutex_lock(&pAlloc->mutex);
    if (vpu_mem_status) {
        int index = ioctl((int)pAlloc->hnd, VPU_MEM_ALLOCATE, &size);
        if (index < 0) {
            err = -2;
        } else {
            pHnd->fd        = index;
            pHnd->phy_addr  = (RK_U32)vpm_vpu.phy_base + index * PAGE_SIZE;
            pHnd->vir_addr  = (RK_U32 *)((RK_U32)vpm_vpu.vir_base + index * PAGE_SIZE);
        }
        VPM_DEBUG("VPUMem: VPUMallocLinear size 0x%x at index %d\n", size, index);
    } else {
        VPM_DEBUG("IONMem: start ioctl ION_IOC_ALLOC\n");
        ion_device_t *dev = (ion_device_t *)pAlloc->hnd;
        ion_buffer_t *buffer = NULL;
        err = dev->alloc(dev, ALIGN_4K(size), _ION_HEAP_RESERVE, &buffer);
        if (err) {
            VPM_DEBUG("IONMem: ION_IOC_ALLOC size 0x%x failed ret %d\n", size, err);
        } else {
            pHnd->phy_addr  = buffer->phys;
            pHnd->vir_addr  = buffer->virt;
            pHnd->handle    = buffer;
            VPM_DEBUG("IONMem: ION_IOC_ALLOC size %x handle %p_ion", size, pHnd->handle);
        }
    }
    pthread_mutex_unlock(&pAlloc->mutex);
    if (err && pSlot) free(pSlot);
    return err;
}

RK_S32 VPUMemImport(RK_HANDLE allocator, RK_S32 mem_fd, VPUMemHnd **p)
{
    if (NULL == p) {
        ALOGE("VPUMemImport: NULL ptr for handle");
        return INVALID_NULL_PTR;
    }
    *p = NULL;
    if (check_allocator(allocator)) {
        ALOGE("VPUMemImport: invalid allocator %p", allocator);
        return INVALID_ALLOCATOR;
    }

    RK_S32 err = 0;
    VPUMemAllocator *pAlloc = allocator;
    VPUMemSlot *pSlot = (VPUMemSlot*)malloc(sizeof(VPUMemSlot));
    if (NULL == pSlot) {
        return -ENOMEM;
    }
    memset(pSlot, 0, sizeof(VPUMemHnd));
    pSlot->check        = (RK_U32)pSlot;
    pSlot->allocator    = allocator;
    VPUMemHnd *pHnd = &pSlot->hnd;
    VPU_MEM_TEST;
    pthread_mutex_lock(&pAlloc->mutex);
    if (vpu_mem_status) {
        err = ioctl((int)pAlloc->hnd, VPU_MEM_DUPLICATE, &mem_fd);
        if (err) {
            VPM_ERROR("VPUMem: VPUMemImport index %d phy_addr 0x%x ret %d\n", mem_fd, pHnd->phy_addr, err);
        } else {
            pHnd->fd        = mem_fd;
            pHnd->phy_addr  = (RK_U32)vpm_vpu.phy_base + mem_fd * PAGE_SIZE;
            pHnd->vir_addr  = (RK_U32 *)((RK_U32)vpm_vpu.vir_base + mem_fd * PAGE_SIZE);
            VPM_DEBUG("VPUMem: VPUMemImport index %d phy_addr 0x%x ret %d\n", mem_fd, pHnd->phy_addr, err);
        }
    } else {
        ion_device_t *dev = pAlloc->hnd;
        ion_buffer_t *buffer = NULL;
        err = dev->map(dev, mem_fd, &buffer);
        if (err) {
            VPM_ERROR("IONMem: VPUMemImport fd %d ret %d\n", mem_fd, err);
        } else {
            pHnd->vir_addr  = buffer->virt;
            pHnd->phy_addr  = buffer->phys;
            pHnd->handle    = buffer;
            VPM_DEBUG("IONMem: VPUMemImport fd %d get handle %p phy_addr 0x%x", mem_fd, pHnd->handle, pHnd->phy_addr);
        }
    }
    pthread_mutex_unlock(&pAlloc->mutex);
    if (err && pSlot) free(pSlot);
    return err;
}

RK_S32 VPUMemFree(VPUMemHnd *p)
{
    if (NULL == p) {
        ALOGE("VPUMemFree: NULL ptr for handle");
        return INVALID_NULL_PTR;
    }
    if (check_mem_hnd(p)) {
        ALOGE("VPUMemFree: invalid mem hnd %p", p);
        return INVALID_MEM_HND;
    }
    VPUMemSlot *pSlot = (VPUMemSlot *)p;
    if (check_allocator(pSlot->allocator)) {
        ALOGE("VPUMemFree: invalid allocator %p", pSlot->allocator);
        return INVALID_ALLOCATOR;
    }
    VPUMemAllocator *pAlloc = (VPUMemAllocator *)pSlot->allocator;
    RK_S32 err;
    pthread_mutex_lock(&pAlloc->mutex);
    VPUMemHnd *pHnd = &pSlot->hnd;
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        err = ioctl((int)pAlloc->hnd, VPU_MEM_FREE, &pHnd->handle);
        VPM_DEBUG("VPUMem: VPUFreeLinear index %d err %d\n", (int)p->handle, err);
    } else {
        ion_device_t *dev = pAlloc->hnd;
        err = dev->free(dev, (ion_buffer_t *)pHnd->handle);
        if (err) {
            VPM_ERROR("IONMem: free handle %p_ion phy_addr 0x%x ret %d\n", pHnd->handle, pHnd->phy_addr, err);
        } else {
            VPM_DEBUG("IONMem: free handle %p_ion phy_addr 0x%x ret %d\n", pHnd->handle, pHnd->phy_addr, err);
        }
    }
    free(pSlot);
    pthread_mutex_unlock(&pAlloc->mutex);
    return (err);
}

RK_S32 VPUMemShare(VPUMemHnd *p, RK_S32 *mem_fd)
{
    if (NULL == p || NULL == mem_fd) {
        ALOGE("VPUMemShare: NULL ptr for handle");
        return INVALID_NULL_PTR;
    }
    if (check_mem_hnd(p)) {
        ALOGE("VPUMemShare: invalid mem hnd %p", p);
        return INVALID_MEM_HND;
    }
    VPUMemSlot *pSlot = (VPUMemSlot *)p;
    if (check_allocator(pSlot->allocator)) {
        ALOGE("VPUMemShare: invalid allocator %p", pSlot->allocator);
        return INVALID_ALLOCATOR;
    }
    VPUMemAllocator *pAlloc = (VPUMemAllocator *)pSlot->allocator;
    RK_S32 err;
    pthread_mutex_lock(&pAlloc->mutex);
    VPUMemHnd *pHnd = &pSlot->hnd;
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        err = ioctl((int)pAlloc->hnd, VPU_MEM_DUPLICATE, &pHnd->handle);
        VPM_DEBUG("VPUMem: VPUMemShare index %d err %d\n", (int)p->handle, err);
    } else {
        ion_device_t *dev = pAlloc->hnd;
        err = dev->share(dev, (ion_buffer_t *)pHnd->handle, mem_fd);
        if (err) {
            VPM_ERROR("IONMem: VPUMemDuplicate ION_IOC_SHARE handle %p phy_addr 0x%x ret %d\n", pHnd->handle, pHnd->phy_addr, err);
        } else {
            VPM_DEBUG("IONMem: VPUMemDuplicate handle %p phy_addr 0x%x ret %d\n", pHnd->handle, pHnd->phy_addr, *mem_fd);
        }
    }
    pthread_mutex_unlock(&pAlloc->mutex);
    return (err);
}

RK_S32 VPUMemCache(VPUMemHnd *p, VPUCacheOp_E cmd)
{
    if (NULL == p) {
        ALOGE("VPUMemCache: NULL ptr for handle");
        return INVALID_NULL_PTR;
    }
    if (check_mem_hnd(p)) {
        ALOGE("VPUMemCache: invalid mem hnd %p", p);
        return INVALID_MEM_HND;
    }
    VPUMemSlot *pSlot = (VPUMemSlot *)p;
    if (check_allocator(pSlot->allocator)) {
        ALOGE("VPUMemCache: invalid allocator %p", pSlot->allocator);
        return INVALID_ALLOCATOR;
    }
    VPUMemAllocator *pAlloc = (VPUMemAllocator *)pSlot->allocator;
    RK_S32 err;
    pthread_mutex_lock(&pAlloc->mutex);
    VPUMemHnd *pHnd = &pSlot->hnd;
    VPU_MEM_TEST;
    if (vpu_mem_status) {
        switch (cmd) {
        case VPU_CACHE_FLUSH : {
            err = ioctl((int)pAlloc->hnd, VPU_MEM_CACHE_FLUSH, &pHnd->handle);
        } break;
        case VPU_CACHE_CLEAN : {
            err = ioctl((int)pAlloc->hnd, VPU_MEM_CACHE_FLUSH, &pHnd->handle);
        } break;
        case VPU_CACHE_INVALID : {
            err = ioctl((int)pAlloc->hnd, VPU_MEM_CACHE_INVALID, &pHnd->handle);
        } break;
        default : {
            err = INVALID_CACHE_OP;
        } break;
        };
        if (err) {
            VPM_ERROR("VPUMem: VPUCacheOp index %d cmd %d ret %d\n", (RK_S32)pHnd->handle, cmd, err);
        } else {
            VPM_DEBUG("VPUMem: VPUCacheOp index %d cmd %d ret %d\n", (RK_S32)pHnd->handle, cmd, err);
        }
    } else {
        ion_device_t *dev = pAlloc->hnd;
        switch (cmd) {
        case VPU_CACHE_FLUSH : {
            err = dev->cache_op(dev, (ion_buffer_t *)pHnd->handle, ION_FLUSH_CACHE);
        } break;
        case VPU_CACHE_CLEAN : {
            err = dev->cache_op(dev, (ion_buffer_t *)pHnd->handle, ION_CLEAN_CACHE);
        } break;
        case VPU_CACHE_INVALID : {
            err = dev->cache_op(dev, (ion_buffer_t *)pHnd->handle, ION_INVALID_CACHE);
        } break;
        default : {
            err = INVALID_CACHE_OP;
        } break;
        };
        if (err) {
            VPM_ERROR("IONMem: VPUCacheOp handle %p ret %d\n", pHnd->handle, err);
        } else {
            VPM_DEBUG("IONMem: VPUCacheOp handle %p ret %d\n", pHnd->handle, err);
        }
    }
    pthread_mutex_unlock(&pAlloc->mutex);
    return err;
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
