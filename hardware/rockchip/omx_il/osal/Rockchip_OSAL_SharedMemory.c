/*
 *
 * Copyright 2013 Rockchip Electronics Co. LTD
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
 * @file        Rockchip_OSAL_SharedMemory.c
 * @brief
 * @author      csy(csy@rock-chips.com)
 * @version     1.0.0
 * @history
 *   2013.11.26 : Create
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "vpu.h"

#include "drm_mode.h"
#include "drm.h"

#include "Rockchip_OSAL_SharedMemory.h"
#include "Rockchip_OSAL_Mutex.h"

#include <linux/rockchip_ion.h>
#include <linux/ion.h>
#include <dlfcn.h>
#include <cutils/native_handle.h>



#define ROCKCHIP_LOG_OFF
#include "Rockchip_OSAL_Log.h"

#ifndef ION_SECURE_HEAP_ID
#define ION_SECURE_HEAP_ID ION_CMA_HEAP_ID
#endif

static int mem_cnt = 0;
static int mem_type = MEMORY_TYPE_ION;

struct ROCKCHIP_SHAREDMEM_LIST;
typedef struct _ROCKCHIP_SHAREDMEM_LIST {
    RK_U32                         ion_hdl;
    OMX_PTR                        mapAddr;
    OMX_U32                        allocSize;
    OMX_BOOL                       owner;
    struct _ROCKCHIP_SHAREDMEM_LIST *pNextMemory;
} ROCKCHIP_SHAREDMEM_LIST;

typedef struct _ROCKCHIP_SHARED_MEMORY {
    int         fd;
    ROCKCHIP_SHAREDMEM_LIST *pAllocMemory;
    OMX_HANDLETYPE         hSMMutex;
} ROCKCHIP_SHARED_MEMORY;

#define ION_FUNCTION                (0x00000001)
#define ION_DEVICE                  (0x00000002)
#define ION_CLINET                  (0x00000004)
#define ION_IOCTL                   (0x00000008)

static int ion_ioctl(int fd, int req, void *arg)
{
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
        omx_err("ion_ioctl %x failed with code %d: %s\n", req,
                ret, strerror(errno));
        return -errno;
    }
    return ret;
}

static int ion_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
                     unsigned int flags, ion_user_handle_t *handle)
{
    int ret;
    struct ion_allocation_data data = {
        .len = len,
        .align = align,
        .heap_id_mask = heap_mask,
        .flags = flags,
    };

    if (handle == NULL)
        return -EINVAL;

    ret = ion_ioctl(fd, ION_IOC_ALLOC, &data);
    if (ret < 0)
        return ret;
    *handle = data.handle;
    return ret;
}

static int ion_free(int fd, ion_user_handle_t handle)
{
    struct ion_handle_data data = {
        .handle = handle,
    };
    return ion_ioctl(fd, ION_IOC_FREE, &data);
}

static int ion_map(int fd, ion_user_handle_t handle, size_t length, int prot,
                   int flags, off_t offset, unsigned char **ptr, int *map_fd)
{
    int ret;
    struct ion_fd_data data = {
        .handle = handle,
    };

    if (map_fd == NULL)
        return -EINVAL;
    if (ptr == NULL)
        return -EINVAL;

    ret = ion_ioctl(fd, ION_IOC_MAP, &data);
    if (ret < 0)
        return ret;
    *map_fd = data.fd;
    if (*map_fd < 0) {
        omx_err("map ioctl returned negative fd\n");
        return -EINVAL;
    }
    *ptr = mmap(NULL, length, prot, flags, *map_fd, offset);
    if (*ptr == MAP_FAILED) {
        omx_err("mmap failed: %s\n", strerror(errno));
        return -errno;
    }
    return ret;
}

int ion_import(int fd, int share_fd, ion_user_handle_t *handle)
{
    int ret;
    struct ion_fd_data data = {
        .fd = share_fd,
    };

    if (handle == NULL)
        return -EINVAL;

    ret = ion_ioctl(fd, ION_IOC_IMPORT, &data);
    if (ret < 0)
        return ret;
    *handle = data.handle;
    return ret;
}

int ion_get_phys(int fd, ion_user_handle_t handle, unsigned long *phys)
{
    struct ion_phys_data phys_data;
    struct ion_custom_data data;

    phys_data.handle = handle;
    phys_data.phys = 0;

    data.cmd = ION_IOC_GET_PHYS;
    data.arg = (unsigned long)&phys_data;

    int ret = ion_ioctl(fd, ION_IOC_CUSTOM, &data);
    omx_err("ion_get_phys:phys_data.phys = %p", phys_data.phys);
    omx_err("ion_get_phys:phys_data.size = %ld", phys_data.size);
    if (ret < 0)
        return ret;

    *phys = phys_data.phys;

    return 0;
}


static int drm_ioctl(int fd, int req, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, req, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return ret;
}


typedef void *(*func_mmap64)(void* addr, size_t length, int prot, int flags,
                             int fd, off_t offset);
static func_mmap64 mpp_rt_mmap64 = NULL;


func_mmap64 mpp_rt_get_mmap64()
{
    static RK_U32 once = 1;

    if (once) {
        void *libc_hdl = dlopen("libc", RTLD_LAZY);
        if (libc_hdl) {
            mpp_rt_mmap64 = (func_mmap64)dlsym(libc_hdl, "mmap64");
            dlclose(libc_hdl);
        }

        once = 0;
    }


    return mpp_rt_mmap64;
}


static void* drm_mmap(int fd, size_t len, int prot, int flags, loff_t offset)
{
    static unsigned long pagesize_mask = 0;
    func_mmap64 fp_mmap64 = mpp_rt_get_mmap64();

    if (fd < 0)
        return NULL;

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;

    len = (len + pagesize_mask) & ~pagesize_mask;

    if (offset & 4095) {
        return NULL;
    }

    return mmap64(NULL, len, prot, flags, fd, offset);
}


static int drm_handle_to_fd(int fd, RK_U32 handle, int *map_fd, RK_U32 flags)
{
    int ret;
    struct drm_prime_handle dph;
    memset(&dph, 0, sizeof(struct drm_prime_handle));
    dph.handle = handle;
    dph.fd = -1;
    dph.flags = flags;

    if (map_fd == NULL)
        return -EINVAL;

    ret = drm_ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &dph);
    if (ret < 0) {
        return ret;
    }

    *map_fd = dph.fd;
    if (*map_fd < 0) {
        omx_err("map ioctl returned negative fd\n");
        return -EINVAL;
    }

    return ret;
}



static int drm_fd_to_handle(int fd, int map_fd, RK_U32 *handle, RK_U32 flags)
{
    int ret;
    struct drm_prime_handle dph;

    dph.fd = map_fd;
    dph.flags = flags;

    ret = drm_ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &dph);
    if (ret < 0) {
        return ret;
    }

    *handle = dph.handle;
    return ret;
}


static int drm_map(int fd, RK_U32 handle, size_t length, int prot,
                   int flags, unsigned char **ptr, int *map_fd)
{
    int ret;
    struct drm_mode_map_dumb dmmd;
    memset(&dmmd, 0, sizeof(dmmd));
    dmmd.handle = handle;

    if (map_fd == NULL)
        return -EINVAL;
    if (ptr == NULL)
        return -EINVAL;

    ret = drm_handle_to_fd(fd, handle, map_fd, 0);
    omx_err("drm_map fd %d", *map_fd);
    if (ret < 0)
        return ret;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret < 0) {
        omx_err("drm_map FAIL");
        close(*map_fd);
        return ret;
    }

    omx_trace("dev fd %d length %d", fd, length);

    *ptr = drm_mmap(fd, length, prot, flags, dmmd.offset);
    if (*ptr == MAP_FAILED) {
        close(*map_fd);
        *map_fd = -1;
        omx_err("mmap failed: %s\n", strerror(errno));
        return -errno;
    }

    return ret;
}





static int drm_alloc(int fd, size_t len, size_t align, RK_U32 *handle, int flag)
{
    int ret;
    struct drm_mode_create_dumb dmcb;

    memset(&dmcb, 0, sizeof(struct drm_mode_create_dumb));
    dmcb.bpp = 8;
    dmcb.width = (len + align - 1) & (~(align - 1));
    dmcb.height = 1;
    dmcb.size = dmcb.width * dmcb.bpp;
    dmcb.flags = flag;

    if (handle == NULL)
        return -1;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dmcb);
    if (ret < 0) {
        omx_err("drm_alloc fail: ret = %d", ret);
        return ret;
    }
    *handle = dmcb.handle;

    omx_trace("drm_alloc success:  handle %d size %d", *handle, dmcb.size);

    return ret;
}




static int drm_free(int fd, RK_U32 handle)
{
    struct drm_mode_destroy_dumb data = {
        .handle = handle,
    };
    return drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &data);
}
OMX_U32 Rockchip_OSAL_SharedMemory_HandleToAddress(OMX_HANDLETYPE handle, OMX_HANDLETYPE handle_ptr)
{
    int map_fd = -1;
    native_handle_t* pnative_handle_t = NULL;
    RK_S32 err = 0;
    RK_S32 mClient = 0;
    RK_U32 mHandle = 0;
    struct drm_rockchip_gem_phys phys_arg;

    (void)handle;

    pnative_handle_t = (native_handle_t*)handle_ptr;
    map_fd = pnative_handle_t->data[0];
    //omx_err("Rockchip_OSAL_SharedMemory_HandleToAddress map_fd = %d", map_fd);

    mClient = open("/dev/dri/card0", O_RDWR);
    if (mClient < 0) {
        omx_err("Rockchip_OSAL_SharedMemory_HandleToAddress open drm fail");
        return 0;
    }
    drm_fd_to_handle(mClient, (int32_t)map_fd, &mHandle, 0);
    phys_arg.handle = mHandle;
    err = drm_ioctl(mClient, DRM_IOCTL_ROCKCHIP_GEM_GET_PHYS, &phys_arg);
    if (err)
        omx_err("failed to get phy address: %s\n", strerror(errno));

    // omx_err("Rockchip_OSAL_SharedMemory_HandleToAddress phys_arg.phy_addr = %0x",phys_arg.phy_addr);
    close(mClient);
    mClient = -1;
    return (OMX_U32)phys_arg.phy_addr;
}



OMX_U32 Rockchip_OSAL_SharedMemory_HandleToSecureAddress(OMX_HANDLETYPE handle, OMX_HANDLETYPE handle_ptr, RK_S32 size)
{
    int map_fd = -1;
    native_handle_t* pnative_handle_t = NULL;
    RK_S32 err = 0;
    RK_S32 mClient = 0;
    RK_U32 mHandle = 0;
    RK_U8* pBuffer = NULL;
    int ret;
    struct drm_mode_map_dumb dmmd;
    memset(&dmmd, 0, sizeof(dmmd));
    (void)handle;

    pnative_handle_t = (native_handle_t*)handle_ptr;
    map_fd = pnative_handle_t->data[0];
    //omx_err("Rockchip_OSAL_SharedMemory_HandleToSecureAddress map_fd = %d", map_fd);

    mClient = open("/dev/dri/card0", O_RDWR);
    if (mClient < 0) {
        omx_err("Rockchip_OSAL_SharedMemory_HandleToAddress open drm fail");
        return 0;
    }
    drm_fd_to_handle(mClient, (int32_t)map_fd, &mHandle, 0);
    //omx_err("Rockchip_OSAL_SharedMemory_HandleToSecureAddress mHandle = %d", mHandle);
    dmmd.handle = mHandle;


    ret = drm_ioctl(mClient, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret < 0) {
        close(mClient);
        mClient = -1;
        omx_err("drm_ioctl  DRM_IOCTL_MODE_MAP_DUMB failed: %s\n", strerror(errno));
        return ret;
    }

    pBuffer = (uint8_t*)drm_mmap(mClient, size, PROT_READ | PROT_WRITE, MAP_SHARED, dmmd.offset);
    if (pBuffer == MAP_FAILED) {
        close(mClient);
        mClient = -1;
        omx_err("mmap failed:fd = %d,length = %d, %s\n", mClient, size, strerror(errno));
        return -errno;
    }


    close(mClient);
    mClient = -1;
    //omx_err("Rockchip_OSAL_SharedMemory_HandleToSecureAddress pBuffer = %p", pBuffer);
    return (OMX_U32)pBuffer;
}

void    Rockchip_OSAL_SharedMemory_SecureUnmap(OMX_HANDLETYPE handle, OMX_HANDLETYPE handle_ptr, RK_S32 size)
{
    (void)handle;
    if (munmap(handle_ptr, size)) {
        omx_err("ion_unmap fail");
    }
}

OMX_HANDLETYPE Rockchip_OSAL_SharedMemory_Open()
{
    ROCKCHIP_SHARED_MEMORY *pHandle = NULL;
    int  IONClient = 0;

    pHandle = (ROCKCHIP_SHARED_MEMORY *)malloc(sizeof(ROCKCHIP_SHARED_MEMORY));
    Rockchip_OSAL_Memset(pHandle, 0, sizeof(ROCKCHIP_SHARED_MEMORY));
    if (pHandle == NULL)
        goto EXIT;
    if (!access("/dev/dri/card0", F_OK)) {
        IONClient = open("/dev/dri/card0", O_RDWR);
        mem_type = MEMORY_TYPE_DRM;
    } else {
        IONClient = open("/dev/ion", O_RDWR);
        mem_type = MEMORY_TYPE_ION;
    }

    if (IONClient <= 0) {
        omx_err("ion_client_create Error: %d", IONClient);
        Rockchip_OSAL_Free((void *)pHandle);
        pHandle = NULL;
        goto EXIT;
    }

    pHandle->fd = IONClient;

    Rockchip_OSAL_MutexCreate(&pHandle->hSMMutex);

EXIT:
    return (OMX_HANDLETYPE)pHandle;
}

void Rockchip_OSAL_SharedMemory_Close(OMX_HANDLETYPE handle, OMX_BOOL b_secure)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pDeleteElement = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pCurrentElement = pSMList = pHandle->pAllocMemory;

    while (pCurrentElement != NULL) {
        pDeleteElement = pCurrentElement;
        pCurrentElement = pCurrentElement->pNextMemory;
        if (b_secure) {
#ifdef AVS80
            native_handle_t* pnative_handle_t = NULL;
            int map_fd = 0;
            void *pTrueAddree = NULL;
            pnative_handle_t = (native_handle_t*)pDeleteElement->mapAddr;
            map_fd = pnative_handle_t->data[0];
            pTrueAddree = (void *)Rockchip_OSAL_SharedMemory_HandleToSecureAddress(handle, pDeleteElement->mapAddr, pDeleteElement->allocSize);
            pDeleteElement->mapAddr = pTrueAddree;
            close(map_fd);
            map_fd = -1;
#endif
        }
        if (munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize)) {
            omx_err("ion_unmap fail");
        }
        pDeleteElement->mapAddr = NULL;
        pDeleteElement->allocSize = 0;

        if (pDeleteElement->owner) {
            if (mem_type == MEMORY_TYPE_ION) {
                ion_free(pHandle->fd, pDeleteElement->ion_hdl);
            } else {
                drm_free(pHandle->fd, pDeleteElement->ion_hdl);
            }
        }
        pDeleteElement->ion_hdl = -1;

        Rockchip_OSAL_Free(pDeleteElement);

        mem_cnt--;
        omx_trace("SharedMemory free count: %d", mem_cnt);
    }

    pHandle->pAllocMemory = pSMList = NULL;
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

    Rockchip_OSAL_MutexTerminate(pHandle->hSMMutex);
    pHandle->hSMMutex = NULL;

    close(pHandle->fd);

    pHandle->fd = -1;

    Rockchip_OSAL_Free(pHandle);

EXIT:
    return;
}
static int ion_custom_op(int ion_client, int op, void *op_data)
{
    struct ion_custom_data data;
    int err;
    data.cmd = op;
    data.arg = (unsigned long)op_data;
    err = ioctl(ion_client, ION_IOC_CUSTOM, &data);
    if (err < 0) {
        omx_err("ION_IOC_CUSTOM (%d) failed with error - %s", op, strerror(errno));
        return err;
    }
    return err;
}

OMX_PTR Rockchip_OSAL_SharedMemory_Alloc(OMX_HANDLETYPE handle, OMX_U32 size, MEMORY_TYPE memoryType)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle         = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList         = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pElement        = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    OMX_U32                 ion_hdl          = 0;
    OMX_PTR                 pBuffer          = NULL;
    unsigned long           phys             = 0;
    unsigned int mask;
    unsigned int flag;
    int err = 0;
    int map_fd = -1;


    if (pHandle == NULL)
        goto EXIT;

    pElement = (ROCKCHIP_SHAREDMEM_LIST *)malloc(sizeof(ROCKCHIP_SHAREDMEM_LIST));
    Rockchip_OSAL_Memset(pElement, 0, sizeof(ROCKCHIP_SHAREDMEM_LIST));
    pElement->owner = OMX_TRUE;
    int secure_flag = 0;

    switch (memoryType) {
    case SECURE_MEMORY:
        mask = ION_HEAP(ION_SECURE_HEAP_ID);
        flag = 0;
        omx_info("pHandle->fd = %d,size = %d, mem_type = %d", pHandle->fd, size, mem_type);
        if (mem_type == MEMORY_TYPE_DRM) {
            err = drm_alloc(pHandle->fd, size, 4096, (RK_U32 *)&ion_hdl, 0);
        } else {
            err = ion_alloc(pHandle->fd, size, 4096, mask, 0, (ion_user_handle_t *)&ion_hdl);
        }
        if (err) {
            omx_err("ion_alloc failed with err (%d)", err);
            goto EXIT;
        }
        if (err) {
            omx_err("failed to get phy address: %s\n", strerror(errno));
            goto EXIT;
        }

        secure_flag = 1;
        pElement->allocSize = size;
        pElement->ion_hdl = ion_hdl;
        pElement->pNextMemory = NULL;

#ifdef AVS80
        native_handle_t* pnative_handle_t = NULL;
        pnative_handle_t = native_handle_create(1, 0);
        err = drm_handle_to_fd(pHandle->fd, ion_hdl, &map_fd, 0);
        if (err < 0) {
            omx_err("failed to trans handle to fd: %s\n", strerror(errno));
            goto EXIT;
        }
        omx_trace("pnative_handle_t = %p, map_fd = %d, handle = %d", pnative_handle_t, map_fd, ion_hdl);
        pnative_handle_t->data[0] = map_fd;
        pBuffer = (void *)pnative_handle_t;
        if (mem_type == MEMORY_TYPE_DRM) {
            pElement->mapAddr = (OMX_PTR)((__u64)pnative_handle_t);
        } else {
            pElement->mapAddr = (OMX_PTR)((__u64)pnative_handle_t);
        }
#endif
        break;
    case SYSTEM_MEMORY:
        mask =  ION_HEAP(ION_VMALLOC_HEAP_ID);;
        flag = 0;
        break;
    default:
        pBuffer = NULL;
        goto EXIT;
        break;
    }

    if (!secure_flag) {
        err = ion_alloc((int)pHandle->fd, size, 4096, mask, flag, (ion_user_handle_t *)&ion_hdl);

        if (err < 0) {
            omx_err("ion_alloc Error: %d", ion_hdl);
            Rockchip_OSAL_Free((OMX_PTR)pElement);
            goto EXIT;
        }

        err = ion_map((int)pHandle->fd, ion_hdl, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, (off_t)0, (unsigned char**)&pBuffer, &map_fd);

        if (err) {
            omx_err("ion_map Error");
            ion_free(pHandle->fd, ion_hdl);
            Rockchip_OSAL_Free((OMX_PTR)pElement);
            pBuffer = NULL;
            goto EXIT;
        }

        pElement->ion_hdl = ion_hdl;
        pElement->mapAddr = pBuffer;
        pElement->allocSize = size;
        pElement->pNextMemory = NULL;
    }

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        pHandle->pAllocMemory = pSMList = pElement;
    } else {
        pCurrentElement = pSMList;
        while (pCurrentElement->pNextMemory != NULL) {
            pCurrentElement = pCurrentElement->pNextMemory;
        }
        pCurrentElement->pNextMemory = pElement;
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

    mem_cnt++;
    omx_trace("SharedMemory alloc count: %d", mem_cnt);

EXIT:
    return pBuffer;
}

void Rockchip_OSAL_SharedMemory_Free(OMX_HANDLETYPE handle, OMX_PTR pBuffer)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle         = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList         = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pDeleteElement  = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->mapAddr == pBuffer) {
        pDeleteElement = pSMList;
        pHandle->pAllocMemory = pSMList = pSMList->pNextMemory;
    } else {
        while ((pCurrentElement != NULL) && (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr != pBuffer))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr == pBuffer)) {
            pDeleteElement = pCurrentElement->pNextMemory;
            pCurrentElement->pNextMemory = pDeleteElement->pNextMemory;
        } else {
            Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
            omx_err("Can not find SharedMemory");
            goto EXIT;
        }
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
    if (munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize)) {
        omx_err("ion_unmap fail");
        goto EXIT;
    }
    pDeleteElement->mapAddr = NULL;
    pDeleteElement->allocSize = 0;

    if (pDeleteElement->owner) {
        if (mem_cnt == MEMORY_TYPE_ION) {
            ion_free(pHandle->fd, (ion_user_handle_t)pDeleteElement->ion_hdl);
        } else {
            drm_free(pHandle->fd, pDeleteElement->ion_hdl);
        }
    }
    pDeleteElement->ion_hdl = -1;

    Rockchip_OSAL_Free(pDeleteElement);

    mem_cnt--;
    omx_trace("SharedMemory free count: %d", mem_cnt);

EXIT:
    return;
}

OMX_PTR Rockchip_OSAL_SharedMemory_Map(OMX_HANDLETYPE handle, OMX_U32 size, int ion_hdl)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    OMX_PTR pBuffer = NULL;
    int err = 0;
    int map_fd = -1;

    if (pHandle == NULL)
        goto EXIT;

    pElement = Rockchip_OSAL_Malloc(sizeof(ROCKCHIP_SHAREDMEM_LIST));
    Rockchip_OSAL_Memset(pElement, 0, sizeof(ROCKCHIP_SHAREDMEM_LIST));

    if (ion_hdl == -1) {
        omx_err("ion_alloc Error: %d", ion_hdl);
        Rockchip_OSAL_Free((void*)pElement);
        goto EXIT;
    }
    if (mem_type == MEMORY_TYPE_ION) {
        err = ion_map(pHandle->fd, (ion_user_handle_t)ion_hdl, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, (off_t)0, (unsigned char**)&pBuffer, &map_fd);
        if (pBuffer == NULL) {
            omx_err("ion_map Error");
            ion_free(pHandle->fd, (ion_user_handle_t)ion_hdl);
            Rockchip_OSAL_Free((void*)pElement);
            goto EXIT;
        }

    } else {
        err = drm_map(pHandle->fd, ion_hdl, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, (unsigned char**)&pBuffer, &map_fd);
        if (pBuffer == NULL) {
            omx_err("ion_map Error");
            drm_free(pHandle->fd, ion_hdl);
            Rockchip_OSAL_Free((void*)pElement);
            goto EXIT;
        }

    }


    pElement->ion_hdl = ion_hdl;
    pElement->mapAddr = pBuffer;
    pElement->allocSize = size;
    pElement->pNextMemory = NULL;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        pHandle->pAllocMemory = pSMList = pElement;
    } else {
        pCurrentElement = pSMList;
        while (pCurrentElement->pNextMemory != NULL) {
            pCurrentElement = pCurrentElement->pNextMemory;
        }
        pCurrentElement->pNextMemory = pElement;
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

    mem_cnt++;
    omx_trace("SharedMemory alloc count: %d", mem_cnt);

EXIT:
    return pBuffer;
}

void Rockchip_OSAL_SharedMemory_Unmap(OMX_HANDLETYPE handle, int ionfd)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pDeleteElement = NULL;

    if (pHandle == NULL)
        goto EXIT;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->ion_hdl == (RK_U32)ionfd) {
        pDeleteElement = pSMList;
        pHandle->pAllocMemory = pSMList = pSMList->pNextMemory;
    } else {
        while ((pCurrentElement != NULL) && (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->ion_hdl != (RK_U32)ionfd))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->ion_hdl == (RK_U32)ionfd)) {
            pDeleteElement = pCurrentElement->pNextMemory;
            pCurrentElement->pNextMemory = pDeleteElement->pNextMemory;
        } else {
            Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
            omx_err("Can not find SharedMemory");
            goto EXIT;
        }
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

    if (munmap(pDeleteElement->mapAddr, pDeleteElement->allocSize)) {
        omx_err("ion_unmap fail");
        goto EXIT;
    }
    pDeleteElement->mapAddr = NULL;
    pDeleteElement->allocSize = 0;
    pDeleteElement->ion_hdl = -1;

    Rockchip_OSAL_Free(pDeleteElement);

    mem_cnt--;
    omx_trace("SharedMemory free count: %d", mem_cnt);

EXIT:
    return;
}

int Rockchip_OSAL_SharedMemory_VirtToION(OMX_HANDLETYPE handle, OMX_PTR pBuffer)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle         = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList         = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pFindElement    = NULL;
    if (pHandle == NULL || pBuffer == NULL)
        goto EXIT;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->mapAddr == pBuffer) {
        pFindElement = pSMList;
    } else {
        while ((pCurrentElement != NULL) && (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr != pBuffer))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->mapAddr == pBuffer)) {
            pFindElement = pCurrentElement->pNextMemory;
        } else {
            Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
            omx_warn("Can not find SharedMemory");
            goto EXIT;
        }
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

EXIT:
    return pFindElement->ion_hdl;
}

OMX_PTR Rockchip_OSAL_SharedMemory_IONToVirt(OMX_HANDLETYPE handle, int ion_fd)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle         = (ROCKCHIP_SHARED_MEMORY *)handle;
    ROCKCHIP_SHAREDMEM_LIST *pSMList         = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pCurrentElement = NULL;
    ROCKCHIP_SHAREDMEM_LIST *pFindElement    = NULL;
    OMX_PTR pBuffer = NULL;
    if (pHandle == NULL || ion_fd == -1)
        goto EXIT;

    Rockchip_OSAL_MutexLock(pHandle->hSMMutex);
    pSMList = pHandle->pAllocMemory;
    if (pSMList == NULL) {
        Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
        goto EXIT;
    }

    pCurrentElement = pSMList;
    if (pSMList->ion_hdl == (RK_U32)ion_fd) {
        pFindElement = pSMList;
    } else {
        while ((pCurrentElement != NULL) && (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
               (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->ion_hdl != (RK_U32)ion_fd))
            pCurrentElement = pCurrentElement->pNextMemory;

        if ((((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory)) != NULL) &&
            (((ROCKCHIP_SHAREDMEM_LIST *)(pCurrentElement->pNextMemory))->ion_hdl == (RK_U32)ion_fd)) {
            pFindElement = pCurrentElement->pNextMemory;
        } else {
            Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);
            omx_warn("Can not find SharedMemory");
            goto EXIT;
        }
    }
    Rockchip_OSAL_MutexUnlock(pHandle->hSMMutex);

    pBuffer = pFindElement->mapAddr;

EXIT:
    return pBuffer;
}
static OMX_S32 check_used_heaps_type()
{
    // TODO, use property_get
    if (!VPUClientGetIOMMUStatus()) {
        return ION_HEAP(ION_CMA_HEAP_ID);
    } else {
        omx_trace("USE ION_SYSTEM_HEAP");
        return ION_HEAP(ION_VMALLOC_HEAP_ID);
    }

    return 0;
}
OMX_S32 Rockchip_OSAL_SharedMemory_getPhyAddress(OMX_HANDLETYPE handle, int share_fd, OMX_U32 *phyaddress)
{
    ROCKCHIP_SHARED_MEMORY  *pHandle = (ROCKCHIP_SHARED_MEMORY *)handle;
    int err = 0;
    ion_user_handle_t  ion_handle = 0;
    struct ion_phys_data phys_data;
    if (check_used_heaps_type() == ION_HEAP(ION_CMA_HEAP_ID)) {
        err = ion_import(pHandle->fd, share_fd, &ion_handle);
        if (err) {
            omx_err("ion import failed, share fd %d\n", share_fd);
            return err;
        }
        phys_data.handle = ion_handle;
        err = ion_custom_op(pHandle->fd, ION_IOC_GET_PHYS, &phys_data);
        *phyaddress = phys_data.phys;
        ion_free(pHandle->fd, ion_handle);
    } else {
        *phyaddress = share_fd;
    }
    return 0;
}



