/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 *
 * author: rimon.xu@rock-chips.com
 *   date: 2019/01/16
 * module: drm native api
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <utils/Log.h>
#include <dlfcn.h>
#include <errno.h>

#include "include/rt_drm.h"
#include "include/drm.h"
#include "include/drm_mode.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rt_drm"
#ifdef DEBUG_FLAG
#undef DEBUG_FLAG
#endif
#define DEBUG_FLAG 0x0

static const char *DRM_DEV_NAME = "/dev/dri/card0";

int drm_open() {
    int fd = open(DRM_DEV_NAME, O_RDWR);
    if (fd < 0) {
        ALOGE("fail to open drm device(%s)", DRM_DEV_NAME);
    } else {
        ALOGD("success to open drm device(%s)", DRM_DEV_NAME);
    }

    return fd;
}

int drm_close(int fd) {
    int ret = close(fd);
    if (ret < 0) {
        return -errno;
    }

    return ret;
}

int drm_get_phys(int fd, uint32_t handle, uint32_t *phy, uint32_t heaps) {
    /* no physical address */
    if (heaps != ROCKCHIP_BO_SECURE || heaps != ROCKCHIP_BO_CONTIG) {
        *phy = 0;
        return 0;
    }

    struct drm_rockchip_gem_phys phys_arg;
    phys_arg.handle = handle;
    int ret = drm_ioctl(fd, DRM_IOCTL_ROCKCHIP_GEM_GET_PHYS, &phys_arg);
    if (ret < 0) {
        ALOGE("fail to get phys(fd = %d), error: %s", fd, strerror(errno));
        return ret;
    } else {
        *phy = phys_arg.phy_addr;
    }
    return ret;
}

int drm_ioctl(int fd, int req, void* arg) {
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
        ALOGE("fail to drm_ioctl(fd = %d, req =%d), error: %s", fd, req, strerror(errno));
        return -errno;
    }
    return ret;
}

int drm_handle_to_fd(int fd, uint32_t handle, int *map_fd, uint32_t flags) {
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
        ALOGE("fail to handle_to_fd(fd=%d)", fd);
        return -EINVAL;
    }

    return ret;
}

int drm_fd_to_handle(
        int fd,
        int map_fd,
        uint32_t *handle,
        uint32_t flags) {
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

int drm_get_info_from_name(
        int   fd,
        uint32_t  name,
        uint32_t *handle,
        int  *size) {
    int  ret = 0;
    struct drm_gem_open req;

    req.name = name;
    ret = drm_ioctl(fd, DRM_IOCTL_GEM_OPEN, &req);
    if (ret < 0) {
        return ret;
    }

    *handle = req.handle;
    *size   = (int)req.size;

    return ret;
}

int drm_alloc(
        int fd,
        uint32_t len,
        uint32_t align,
        uint32_t *handle,
        uint32_t flags,
        uint32_t heaps) {
    (void)flags;
    int ret;
    struct drm_mode_create_dumb dmcb;

    // RT_LOGD("allocate buffer(size=%d, align=%d, flags=%x) from drm device(fd=%d)", len, align, flags, fd);

    memset(&dmcb, 0, sizeof(struct drm_mode_create_dumb));
    dmcb.bpp = 8;
    dmcb.width = (len + align - 1) & (~(align - 1));
    dmcb.height = 1;
    dmcb.flags = heaps;

    if (handle == NULL) {
        ALOGE("illegal parameter, handler is null.");
        return -EINVAL;
    }

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dmcb);
    if (ret < 0) {
        return ret;
    }

    *handle = dmcb.handle;
    return ret;
}

int drm_free(int fd, uint32_t handle) {
    struct drm_mode_destroy_dumb data = {
        .handle = handle,
    };
    return drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &data);
}


inline void *drm_mmap(void *addr, uint32_t length, int prot, int flags,
                             int fd, loff_t offset) {
    /* offset must be aligned to 4096 (not necessarily the page size) */
    if (offset & 4095) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    return mmap64(addr, length, prot, flags, fd, offset);
}

int drm_munmap(void *addr,int length) {
    return munmap(addr, length);
}

int drm_map(int fd, uint32_t handle, uint32_t length, int prot,
                   int flags, int offset, void **ptr, int *map_fd, uint32_t heaps) {
    int ret;
    struct drm_mode_map_dumb dmmd;
    static uint32_t pagesize_mask = 0;
    memset(&dmmd, 0, sizeof(dmmd));
    dmmd.handle = handle;

    (void)offset;
    (void)flags;

    // RT_LOGT("mmap ion buffer(fd=%d, offset=%d, size=%d, flags=%x)", fd, offset, length, flags);

    if (map_fd == NULL)
        return -EINVAL;
    if (ptr == NULL)
        return -EINVAL;

    ret = drm_handle_to_fd(fd, handle, map_fd, 0);
    if (ret < 0)
        return ret;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret < 0) {
        close(*map_fd);
        return ret;
    }

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;

    length = (length + pagesize_mask) & ~pagesize_mask;

    *ptr = drm_mmap(NULL, length, prot, MAP_SHARED, fd, dmmd.offset);
    if (*ptr == MAP_FAILED) {
        close(*map_fd);
        *map_fd = -1;
        if (heaps == ROCKCHIP_BO_SECURE) {
            ALOGD("fail to drm_mmap(fd = %d), without physical address", fd);
            ret = 0;
        } else {
            ALOGD("fail to drm_mmap(fd = %d), error: %s", fd, strerror(errno));
            ret = -errno;
        }
    }

    return ret;
}
