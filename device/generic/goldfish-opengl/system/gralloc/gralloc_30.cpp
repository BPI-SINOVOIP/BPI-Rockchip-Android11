/*
* Copyright (C) 2019 The Android Open Source Project
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

#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

#include <log/log.h>
#include <sys/mman.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <gralloc_cb_bp.h>
#include "gralloc_common.h"
#include "goldfish_address_space.h"
#include "HostConnection.h"
#include "FormatConversions.h"
#include <qemu_pipe_bp.h>

#define CRASH(MSG) \
    do { \
        ALOGE("%s:%d crashed with '%s'", __func__, __LINE__, MSG); \
        ::abort(); \
    } while (false)

#define CRASH_IF(COND, MSG) \
    do { \
        if ((COND)) { \
            ALOGE("%s:%d crashed on '%s' with '%s'", __func__, __LINE__, #COND, MSG); \
            ::abort(); \
        } \
    } while (false)

#define RETURN_ERROR_CODE(X) \
    do { \
        ALOGE("%s:%d failed with '%s' (%d)", \
              __func__, __LINE__, strerror(-(X)), -(X)); \
        return (X); \
    } while (false)

#define RETURN_ERROR(X) \
    do { \
        ALOGE("%s:%d failed with '%s'", __func__, __LINE__, #X); \
        return (X); \
    } while (false)

#define OMX_COLOR_FormatYUV420Planar 19

namespace {

const char GOLDFISH_GRALLOC_MODULE_NAME[] = "Graphics Memory Allocator Module";

hw_device_t make_hw_device(hw_module_t* module, int (*close)(hw_device_t*)) {
    hw_device_t result = {};

    result.tag = HARDWARE_DEVICE_TAG;
    result.version = 0;
    result.module = module;
    result.close = close;

    return result;
}

size_t align(const size_t v, const size_t a) { return (v + a - 1) & ~(a - 1); }

class HostConnectionSession {
public:
    explicit HostConnectionSession(HostConnection* hc) : conn(hc) {
        hc->lock();
    }

    ~HostConnectionSession() {
        if (conn) {
            conn->unlock();
        }
     }

    HostConnectionSession(HostConnectionSession&& rhs) : conn(rhs.conn) {
        rhs.conn = nullptr;
    }

    HostConnectionSession& operator=(HostConnectionSession&& rhs) {
        if (this != &rhs) {
            std::swap(conn, rhs.conn);
        }
        return *this;
    }

    HostConnectionSession(const HostConnectionSession&) = delete;
    HostConnectionSession& operator=(const HostConnectionSession&) = delete;

    ExtendedRCEncoderContext* getRcEncoder() const {
        return conn->rcEncoder();
    }

private:
    HostConnection* conn;
};

class goldfish_gralloc30_module_t;
class goldfish_gralloc30_device_t;
class goldfish_fb30_device_t;

class buffer_manager_t {
public:
    buffer_manager_t() = default;
    buffer_manager_t(const buffer_manager_t&) = delete;
    buffer_manager_t& operator=(const buffer_manager_t&) = delete;
    buffer_manager_t(buffer_manager_t&&) = delete;
    buffer_manager_t& operator=(buffer_manager_t&&) = delete;
    virtual ~buffer_manager_t() {}

    virtual uint64_t getMmapedPhysAddr(uint64_t offset) const = 0;

    virtual int alloc_buffer(int usage,
                             int width, int height, int format,
                             EmulatorFrameworkFormat emulatorFrameworkFormat,
                             int glFormat, int glType,
                             size_t bufferSize,
                             buffer_handle_t* pHandle) = 0;
    virtual int free_buffer(buffer_handle_t h) = 0;
    virtual int register_buffer(buffer_handle_t h) = 0;
    virtual int unregister_buffer(buffer_handle_t h) = 0;
};

std::unique_ptr<buffer_manager_t> create_buffer_manager(goldfish_gralloc30_module_t*);

class goldfish_gralloc30_module_t {
public:
    goldfish_gralloc30_module_t(): m_hostConn(HostConnection::createUnique()) {
        CRASH_IF(!m_hostConn, "m_hostConn cannot be nullptr");
        m_bufferManager = create_buffer_manager(this);
        CRASH_IF(!m_bufferManager, "m_bufferManager cannot be nullptr");
    }

    HostConnectionSession getHostConnectionSession() const {
        return HostConnectionSession(m_hostConn /*.get()*/);
    }

    int alloc_buffer(int usage,
                     int width, int height, int format,
                     EmulatorFrameworkFormat emulatorFrameworkFormat,
                     int glFormat, int glType,
                     size_t bufferSize,
                     buffer_handle_t* pHandle) {
        return m_bufferManager->alloc_buffer(usage,
                                             width, height, format,
                                             emulatorFrameworkFormat,
                                             glFormat, glType,
                                             bufferSize,
                                             pHandle);
    }

    int free_buffer(buffer_handle_t h) {
        return m_bufferManager->free_buffer(h);
    }

    int register_buffer(buffer_handle_t h) {
        return m_bufferManager->register_buffer(h);
    }

    int unregister_buffer(buffer_handle_t h) {
        return m_bufferManager->unregister_buffer(h);
    }

    int lock(cb_handle_t& handle,
             const int usage,
             const int left, const int top, const int width, const int height,
             void** vaddr) {
        if (!handle.bufferSize) { RETURN_ERROR_CODE(-EINVAL); }
        char* const bufferBits = static_cast<char*>(handle.getBufferPtr());
        if (!bufferBits) { RETURN_ERROR_CODE(-EINVAL); }

        if (handle.hostHandle) {
            const int res = lock_impl(handle,
                                      usage,
                                      left, top, width, height,
                                      bufferBits);
            if (res) { return res; }
        }

        *vaddr = bufferBits;
        return 0;
    }

    int unlock(cb_handle_t& handle) {
        if (!handle.bufferSize) { RETURN_ERROR_CODE(-EINVAL); }

        char* const bufferBits = static_cast<char*>(handle.getBufferPtr());
        if (!bufferBits) { RETURN_ERROR_CODE(-EINVAL); }

        if (handle.hostHandle) {
            unlock_impl(handle, bufferBits);
        }

        return 0;
    }

    int lock_ycbcr(cb_handle_t& handle,
                   const int usage,
                   const int left, const int top, const int width, const int height,
                   android_ycbcr* ycbcr) {
        if (!ycbcr) { RETURN_ERROR_CODE(-EINVAL); }
        if (!handle.bufferSize) { RETURN_ERROR_CODE(-EINVAL); }
        char* const bufferBits = static_cast<char*>(handle.getBufferPtr());
        if (!bufferBits) { RETURN_ERROR_CODE(-EINVAL); }

        size_t uOffset;
        size_t vOffset;
        size_t yStride;
        size_t cStride;
        size_t cStep;

        switch (handle.format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            yStride = handle.width;
            cStride = yStride;
            vOffset = yStride * handle.height;
            uOffset = vOffset + 1;
            cStep = 2;
            break;

        case HAL_PIXEL_FORMAT_YV12:
            // https://developer.android.com/reference/android/graphics/ImageFormat.html#YV12
            yStride = align(handle.width, 16);
            cStride = align(yStride / 2, 16);
            vOffset = yStride * handle.height;
            uOffset = vOffset + (cStride * handle.height / 2);
            cStep = 1;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            yStride = handle.width;
            cStride = yStride / 2;
            uOffset = handle.height * yStride;
            vOffset = uOffset + cStride * handle.height / 2;
            cStep = 1;
            break;

        default:
            ALOGE("%s:%d unexpected format (%d)", __func__, __LINE__, handle.format);
            RETURN_ERROR_CODE(-EINVAL);
        }

        if (handle.hostHandle) {
            const int res = lock_impl(handle,
                                      usage,
                                      left, top, width, height,
                                      bufferBits);
            if (res) { return res; }
        }

        memset(ycbcr->reserved, 0, sizeof(ycbcr->reserved));
        char* const vaddr1 = static_cast<char*>(bufferBits);
        ycbcr->y = vaddr1;
        ycbcr->cb = vaddr1 + uOffset;
        ycbcr->cr = vaddr1 + vOffset;
        ycbcr->ystride = yStride;
        ycbcr->cstride = cStride;
        ycbcr->chroma_step = cStep;
        return 0;
    }

