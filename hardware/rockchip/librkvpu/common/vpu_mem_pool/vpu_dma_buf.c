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

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <linux/rockchip_ion.h>
#include <ion/ion.h>

#include <cutils/log.h>

#include "list.h"
#include "classmagic.h"
#include "vpu_dma_buf.h"
#include "atomic.h"
#include "vpu.h"
#include <errno.h>
#define LOG_TAG "vpu_dma_buf"

#define ENABLE_DUPLICATE_WITH_REF   1

//#define VPU_DMABUF_DEBUG
#define VPU_DMABUF_ERROR

#ifdef VPU_DMABUF_DEBUG
#define DMABUF_DBG(fmt, args...) ALOGD("%s:%d: " fmt, __func__, __LINE__, ## args)
#else
#define DMABUF_DBG(fmt, args...)
#endif

#ifdef VPU_DMABUF_ERROR
#define DMABUF_ERR(fmt, args...) ALOGE("%s:%d: " fmt, __func__, __LINE__, ## args)
#else
#define DMABUF_ERR(fmt, args...)
#endif

DERIVEDCLASS(vpu_dmabuf_dev_impl, vpu_dmabuf_dev)
#define vpu_dmabuf_dev_impl_FIELDS vpu_dmabuf_dev_FIELDS \
    struct list_head mem_list; \
    int ion_client; \
    unsigned long align; \
    pthread_mutex_t list_lock; \
    char title[20];
ENDCLASS(vpu_dmabuf_dev_impl)

#define DMABUF_STATUS_ALLOC     (1L<<0)
#define DMABUF_STATUS_SHARE     (1L<<1)
#define DMABUF_STATUS_MAP       (1L<<2)
#define DMABUF_STATUS_MAX       (DMABUF_STATUS_MAP+1)

char dmabuf_status[5][6] = {"inval", "alloc", "share", "inval", "map"};

#define ION_HEAP_INVALID_ID (~(ION_HEAP(ION_CARVEOUT_HEAP_ID) | ION_HEAP(ION_CMA_HEAP_ID) | ION_HEAP(ION_VMALLOC_HEAP_ID)))
static int g_heap_mask = ION_HEAP_INVALID_ID;

union addr_cfg {
    RK_U32 map_fd;
    RK_U32 phy_addr;
};

typedef struct VPUMemLinear_dmabuf {
    union addr_cfg cfg;
    RK_U32  *vir_addr;
    RK_U32  size;
    RK_S32  offset;
#ifdef RVDS_PROGRAME
    RK_U8  *pbase;
#endif
    int origin_fd;
    int hdl;
    ion_user_handle_t handle;
    void *priv;
    struct list_head mgr_lnk;
    int status;
#if ENABLE_DUPLICATE_WITH_REF
    atomic_t ref_cnt;
#endif
} VPUMemLinear_dmabuf;

#define ENABLE_MEMORY_MANAGE    1
#define ENABLE_MEMORY_DUMP	0

int vpu_dmabuf_dump(struct vpu_dmabuf_dev *idev, const char *parent)
{
    struct vpu_dmabuf_dev_impl *dev = (struct vpu_dmabuf_dev_impl*)idev;
    VPUMemLinear_dmabuf *ldata = NULL, *n;
    int total = 0;
    int cnt = 0;

    ALOGD("current vpu memory status in %s from %s\n", dev->title, parent);

    pthread_mutex_lock(&dev->list_lock);
    list_for_each_entry_safe(ldata, n, &dev->mem_list, mgr_lnk) {
        if ((ldata->status & (DMABUF_STATUS_ALLOC | DMABUF_STATUS_MAP | DMABUF_STATUS_SHARE)) != 0) {
            ALOGD("[%02d]\t%08d @ 0x%08x (%s)\n", ldata->hdl, ldata->size, ldata->cfg.phy_addr, dmabuf_status[ldata->status]);
            total += ldata->size;
            cnt++;
        }
    }
    pthread_mutex_unlock(&dev->list_lock);

    ALOGD("---------- total %d count %d -------------\n", total, cnt);

    return 0;
}

static int ion_heap_type_test(int heap_mask)
{
    int ion_client;
    ion_user_handle_t handle;

    if (g_heap_mask != ION_HEAP_INVALID_ID) {
        return g_heap_mask;
    }
        
    ion_client = ion_open();

    if (ion_alloc(ion_client, 1, 0, heap_mask, 0, &handle) < 0) {
        ion_close(ion_client);
        return ION_HEAP_INVALID_ID;
    }

    ion_free(ion_client, handle);
    ion_close(ion_client);

    g_heap_mask = heap_mask;

    return g_heap_mask;
}

