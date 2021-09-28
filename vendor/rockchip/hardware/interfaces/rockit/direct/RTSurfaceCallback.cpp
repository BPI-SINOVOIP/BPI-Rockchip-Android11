/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "RTSurfaceCallback"

#include <gralloc_priv_omx.h>
#include <string.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <system/window.h>
#include <gui/Surface.h>
#include "RTSurfaceCallback.h"
#include "RTChips.h"
#include "RockitPlayer.h"
#include "sideband/RTSidebandWindow.h"
extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drm_fourcc.h"
}

using namespace ::android;

static const char *DRM_DEV_PATH = "/dev/dri/card0";

INT32 drm_open() {
    INT32 fd = open(DRM_DEV_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("open %s failed!\n", DRM_DEV_PATH);
    }

    return fd;
}

INT32 drm_close(INT32 fd) {
    INT32 ret = close(fd);
    if (ret < 0) {
        return -errno;
    }

    return ret;
}

inline void *drm_mmap(void *addr, UINT32 length, INT32 prot,
                      INT32 flags, INT32 fd, loff_t offset) {
    /* offset must be aligned to 4096 (not necessarily the page size) */
    if (offset & 4095) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    return mmap64(addr, length, prot, flags, fd, offset);
}


INT32 drm_ioctl(INT32 fd, INT32 req, void* arg) {
    INT32 ret = ioctl(fd, req, arg);
    if (ret < 0) {
        ALOGE("fd: %d ioctl %x failed with code %d: %s\n", fd, req, ret, strerror(errno));
        return -errno;
    }
    return ret;
}

INT32 drm_free(INT32 fd, UINT32 handle) {
    struct drm_mode_destroy_dumb data = {
        .handle = handle,
    };
    return drm_ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &data);
}

INT32 drm_fd_to_handle(
        INT32 fd,
        INT32 map_fd,
        UINT32 *handle,
        UINT32 flags) {
    INT32 ret;
    struct drm_prime_handle dph;

    dph.fd = map_fd;
    dph.flags = flags;

    ret = drm_ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &dph);
    if (ret < 0) {
        ALOGE("DRM_IOCTL_PRIME_FD_TO_HANDLE failed!");
        return ret;
    }

    *handle = dph.handle;

    return ret;
}

INT32 drm_handle_to_fd(INT32 fd, UINT32 handle, INT32 *map_fd, UINT32 flags) {
    INT32 ret;
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

INT32 drm_map(INT32 fd, INT32 share_fd, UINT32 length, INT32 prot,INT32 flags, INT32 offset, void **ptr, UINT32 heaps) {
    INT32 ret;
    static UINT32 pagesize_mask = 0;
    struct drm_mode_map_dumb dmmd;
    UINT32 handle;
    (void)offset;
    (void)heaps;

    if (fd <= 0)
        return -EINVAL;
    if (ptr == NULL)
        return -EINVAL;

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;

    length = (length + pagesize_mask) & ~pagesize_mask;

    drm_fd_to_handle(fd, share_fd, &handle, 0);
    memset(&dmmd, 0, sizeof(dmmd));
    dmmd.handle = (UINT32)handle;

    ret = drm_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &dmmd);
    if (ret) {
        ALOGE("map_dumb failed: %s\n", strerror(ret));
        return ret;
    }
    *ptr = drm_mmap(NULL, length, prot, flags, fd, dmmd.offset);

    if (*ptr == MAP_FAILED) {
        ALOGD("fail to drm_mmap(fd = %d), error: %s", fd, strerror(errno));
        ret = -errno;
    }

    return ret;
}

INT32 ion_map(INT32 fd, UINT32 length, INT32 prot, INT32 flags, off_t offset, void **ptr) {
    static UINT32 pagesize_mask = 0;
    if (ptr == NULL)
        return -EINVAL;

    if (!pagesize_mask)
        pagesize_mask = sysconf(_SC_PAGESIZE) - 1;
    offset = offset & (~pagesize_mask);

    *ptr = mmap(NULL, length, prot, flags, fd, offset);
    if (*ptr == MAP_FAILED) {
        ALOGE("fail to mmap(fd = %d), error:%s", fd, strerror(errno));
        *ptr = NULL;
        return -errno;
    }
    return 0;
}