private:
    int lock_impl(cb_handle_t& handle,
                  const int usage,
                  const int left, const int top, const int width, const int height,
                  char* const bufferBits) {
        const bool usageSwRead = usage & GRALLOC_USAGE_SW_READ_MASK;
        const bool usageSwWrite = usage & GRALLOC_USAGE_SW_WRITE_MASK;
        const bool usageHwCamera = usage & GRALLOC_USAGE_HW_CAMERA_MASK;
        const bool usageHwCameraWrite = usage & GRALLOC_USAGE_HW_CAMERA_WRITE;

        const HostConnectionSession conn = getHostConnectionSession();
        ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();

        const int res = rcEnc->rcColorBufferCacheFlush(
            rcEnc, handle.hostHandle, 0, usageSwRead);
        if (res < 0) {
            RETURN_ERROR_CODE(-EBUSY);
        }

        // camera delivers bits to the buffer directly and does not require
        // an explicit read.
        if (usageSwRead && !usageHwCamera) {
            if (gralloc_is_yuv_format(handle.format)) {
                if (rcEnc->hasYUVCache()) {
                    uint32_t bufferSize;

                    switch (handle.format) {
                    case HAL_PIXEL_FORMAT_YV12:
                        get_yv12_offsets(handle.width, handle.height,
                                         nullptr, nullptr, &bufferSize);
                        break;
                    case HAL_PIXEL_FORMAT_YCbCr_420_888:
                        get_yuv420p_offsets(handle.width, handle.height,
                                            nullptr, nullptr, &bufferSize);
                        break;
                    default:
                        CRASH("Unexpected format, switch is out of sync with gralloc_is_yuv_format");
                        break;
                    }

                    rcEnc->rcReadColorBufferYUV(rcEnc, handle.hostHandle,
                        0, 0, handle.width, handle.height,
                        bufferBits, bufferSize);
                } else {
                    // We are using RGB888
                    std::vector<char> tmpBuf(handle.width * handle.height * 3);
                    rcEnc->rcReadColorBuffer(rcEnc, handle.hostHandle,
                                             0, 0, handle.width, handle.height,
                                             handle.glFormat, handle.glType,
                                             tmpBuf.data());

                    switch (handle.format) {
                    case HAL_PIXEL_FORMAT_YV12:
                        rgb888_to_yv12(bufferBits, tmpBuf.data(),
                                       handle.width, handle.height,
                                       left, top,
                                       left + width - 1, top + height - 1);
                        break;
                    case HAL_PIXEL_FORMAT_YCbCr_420_888:
                        rgb888_to_yuv420p(bufferBits, tmpBuf.data(),
                                          handle.width, handle.height,
                                          left, top,
                                          left + width - 1, top + height - 1);
                        break;
                    default:
                        CRASH("Unexpected format, switch is out of sync with gralloc_is_yuv_format");
                        break;
                    }
                }
            } else {
                rcEnc->rcReadColorBuffer(rcEnc,
                                         handle.hostHandle,
                                         0, 0, handle.width, handle.height,
                                         handle.glFormat, handle.glType,
                                         bufferBits);
            }
        }

        if (usageSwWrite || usageHwCameraWrite) {
            handle.lockedLeft = left;
            handle.lockedTop = top;
            handle.lockedWidth = width;
            handle.lockedHeight = height;
        } else {
            handle.lockedLeft = 0;
            handle.lockedTop = 0;
            handle.lockedWidth = handle.width;
            handle.lockedHeight = handle.height;
        }

        return 0;
    }

    void unlock_impl(cb_handle_t& handle, char* const bufferBits) {
        const int bpp = glUtilsPixelBitSize(handle.glFormat, handle.glType) >> 3;
        const int left = handle.lockedLeft;
        const int top = handle.lockedTop;
        const int width = handle.lockedWidth;
        const int height = handle.lockedHeight;
        const uint32_t rgbSize = width * height * bpp;

        std::vector<char> convertedBuf;
        const char* bitsToSend;
        uint32_t sizeToSend;
        if (gralloc_is_yuv_format(handle.format)) {
            bitsToSend = bufferBits;
            switch (handle.format) {
            case HAL_PIXEL_FORMAT_YV12:
                get_yv12_offsets(width, height, nullptr, nullptr, &sizeToSend);
                break;

            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                get_yuv420p_offsets(width, height, nullptr, nullptr, &sizeToSend);
                break;

            default:
                CRASH("Unexpected format, switch is out of sync with gralloc_is_yuv_format");
                break;
            }
        } else {
            convertedBuf.resize(rgbSize);
            copy_rgb_buffer_from_unlocked(
                convertedBuf.data(), bufferBits,
                handle.width,
                width, height, top, left, bpp);
            bitsToSend = convertedBuf.data();
            sizeToSend = rgbSize;
        }

        {
            const HostConnectionSession conn = getHostConnectionSession();
            ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();

            rcEnc->bindDmaDirectly(bufferBits,
                                   m_bufferManager->getMmapedPhysAddr(handle.getMmapedOffset()));
            rcEnc->rcUpdateColorBufferDMA(rcEnc, handle.hostHandle,
                    left, top, width, height,
                    handle.glFormat, handle.glType,
                    const_cast<char*>(bitsToSend), sizeToSend);
        }

        handle.lockedLeft = 0;
        handle.lockedTop = 0;
        handle.lockedWidth = 0;
        handle.lockedHeight = 0;
    }

    //std::unique_ptr<HostConnection> m_hostConn;  // b/142677230
    HostConnection* m_hostConn;
    std::unique_ptr<buffer_manager_t> m_bufferManager;
};