int vpu_mem_judge_used_heaps_type()
{
    // TODO, use property_get
    if (!VPUClientGetIOMMUStatus() > 0) {
        if (ion_heap_type_test(ION_HEAP(ION_CARVEOUT_HEAP_ID)) == ION_HEAP(ION_CARVEOUT_HEAP_ID)) {
            ALOGV("USE ION_CARVEOUT_HEAP_ID");
            return ION_HEAP(ION_CARVEOUT_HEAP_ID);
        } else if (ion_heap_type_test(ION_HEAP(ION_CMA_HEAP_ID)) == ION_HEAP(ION_CMA_HEAP_ID)) {
            ALOGV("USE ION_CMA_HEAP_ID");
            return ION_HEAP(ION_CMA_HEAP_ID);
        }
    } else {
        ALOGV("USE ION_SYSTEM_HEAP");
        return ION_HEAP(ION_VMALLOC_HEAP_ID);
    }

    return 0;
}

static int ion_custom_op(vpu_dmabuf_dev_impl *dev, int op, void *op_data)
{
    struct ion_custom_data data;
    int err;

    data.cmd = op;
    data.arg = (unsigned long)op_data;

    err = ioctl(dev->ion_client, ION_IOC_CUSTOM, &data);
    if (err < 0) {
        DMABUF_ERR("ION_IOC_CUSTOM (%d) failed with error - %s", op, strerror(errno));
        return err;
    }

    return err;
}

static int vpu_dmabuf_alloc(struct vpu_dmabuf_dev *idev, size_t size, VPUMemLinear_t **data)
{
    int err = 0;
    struct ion_phys_data phys_data;
    VPUMemLinear_dmabuf *dmabuf;
    int map_fd;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;

    if (dev == NULL) {
        DMABUF_ERR("input parameter invalidate\n");
        return -EINVAL;
    }

    dmabuf = (VPUMemLinear_dmabuf*)calloc(1, sizeof(VPUMemLinear_dmabuf));
    if (dmabuf == NULL) {
        DMABUF_ERR("memory allocate failed\n");
        return -ENOMEM;
    }

    err = ion_alloc(dev->ion_client, size, dev->align, vpu_mem_judge_used_heaps_type(), 0, &dmabuf->handle);
    if (err) {
        DMABUF_ERR("ion alloc failed\n");
#if ENABLE_MEMORY_MANAGE & ENABLE_MEMORY_DUMP
        vpu_dmabuf_dump(idev, __func__);
#endif
        free(dmabuf);
        return err;
    }

    DMABUF_DBG("handle %p\n", dmabuf->handle);

    err = ion_map(dev->ion_client, dmabuf->handle, size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, (off_t)0, (unsigned char**)&dmabuf->vir_addr, &map_fd);
    if (err) {
        DMABUF_ERR("ion map failed\n");
        free(dmabuf);
        return err;
    }

    if (vpu_mem_judge_used_heaps_type() != ION_HEAP(ION_VMALLOC_HEAP_ID)) {
        phys_data.handle = dmabuf->handle;
        err = ion_custom_op(dev, ION_IOC_GET_PHYS, &phys_data);
        if (err) {
            DMABUF_ERR("ion get phys failed\n");
            free(dmabuf);
            return err;
        }
        dmabuf->cfg.phy_addr = phys_data.phys;
    } else {
        dmabuf->cfg.map_fd = map_fd;
    }

    dmabuf->size = size;//phys_data.size;
    dmabuf->origin_fd = -1;
    dmabuf->hdl = map_fd;
#if ENABLE_DUPLICATE_WITH_REF
    atomic_set(&dmabuf->ref_cnt, 1);
#endif

    dmabuf->status = DMABUF_STATUS_ALLOC;

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_lock(&dev->list_lock);
    INIT_LIST_HEAD(&dmabuf->mgr_lnk);
    list_add_tail(&dmabuf->mgr_lnk, &dev->mem_list);
    pthread_mutex_unlock(&dev->list_lock);

#if ENABLE_MEMORY_DUMP
    vpu_dmabuf_dump(idev, __func__);
#endif
#endif

    *data = (VPUMemLinear_t*)dmabuf;

    DMABUF_DBG("success\n");

    return 0;
}

