/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <utils/Log.h>
#include "common/camera_buffer_manager_gralloc_impl.h"

#include <linux/videodev2.h>

#include "arc/common.h"

namespace arc {

namespace {

struct dma_buf_sync {
    __u64 flags;
};

#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)
#define DMA_BUF_SYNC_VALID_FLAGS_MASK \
    (DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END)
#define DMA_BUF_BASE            'b'
#define DMA_BUF_IOCTL_SYNC      _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

}

gralloc_module_t* CameraBufferManagerImpl::gm_module_ = nullptr;
struct alloc_device_t* CameraBufferManagerImpl::alloc_device_ = nullptr;
// static
CameraBufferManager* CameraBufferManager::GetInstance() {
    static CameraBufferManagerImpl instance;

    if (!instance.alloc_device_) {
        gralloc_module_t** gm_module_ =
            &CameraBufferManagerImpl::gm_module_;
        struct alloc_device_t** alloc_device_ =
            &CameraBufferManagerImpl::alloc_device_;
        int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                                (const hw_module_t **)gm_module_);
        if (ret < 0) {
            LOGF(ERROR) << "Unable to get gralloc module(error"  << ret << ")";
            return nullptr;
        }

        ret = gralloc_open((const struct hw_module_t*)*gm_module_, alloc_device_);
        if (ret < 0) {
            LOGF(ERROR) << " Unable to open gralloc alloc device (error " << ret << ")";
            alloc_device_ = nullptr;
            return nullptr;
        }
    }
    return &instance;
}

//static
int CameraBufferManagerImpl::GetHalPixelFormat(buffer_handle_t buffer) {
    int hal_pixel_format;
    gralloc_module_t* gm_module_ =
        CameraBufferManagerImpl::gm_module_;

    if (gm_module_ && gm_module_->perform) {
        int ret = gm_module_->perform(gm_module_,
                                      GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT,
                                      buffer, &hal_pixel_format);
        if (ret < 0) {
            LOGF(ERROR) << "get format error " << ret;
            return -EINVAL;
        }
    } else {
        LOGF(ERROR) << "can't get format";
        return -EINVAL;
    }

    return hal_pixel_format;
}

// static
uint32_t CameraBufferManager::GetNumPlanes(buffer_handle_t buffer) {
    int hal_pixel_format = CameraBufferManagerImpl::GetHalPixelFormat(buffer);
    /* only support one physical plane now */
    switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: // NV21
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED: //NV12
    default :
        return 1;
    }

    LOGF(ERROR) << "Unknown format: " << hal_pixel_format << " - " << FormatToString(hal_pixel_format);
    return 0;
}

// static
uint32_t CameraBufferManager::GetV4L2PixelFormat(buffer_handle_t buffer) {
    int hal_pixel_format = CameraBufferManagerImpl::GetHalPixelFormat(buffer);

    switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return V4L2_PIX_FMT_ABGR32;

    // There is no standard V4L2 pixel format corresponding to
    // DRM_FORMAT_xBGR8888.  We use our own V4L2 format extension
    // V4L2_PIX_FMT_RGBX32 here.
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return V4L2_PIX_FMT_RGBX32;

    case HAL_PIXEL_FORMAT_BLOB:
        return V4L2_PIX_FMT_JPEG;
        // Semi-planar formats.
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
        return V4L2_PIX_FMT_NV12;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        return V4L2_PIX_FMT_NV21;

    default:
      return V4L2_PIX_FMT_NV12;
    }
    LOGF(ERROR) << "Could not convert format "
        << FormatToString(hal_pixel_format) << " to V4L2 pixel format";
    return 0;
}

// static
size_t CameraBufferManager::GetPlaneStride(buffer_handle_t buffer,
                                           size_t plane) {
    if (plane >= GetNumPlanes(buffer)) {
        LOGF(ERROR) << "Invalid plane: " << plane;
        return 0;
    }

    gralloc_module_t* gm_module_ =
        CameraBufferManagerImpl::gm_module_;
    int plane_stride = 0;
    if (gm_module_ && gm_module_->perform) {
        int ret = gm_module_->perform(gm_module_,
                                      GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE,
                                      buffer, &plane_stride);

        if (ret < 0) {
            LOGF(ERROR) << "get stride error " << ret;
            return 0;
        }
    }

    return plane_stride;
}