// no ctor/dctor here
struct private_module_t {
    goldfish_gralloc30_module_t* impl() {
        return &gralloc30;
    }

    hw_module_t* to_hw_module() { return &base.common; }

    static private_module_t* from_hw_module(const hw_module_t* m) {
        if (!m) {
            RETURN_ERROR(nullptr);
        }

        if ((m->id == GRALLOC_HARDWARE_MODULE_ID) && (m->name == GOLDFISH_GRALLOC_MODULE_NAME)) {
            return reinterpret_cast<private_module_t*>(const_cast<hw_module_t*>(m));
        } else {
            RETURN_ERROR(nullptr);
        }
    }

    static private_module_t* from_gralloc_module(const gralloc_module_t* m) {
        if (!m) {
            RETURN_ERROR(nullptr);
        }

        return from_hw_module(&m->common);
    }

    gralloc_module_t base;
    goldfish_gralloc30_module_t gralloc30;
};

class goldfish_gralloc30_device_t {
    alloc_device_t device;
    goldfish_gralloc30_module_t* gralloc_module;

public:
    goldfish_gralloc30_device_t(private_module_t* module)
            : gralloc_module(module->impl()) {
        memset(&device, 0, sizeof(device));
        device.common = make_hw_device(module->to_hw_module(),
                                       &s_goldfish_gralloc30_device_close);
        device.alloc = &s_gralloc_alloc;
        device.free  = &s_gralloc_free;
    }

