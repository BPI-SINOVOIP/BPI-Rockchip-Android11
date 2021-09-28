/*
 * Copyright(C) 2010 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS
 * SOFTWARE, YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FORM ROCKCHIP
 * IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL
 * WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED
 * WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY,
 * ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license
 *   (a) to install, save and use the Software;
 *   (b) to * copy and distribute the Software in binary code format only
 * Except as expressively authorized by Rockchip in writing, you may NOT:
 *   (a) distribute the Software in source code;
 *   (b) distribute on a standalone basis but you may distribute the Software in
 *   conjunction with platforms incorporating Rockchip integrated circuits;
 *   (c) modify the Software in whole or part;
 *   (d) decompile, reverse-engineer, dissemble, or attempt to derive any source
 *   code from the Software;
 *   (e) remove or obscure any copyright, patent, or trademark statement or
 *   notices contained in the Software
 */

#include <cutils/log.h>

#undef LOGE
#define LOGE    ALOGE

#undef LOGV
#define LOGV    ALOGV

#include "ion_priv_vpu.h"

inline size_t roundUpToPageSize(size_t x)
{
    return (x + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
}

static int validate(private_handle_t *h)
{
    const private_handle_t *hnd = (const private_handle_t *)h;
#if 1
    if (!hnd ||
        hnd->s_num_ints != NUM_INTS ||
        hnd->s_num_fds != NUM_FDS ||
        hnd->s_magic != MAGIC) {
        LOGE("Invalid ion handle (at %p)", hnd);
        return -EINVAL;
    }
#endif
    return 0;
}

static int ion_custom_op(struct ion_device_t *ion, int op, void *op_data)
{
    /*union {
        struct ion_phys_data *phys_data;
        struct ion_flush_data *flush_data;
    } custom_data;*/

    struct ion_custom_data data;
    int err;
    private_device_t *dev = (private_device_t *)ion;

    data.cmd = op;
    data.arg = (unsigned long)op_data;

    err = ioctl(dev->ionfd, ION_IOC_CUSTOM, &data);
    if (err < 0) {
        LOGE("%s: ION_IOC_CUSTOM (%d) failed with error - %s",
             __FUNCTION__, op, strerror(errno));
        return err;
    }

    return err;
}

static int ion_get_phys(struct ion_device_t *ion, struct ion_phys_data *phys_data)
{
    struct ion_custom_data data;
    int err;
    private_device_t *dev = (private_device_t *)ion;

    data.cmd = ION_IOC_GET_PHYS;
    data.arg = (unsigned long)phys_data;

    err = ioctl(dev->ionfd, ION_IOC_CUSTOM, &data);
    if (err < 0) {
        LOGE("%s: ION_CUSTOM_GET_PHYS failed with error - %s",
             __FUNCTION__, strerror(errno));
        return err;
    }

    return err;
}

unsigned long ion_get_flags(enum ion_module_id id, enum _ion_heap_type type)
{
    unsigned long flags = 0;

    switch (id) {
    case ION_MODULE_CAM:
        flags |= 1 << ION_CAM_ID;
        break;
    case ION_MODULE_VPU:
        flags |= 1 << ION_VPU_ID;
        break;
    case ION_MODULE_UI:
        flags |= 1 << ION_UI_ID;
        break;
    default:
        break;
    }

    switch (type) {
    case _ION_HEAP_RESERVE:
        flags |= 1 << ION_NOR_HEAP_ID;
        break;
    default:
        flags |= 1 << ION_NOR_HEAP_ID;
        break;
    }
    return flags;
}
static int ion_alloc_vpu(struct ion_device_t *ion, unsigned long size, enum _ion_heap_type type, ion_buffer_t **data)
{
    int err = 0;
    struct ion_handle_data handle_data;
    struct ion_fd_data fd_data;
    struct ion_allocation_data ionData;
    struct ion_phys_data phys_data;
    void *virt = 0;
    unsigned long phys = 0;

    ALOGE("%s %d\n", __func__, __LINE__);

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }
    private_handle_t *hnd = (private_handle_t *)malloc(sizeof(private_handle_t));
    if (!hnd) {
        LOGE("%s: Failed to malloc private_handle_t hnd", __FUNCTION__);
        return -EINVAL;
    }

    ALOGE("%s %d\n", __func__, __LINE__);

    ionData.len = roundUpToPageSize(size);
    ionData.align = dev->align;
    if (dev->ionfd == FD_INIT) {
        dev->ionfd = open(ION_DEVICE, O_RDONLY);
        if (dev->ionfd < 0) {
            LOGE("%s: Failed to open %s - %s",
                 __FUNCTION__, ION_DEVICE, strerror(errno));
            goto free_hnd;
        }
    }

    ALOGE("%s %d\n", __func__, __LINE__);

    //ionData.flags = ion_get_flags(dev->id, type);
    ionData.flags = 0;
    ionData.heap_id_mask = 1 << ION_CMA_HEAP_ID;

    err = ioctl(dev->ionfd, ION_IOC_ALLOC, &ionData);
    if (err) {
        LOGE("%s: ION_IOC_ALLOC failed to alloc 0x%lx bytes with error(flags = 0x%x) - %s",
             __FUNCTION__, size, ionData.flags, strerror(errno));
        goto err_alloc;
    }
    fd_data.handle = ionData.handle;
    handle_data.handle = ionData.handle;

    ALOGE("%s %d\n", __func__, __LINE__);

    err = ioctl(dev->ionfd, ION_IOC_MAP, &fd_data);
    if (err) {
        LOGE("%s: ION_IOC_MAP failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_map;
    }

    ALOGE("%s %d\n", __func__, __LINE__);

    phys_data.handle = ionData.handle;
#if 0
    err = ioctl(dev->ionfd, ION_CUSTOM_GET_PHYS, &phys_data);
#else
    err = ion_custom_op(ion, ION_IOC_GET_PHYS, &phys_data);
#endif
    if (err) {
        LOGE("%s: ION_CUSTOM_GET_PHYS failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_phys;
    }

    ALOGE("%s %d\n", __func__, __LINE__);

    virt = mmap(0, ionData.len, PROT_READ | PROT_WRITE,
                MAP_SHARED, fd_data.fd, 0);
    if (virt == MAP_FAILED) {
        LOGE("%s: Failed to map the allocated memory: %s",
             __FUNCTION__, strerror(errno));
        goto err_mmap;
    }

    ALOGE("%s %d\n", __func__, __LINE__);

#if 0
    ALOGE("%s %d call free in alloc\n", __func__, __LINE__);
    err = ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
    if(err){
        LOGE("%s: ION_IOC_FREE failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_free;
    }
#endif
    hnd->data.virt = virt;
    hnd->data.phys = phys_data.phys;
    hnd->data.size = ionData.len;

    hnd->fd = fd_data.fd;
    hnd->pid = getpid();
    hnd->handle = (ion_user_handle_t)fd_data.handle;
    hnd->s_num_ints = NUM_INTS;
    hnd->s_num_fds = NUM_FDS;
    hnd->s_magic = MAGIC;

    *data = &hnd->data;
    LOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle %p",
         __FUNCTION__, gettid(), hnd->data.virt, hnd->data.phys, hnd->data.size / 1024, hnd->fd, hnd->handle);

#if 0
    {
        int count = 0;
        unsigned long client_allocated, heap_allocated, heap_size;

        ion->perform(ion, ION_MODULE_PERFORM_QUERY_BUFCOUNT, hnd->data.size, &count);
        ion->perform(ion, ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED, &client_allocated);
        ion->perform(ion, ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED, &heap_allocated);
        ion->perform(ion, ION_MODULE_PERFORM_QUERY_HEAP_SIZE, &heap_size);

        LOGV("%s: size(%luK) allocated %d times, client_allocated(%luK), heap_allocated(%luK), heap_size(%luK)",
             __FUNCTION__, size/1024, count, client_allocated/1024, heap_allocated/1024, heap_size/1024);
    }

#endif
    return 0;

err_free:
    munmap(virt, size);
err_mmap:
err_phys:
    close(fd_data.fd);
err_map:
    ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
err_alloc:
    close(dev->ionfd);
free_hnd:
    free(hnd);
    dev->ionfd = FD_INIT;
    *data = NULL;
    err = -errno;
    return err;

}
static int ion_free_vpu(struct ion_device_t *ion, ion_buffer_t *data)
{
    int err = 0;
    struct ion_handle_data handle_data;
    

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }
    private_handle_t *hnd = (private_handle_t *)data;
    if (validate(hnd) < 0) {
        LOGE("%s: Ion Handle Invalidate\n", __FUNCTION__);
        return -EINVAL;
    }

#if 1
    handle_data.handle = hnd->handle;
    err = ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
    if(err){
        LOGE("%s: ION_IOC_FREE failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_free;
    }
#endif

err_free:
    LOGV("%s: tid %d, base %p, phys %lx, size %luK, fd %d, handle %p",
         __FUNCTION__, gettid(), hnd->data.virt, hnd->data.phys, hnd->data.size / 1024, hnd->fd, hnd->handle);
    if (!hnd->data.virt) {
        LOGE("%s: Invalid free", __FUNCTION__);
        return -EINVAL;
    }
    err = munmap(hnd->data.virt, hnd->data.size);
    if (err) LOGE("%s: munmap failed", __FUNCTION__);

    close(hnd->fd);
    free(hnd);
    hnd = NULL;
    return err;
}

static int ion_share_vpu(struct ion_device_t *ion, ion_buffer_t *data, int *share_fd)
{
    int err = 0;
    struct ion_fd_data fd_data;

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }
    private_handle_t *hnd = (private_handle_t *)data;
    if (validate(hnd) < 0) {
        LOGE("%s, Ion_handle_t ion is Invalidate\n", __FUNCTION__);
        return -EINVAL;
    }

    fd_data.handle = (ion_user_handle_t)hnd->handle;
    err = ioctl(dev->ionfd, ION_IOC_SHARE, &fd_data);
    if (err) {
        LOGE("%s: ION_IOC_SHARE failed with error - %s",
             __FUNCTION__, strerror(errno));
        *share_fd = FD_INIT;
        err = -errno;
    } else {
        *share_fd = fd_data.fd;
    }
    LOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle: %p",
         __FUNCTION__, gettid(), hnd->data.virt, hnd->data.phys, hnd->data.size / 1024, *share_fd, hnd->handle);
    return err;
}