static int vpu_dmabuf_free(struct vpu_dmabuf_dev *idev, VPUMemLinear_t *idata)
{
    int err = 0;
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;
    int ref;

    if (dev == NULL || data == NULL) {
        DMABUF_ERR("input parameters invalidate\n");
        return -EINVAL;
    }

#if ENABLE_DUPLICATE_WITH_REF
    atomic_dec(&data->ref_cnt);
    ref = atomic_read(&data->ref_cnt);
    if (ref > 0) {
        return 0;
    }
#endif

#if ENABLE_MEMORY_MANAGE
    {
        VPUMemLinear_dmabuf *ldata = NULL, *n;

        pthread_mutex_lock(&dev->list_lock);
        list_for_each_entry_safe(ldata, n, &dev->mem_list, mgr_lnk) {
            if (ldata->hdl == data->hdl) {
                if ((ldata->status & (DMABUF_STATUS_ALLOC | DMABUF_STATUS_MAP)) != 0) {
                    DMABUF_DBG("a validate dmabuf\n");
                    break;
                }
            }
        }
        pthread_mutex_unlock(&dev->list_lock);

        if (ldata == NULL) {
            DMABUF_ERR("A invalidate dmabuf to free, fd = %d, phy_addr = 0x%08x, vir_addr = %p\n", data->hdl, data->cfg.phy_addr, data->vir_addr);
            return -EINVAL;
        }
    }
#else
    if ((data->status & (DMABUF_STATUS_ALLOC | DMABUF_STATUS_MAP)) != 0) {
        DMABUF_DBG("a validate dmabuf\n");
    } else {
        if (data->status < DMABUF_STATUS_MAX) {
            DMABUF_ERR("a invalidate dmabuf status = %s\n", dmabuf_status[data->status]);
        } else {
            DMABUF_ERR("Fatal ERROR!!!, a unkown dmabuf\n");
        }
        return -EINVAL;
    }
#endif

#if ENABLE_MEMORY_DUMP
    ALOGD("[%02d]\t%08d @ 0x%08x (%s)\n", data->hdl, data->size, data->cfg.phy_addr, "free");
#endif

    err = munmap(data->vir_addr, data->size);
    if (err) {
        DMABUF_ERR("munmap failed, vir addr %p\n", data->vir_addr);
        return err;
    }

    err = ion_free(dev->ion_client, data->handle);
    if (err) {
        DMABUF_ERR("ion free failed, handle %d\n", data->handle);
        return err;
    }

    if (data->hdl != data->origin_fd) {
        close(data->hdl);
    }

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_lock(&dev->list_lock);
    list_del_init(&data->mgr_lnk);
    pthread_mutex_unlock(&dev->list_lock);

#if ENABLE_MEMORY_DUMP
    vpu_dmabuf_dump(idev, __func__);
#endif
#endif

    free(data);

    DMABUF_DBG("success\n");

    return 0;
}