    hw_device_t* get_hw_device_ptr() { return &device.common; }

private:
    static int get_buffer_format(const int frameworkFormat, const int usage) {
        if (frameworkFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            if (usage & GRALLOC_USAGE_HW_CAMERA_WRITE) {
                if (usage & GRALLOC_USAGE_HW_TEXTURE) {
                    // Camera-to-display is RGBA
                    return HAL_PIXEL_FORMAT_RGBA_8888;
                } else if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                    // Camera-to-encoder is NV21
                    return HAL_PIXEL_FORMAT_YCrCb_420_SP;
                }
            }

            RETURN_ERROR_CODE(-EINVAL);
        } else if (frameworkFormat == OMX_COLOR_FormatYUV420Planar &&
               (usage & GOLDFISH_GRALLOC_USAGE_GPU_DATA_BUFFER)) {
            ALOGW("gralloc_alloc: Requested OMX_COLOR_FormatYUV420Planar, given "
              "YCbCr_420_888, taking experimental path. "
              "usage=%x", usage);
            return HAL_PIXEL_FORMAT_YCbCr_420_888;
        }
        else  {
            return frameworkFormat;
        }
    }

    int gralloc_alloc(const int width, const int height,
                      const int frameworkFormat,
                      const int usage,
                      buffer_handle_t* pHandle,
                      int* pStride) {
        const bool usageSwWrite = usage & GRALLOC_USAGE_SW_WRITE_MASK;
        const bool usageSwRead = usage & GRALLOC_USAGE_SW_READ_MASK;
        const bool usageHwTexture = usage & GRALLOC_USAGE_HW_TEXTURE;
        const bool usageHwRender = usage & GRALLOC_USAGE_HW_RENDER;
        const bool usageHw2d = usage & GRALLOC_USAGE_HW_2D;
        const bool usageHwComposer = usage & GRALLOC_USAGE_HW_COMPOSER;
        const bool usageHwFb = usage & GRALLOC_USAGE_HW_FB;
        const bool usageHwCamWrite = usage & GRALLOC_USAGE_HW_CAMERA_WRITE;
        const bool usageHwCamRead = usage & GRALLOC_USAGE_HW_CAMERA_READ;
        const bool usageRGB888Unsupported = usageHwTexture
            || usageHwRender ||usageHw2d || usageHwComposer || usageHwFb;

        int bpp = 1;
        int glFormat = 0;
        int glType = 0;
        int align = 1;
        bool yuv_format = false;
        EmulatorFrameworkFormat emulatorFrameworkFormat = FRAMEWORK_FORMAT_GL_COMPATIBLE;

        const int format = get_buffer_format(frameworkFormat, usage);
        if (format < 0) {
            ALOGE("%s:%d Unsupported format: frameworkFormat=%d, usage=%x",
                  __func__, __LINE__, frameworkFormat, usage);
            return format;
        }

        switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bpp = 4;
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            break;

        case HAL_PIXEL_FORMAT_RGB_888:
            if (usageRGB888Unsupported) {
                RETURN_ERROR_CODE(-EINVAL);  // we dont support RGB_888 for HW usage
            } else {
                bpp = 3;
                glFormat = GL_RGB;
                glType = GL_UNSIGNED_BYTE;
            }
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
            bpp = 2;
            glFormat = GL_RGB565;
            glType = GL_UNSIGNED_SHORT_5_6_5;
            break;

        case HAL_PIXEL_FORMAT_RGBA_FP16:
            bpp = 8;
            glFormat = GL_RGBA16F;
            glType = GL_HALF_FLOAT;
            break;

        case HAL_PIXEL_FORMAT_RGBA_1010102:
            bpp = 4;
            glFormat = GL_RGB10_A2;
            glType = GL_UNSIGNED_INT_2_10_10_10_REV;
            break;

        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_Y16:
            bpp = 2;
            align = 16 * bpp;
            if (!((usageSwRead || usageHwCamRead) && (usageSwWrite || usageHwCamWrite))) {
                // Raw sensor data or Y16 only goes between camera and CPU
                RETURN_ERROR_CODE(-EINVAL);
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_SHORT;
            break;

        case HAL_PIXEL_FORMAT_BLOB:
            if (!usageSwRead) {
                // Blob data cannot be used by HW other than camera emulator
                // But there is a CTS test trying to have access to it
                // BUG: https://buganizer.corp.google.com/issues/37719518
                RETURN_ERROR_CODE(-EINVAL);
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_BYTE;
            break;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            yuv_format = true;
            // Not expecting to actually create any GL surfaces for this
            break;

        case HAL_PIXEL_FORMAT_YV12:
            align = 16;
            yuv_format = true;
            // We are going to use RGB8888 on the host for Vulkan
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            emulatorFrameworkFormat = FRAMEWORK_FORMAT_YV12;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            yuv_format = true;
            // We are going to use RGB888 on the host
            glFormat = GL_RGB;
            glType = GL_UNSIGNED_BYTE;
            emulatorFrameworkFormat = FRAMEWORK_FORMAT_YUV_420_888;
            break;

        default:
            ALOGE("%s:%d Unsupported format: format=%d, frameworkFormat=%d, usage=%x",
                  __func__, __LINE__, format, frameworkFormat, usage);
            RETURN_ERROR_CODE(-EINVAL);
        }

        const size_t align1 = align - 1;
        int stride;
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

        const int res = gralloc_module->alloc_buffer(
            usage,
            width, height, format,
            emulatorFrameworkFormat,
            glFormat, glType,
            bufferSize,
            pHandle);
        if (res) {
            return res;
        }

        *pStride = stride;
        return 0;
    }

    int gralloc_free(buffer_handle_t h) {
        return gralloc_module->free_buffer(h);
    }

    static int s_goldfish_gralloc30_device_close(hw_device_t* d) {
        goldfish_gralloc30_device_t* gd = from_hw_device(d);
        if (!gd) {
            RETURN_ERROR_CODE(-EINVAL);
        }

        std::unique_ptr<goldfish_gralloc30_device_t> deleter(gd);
        return 0;
    }

    static int s_gralloc_alloc(alloc_device_t* ad,
                         int w, int h, int format, int usage,
                         buffer_handle_t* pHandle, int* pStride) {
        goldfish_gralloc30_device_t* gd = from_alloc_device(ad);
        if (!gd) {
            RETURN_ERROR_CODE(-EINVAL);
        }

        return gd->gralloc_alloc(w, h, format, usage, pHandle, pStride);
    }

    static int s_gralloc_free(alloc_device_t* ad, buffer_handle_t h) {
        goldfish_gralloc30_device_t* gd = from_alloc_device(ad);
        if (!gd) {
            RETURN_ERROR_CODE(-EINVAL);
        }

        return gd->gralloc_free(h);
    }

    static goldfish_gralloc30_device_t* from_hw_device(hw_device_t* d) {
        if (!d) {
            RETURN_ERROR(nullptr);
        }

        if (d->close == &s_goldfish_gralloc30_device_close) {
            return reinterpret_cast<goldfish_gralloc30_device_t*>(d);
        } else {
            RETURN_ERROR(nullptr);
        }
    }

    static goldfish_gralloc30_device_t* from_alloc_device(alloc_device_t* d) {
        if (!d) {
            RETURN_ERROR(nullptr);
        }

        return from_hw_device(&d->common);
    }
};