// static
size_t CameraBufferManager::GetPlaneSize(buffer_handle_t buffer, size_t plane) {
    if (plane >= GetNumPlanes(buffer)) {
        LOGF(ERROR) << "Invalid plane: " << plane;
        return 0;
    }

    // only support one physical plane, so plane size is
    // the same as frame size
    unsigned int size = 0;
    gralloc_module_t* gm_module_ =
        CameraBufferManagerImpl::gm_module_;
    if (gm_module_ && gm_module_->perform) {
        //GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE gets the whole buffer size
        //while it should get the plane size here, but dut to we now support one
        //plane only, so it's ok to use buffer size to replace plane size
        gm_module_->perform(gm_module_,
                            GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE,
                            buffer,
                            &size);
    }
    return size;
}

CameraBufferManagerImpl::CameraBufferManagerImpl()
{
}

CameraBufferManagerImpl::~CameraBufferManagerImpl() {
}

int CameraBufferManagerImpl::Allocate(size_t width,
                                      size_t height,
                                      uint32_t format,
                                      uint32_t usage,
                                      BufferType type,
                                      buffer_handle_t* out_buffer,
                                      uint32_t* out_stride) {
    if (type == GRALLOC) {
        return AllocateGrallocBuffer(width, height, format, usage, out_buffer,
                                     out_stride);
    } else {
        NOTREACHED() << "Invalid buffer type: " << type;
        return -EINVAL;
    }
}