static int vpu_dmabuf_share(struct vpu_dmabuf_dev *idev,
                            VPUMemLinear_t *idata,
                            VPUMemLinear_t **out_data)
{
    int err = 0;
    VPUMemLinear_dmabuf *out;
    int share_fd;
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;

    if (dev == NULL || data == NULL) {
        DMABUF_ERR("input paramters invalidate\n");
        return -EINVAL;
    }

#if ENABLE_DUPLICATE_WITH_REF
    atomic_inc(&data->ref_cnt);
    *out_data = idata;
    return 0;
#endif

#if ENABLE_MEMORY_MANAGE
    {
        VPUMemLinear_dmabuf *ldata = NULL, *n;

        pthread_mutex_lock(&dev->list_lock);
        list_for_each_entry_safe(ldata, n, &dev->mem_list, mgr_lnk) {
            if (ldata->hdl == data->hdl) {
                if ((ldata->status & (DMABUF_STATUS_ALLOC | DMABUF_STATUS_MAP)) != 0) {
                    DMABUF_DBG("a validate dmabuf\n");
                    break;
                } else if (ldata->status < DMABUF_STATUS_MAX) {
                    pthread_mutex_unlock(&dev->list_lock);
                    DMABUF_ERR("a dmabuf in invalidate status %s\n", dmabuf_status[ldata->status]);
                    return -EINVAL;
                } else {
                    pthread_mutex_unlock(&dev->list_lock);
                    DMABUF_ERR("fatal error!!!, unkown status %d\n", ldata->status);
                    return -EINVAL;
                }
            }
        }
        pthread_mutex_unlock(&dev->list_lock);

        if (ldata == NULL) {
            DMABUF_ERR("A invalidate dmabuf to share, fd = %d, phy_addr = 0x%08x, vir_addr = %p\n", data->hdl, data->cfg.phy_addr, data->vir_addr);
            return -EINVAL;
        }
    }
#else
    if ((data->status & (DMABUF_STATUS_ALLOC | DMABUF_STATUS_MAP)) != 0) {
        DMABUF_DBG("a validate dmabuf\n");
    } else {
        if (data->status < DMABUF_STATUS_MAX) {
            DMABUF_ERR("a invalidate dmabuf status = %s\n", dmabuf_status[data->status]);
        } else {
            DMABUF_ERR("Fatal ERROR!!!, a unkown dmabuf\n");
        }
        return -EINVAL;
    }
#endif

    err = ion_share(dev->ion_client, data->handle, &share_fd);
    if (err) {
        DMABUF_ERR("ion share failed, input handle %d\n", data->handle);
        return err;
    }

    out = (VPUMemLinear_dmabuf*)calloc(1, sizeof(VPUMemLinear_dmabuf));
    if (out == NULL) {
        DMABUF_ERR("memory allocate failed\n");
        return -EINVAL;
    }

    out->origin_fd = data->origin_fd;
    out->hdl = share_fd;
    out->vir_addr = NULL;
    out->cfg.phy_addr = data->cfg.phy_addr;
    out->size     = data->size;
    out->handle   = 0;
    out->priv     = data->priv;
    out->status = DMABUF_STATUS_SHARE;

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_lock(&dev->list_lock);
    INIT_LIST_HEAD(&out->mgr_lnk);
    list_add_tail(&out->mgr_lnk, &dev->mem_list);
    pthread_mutex_unlock(&dev->list_lock);
#endif

    *out_data = (VPUMemLinear_t*)out;

    return 0;
}