template <class T> T& unconst(const T& x) { return const_cast<T&>(x); }

const uint32_t CB_HANDLE_MAGIC_30 = CB_HANDLE_MAGIC_BASE | 0x2;

struct cb_handle_30_t : public cb_handle_t {
    cb_handle_30_t(address_space_handle_t p_bufferFd,
                   QEMU_PIPE_HANDLE p_hostHandleRefCountFd,
                   uint32_t p_hostHandle,
                   int32_t p_usage,
                   int32_t p_width,
                   int32_t p_height,
                   int32_t p_format,
                   int32_t p_glFormat,
                   int32_t p_glType,
                   uint32_t p_bufSize,
                   void* p_bufPtr,
                   int32_t p_bufferPtrPid,
                   uint32_t p_mmapedSize,
                   uint64_t p_mmapedOffset)
            : cb_handle_t(p_bufferFd,
                          p_hostHandleRefCountFd,
                          CB_HANDLE_MAGIC_30,
                          p_hostHandle,
                          p_usage,
                          p_width,
                          p_height,
                          p_format,
                          p_glFormat,
                          p_glType,
                          p_bufSize,
                          p_bufPtr,
                          p_mmapedOffset),
              bufferFdAsInt(p_bufferFd),
              bufferPtrPid(p_bufferPtrPid),
              mmapedSize(p_mmapedSize) {
        numInts = CB_HANDLE_NUM_INTS(numFds);
    }