RTSurfaceCallback::RTSurfaceCallback(const sp<IGraphicBufferProducer> &bufferProducer)
    : mDrmFd(-1),
      mTunnel(0),
      mSidebandHandle(NULL),
      mSidebandWindow(NULL) {
    if (mDrmFd < 0) {
        mDrmFd = drm_open();
    }
    mNativeWindow = new Surface(bufferProducer, true);
}

RTSurfaceCallback::~RTSurfaceCallback() {
    ALOGD("~RTSurfaceCallback(%p) construct", this);
    if (mSidebandHandle) {
        mSidebandWindow->freeBuffer(&mSidebandHandle);
    }
    if (mSidebandWindow.get()) {
        mSidebandWindow->release();
        mSidebandWindow.clear();
    }
    if (mDrmFd >= 0) {
        drm_close(mDrmFd);
        mDrmFd = -1;
    }

    if (mNativeWindow.get() != NULL) {
        mNativeWindow.clear();
    }
}

INT32 RTSurfaceCallback::setNativeWindow(const sp<IGraphicBufferProducer> &bufferProducer) {
    if (bufferProducer.get() == NULL)
        return 0;

    if(getNativeWindow() == NULL) {
        mNativeWindow = new Surface(bufferProducer, true);
    } else {
        ALOGD("already set native window");
    }
    return 0;
}

INT32 RTSurfaceCallback::connect(INT32 mode) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    (void)mode;
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_api_connect(mNativeWindow.get(), NATIVE_WINDOW_API_MEDIA);
}

INT32 RTSurfaceCallback::disconnect(INT32 mode) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    (void)mode;
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_api_disconnect(mNativeWindow.get(), NATIVE_WINDOW_API_MEDIA);;
}

INT32 RTSurfaceCallback::allocateBuffer(RTNativeWindowBufferInfo *info) {
    INT32                       ret = 0;
    buffer_handle_t             bufferHandle = NULL;
    gralloc_private_handle_t    priv_hnd;
    ANativeWindowBuffer        *buf = NULL;

    memset(info, 0, sizeof(RTNativeWindowBufferInfo));
    if (mTunnel) {
        mSidebandWindow->allocateBuffer((buffer_handle_t *)&bufferHandle);
    } else {
        if (getNativeWindow() == NULL)
            return -1;
        ret = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf);
        if (buf) {
            bufferHandle = buf->handle;
        }

    }
    if (bufferHandle) {
        Rockchip_get_gralloc_private((UINT32 *)bufferHandle, &priv_hnd);
        UINT32 handle = 0;
        struct drm_gem_flink req;

        if (mDrmFd >= 0) {
            drm_fd_to_handle(mDrmFd, priv_hnd.share_fd, &handle, 0);
            /* Flink creates a name for the object and returns it to the
             * application. This name can be used by other applications to gain
             * access to the same object. */
            req.handle = handle,
            drm_ioctl(mDrmFd, DRM_IOCTL_GEM_FLINK, &req);
            //drm_free(mDrmFd, handle);
        }

        info->graphicBuffer = NULL;
        if (mTunnel) {
            info->windowBuf = (void *)bufferHandle;
        } else {
            info->windowBuf = (void *)buf;
        }
        info->name = req.name;
        info->size = priv_hnd.size;
        info->dupFd = priv_hnd.share_fd;
    }

    return 0;
}

INT32 RTSurfaceCallback::freeBuffer(void *buf, INT32 fence) {
    ALOGV("%s %d buf=%p in", __FUNCTION__, __LINE__, buf);
    INT32 ret = 0;
    if (mTunnel) {
        ret = mSidebandWindow->freeBuffer((buffer_handle_t *)&buf);
    } else {
        if (getNativeWindow() == NULL)
            return -1;

        ret = mNativeWindow->cancelBuffer(mNativeWindow.get(), (ANativeWindowBuffer *)buf, fence);
    }

    return ret;
}

