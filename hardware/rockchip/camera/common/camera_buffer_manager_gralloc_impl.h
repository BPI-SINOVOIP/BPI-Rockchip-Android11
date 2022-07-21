/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_CAMERA_BUFFER_MANAGER_IMPL_H_
#define COMMON_CAMERA_BUFFER_MANAGER_IMPL_H_

#include "arc/camera_buffer_manager.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include <base/synchronization/lock.h>

#ifdef TARGET_RK3368
#include <hardware/img_gralloc_public.h>
#if PLATFORM_SDK_API_VERSION >= 31
#include <hardware/hardware_rockchip.h>
#include <hardware/gralloc_rockchip.h>
#else
#include <hardware/gralloc.h>
#endif
#else
#include <gralloc_drm.h>
#include <gralloc_drm_handle.h>
#endif

// A V4L2 extension format which represents 32bit RGBX-8-8-8-8 format. This
// corresponds to DRM_FORMAT_XBGR8888 which is used as the underlying format for
// the HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINEND format on all CrOS boards.
#define V4L2_PIX_FMT_RGBX32 v4l2_fourcc('X', 'B', '2', '4')

struct android_ycbcr;

namespace arc {

struct BufferContext {
    uint64_t buffer_id;
    BufferType type;
    uint32_t usage;
};

typedef std::unordered_map<buffer_handle_t,
        std::unique_ptr<struct BufferContext>>
        BufferContextCache;

class CameraBufferManagerImpl final : public CameraBufferManager {
public:
    CameraBufferManagerImpl();

    // CameraBufferManager implementation.
    ~CameraBufferManagerImpl() final;
    int Allocate(size_t width,
                 size_t height,
                 uint32_t format,
                 uint32_t usage,
                 BufferType type,
                 buffer_handle_t* out_buffer,
                 uint32_t* out_stride) final;
    int Free(buffer_handle_t buffer) final;
    int Register(buffer_handle_t buffer) final;
    int Deregister(buffer_handle_t buffer) final;
    int Lock(buffer_handle_t buffer,
             uint32_t flags,
             uint32_t x,
             uint32_t y,
             uint32_t width,
             uint32_t height,
             void** out_addr) final;
    int LockYCbCr(buffer_handle_t buffer,
                  uint32_t flags,
                  uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height,
                  struct android_ycbcr* out_ycbcr) final;
    int Unlock(buffer_handle_t buffer) final;
    int FlushCache(buffer_handle_t buffer) final;
    int GetHandleFd(buffer_handle_t buffer) final;

private:
    static int GetHalPixelFormat(buffer_handle_t buffer);
private:
    friend class CameraBufferManager;

    int AllocateGrallocBuffer(size_t width,
                              size_t height,
                              uint32_t format,
                              uint32_t usage,
                              buffer_handle_t* out_buffer,
                              uint32_t* out_stride);

    // Lock to guard access member variables.
    base::Lock lock_;

    // ** Start of lock_ scope **

    // The handle to the opened GBM device.
    static gralloc_module_t* gm_module_; 
    static struct alloc_device_t* alloc_device_;
    // A cache which stores all the context of the registered buffers.
    BufferContextCache buffer_context_;

    // ** End of lock_ scope **

    DISALLOW_COPY_AND_ASSIGN(CameraBufferManagerImpl);
};

}  // namespace arc

#endif  // COMMON_CAMERA_BUFFER_MANAGER_IMPL_H_