    bool isValid() const { return (version == sizeof(native_handle_t)) && (magic == CB_HANDLE_MAGIC_30); }

    static cb_handle_30_t* from(void* p) {
        if (!p) { return nullptr; }
        cb_handle_30_t* cb = static_cast<cb_handle_30_t*>(p);
        return cb->isValid() ? cb : nullptr;
    }

    static const cb_handle_30_t* from(const void* p) {
        return from(const_cast<void*>(p));
    }

    static cb_handle_30_t* from_unconst(const void* p) {
        return from(const_cast<void*>(p));
    }

    int32_t  bufferFdAsInt;         // int copy of bufferFd, to check if fd was duped
    int32_t  bufferPtrPid;          // pid where bufferPtr belongs to
    uint32_t mmapedSize;            // real allocation side
};

// goldfish_address_space_host_malloc_handle_manager_t uses
// GoldfishAddressSpaceHostMemoryAllocator and GoldfishAddressSpaceBlock
// to allocate buffers on the host.
// It keeps track of usage of host handles allocated by rcCreateColorBufferDMA
// on the guest by qemu_pipe_open("refcount").
class goldfish_address_space_host_malloc_buffer_manager_t : public buffer_manager_t {
public:
    goldfish_address_space_host_malloc_buffer_manager_t(goldfish_gralloc30_module_t* gr): m_gr(gr) {
        GoldfishAddressSpaceHostMemoryAllocator host_memory_allocator(false);
        CRASH_IF(!host_memory_allocator.is_opened(),
                 "GoldfishAddressSpaceHostMemoryAllocator failed to open");

        GoldfishAddressSpaceBlock bufferBits;
        CRASH_IF(host_memory_allocator.hostMalloc(&bufferBits, 256),
                 "hostMalloc failed");

        m_physAddrToOffset = bufferBits.physAddr() - bufferBits.offset();
    }

    uint64_t getMmapedPhysAddr(uint64_t offset) const override {
        return m_physAddrToOffset + offset;
    }

    int alloc_buffer(int usage,
                     int width, int height, int format,
                     EmulatorFrameworkFormat emulatorFrameworkFormat,
                     int glFormat, int glType,
                     size_t bufferSize,
                     buffer_handle_t* pHandle) override {
        const HostConnectionSession conn = m_gr->getHostConnectionSession();
        ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();

        GoldfishAddressSpaceHostMemoryAllocator host_memory_allocator(
            rcEnc->featureInfo_const()->hasSharedSlotsHostMemoryAllocator);
        if (!host_memory_allocator.is_opened()) { RETURN_ERROR_CODE(-EIO); }

        GoldfishAddressSpaceBlock bufferBits;
        if (host_memory_allocator.hostMalloc(&bufferBits, bufferSize)) { RETURN_ERROR_CODE(-EIO); }

        uint32_t hostHandle = 0;
        QEMU_PIPE_HANDLE hostHandleRefCountFd = QEMU_PIPE_INVALID_HANDLE;
        if (need_host_cb(usage, format)) {
            hostHandleRefCountFd = qemu_pipe_open("refcount");
            if (!qemu_pipe_valid(hostHandleRefCountFd)) { RETURN_ERROR_CODE(-EIO); }

            const GLenum allocFormat =
                (HAL_PIXEL_FORMAT_RGBX_8888 == format) ? GL_RGB : glFormat;

            hostHandle = rcEnc->rcCreateColorBufferDMA(
                rcEnc,
                width, height,
                allocFormat, emulatorFrameworkFormat);
            if (!hostHandle) {
                qemu_pipe_close(hostHandleRefCountFd);
                RETURN_ERROR_CODE(-EIO);
            }
            if (qemu_pipe_write(hostHandleRefCountFd, &hostHandle, sizeof(hostHandle)) != sizeof(hostHandle)) {
                rcEnc->rcCloseColorBuffer(rcEnc, hostHandle);
                qemu_pipe_close(hostHandleRefCountFd);
                RETURN_ERROR_CODE(-EIO);
            }
        }

        std::unique_ptr<cb_handle_30_t> handle =
            std::make_unique<cb_handle_30_t>(
                host_memory_allocator.release(), hostHandleRefCountFd,
                hostHandle,
                usage, width, height,
                format, glFormat, glType,
                bufferSize, bufferBits.guestPtr(), getpid(),
                bufferBits.size(), bufferBits.offset());
        bufferBits.release();

        *pHandle = handle.release();
        return 0;
    }