static int vpu_dmabuf_map(struct vpu_dmabuf_dev *idev,
                          int share_fd, size_t size,
                          VPUMemLinear_t **data)
{
    int err = 0;
    int map_fd = -1;
    struct ion_phys_data phys_data;
    VPUMemLinear_dmabuf *dmabuf;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;
    bool bAlloc = false;

    if (dev == NULL) {
        DMABUF_ERR("device is NULL\n");
        return -EINVAL;
    }

    if (*data == NULL) {
        // special for renderer buffer
        dmabuf = (VPUMemLinear_dmabuf*)calloc(1, sizeof(VPUMemLinear_dmabuf));
        if (dmabuf == NULL) {
            DMABUF_ERR("memory allocate buffer failed\n");
            return -ENOMEM;
        }

        bAlloc = true;
    } else {
#if ENABLE_DUPLICATE_WITH_REF
        return 0;
#endif

        dmabuf = (VPUMemLinear_dmabuf*)*data;
        share_fd = dmabuf->hdl;

        if (dmabuf->handle) {
            DMABUF_ERR("Memory Handle %d, nothing to map\n", dmabuf->handle);
            return -EINVAL;
        }

        bAlloc = false;
    }

    if (!bAlloc) {
#if ENABLE_MEMORY_MANAGE
        VPUMemLinear_dmabuf *ldata = NULL, *n;

        pthread_mutex_lock(&dev->list_lock);
        list_for_each_entry_safe(ldata, n, &dev->mem_list, mgr_lnk) {
            if (ldata->hdl == dmabuf->hdl) {
                if ((ldata->status & DMABUF_STATUS_SHARE) != 0) {
                    DMABUF_DBG("a validate dmabuf\n");
                    list_del_init(&ldata->mgr_lnk);
                    break;
                } else if (ldata->status < DMABUF_STATUS_MAX) {
                    pthread_mutex_unlock(&dev->list_lock);
                    DMABUF_ERR("a dmabuf in invalidate status %s\n", dmabuf_status[ldata->status]);
                    return -EINVAL;
                } else {
                    pthread_mutex_unlock(&dev->list_lock);
                    DMABUF_ERR("fatal error!!!, unkown status %d\n", ldata->status);
                    return -EINVAL;
                }
            }
        }
        pthread_mutex_unlock(&dev->list_lock);

        if (ldata == NULL) {
            DMABUF_ERR("A invalidate dmabuf to map, fd = %d, phy_addr = 0x%08x, vir_addr = %p\n", dmabuf->hdl, dmabuf->cfg.phy_addr, dmabuf->vir_addr);
            return -EINVAL;
        }
#else
        if ((dmabuf->status & DMABUF_STATUS_SHARE) != 0) {
            DMABUF_DBG("a validate dmabuf\n");
        } else {
            if (dmabuf->status < DMABUF_STATUS_MAX) {
                DMABUF_ERR("a invalidate dmabuf status = %s\n", dmabuf_status[dmabuf->status]);
            } else {
                DMABUF_ERR("Fatal ERROR!!!, a unkown dmabuf\n");
            }
            return -EINVAL;
        }
#endif
    }


    err = ion_import(dev->ion_client, share_fd, &dmabuf->handle);
    if (err) {
        DMABUF_ERR("ion import failed, share fd %d\n", share_fd);
        free(dmabuf);
        return err;
    }

    dmabuf->vir_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
    if (dmabuf->vir_addr == MAP_FAILED) {
        DMABUF_ERR("ion map failed, %s\n", strerror(errno));
        free(dmabuf);
        return -EINVAL;
    }

    if (vpu_mem_judge_used_heaps_type() != ION_HEAP(ION_VMALLOC_HEAP_ID)) {
        phys_data.handle = dmabuf->handle;
        err = ion_custom_op(dev, ION_IOC_GET_PHYS, &phys_data);
        if (err) {
            DMABUF_ERR("ion get phys failed\n");
            munmap(dmabuf->vir_addr, size);
            free(dmabuf);
            return -EINVAL;
        }
        dmabuf->cfg.phy_addr = phys_data.phys;
    } else {
        dmabuf->cfg.map_fd = share_fd;
    }


    dmabuf->size = size;//phys_data.size;
    dmabuf->hdl = share_fd;
    dmabuf->status = DMABUF_STATUS_MAP;
 #if ENABLE_DUPLICATE_WITH_REF
    atomic_set(&dmabuf->ref_cnt, 1);
 #endif

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_lock(&dev->list_lock);
    INIT_LIST_HEAD(&dmabuf->mgr_lnk);
    list_add_tail(&dmabuf->mgr_lnk, &dev->mem_list);
    pthread_mutex_unlock(&dev->list_lock);

#if ENABLE_MEMORY_DUMP
    vpu_dmabuf_dump(idev, __func__);
#endif
#endif

    *data = (VPUMemLinear_t*)dmabuf;

    DMABUF_DBG("success\n");

    return 0;
}

static int vpu_dmabuf_get_phyaddress(struct vpu_dmabuf_dev *idev,
                          int share_fd,uint32_t *phy_addr)
{
    int err = 0;
    int map_fd = -1;
    struct ion_phys_data phys_data;
    VPUMemLinear_dmabuf *dmabuf;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;
    ion_user_handle_t  handle = 0;

    if (dev == NULL) {
        DMABUF_ERR("device is NULL\n");
        return -EINVAL;
    }
    err = ion_import(dev->ion_client, share_fd, &handle);
    if (err) {
        DMABUF_ERR("ion import failed, share fd %d\n", share_fd);
        return err;
    }
    if (vpu_mem_judge_used_heaps_type() != ION_HEAP(ION_VMALLOC_HEAP_ID)) {
        phys_data.handle = handle;
        err = ion_custom_op(dev, ION_IOC_GET_PHYS, &phys_data);
        if (err) {
            ion_free(dev->ion_client,handle);
            return err;
        }
        *phy_addr = phys_data.phys;
    } else {
        *phy_addr = share_fd;
    }
    ion_free(dev->ion_client,handle);
    return 0;
}
static int vpu_dmabuf_unmap(struct vpu_dmabuf_dev *idev, VPUMemLinear_t *idata)
{
    int err = 0;
    int ref = 0;
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;

    DMABUF_DBG("In\n");

    if (dev == NULL || data == NULL) {
        DMABUF_ERR("vpu dmabuf unmap input paramters invalidate\n");
        return -EINVAL;
    }

#if ENABLE_DUPLICATE_WITH_REF
    atomic_dec(&data->ref_cnt);
    ref = atomic_read(&data->ref_cnt);
    if (ref > 0) {
        return 0;
    }
#endif

    DMABUF_DBG("vir_addr %p, size %d\n", data->vir_addr, data->size);

#if ENABLE_MEMORY_DUMP
    ALOGD("[%02d]\t%08d @ 0x%08x (%s)\n", data->hdl, data->size, data->cfg.phy_addr, "unmap");
#endif

    err = munmap(data->vir_addr, data->size);
    if (err) {
        DMABUF_DBG("munmap failed\n");
        return err;
    }

    err = ion_free(dev->ion_client, data->handle);
    if (err) {
        DMABUF_ERR("ion free failed, handle %d\n", data->handle);
        return err;
    }

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_lock(&dev->list_lock);
    list_del_init(&data->mgr_lnk);
    pthread_mutex_unlock(&dev->list_lock);

#if ENABLE_MEMORY_DUMP
    vpu_dmabuf_dump(idev, __func__);
#endif
#endif

    free(data);

    return 0;
}