INT32 RTSurfaceCallback::remainBuffer(void *buf, INT32 fence) {
    ALOGV("%s %d buf=%p in", __FUNCTION__, __LINE__, buf);
    INT32 ret = 0;
    if (mTunnel) {
        ret = mSidebandWindow->remainBuffer((buffer_handle_t)buf);
    } else {
        if (getNativeWindow() == NULL)
            return -1;

        ret = mNativeWindow->cancelBuffer(mNativeWindow.get(), (ANativeWindowBuffer *)buf, fence);
    }

    return ret;
}

INT32 RTSurfaceCallback::queueBuffer(void *buf, INT32 fence) {
    ALOGV("%s %d buf=%p in", __FUNCTION__, __LINE__, buf);
    INT32 ret = 0;
    if (mTunnel) {
        ret = mSidebandWindow->queueBuffer((buffer_handle_t)buf);
    } else {
        if (getNativeWindow() == NULL)
            return -1;

        ret = mNativeWindow->queueBuffer(mNativeWindow.get(), (ANativeWindowBuffer *)buf, fence);
    }
    return ret;
}

INT32 RTSurfaceCallback::dequeueBuffer(void **buf) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    (void)buf;
    return 0;

}

INT32 RTSurfaceCallback::dequeueBufferAndWait(RTNativeWindowBufferInfo *info) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    INT32                       ret = 0;
    buffer_handle_t             bufferHandle = NULL;
    gralloc_private_handle_t    priv_hnd;
    ANativeWindowBuffer *buf = NULL;

    memset(info, 0, sizeof(RTNativeWindowBufferInfo));
    if (mTunnel) {
        mSidebandWindow->dequeueBuffer((buffer_handle_t *)&bufferHandle);
    } else {
        if (getNativeWindow() == NULL)
            return -1;

        ret = native_window_dequeue_buffer_and_wait(mNativeWindow.get(), &buf);
        if (buf) {
            bufferHandle = buf->handle;
        }
    }

    if (bufferHandle) {
        Rockchip_get_gralloc_private((UINT32 *)bufferHandle, &priv_hnd);
        UINT32 handle = 0;
        struct drm_gem_flink req;

        if (mDrmFd >= 0) {
            drm_fd_to_handle(mDrmFd, priv_hnd.share_fd, &handle, 0);
            /* Flink creates a name for the object and returns it to the
             * application. This name can be used by other applications to gain
             * access to the same object. */
            req.handle = handle,
            drm_ioctl(mDrmFd, DRM_IOCTL_GEM_FLINK, &req);
            //drm_free(mDrmFd, handle);
        }

        info->graphicBuffer = NULL;
        if (mTunnel) {
            info->windowBuf = (void *)bufferHandle;
        } else {
            info->windowBuf = (void *)buf;
        }
        info->name = req.name;
        info->dupFd = priv_hnd.share_fd;
    }
    return ret;
}

INT32 RTSurfaceCallback::mmapBuffer(RTNativeWindowBufferInfo *info, void **ptr) {
    status_t err = OK;
    ANativeWindowBuffer *buf = NULL;
    void *tmpPtr = NULL;
    (void)ptr;

    if (info->windowBuf == NULL || ptr == NULL) {
        ALOGE("lockBuffer bad value, windowBuf=%p, &ptr=%p", info->windowBuf, ptr);
        return RT_ERR_VALUE;
    }

    if (mTunnel)
        return RT_ERR_UNSUPPORT;

    buf = static_cast<ANativeWindowBuffer *>(info->windowBuf);

    sp<GraphicBuffer> graphicBuffer(GraphicBuffer::from(buf));
    err = graphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, &tmpPtr);
    if (err != OK) {
        ALOGE("graphicBuffer lock failed err - %d", err);
        return RT_ERR_BAD;
    }

    *ptr = tmpPtr;

    return RT_OK;
}

