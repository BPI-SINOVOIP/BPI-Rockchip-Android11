/*
* Copyright (C) 2020 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <android/hardware/graphics/allocator/3.0/IAllocator.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <hidl/LegacySupport.h>
#include <qemu_pipe_bp.h>

#include "glUtils.h"
#include "cb_handle_30.h"
#include "host_connection_session.h"
#include "types.h"
#include "debug.h"

const int kOMX_COLOR_FormatYUV420Planar = 19;

using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_bitfield;
using ::android::hardware::Return;
using ::android::hardware::Void;

using ::android::hardware::graphics::common::V1_2::PixelFormat;
using ::android::hardware::graphics::common::V1_0::BufferUsage;

namespace AllocatorV3 = ::android::hardware::graphics::allocator::V3_0;
namespace MapperV3 = ::android::hardware::graphics::mapper::V3_0;

using IAllocator3 = AllocatorV3::IAllocator;
using IMapper3 = MapperV3::IMapper;
using Error3 = MapperV3::Error;
using BufferDescriptorInfo = IMapper3::BufferDescriptorInfo;

class GoldfishAllocator : public IAllocator3 {
public:
    GoldfishAllocator() : m_hostConn(HostConnection::createUnique()) {}

    Return<void> dumpDebugInfo(dumpDebugInfo_cb hidl_cb) {
        hidl_cb("GoldfishAllocator::dumpDebugInfo is not implemented");
        return {};
    }

    Return<void> allocate(const hidl_vec<uint32_t>& rawDescriptor,
                          uint32_t count,
                          allocate_cb hidl_cb) {
        uint32_t stride = 0;
        std::vector<cb_handle_30_t*> cbs;
        cbs.reserve(count);

        const Error3 e = allocateImpl(rawDescriptor, count, &stride, &cbs);
        if (e == Error3::NONE) {
            hidl_vec<hidl_handle> handles(cbs.cbegin(), cbs.cend());
            hidl_cb(Error3::NONE, stride, handles);
        } else {
            hidl_cb(e, 0, {});
        }

        for (cb_handle_30_t* cb : cbs) {
            freeCb(std::unique_ptr<cb_handle_30_t>(cb));
        }

        return {};
    }

private:
    // this function should be in sync with GoldfishMapper::isSupportedImpl
    Error3 allocateImpl(const hidl_vec<uint32_t>& rawDescriptor,
                        uint32_t count,
                        uint32_t* pStride,
                        std::vector<cb_handle_30_t*>* cbs) {
        BufferDescriptorInfo descriptor;
        if (!decodeBufferDescriptorInfo(rawDescriptor, &descriptor)) {
            RETURN_ERROR(Error3::BAD_DESCRIPTOR);
        }

        if (!descriptor.width) { RETURN_ERROR(Error3::UNSUPPORTED); }
        if (!descriptor.height) { RETURN_ERROR(Error3::UNSUPPORTED); }
        if (descriptor.layerCount != 1) { RETURN_ERROR(Error3::UNSUPPORTED); }

        const uint32_t usage = descriptor.usage;
        const bool usageSwWrite = usage & BufferUsage::CPU_WRITE_MASK;
        const bool usageSwRead = usage & BufferUsage::CPU_READ_MASK;
        const bool usageHwCamWrite = usage & BufferUsage::CAMERA_OUTPUT;
        const bool usageHwCamRead = usage & BufferUsage::CAMERA_INPUT;

        int bpp = 1;
        int glFormat = 0;
        int glType = 0;
        int align = 1;
        bool yuv_format = false;
        EmulatorFrameworkFormat emulatorFrameworkFormat =
            EmulatorFrameworkFormat::GL_COMPATIBLE;

        PixelFormat format;
        Error3 e = getBufferFormat(descriptor.format, usage, &format);
        if (e != Error3::NONE) {
            ALOGE("%s:%d Unsupported format: frameworkFormat=%d, usage=%x",
                  __func__, __LINE__, descriptor.format, usage);
            return e;
        }

        switch (format) {
        case PixelFormat::RGBA_8888:
        case PixelFormat::RGBX_8888:
        case PixelFormat::BGRA_8888:
            bpp = 4;
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            break;

        case PixelFormat::RGB_888:
            if (usage & (BufferUsage::GPU_TEXTURE |
                         BufferUsage::GPU_RENDER_TARGET |
                         BufferUsage::COMPOSER_OVERLAY |
                         BufferUsage::COMPOSER_CLIENT_TARGET)) {
                RETURN_ERROR(Error3::UNSUPPORTED);
            } else {
                bpp = 3;
                glFormat = GL_RGB;
                glType = GL_UNSIGNED_BYTE;
            }
            break;

        case PixelFormat::RGB_565:
            bpp = 2;
            glFormat = GL_RGB565;
            glType = GL_UNSIGNED_SHORT_5_6_5;
            break;

        case PixelFormat::RGBA_FP16:
            bpp = 8;
            glFormat = GL_RGBA16F;
            glType = GL_HALF_FLOAT;
            break;

        case PixelFormat::RGBA_1010102:
            bpp = 4;
            glFormat = GL_RGB10_A2;
            glType = GL_UNSIGNED_INT_2_10_10_10_REV;
            break;

        case PixelFormat::RAW16:
        case PixelFormat::Y16:
            bpp = 2;
            align = 16 * bpp;
            if (!((usageSwRead || usageHwCamRead) && (usageSwWrite || usageHwCamWrite))) {
                // Raw sensor data or Y16 only goes between camera and CPU
                RETURN_ERROR(Error3::UNSUPPORTED);
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_SHORT;
            break;

        case PixelFormat::BLOB:
            if (!usageSwRead) {
                // Blob data cannot be used by HW other than camera emulator
                // But there is a CTS test trying to have access to it
                // BUG: https://buganizer.corp.google.com/issues/37719518
                RETURN_ERROR(Error3::UNSUPPORTED);
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_BYTE;
            break;

        case PixelFormat::YCRCB_420_SP:
            yuv_format = true;
            // Not expecting to actually create any GL surfaces for this
            break;

        case PixelFormat::YV12:
            align = 16;
            yuv_format = true;
            // We are going to use RGB8888 on the host for Vulkan
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            emulatorFrameworkFormat = EmulatorFrameworkFormat::YV12;
            break;

        case PixelFormat::YCBCR_420_888:
            yuv_format = true;
            // We are going to use RGBA 8888 on the host
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            emulatorFrameworkFormat = EmulatorFrameworkFormat::YUV_420_888;
            break;

        default:
            ALOGE("%s:%d Unsupported format: format=%d, frameworkFormat=%d, usage=%x",
                  __func__, __LINE__, format, descriptor.format, usage);
            RETURN_ERROR(Error3::UNSUPPORTED);
        }

        const size_t align1 = align - 1;
        const uint32_t width = descriptor.width;
        const uint32_t height = descriptor.height;
        uint32_t stride;
        size_t bufferSize;

        if (yuv_format) {
            const size_t yStride = (width * bpp + align1) & ~align1;
            const size_t uvStride = (yStride / 2 + align1) & ~align1;
            const size_t uvHeight = height / 2;
            bufferSize = yStride * height + 2 * (uvHeight * uvStride);
            stride = yStride / bpp;
        } else {
            const size_t bpr = (width * bpp + align1) & ~align1;
            bufferSize = bpr * height;
            stride = bpr / bpp;
        }

        *pStride = stride;

        return allocateImpl2(usage,
                             width, height,
                             format, emulatorFrameworkFormat,
                             glFormat, glType,
                             bufferSize,
                             bpp, stride,
                             count, cbs);
    }

    Error3 allocateImpl2(const uint32_t usage,
                         const uint32_t width, const uint32_t height,
                         const PixelFormat format,
                         const EmulatorFrameworkFormat emulatorFrameworkFormat,
                         const int glFormat, const int glType,
                         const size_t bufferSize,
                         const uint32_t bytesPerPixel,
                         const uint32_t stride,
                         const uint32_t count,
                         std::vector<cb_handle_30_t*>* cbs) {
        for (uint32_t i = 0; i < count; ++i) {
            cb_handle_30_t* cb;
            Error3 e = allocateCb(usage,
                                  width, height,
                                  format, emulatorFrameworkFormat,
                                  glFormat, glType,
                                  bufferSize,
                                  bytesPerPixel, stride,
                                  &cb);
            if (e == Error3::NONE) {
                cbs->push_back(cb);
            } else {
                return e;
            }
        }

        RETURN(Error3::NONE);
    }

    // see GoldfishMapper::encodeBufferDescriptorInfo
    static bool decodeBufferDescriptorInfo(const hidl_vec<uint32_t>& raw,
                                           BufferDescriptorInfo* d) {
        if (raw.size() == 5) {
            d->width = raw[0];
            d->height = raw[1];
            d->layerCount = raw[2];
            d->format = static_cast<PixelFormat>(raw[3]);
            d->usage = raw[4];

            RETURN(true);
        } else {
            RETURN_ERROR(false);
        }
    }

    static Error3 getBufferFormat(const PixelFormat frameworkFormat,
                                  const uint32_t usage,
                                  PixelFormat* format) {
        if (frameworkFormat == PixelFormat::IMPLEMENTATION_DEFINED) {
            if (usage & BufferUsage::CAMERA_OUTPUT) {
                if (usage & BufferUsage::GPU_TEXTURE) {
                    // Camera-to-display is RGBA
                    *format = PixelFormat::RGBA_8888;
                    RETURN(Error3::NONE);
                } else if (usage & BufferUsage::VIDEO_ENCODER) {
                    // Camera-to-encoder is NV21
                    *format = PixelFormat::YCRCB_420_SP;
                    RETURN(Error3::NONE);
                }
            }
            RETURN_ERROR(Error3::UNSUPPORTED);
        } else if (static_cast<int>(frameworkFormat) == kOMX_COLOR_FormatYUV420Planar &&
               (usage & BufferUsage::GPU_DATA_BUFFER)) {
            ALOGW("gralloc_alloc: Requested OMX_COLOR_FormatYUV420Planar, given "
              "YCbCr_420_888, taking experimental path. "
              "usage=%x", usage);
            *format = PixelFormat::YCBCR_420_888;
            RETURN(Error3::NONE);
        } else  {
            *format = frameworkFormat;
            RETURN(Error3::NONE);
        }
    }

    static bool needHostCb(const uint32_t usage, const PixelFormat format) {
        return ((usage & BufferUsage::GPU_DATA_BUFFER)
                   || (format != PixelFormat::BLOB &&
                       format != PixelFormat::RAW16 &&
                       format != PixelFormat::Y16))
               && (usage & (BufferUsage::GPU_TEXTURE
                            | BufferUsage::GPU_RENDER_TARGET
                            | BufferUsage::COMPOSER_OVERLAY
                            | BufferUsage::VIDEO_ENCODER
                            | BufferUsage::COMPOSER_CLIENT_TARGET
                            | BufferUsage::CPU_READ_MASK));
    }

    Error3 allocateCb(const uint32_t usage,
                      const uint32_t width, const uint32_t height,
                      const PixelFormat format,
                      const EmulatorFrameworkFormat emulatorFrameworkFormat,
                      const int glFormat, const int glType,
                      const size_t bufferSize,
                      const int32_t bytesPerPixel,
                      const int32_t stride,
                      cb_handle_30_t** cb) {
        const HostConnectionSession conn = getHostConnectionSession();
        ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();
        CRASH_IF(!rcEnc, "conn.getRcEncoder() failed");

        GoldfishAddressSpaceHostMemoryAllocator host_memory_allocator(
            rcEnc->featureInfo_const()->hasSharedSlotsHostMemoryAllocator);
        if (!host_memory_allocator.is_opened()) {
            RETURN_ERROR(Error3::NO_RESOURCES);
        }

        GoldfishAddressSpaceBlock bufferBits;
        if (host_memory_allocator.hostMalloc(&bufferBits, bufferSize)) {
            RETURN_ERROR(Error3::NO_RESOURCES);
        }

        uint32_t hostHandle = 0;
        QEMU_PIPE_HANDLE hostHandleRefCountFd = QEMU_PIPE_INVALID_HANDLE;
        if (needHostCb(usage, format)) {
            hostHandleRefCountFd = qemu_pipe_open("refcount");
            if (!qemu_pipe_valid(hostHandleRefCountFd)) {
                RETURN_ERROR(Error3::NO_RESOURCES);
            }

            const GLenum allocFormat =
                (PixelFormat::RGBX_8888 == format) ? GL_RGB : glFormat;

            hostHandle = rcEnc->rcCreateColorBufferDMA(
                rcEnc,
                width, height,
                allocFormat, static_cast<int>(emulatorFrameworkFormat));

            if (!hostHandle) {
                qemu_pipe_close(hostHandleRefCountFd);
                RETURN_ERROR(Error3::NO_RESOURCES);
            }

            if (qemu_pipe_write(hostHandleRefCountFd,
                                &hostHandle,
                                sizeof(hostHandle)) != sizeof(hostHandle)) {
                rcEnc->rcCloseColorBuffer(rcEnc, hostHandle);
                qemu_pipe_close(hostHandleRefCountFd);
                RETURN_ERROR(Error3::NO_RESOURCES);
            }
        }

        std::unique_ptr<cb_handle_30_t> handle =
            std::make_unique<cb_handle_30_t>(
                host_memory_allocator.release(),
                hostHandleRefCountFd,
                hostHandle,
                usage,
                width,
                height,
                static_cast<int>(format),
                glFormat,
                glType,
                bufferSize,
                bufferBits.guestPtr(),
                bufferBits.size(),
                bufferBits.offset(),
                bytesPerPixel,
                stride);

        bufferBits.release();
        *cb = handle.release();
        RETURN(Error3::NONE);
    }

    void freeCb(std::unique_ptr<cb_handle_30_t> cb) {
        // no need to undo .hostMalloc: the kernel will take care of it once the
        // last bufferFd (duped) is closed.

        if (qemu_pipe_valid(cb->hostHandleRefCountFd)) {
            qemu_pipe_close(cb->hostHandleRefCountFd);
        }
        GoldfishAddressSpaceBlock::memoryUnmap(cb->getBufferPtr(), cb->mmapedSize);
        GoldfishAddressSpaceHostMemoryAllocator::closeHandle(cb->bufferFd);
    }

    HostConnectionSession getHostConnectionSession() const {
        return HostConnectionSession(m_hostConn.get());
    }

    std::unique_ptr<HostConnection> m_hostConn;
};

int main(int, char**) {
    using ::android::sp;

    ::android::hardware::configureRpcThreadpool(4, true /* callerWillJoin */);

    sp<IAllocator3> allocator(new GoldfishAllocator());
    if (allocator->registerAsService() != ::android::NO_ERROR) {
        ALOGE("failed to register graphics IAllocator@3.0 service");
        return -EINVAL;
    }

    ALOGI("graphics IAllocator@3.0 service is initialized");
    ::android::hardware::joinRpcThreadpool();

    ALOGI("graphics IAllocator@3.0 service is terminating");
    return 0;
}