static int ion_map_vpu(struct ion_device_t *ion, int share_fd, ion_buffer_t **data)
{
    int err = 0;
    void *virt = NULL;
    unsigned long phys = 0;
    struct ion_phys_data phys_data;
    struct ion_fd_data fd_data;
    struct ion_handle_data handle_data;

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }
    private_handle_t *hnd = (private_handle_t *)malloc(sizeof(private_handle_t));
    if (!hnd) {
        LOGE("%s: Failed to malloc private_handle_t hnd", __FUNCTION__);
        return -EINVAL;
    }

    if (dev->ionfd == FD_INIT) {
        dev->ionfd = open(ION_DEVICE, O_RDONLY);
        if (dev->ionfd < 0) {
            LOGE("%s: Failed to open %s - %s",
                 __FUNCTION__, ION_DEVICE, strerror(errno));
            goto free_hnd;
        }
    }

    fd_data.fd = share_fd;
    err = ioctl(dev->ionfd, ION_IOC_IMPORT, &fd_data);
    if (err) {
        LOGE("%s: ION_IOC_IMPORT failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_import;
    }

    phys_data.handle = fd_data.handle;
#if 0
    err = ioctl(dev->ionfd, ION_CUSTOM_GET_PHYS, &phys_data);
#else
    err = ion_custom_op(ion, ION_IOC_GET_PHYS, &phys_data);
#endif
    if (err) {
        LOGE("%s: ION_CUSTOM_GET_PHYS failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_phys;
    }
    handle_data.handle = fd_data.handle;
    virt = mmap(0, phys_data.size, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
    if (virt == MAP_FAILED) {
        LOGE("%s: Failed to map memory in the client: %s",
             __FUNCTION__, strerror(errno));
        goto err_mmap;
    }
#if 0
    err = ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
    if (err) {
        LOGE("%s: ION_IOC_FREE failed with error - %s",
             __FUNCTION__, strerror(errno));
        goto err_free;
    }
#endif

    hnd->data.virt = virt;
    hnd->data.phys = phys_data.phys;
    hnd->data.size = phys_data.size;

    hnd->fd = share_fd;
    hnd->pid = getpid();
    hnd->handle = (ion_user_handle_t)fd_data.handle;
    hnd->s_num_ints = NUM_INTS;
    hnd->s_num_fds = NUM_FDS;
    hnd->s_magic = MAGIC;

    *data = &hnd->data;
    LOGV("%s: tid = %d, base %p, phys %lx, size %luK, fd %d, handle %p",
         __FUNCTION__, gettid(), hnd->data.virt, hnd->data.phys, hnd->data.size / 1024, hnd->fd, hnd->handle);

    return 0;
err_free:
err_mmap:
err_phys:
    ioctl(dev->ionfd, ION_IOC_FREE, &handle_data);
err_import:
    close(dev->ionfd);
free_hnd:
    free(hnd);
    dev->ionfd = FD_INIT;
    *data = NULL;
    err = -errno;
    return err;
}
static int ion_unmap_vpu(struct ion_device_t *ion, ion_buffer_t *data)
{
    int err = 0;

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }
    private_handle_t *hnd = (private_handle_t *)data;
    if (validate(hnd) < 0) return -EINVAL;

    LOGV("%s: base %p, phys %lx, size %luK, fd %d, handle %p",
         __FUNCTION__, hnd->data.virt, hnd->data.phys, hnd->data.size / 1024, hnd->fd, hnd->handle);
    if (!hnd->data.virt) {
        LOGE("%s: Invalid free", __FUNCTION__);
        return -EINVAL;
    }
    err = munmap(hnd->data.virt, hnd->data.size);
    if (err) return err;

    close(hnd->fd);
    free(hnd);
    hnd = NULL;
    return 0;
}

static int ion_cache_op(struct ion_device_t *ion, ion_buffer_t *data, enum cache_op_type type)
{
    int err = 0; 
    struct ion_flush_data flush_data;
    private_handle_t *hnd = (private_handle_t *)data;
    int op = 0;

    flush_data.handle = hnd->handle;
    flush_data.vaddr = hnd->data.virt;
    flush_data.offset = 0;
    flush_data.length = hnd->data.size;

    switch (type) {
    case ION_CLEAN_CACHE:
        op = ION_IOC_CLEAN_CACHES;
        break;
    case ION_INVALID_CACHE:
        op = ION_IOC_CLEAN_INV_CACHES;
        break;
    case ION_FLUSH_CACHE:
        op = ION_IOC_CLEAN_INV_CACHES;
        break;
    default:
        LOGE("%s: Unkown cmd type %d\n", __FUNCTION__, type);
        return -EINVAL;
    }

    err = ion_custom_op(ion, op, &flush_data);
    if (err) {
        LOGE("%s: ION_CUSTOM_CACHE_OP failed with error - %s",
                 __FUNCTION__, strerror(errno));
    }

    return err;

#if 0
    struct ion_cacheop_data cache_data;
    unsigned int cmd;

    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }

    private_handle_t *hnd = (private_handle_t *)data;
    if (validate(hnd) < 0) return -EINVAL;

    switch (type) {
    case ION_CLEAN_CACHE:
        cache_data.type = ION_CACHE_CLEAN;
        break;
    case ION_INVALID_CACHE:
        cache_data.type = ION_CACHE_INV;
        break;
    case ION_FLUSH_CACHE:
        cache_data.type = ION_CACHE_FLUSH;
        break;
    default:
        LOGE("%s: cache_op_type not support - %s",
             __FUNCTION__, strerror(errno));
        return -EINVAL;
    }
    cache_data.handle = hnd->handle;
    cache_data.virt = hnd->data.virt;
    err = ioctl(dev->ionfd, ION_CUSTOM_CACHE_OP, &cache_data);
#if 1
    if (err) {
        struct ion_pmem_region region;

        region.offset = (unsigned long)hnd->data.virt;
        region.len = hnd->data.size;
        err = ioctl(hnd->fd, ION_PMEM_CACHE_FLUSH, &region);
    }
#endif
    if (err) {
        LOGE("%s: ION_CUSTOM_CACHE_OP failed with error - %s",
             __FUNCTION__, strerror(errno));
        err = -errno;
        return err;
    }

    return 0;
#endif
}