int CameraBufferManagerImpl::Free(buffer_handle_t buffer) {
    base::AutoLock l(lock_);

    auto context_it = buffer_context_.find(buffer);
    if (context_it == buffer_context_.end()) {
        LOGF(ERROR) << "Unknown buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }

    auto buffer_context = context_it->second.get();

    if (buffer_context->type == GRALLOC) {
        return alloc_device_->free(alloc_device_, buffer);
    } else {
        // TODO(jcliang): Implement deletion of SharedMemory.
        return -EINVAL;
    }
}

int CameraBufferManagerImpl::Register(buffer_handle_t buffer) {
    base::AutoLock l(lock_);
    auto context_it = buffer_context_.find(buffer);
    if (context_it != buffer_context_.end()) {
        context_it->second->usage++;
        return 0;
    }

    std::unique_ptr<BufferContext> buffer_context(new struct BufferContext);

    buffer_context->type = GRALLOC;
    int ret = gm_module_->registerBuffer(gm_module_, buffer);

    if (ret) {
        LOGF(ERROR) << "Failed to register gralloc buffer";
        return ret;
    }

    buffer_context->usage = 1;
    buffer_context_[buffer] = std::move(buffer_context);

    return 0;
}

int CameraBufferManagerImpl::Deregister(buffer_handle_t buffer) {
    base::AutoLock l(lock_);

    auto context_it = buffer_context_.find(buffer);
    if (context_it == buffer_context_.end()) {
        LOGF(ERROR) << "Unknown buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }
    auto buffer_context = context_it->second.get();
    if (buffer_context->type == GRALLOC) {
        if (!--buffer_context->usage) {
            // Unmap all the existing mapping of bo.
            buffer_context_.erase(context_it);

            int ret = gm_module_->unregisterBuffer(gm_module_, buffer);

            if (ret) {
                LOGF(ERROR) << "Failed to unregister gralloc buffer";
                return ret;
            }
        }
        return 0;
    } else {
        NOTREACHED() << "Invalid buffer type: " << buffer_context->type;
        return -EINVAL;
    }
}

int CameraBufferManagerImpl::Lock(buffer_handle_t buffer,
                                  uint32_t flags,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  void** out_addr) {
    base::AutoLock l(lock_);

    auto context_it = buffer_context_.find(buffer);
    if (context_it == buffer_context_.end()) {
        LOGF(ERROR) << "Unknown buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }
    auto buffer_context = context_it->second.get();

    uint32_t num_planes = GetNumPlanes(buffer);
    if (!num_planes) {
        return -EINVAL;
    }
    if (num_planes > 1) {
        LOGF(ERROR) << "Lock called on multi-planar buffer 0x" << std::hex
            << buffer;
        return -EINVAL;
    }

    if (buffer_context->type == GRALLOC) {
        void* vir_addr = nullptr;
        if (gm_module_->lock) {
            int ret = gm_module_->lock(gm_module_, buffer, flags,
                                       x, y, width, height, &vir_addr);
            if (ret < 0)
                return -EINVAL;
        }
        *out_addr = vir_addr;
    } else {
        NOTREACHED() << "Invalid buffer type: " << buffer_context->type;
        return -EINVAL;
    }

    return 0;
}

int CameraBufferManagerImpl::LockYCbCr(buffer_handle_t buffer,
                                       uint32_t flags,
                                       uint32_t x,
                                       uint32_t y,
                                       uint32_t width,
                                       uint32_t height,
                                       struct android_ycbcr* out_ycbcr) {
    base::AutoLock l(lock_);

    auto context_it = buffer_context_.find(buffer);
    if (context_it == buffer_context_.end()) {
        LOGF(ERROR) << "Unknown buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }
    auto buffer_context = context_it->second.get();
    uint32_t num_planes = GetNumPlanes(buffer);
    if (!num_planes) {
        return -EINVAL;
    }
    if (num_planes < 2) {
        LOGF(ERROR) << "LockYCbCr called on single-planar buffer 0x" << std::hex
            << buffer_context->buffer_id;
        return -EINVAL;
    }

    DCHECK_LE(num_planes, 3u);

    if (buffer_context->type == GRALLOC) {
        if (gm_module_->lock_ycbcr) {
            int ret = gm_module_->lock_ycbcr(gm_module_, buffer, flags,
                                             x, y, width, height, out_ycbcr);
            if (ret < 0)
                return -EINVAL;
        }
    } else {
        NOTREACHED() << "Invalid buffer type: " << buffer_context->type;
        return -EINVAL;
    }

    return 0;
}

int CameraBufferManagerImpl::Unlock(buffer_handle_t buffer) {
    base::AutoLock l(lock_);

    auto context_it = buffer_context_.find(buffer);
    if (context_it == buffer_context_.end()) {
        LOGF(ERROR) << "Unknown buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }
    auto buffer_context = context_it->second.get();
    if (buffer_context->type == GRALLOC && gm_module_->unlock)
        return gm_module_->unlock(gm_module_, buffer);

    return 0;
}


int CameraBufferManagerImpl::FlushCache(buffer_handle_t buffer) {
    int fd = -1;

    if (gm_module_ && gm_module_->perform)
        gm_module_->perform(gm_module_,
                            GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                            buffer, &fd);
    if (fd == -1) {
        LOGF(ERROR) << "get fd error for buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }

    struct dma_buf_sync sync_args;
    sync_args.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    if (ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync_args))
        return -EINVAL;

    return 0;
}

int CameraBufferManagerImpl::GetHandleFd(buffer_handle_t buffer) {
    int fd = -1;

    if (gm_module_ && gm_module_->perform)
        gm_module_->perform(gm_module_,
                            GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD,
                            buffer, &fd);
    if (fd == -1) {
        LOGF(ERROR) << "get fd error for buffer 0x" << std::hex << buffer;
        return -EINVAL;
    }

    return fd;
}

int CameraBufferManagerImpl::AllocateGrallocBuffer(size_t width,
                                                   size_t height,
                                                   uint32_t format,
                                                   uint32_t usage,
                                                   buffer_handle_t* out_buffer,
                                                   uint32_t* out_stride) {
    base::AutoLock l(lock_);

    std::unique_ptr<BufferContext> buffer_context(new struct BufferContext);

    buffer_context->buffer_id = reinterpret_cast<uint64_t>(buffer_context.get());
    buffer_context->type = GRALLOC;
    int ret = alloc_device_->alloc(alloc_device_, width, height,
                                   format, usage, out_buffer,
                                   reinterpret_cast<int*>(out_stride));
    if (ret < 0)
        return -EINVAL;
    buffer_context->usage = 1;
    buffer_context_[*out_buffer] = std::move(buffer_context);
    return 0;
}

}  // namespace arc