    int free_buffer(buffer_handle_t h) override {
        std::unique_ptr<cb_handle_30_t> handle(cb_handle_30_t::from_unconst(h));
        if (!handle) {
            RETURN_ERROR_CODE(-EINVAL);
        }

        if (handle->bufferPtrPid != getpid()) { RETURN_ERROR_CODE(-EACCES); }
        if (handle->bufferFd != handle->bufferFdAsInt) { RETURN_ERROR_CODE(-EACCES); }

        if (qemu_pipe_valid(handle->hostHandleRefCountFd)) {
            qemu_pipe_close(handle->hostHandleRefCountFd);
        }
        // We can't recycle the address block and host resources because this
        // fd could be duped. The kernel will take care of it when the last fd
        // will be closed.
        if (handle->mmapedSize > 0) {
            GoldfishAddressSpaceBlock::memoryUnmap(handle->getBufferPtr(), handle->mmapedSize);
        }
        GoldfishAddressSpaceHostMemoryAllocator::closeHandle(handle->bufferFd);

        return 0;
    }

    int register_buffer(buffer_handle_t h) override {
#ifndef HOST_BUILD
        cb_handle_30_t *handle = cb_handle_30_t::from_unconst(h);
        if (!handle) { RETURN_ERROR_CODE(-EINVAL); }

        if (handle->mmapedSize > 0) {
            void* ptr;
            const int res = GoldfishAddressSpaceBlock::memoryMap(
                handle->getBufferPtr(),
                handle->mmapedSize,
                handle->bufferFd,
                handle->getMmapedOffset(),
                &ptr);
            if (res) {
                RETURN_ERROR_CODE(-res);
            }

            handle->setBufferPtr(ptr);
        }
        if (handle->hostHandle) {
            const HostConnectionSession conn = m_gr->getHostConnectionSession();
            ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();
            rcEnc->rcOpenColorBuffer2(rcEnc, handle->hostHandle);
        }

        handle->bufferFdAsInt = handle->bufferFd;
        handle->bufferPtrPid = getpid();
#endif  // HOST_BUILD

        return 0;
    }

    int unregister_buffer(buffer_handle_t h) override {
#ifndef HOST_BUILD
        cb_handle_30_t *handle = cb_handle_30_t::from_unconst(h);
        if (!handle) { RETURN_ERROR_CODE(-EINVAL); }

        if (handle->bufferPtrPid != getpid()) { RETURN_ERROR_CODE(-EACCES); }
        if (handle->bufferFd != handle->bufferFdAsInt) { RETURN_ERROR_CODE(-EACCES); }

        if (handle->hostHandle) {
            const HostConnectionSession conn = m_gr->getHostConnectionSession();
            ExtendedRCEncoderContext *const rcEnc = conn.getRcEncoder();
            rcEnc->rcCloseColorBuffer(rcEnc, handle->hostHandle);
        }
        if (handle->mmapedSize > 0) {
            GoldfishAddressSpaceBlock::memoryUnmap(handle->getBufferPtr(), handle->mmapedSize);
        }

        handle->bufferFdAsInt = -1;
        handle->bufferPtrPid = -1;
#endif  // HOST_BUILD
        return 0;
    }