int ion_perform(struct ion_device_t *ion, int operation, ...)
{
    int err = 0;
#if 0
    va_list args;
    private_device_t *dev = (private_device_t *)ion;
    if (!dev) {
        LOGE("%s: Ion_deivice_t ion is NULL", __FUNCTION__);
        return -EINVAL;
    }

    va_start(args, operation);
    switch (operation) {
    case ION_MODULE_PERFORM_QUERY_BUFCOUNT:
    case ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED:
        {
            struct ion_client_info info;
            unsigned long i, _count = 0, size = 0, *p;

            if (operation == ION_MODULE_PERFORM_QUERY_BUFCOUNT) size = va_arg(args, unsigned long);
            p = (unsigned long *)va_arg(args, void *);

            memset(&info, 0, sizeof(struct ion_client_info));
            err = ioctl(dev->ionfd, ION_CUSTOM_GET_CLIENT_INFO, &info);
            if (err) {
                LOGE("%s: ION_GET_CLIENT failed with error - %s",
                     __FUNCTION__, strerror(errno));
                err = -errno;
            } else {
                if (operation == ION_MODULE_PERFORM_QUERY_CLIENT_ALLOCATED) *p = info.total_size;
                else {
                    for (i = 0; i < info.count; i++) {
                        if (info.buf[i].size == size) _count++;
                    }
                    *p = _count;
                }
                err = 0;
            }
            break;
        }

    case ION_MODULE_PERFORM_QUERY_HEAP_SIZE:
    case ION_MODULE_PERFORM_QUERY_HEAP_ALLOCATED:
        {
            struct ion_heap_info info;

            unsigned long *p = (unsigned long *)va_arg(args, void *);

            info.id = ION_NOR_HEAP_ID;

            err = ioctl(dev->ionfd, ION_CUSTOM_GET_HEAP_INFO, &info);
            if (err) {
                LOGE("%s: ION_GET_CLIENT failed with error - %s",
                     __FUNCTION__, strerror(errno));
                err = -errno;
            } else {
                if (operation == ION_MODULE_PERFORM_QUERY_HEAP_SIZE) *p = info.total_size;
                else *p = info.allocated_size;
                err = 0;
            }
            break;
        }
    default:
        LOGE("%s: operation(0x%x) not support", __FUNCTION__, operation);
        err = -EINVAL;
        break;
    }

    va_end(args);
#endif
    return err;
}
int ion_open_vpu(unsigned long align, enum ion_module_id id, ion_device_t **ion)
{
    char name[16];
    private_device_t *dev = (private_device_t *)malloc(sizeof(private_device_t));
    if (!dev) return -EINVAL;

    dev->ionfd = FD_INIT;
    dev->align = align;
    dev->id = id;
    dev->ion.alloc = ion_alloc_vpu;
    dev->ion.free = ion_free_vpu;
    dev->ion.share = ion_share_vpu;
    dev->ion.map = ion_map_vpu;
    dev->ion.unmap = ion_unmap_vpu;
    dev->ion.cache_op = ion_cache_op;
    dev->ion.perform = ion_perform;

    *ion = &dev->ion;
    switch (id) {
    case ION_MODULE_VPU:
        strcpy(name, "vpu");
        break;
    case ION_MODULE_CAM:
        strcpy(name, "camera");
        break;
    case ION_MODULE_UI:
        strcpy(name, "ui");
        break;
    default:
        strcpy(name, "ui");
        break;
    }
    LOGV("Ion(version: %s) is successfully opened by %s",
         ION_VERSION, name);
    return 0;
}
int ion_close_vpu(ion_device_t *ion)
{
    private_device_t *dev = (private_device_t *)ion;

    if (dev->ionfd != FD_INIT) close(dev->ionfd);
    free(dev);

    return 0;
}