INT32 RTSurfaceCallback::munmapBuffer(void **ptr, INT32 size, void *buf) {
    status_t err = OK;
    (void)ptr;
    (void)size;

    if (buf == NULL) {
        ALOGE("unlockBuffer null input");
        return RT_ERR_VALUE;
    }

    if (mTunnel)
        return RT_ERR_UNSUPPORT;

    sp<GraphicBuffer> graphicBuffer(
            GraphicBuffer::from(static_cast<ANativeWindowBuffer *>(buf)));
    err = graphicBuffer->unlock();
    if (err != OK) {
        ALOGE("graphicBuffer unlock failed err - %d", err);
        return RT_ERR_BAD;
    }

    return RT_OK;
}

INT32 RTSurfaceCallback::setCrop(
        INT32 left,
        INT32 top,
        INT32 right,
        INT32 bottom) {
        ALOGV("%s %d in crop(%d,%d,%d,%d)", __FUNCTION__, __LINE__, left, top, right, bottom);
        android_native_rect_t crop;

        crop.left = left;
        crop.top = top;
        crop.right = right;
        crop.bottom = bottom;
        if (mTunnel) {
            mSidebandWindow->setCrop(left, top, right, bottom);
        }

        if (getNativeWindow() == NULL)
            return -1;

        return native_window_set_crop(mNativeWindow.get(), &crop);
}

INT32 RTSurfaceCallback::setUsage(INT32 usage) {
    ALOGV("%s %d in usage=0x%x", __FUNCTION__, __LINE__, usage);
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_usage(mNativeWindow.get(), usage);;
}

INT32 RTSurfaceCallback::setScalingMode(INT32 mode) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_scaling_mode(mNativeWindow.get(), mode);;
}

INT32 RTSurfaceCallback::setDataSpace(INT32 dataSpace) {
    ALOGV("%s %d in dataSpace=0x%x", __FUNCTION__, __LINE__, dataSpace);
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_buffers_data_space(mNativeWindow.get(), (android_dataspace_t)dataSpace);
}

INT32 RTSurfaceCallback::setTransform(INT32 transform) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_buffers_transform(mNativeWindow.get(), transform);
}

INT32 RTSurfaceCallback::setSwapInterval(INT32 interval) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    (void)interval;
    return 0;
}

INT32 RTSurfaceCallback::setBufferCount(INT32 bufferCount) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_buffer_count(mNativeWindow.get(), bufferCount);
}

INT32 RTSurfaceCallback::setBufferGeometry(
        INT32 width,
        INT32 height,
        INT32 format) {
    ALOGV("%s %d in width=%d, height=%d, format=0x%x", __FUNCTION__, __LINE__, width, height, format);
    if (getNativeWindow() == NULL)
        return -1;

    native_window_set_buffers_dimensions(mNativeWindow.get(), width, height);
    native_window_set_buffers_format(mNativeWindow.get(), format);
    if (mTunnel) {
        mSidebandWindow->setBufferGeometry(width, height, format);
    }

    return 0;
}

INT32 RTSurfaceCallback::setSidebandStream(RTSidebandInfo info) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);

    buffer_handle_t             buffer = NULL;

    mSidebandWindow = new RTSidebandWindow();
    mSidebandWindow->init(info);
    mSidebandWindow->allocateSidebandHandle(&buffer);
    if (!buffer) {
        ALOGE("allocate buffer from sideband window failed!");
        return -1;
    }
    mSidebandHandle = buffer;
    mTunnel = 1;

    if (getNativeWindow() == NULL)
        return -1;

    return native_window_set_sideband_stream(mNativeWindow.get(), (native_handle_t *)buffer);
}

INT32 RTSurfaceCallback::query(INT32 cmd, INT32 *param) {
    ALOGV("%s %d in", __FUNCTION__, __LINE__);
    if (getNativeWindow() == NULL)
        return -1;

    return mNativeWindow->query(mNativeWindow.get(), cmd, param);

}

void* RTSurfaceCallback::getNativeWindow() {
    return (void *)mNativeWindow.get();
}