    static bool need_host_cb(const int usage, const int format) {
        return ((usage & GOLDFISH_GRALLOC_USAGE_GPU_DATA_BUFFER)
                   || (format != HAL_PIXEL_FORMAT_BLOB &&
                       format != HAL_PIXEL_FORMAT_RAW16 &&
                       format != HAL_PIXEL_FORMAT_Y16))
               && (usage & (GRALLOC_USAGE_HW_TEXTURE
                            | GRALLOC_USAGE_HW_RENDER
                            | GRALLOC_USAGE_HW_2D
                            | GRALLOC_USAGE_HW_COMPOSER
                            | GRALLOC_USAGE_HW_VIDEO_ENCODER
                            | GRALLOC_USAGE_HW_FB
                            | GRALLOC_USAGE_SW_READ_MASK));
    }

private:
    goldfish_gralloc30_module_t* m_gr;
    uint64_t m_physAddrToOffset;
};

std::unique_ptr<buffer_manager_t> create_buffer_manager(goldfish_gralloc30_module_t* gr) {
    if (!gr) {
        RETURN_ERROR(nullptr);
    }

    // TODO: negotiate with the host the best way to allocate memory.

    return std::make_unique<goldfish_address_space_host_malloc_buffer_manager_t>(gr);
}

int gralloc_register_buffer(const gralloc_module_t* gralloc_module, buffer_handle_t h) {
    private_module_t* module = private_module_t::from_gralloc_module(gralloc_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    return module->impl()->register_buffer(h);
}

int gralloc_unregister_buffer(const gralloc_module_t* gralloc_module, buffer_handle_t h) {
    private_module_t* module = private_module_t::from_gralloc_module(gralloc_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }

   return module->impl()->unregister_buffer(h);
}

int gralloc_lock(const gralloc_module_t* gralloc_module,
                 buffer_handle_t bh, int usage,
                 int l, int t, int w, int h,
                 void** vaddr) {
    private_module_t* module = private_module_t::from_gralloc_module(gralloc_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    cb_handle_t* handle = cb_handle_t::from_unconst(bh);
    if (!handle) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    return module->impl()->lock(*handle, usage, l, t, w, h, vaddr);
}

int gralloc_unlock(const gralloc_module_t* gralloc_module, buffer_handle_t bh) {
    private_module_t* module = private_module_t::from_gralloc_module(gralloc_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    cb_handle_t* handle = cb_handle_t::from_unconst(bh);
    if (!handle) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    return module->impl()->unlock(*handle);
}

int gralloc_lock_ycbcr(const gralloc_module_t* gralloc_module,
                       buffer_handle_t bh, int usage,
                       int l, int t, int w, int h,
                       android_ycbcr *ycbcr) {
    private_module_t* module = private_module_t::from_gralloc_module(gralloc_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    cb_handle_t* handle = cb_handle_t::from_unconst(bh);
    if (!handle) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    return module->impl()->lock_ycbcr(*handle, usage, l, t, w, h, ycbcr);
}

int gralloc_device_open_gpu0(private_module_t* module, hw_device_t** device) {
    std::unique_ptr<goldfish_gralloc30_device_t> gralloc_device =
        std::make_unique<goldfish_gralloc30_device_t>(module);
    if (!gralloc_device) {
        RETURN_ERROR_CODE(-ENOMEM);
    }

    *device = gralloc_device->get_hw_device_ptr();
    gralloc_device.release();
    return 0;
}

int gralloc_device_open(const hw_module_t* hw_module,
                        const char* name,
                        hw_device_t** device) {
    private_module_t* module = private_module_t::from_hw_module(hw_module);
    if (!module) {
        RETURN_ERROR_CODE(-EINVAL);
    }
    if (!name) {
        RETURN_ERROR_CODE(-EINVAL);
    }
    if (!device) {
        RETURN_ERROR_CODE(-EINVAL);
    }

    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {
        return gralloc_device_open_gpu0(module, device);
    }

    RETURN_ERROR_CODE(-EINVAL);
}

struct hw_module_methods_t gralloc_module_methods = {
    .open = &gralloc_device_open
};
}  // namespace

extern "C" __attribute__((visibility("default")))
struct private_module_t HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = GRALLOC_MODULE_API_VERSION_0_2,
            .hal_api_version = 0,
            .id = GRALLOC_HARDWARE_MODULE_ID,
            .name = GOLDFISH_GRALLOC_MODULE_NAME,
            .author = "The Android Open Source Project",
            .methods = &gralloc_module_methods,
            .dso = nullptr,
            .reserved = {0}
        },
        .registerBuffer   = &gralloc_register_buffer,
        .unregisterBuffer = &gralloc_unregister_buffer,
        .lock             = &gralloc_lock,
        .unlock           = &gralloc_unlock,
        .perform          = nullptr,  /* reserved for future use */
        .lock_ycbcr       = &gralloc_lock_ycbcr,
    },
};