static int vpu_dmabuf_reserve_private_data(VPUMemLinear_t *idata, int origin_fd, void *priv)
{
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;

    if (data == NULL || priv == NULL) {
        DMABUF_ERR("vpu dmabuf reserve input parameters invalidate\n");
        return -EINVAL;
    }

    data->origin_fd = origin_fd;
    data->priv = priv;

    return 0;
}

static int vpu_dmabuf_get_origin_fd(VPUMemLinear_t *idata)
{
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;

     if (data == NULL) {
        DMABUF_ERR("vpu dmabuf get origin fd input parameter invalidate\n");
        return -1;
    }

    return data->origin_fd;
}

static int vpu_dmabuf_get_fd(VPUMemLinear_t *idata)
{
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;

     if (data == NULL) {
        DMABUF_ERR("vpu dmabuf get fd input parameter invalidate\n");
        return -1;
    }

    return data->hdl;
}

static void* vpu_dmabuf_get_priv(VPUMemLinear_t *idata)
{
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;

    if (data == NULL) {
        DMABUF_ERR("vpu dmabuf get priv input parameter invalidate\n");
        return NULL;
    }

    return data->priv;
}

static int vpu_dmabuf_get_ref(VPUMemLinear_t *idata)
{
    VPUMemLinear_dmabuf *data = (VPUMemLinear_dmabuf*)idata;
    int ref;

    if (data == NULL) {
        DMABUF_ERR("vpu dmabuf get priv input parameter invalidate\n");
        return -1;
    }
#if ENABLE_DUPLICATE_WITH_REF
    ref = atomic_read(&data->ref_cnt);
#endif
    return ref;
}

int vpu_dmabuf_open(unsigned long align, struct vpu_dmabuf_dev **dev, char *title)
{
    vpu_dmabuf_dev_impl *d = (vpu_dmabuf_dev_impl *)calloc(1, sizeof(vpu_dmabuf_dev_impl));
    if (!d) return -EINVAL;

    d->ion_client = ion_open();
    d->alloc = vpu_dmabuf_alloc;
    d->free = vpu_dmabuf_free;
    d->map = vpu_dmabuf_map;
    d->unmap = vpu_dmabuf_unmap;
    d->share = vpu_dmabuf_share;
    d->reserve = vpu_dmabuf_reserve_private_data;
    d->get_origin_fd = vpu_dmabuf_get_origin_fd;
    d->get_fd = vpu_dmabuf_get_fd;
    d->get_ref = vpu_dmabuf_get_ref;
    d->get_priv = vpu_dmabuf_get_priv;
    d->get_phyaddr = vpu_dmabuf_get_phyaddress;
    d->align = align;

    if (strlen(title) != 0) {
        strcpy(d->title, title);
    } else {
        strcpy(d->title, "anonymous");
    }

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_init(&d->list_lock, NULL);
    INIT_LIST_HEAD(&d->mem_list);
#endif

    *dev = (vpu_dmabuf_dev*)d;

    DMABUF_DBG("successfully opened\n");

    return 0;
}

int vpu_dmabuf_close(struct vpu_dmabuf_dev *idev)
{
    vpu_dmabuf_dev_impl *dev = (vpu_dmabuf_dev_impl*)idev;

#if ENABLE_MEMORY_MANAGE
    pthread_mutex_destroy(&dev->list_lock);
#endif

    ion_close(dev->ion_client);

    free(dev);

    return 0;
}

