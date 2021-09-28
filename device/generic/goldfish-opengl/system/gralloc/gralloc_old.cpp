/*
* Copyright (C) 2011 The Android Open Source Project
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
#include <cutils/ashmem.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <hardware/gralloc.h>

#include <gralloc_cb_bp.h>
#include "gralloc_common.h"

#include "goldfish_dma.h"
#include "goldfish_address_space.h"
#include "FormatConversions.h"
#include "HostConnection.h"
#include "ProcessPipe.h"
#include "ThreadInfo.h"
#include "glUtils.h"
#include <qemu_pipe_bp.h>

#if PLATFORM_SDK_VERSION < 26
#include <cutils/log.h>
#else
#include <log/log.h>
#endif
#include <cutils/properties.h>

#include <set>
#include <map>
#include <vector>
#include <string>
#include <sstream>

/* Set to 1 or 2 to enable debug traces */
#define DEBUG  0

#ifndef D

#if DEBUG >= 1
#  define D(...)   ALOGD(__VA_ARGS__)
#else
#  define D(...)   ((void)0)
#endif

#endif

#if DEBUG >= 2
#  define DD(...)  ALOGD(__VA_ARGS__)
#else
#  define DD(...)  ((void)0)
#endif

#define DBG_FUNC DBG("%s\n", __FUNCTION__)

#define GOLDFISH_OFFSET_UNIT 8

#define OMX_COLOR_FormatYUV420Planar 19

#ifdef GOLDFISH_HIDL_GRALLOC
static const bool isHidlGralloc = true;
#else
static const bool isHidlGralloc = false;
#endif

const uint32_t CB_HANDLE_MAGIC_OLD = CB_HANDLE_MAGIC_BASE | 0x1;

struct cb_handle_old_t : public cb_handle_t {
    cb_handle_old_t(int p_fd, int p_ashmemSize, int p_usage,
                    int p_width, int p_height,
                    int p_format, int p_glFormat, int p_glType)
            : cb_handle_t(p_fd,
                          QEMU_PIPE_INVALID_HANDLE,
                          CB_HANDLE_MAGIC_OLD,
                          0,
                          p_usage,
                          p_width,
                          p_height,
                          p_format,
                          p_glFormat,
                          p_glType,
                          p_ashmemSize,
                          NULL,
                          ~uint64_t(0)),
              ashmemBasePid(0),
              mappedPid(0) {
        numInts = CB_HANDLE_NUM_INTS(numFds);
    }

    bool hasRefcountPipe() const {
        return qemu_pipe_valid(hostHandleRefCountFd);
    }

    void setRefcountPipeFd(QEMU_PIPE_HANDLE fd) {
        if (qemu_pipe_valid(fd)) {
            numFds++;
        }
        hostHandleRefCountFd = fd;
        numInts = CB_HANDLE_NUM_INTS(numFds);
    }

    bool canBePosted() const {
        return (0 != (usage & GRALLOC_USAGE_HW_FB));
    }

    bool isValid() const {
        return (version == sizeof(native_handle)) && (magic == CB_HANDLE_MAGIC_OLD);
    }

    static cb_handle_old_t* from(void* p) {
        if (!p) { return NULL; }
        cb_handle_old_t* cb = static_cast<cb_handle_old_t*>(p);
        return cb->isValid() ? cb : NULL;
    }

    static const cb_handle_old_t* from(const void* p) {
        return from(const_cast<void*>(p));
    }

    static cb_handle_old_t* from_unconst(const void* p) {
        return from(const_cast<void*>(p));
    }

    int32_t ashmemBasePid;      // process id which mapped the ashmem region
    int32_t mappedPid;          // process id which succeeded gralloc_register call
};

int32_t* getOpenCountPtr(const cb_handle_old_t* cb) {
    return ((int32_t*)cb->getBufferPtr()) + 1;
}

uint32_t getAshmemColorOffset(cb_handle_old_t* cb) {
    uint32_t res = 0;
    if (cb->canBePosted()) res = GOLDFISH_OFFSET_UNIT;
    if (isHidlGralloc) res = GOLDFISH_OFFSET_UNIT * 2;
    return res;
}

//
// our private gralloc module structure
//
struct private_module_t {
    gralloc_module_t base;
};

/* If not NULL, this is a pointer to the fallback module.
 * This really is gralloc.default, which we'll use if we detect
 * that the emulator we're running in does not support GPU emulation.
 */
static gralloc_module_t*  sFallback;
static pthread_once_t     sFallbackOnce = PTHREAD_ONCE_INIT;

static void fallback_init(void);  // forward

//
// Our gralloc device structure (alloc interface)
//
struct gralloc_device_t {
    alloc_device_t  device;
    std::set<buffer_handle_t> allocated;
    pthread_mutex_t lock;
};

struct gralloc_memregions_t {
    typedef std::map<void*, uint32_t> MemRegionMap;  // base -> refCount
    typedef MemRegionMap::const_iterator mem_region_handle_t;

    gralloc_memregions_t() {
        pthread_mutex_init(&lock, NULL);
    }

    MemRegionMap ashmemRegions;
    pthread_mutex_t lock;
};

#define INITIAL_DMA_REGION_SIZE 4096
struct gralloc_dmaregion_t {
    gralloc_dmaregion_t(ExtendedRCEncoderContext *rcEnc)
      : host_memory_allocator(
            rcEnc->featureInfo_const()->hasSharedSlotsHostMemoryAllocator),
        sz(INITIAL_DMA_REGION_SIZE),
        refcount(0),
        bigbufCount(0) {
        memset(&goldfish_dma, 0, sizeof(goldfish_dma));
        pthread_mutex_init(&lock, NULL);

        if (rcEnc->hasDirectMem()) {
            host_memory_allocator.hostMalloc(&address_space_block, sz);
        } else if (rcEnc->getDmaVersion() > 0) {
            goldfish_dma_create_region(sz, &goldfish_dma);
        }
    }

    goldfish_dma_context goldfish_dma;
    GoldfishAddressSpaceHostMemoryAllocator host_memory_allocator;
    GoldfishAddressSpaceBlock address_space_block;
    uint32_t sz;
    uint32_t refcount;
    pthread_mutex_t lock;
    uint32_t bigbufCount;
};

// global device instance
static gralloc_memregions_t* s_memregions = NULL;
static gralloc_dmaregion_t* s_grdma = NULL;

static gralloc_memregions_t* init_gralloc_memregions() {
    if (!s_memregions) {
        s_memregions = new gralloc_memregions_t;
    }
    return s_memregions;
}

static bool has_DMA_support(const ExtendedRCEncoderContext *rcEnc) {
    return rcEnc->getDmaVersion() > 0 || rcEnc->hasDirectMem();
}

static gralloc_dmaregion_t* init_gralloc_dmaregion(ExtendedRCEncoderContext *rcEnc) {
    D("%s: call\n", __func__);
    if (!s_grdma) {
        s_grdma = new gralloc_dmaregion_t(rcEnc);
    }
    return s_grdma;
}

static void get_gralloc_region(ExtendedRCEncoderContext *rcEnc) {
    gralloc_dmaregion_t* grdma = init_gralloc_dmaregion(rcEnc);

    pthread_mutex_lock(&grdma->lock);
    grdma->refcount++;
    D("%s: call. refcount: %u\n", __func__, grdma->refcount);
    pthread_mutex_unlock(&grdma->lock);
}

static void resize_gralloc_dmaregion_locked(gralloc_dmaregion_t* grdma, uint32_t new_sz) {
    if (grdma->goldfish_dma.mapped_addr) {
        goldfish_dma_unmap(&grdma->goldfish_dma);
    }
    close(grdma->goldfish_dma.fd);
    goldfish_dma_create_region(new_sz, &grdma->goldfish_dma);
    grdma->sz = new_sz;
}

// max dma size: 2x 4K rgba8888
#define MAX_DMA_SIZE 66355200

static bool put_gralloc_region_direct_mem_locked(gralloc_dmaregion_t* grdma, uint32_t /* sz, unused */) {
    const bool shouldDelete = !grdma->refcount;
    if (shouldDelete) {
        grdma->host_memory_allocator.hostFree(&grdma->address_space_block);
    }

    return shouldDelete;
}

static bool put_gralloc_region_dma_locked(gralloc_dmaregion_t* grdma, uint32_t sz) {
    D("%s: call. refcount before: %u\n", __func__, grdma->refcount);
    grdma->refcount--;
    if (sz > MAX_DMA_SIZE && grdma->bigbufCount) {
        grdma->bigbufCount--;
    }
    bool shouldDelete = !grdma->refcount;
    if (shouldDelete) {
        D("%s: should delete!\n", __func__);
        resize_gralloc_dmaregion_locked(grdma, INITIAL_DMA_REGION_SIZE);
        D("%s: done\n", __func__);
    }
    D("%s: exit\n", __func__);
    return shouldDelete;
}

static bool put_gralloc_region(ExtendedRCEncoderContext *rcEnc, uint32_t sz) {
    bool shouldDelete;

    gralloc_dmaregion_t* grdma = init_gralloc_dmaregion(rcEnc);
    pthread_mutex_lock(&grdma->lock);
    if (rcEnc->hasDirectMem()) {
        shouldDelete = put_gralloc_region_direct_mem_locked(grdma, sz);
    } else if (rcEnc->getDmaVersion() > 0) {
        shouldDelete = put_gralloc_region_dma_locked(grdma, sz);
    } else {
        shouldDelete = false;
    }
    pthread_mutex_unlock(&grdma->lock);

    return shouldDelete;
}

static void gralloc_dmaregion_register_ashmem_direct_mem_locked(gralloc_dmaregion_t* grdma, uint32_t new_sz) {
    if (new_sz == grdma->sz) return;

    GoldfishAddressSpaceHostMemoryAllocator* allocator = &grdma->host_memory_allocator;
    GoldfishAddressSpaceBlock* block = &grdma->address_space_block;
    allocator->hostFree(block);
    allocator->hostMalloc(block, new_sz);
    grdma->sz = new_sz;
}

static void gralloc_dmaregion_register_ashmem_dma_locked(gralloc_dmaregion_t* grdma, uint32_t new_sz) {
    if (new_sz != grdma->sz) {
        if (new_sz > MAX_DMA_SIZE)  {
            D("%s: requested sz %u too large (limit %u), set to fallback.",
              __func__, new_sz, MAX_DMA_SIZE);
            grdma->bigbufCount++;
        } else {
            D("%s: change sz from %u to %u", __func__, grdma->sz, new_sz);
            resize_gralloc_dmaregion_locked(grdma, new_sz);
        }
    }
    if (!grdma->goldfish_dma.mapped_addr) {
        goldfish_dma_map(&grdma->goldfish_dma);
    }
}

static void gralloc_dmaregion_register_ashmem(ExtendedRCEncoderContext *rcEnc, uint32_t sz) {
    gralloc_dmaregion_t* grdma = init_gralloc_dmaregion(rcEnc);

    pthread_mutex_lock(&grdma->lock);
    D("%s: for sz %u, refcount %u", __func__, sz, grdma->refcount);
    const uint32_t new_sz = std::max(grdma->sz, sz);

    if (rcEnc->hasDirectMem()) {
        gralloc_dmaregion_register_ashmem_direct_mem_locked(grdma, new_sz);
    } else if (rcEnc->getDmaVersion() > 0) {
        gralloc_dmaregion_register_ashmem_dma_locked(grdma, new_sz);
    } else {
        ALOGE("%s: unexpected DMA type", __func__);
    }

    pthread_mutex_unlock(&grdma->lock);
}

static void get_mem_region(void* ashmemBase) {
    D("%s: call for %p", __func__, ashmemBase);

    gralloc_memregions_t* memregions = init_gralloc_memregions();

    pthread_mutex_lock(&memregions->lock);
    ++memregions->ashmemRegions[ashmemBase];
    pthread_mutex_unlock(&memregions->lock);
}

static bool put_mem_region(ExtendedRCEncoderContext *, void* ashmemBase) {
    D("%s: call for %p", __func__, ashmemBase);

    gralloc_memregions_t* memregions = init_gralloc_memregions();
    bool shouldRemove;

    pthread_mutex_lock(&memregions->lock);
    gralloc_memregions_t::MemRegionMap::iterator i = memregions->ashmemRegions.find(ashmemBase);
    if (i == memregions->ashmemRegions.end()) {
        shouldRemove = true;
        ALOGE("%s: error: tried to put a nonexistent mem region (%p)!", __func__, ashmemBase);
    } else {
        shouldRemove = --i->second == 0;
        if (shouldRemove) {
            memregions->ashmemRegions.erase(i);
        }
    }
    pthread_mutex_unlock(&memregions->lock);

    return shouldRemove;
}

#if DEBUG
static void dump_regions(ExtendedRCEncoderContext *) {
    gralloc_memregions_t* memregions = init_gralloc_memregions();
    gralloc_memregions_t::mem_region_handle_t curr = memregions->ashmemRegions.begin();
    std::stringstream res;
    for (; curr != memregions->ashmemRegions.end(); ++curr) {
        res << "\tashmem base " << curr->first << " refcount " << curr->second << "\n";
    }
    ALOGD("ashmem region dump [\n%s]", res.str().c_str());
}
#endif

static void get_ashmem_region(ExtendedRCEncoderContext *rcEnc, cb_handle_old_t *cb) {
#if DEBUG
    dump_regions(rcEnc);
#endif

    get_mem_region(cb->getBufferPtr());

#if DEBUG
    dump_regions(rcEnc);
#endif

    get_gralloc_region(rcEnc);
}

static bool put_ashmem_region(ExtendedRCEncoderContext *rcEnc, cb_handle_old_t *cb) {
#if DEBUG
    dump_regions(rcEnc);
#endif

    const bool should_unmap = put_mem_region(rcEnc, cb->getBufferPtr());

#if DEBUG
    dump_regions(rcEnc);
#endif

    put_gralloc_region(rcEnc, cb->bufferSize);

    return should_unmap;
}

static int map_buffer(cb_handle_old_t *cb, void **vaddr)
{
    if (cb->bufferFd < 0) {
        return -EINVAL;
    }

    void *addr = mmap(0, cb->bufferSize, PROT_READ | PROT_WRITE,
                      MAP_SHARED, cb->bufferFd, 0);
    if (addr == MAP_FAILED) {
        ALOGE("%s: failed to map ashmem region!", __FUNCTION__);
        return -errno;
    }

    cb->setBufferPtr(addr);
    cb->ashmemBasePid = getpid();
    D("%s: %p mapped ashmem base %p size %d\n", __FUNCTION__,
      cb, addr, cb->bufferSize);

    *vaddr = addr;
    return 0;
}

static HostConnection* sHostCon = NULL;

static HostConnection* createOrGetHostConnection() {
    if (!sHostCon) {
        sHostCon = HostConnection::createUnique();
    }
    return sHostCon;
}

#define DEFINE_HOST_CONNECTION \
    HostConnection *hostCon = createOrGetHostConnection(); \
    ExtendedRCEncoderContext *rcEnc = (hostCon ? hostCon->rcEncoder() : NULL); \
    bool hasVulkan = rcEnc->featureInfo_const()->hasVulkan; (void)hasVulkan; \

#define DEFINE_AND_VALIDATE_HOST_CONNECTION \
    HostConnection *hostCon = createOrGetHostConnection(); \
    if (!hostCon) { \
        ALOGE("gralloc: Failed to get host connection\n"); \
        return -EIO; \
    } \
    ExtendedRCEncoderContext *rcEnc = hostCon->rcEncoder(); \
    if (!rcEnc) { \
        ALOGE("gralloc: Failed to get renderControl encoder context\n"); \
        return -EIO; \
    } \
    bool hasVulkan = rcEnc->featureInfo_const()->hasVulkan; (void)hasVulkan;\

#if PLATFORM_SDK_VERSION < 18
// On older APIs, just define it as a value no one is going to use.
#define HAL_PIXEL_FORMAT_YCbCr_420_888 0xFFFFFFFF
#endif

static void updateHostColorBuffer(cb_handle_old_t* cb,
                              bool doLocked,
                              char* pixels) {
    D("%s: call. doLocked=%d", __FUNCTION__, doLocked);

    DEFINE_HOST_CONNECTION;
    gralloc_dmaregion_t* grdma = init_gralloc_dmaregion(rcEnc);

    int bpp = glUtilsPixelBitSize(cb->glFormat, cb->glType) >> 3;
    int left = doLocked ? cb->lockedLeft : 0;
    int top = doLocked ? cb->lockedTop : 0;
    int width = doLocked ? cb->lockedWidth : cb->width;
    int height = doLocked ? cb->lockedHeight : cb->height;

    char* to_send = pixels;
    uint32_t rgbSz = width * height * bpp;
    uint32_t send_buffer_size = rgbSz;
    bool is_rgb_format =
        cb->format != HAL_PIXEL_FORMAT_YV12 &&
        cb->format != HAL_PIXEL_FORMAT_YCbCr_420_888;

    std::vector<char> convertedBuf;

    if (doLocked && is_rgb_format) {
        convertedBuf.resize(rgbSz);
        to_send = &convertedBuf.front();
        copy_rgb_buffer_from_unlocked(
                to_send, pixels,
                cb->width,
                width, height, top, left, bpp);
    }

    const bool hasDMA = has_DMA_support(rcEnc);
    if (hasDMA && grdma->bigbufCount) {
        D("%s: there are big buffers alive, use fallback (count %u)", __FUNCTION__,
          grdma->bigbufCount);
    }

    if (hasDMA && !grdma->bigbufCount) {
        switch (cb->format) {
        case HAL_PIXEL_FORMAT_YV12:
            get_yv12_offsets(width, height, NULL, NULL, &send_buffer_size);
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            get_yuv420p_offsets(width, height, NULL, NULL, &send_buffer_size);
            break;
        }

        if (grdma->address_space_block.guestPtr()) {
            rcEnc->bindDmaDirectly(grdma->address_space_block.guestPtr(),
                                   grdma->address_space_block.physAddr());
        } else if (grdma->goldfish_dma.mapped_addr) {
            rcEnc->bindDmaContext(&grdma->goldfish_dma);
        } else {
            ALOGE("%s: Unexpected DMA", __func__);
        }

        D("%s: call. dma update with sz=%u", __func__, send_buffer_size);
        pthread_mutex_lock(&grdma->lock);
        rcEnc->rcUpdateColorBufferDMA(rcEnc, cb->hostHandle,
                left, top, width, height,
                cb->glFormat, cb->glType,
                to_send, send_buffer_size);
        pthread_mutex_unlock(&grdma->lock);
    } else {
        switch (cb->format) {
        case HAL_PIXEL_FORMAT_YV12:
            convertedBuf.resize(rgbSz);
            to_send = &convertedBuf.front();
            D("convert yv12 to rgb888 here");
            yv12_to_rgb888(to_send, pixels,
                           width, height, left, top,
                           left + width - 1, top + height - 1);
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            convertedBuf.resize(rgbSz);
            to_send = &convertedBuf.front();
            yuv420p_to_rgb888(to_send, pixels,
                              width, height, left, top,
                              left + width - 1, top + height - 1);
            break;
        }

        rcEnc->rcUpdateColorBuffer(rcEnc, cb->hostHandle,
                left, top, width, height,
                cb->glFormat, cb->glType, to_send);
    }
}

//
// gralloc device functions (alloc interface)
//
static void gralloc_dump(struct alloc_device_t* /*dev*/, char* /*buff*/, int /*buff_len*/) {}

static int gralloc_get_buffer_format(const int frameworkFormat, const int usage) {
    // Pick the right concrete pixel format given the endpoints as encoded in
    // the usage bits.  Every end-point pair needs explicit listing here.
#if PLATFORM_SDK_VERSION >= 17
    if (frameworkFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
        // Camera as producer
        if (usage & GRALLOC_USAGE_HW_CAMERA_WRITE) {
            if (usage & GRALLOC_USAGE_HW_TEXTURE) {
                // Camera-to-display is RGBA
                return HAL_PIXEL_FORMAT_RGBA_8888;
            } else if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
                // Camera-to-encoder is NV21
                return HAL_PIXEL_FORMAT_YCrCb_420_SP;
            }
        }

        ALOGE("gralloc_alloc: Requested auto format selection, "
              "but no known format for this usage=%x", usage);
        return -EINVAL;
    } else if (frameworkFormat == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        ALOGW("gralloc_alloc: Requested YCbCr_420_888, taking experimental path. "
              "usage=%x", usage);
    } else if (frameworkFormat == OMX_COLOR_FormatYUV420Planar &&
               (usage & GOLDFISH_GRALLOC_USAGE_GPU_DATA_BUFFER)) {
        ALOGW("gralloc_alloc: Requested OMX_COLOR_FormatYUV420Planar, given "
              "YCbCr_420_888, taking experimental path. "
              "usage=%x", usage);
        return HAL_PIXEL_FORMAT_YCbCr_420_888;
    }
#endif // PLATFORM_SDK_VERSION >= 17

    return frameworkFormat;
}

static int gralloc_alloc(alloc_device_t* dev,
                         int w, int h, const int frameworkFormat, int usage,
                         buffer_handle_t* pHandle, int* pStride)
{
    D("gralloc_alloc w=%d h=%d usage=0x%x frameworkFormat=0x%x\n", w, h, usage, frameworkFormat);

    gralloc_device_t *grdev = (gralloc_device_t *)dev;
    if (!grdev || !pHandle || !pStride) {
        ALOGE("gralloc_alloc: Bad inputs (grdev: %p, pHandle: %p, pStride: %p",
                grdev, pHandle, pStride);
        return -EINVAL;
    }

    const int format = gralloc_get_buffer_format(frameworkFormat, usage);
    if (format < 0) {
        return format;
    }

    //
    // Note: in screen capture mode, both sw_write and hw_write will be on
    // and this is a valid usage
    //
    bool sw_write = (0 != (usage & GRALLOC_USAGE_SW_WRITE_MASK));
    bool hw_write = (usage & GRALLOC_USAGE_HW_RENDER); (void)hw_write;
    bool sw_read = (0 != (usage & GRALLOC_USAGE_SW_READ_MASK));
    const bool hw_texture = usage & GRALLOC_USAGE_HW_TEXTURE;
    const bool hw_render = usage & GRALLOC_USAGE_HW_RENDER;
    const bool hw_2d = usage & GRALLOC_USAGE_HW_2D;
    const bool hw_composer = usage & GRALLOC_USAGE_HW_COMPOSER;
    const bool hw_fb = usage & GRALLOC_USAGE_HW_FB;
    const bool rgb888_unsupported_usage =
        hw_texture || hw_render || hw_2d || hw_composer || hw_fb;
#if PLATFORM_SDK_VERSION >= 17
    bool hw_cam_write = (usage & GRALLOC_USAGE_HW_CAMERA_WRITE);
    bool hw_cam_read = (usage & GRALLOC_USAGE_HW_CAMERA_READ);
#else // PLATFORM_SDK_VERSION
    bool hw_cam_write = false;
    bool hw_cam_read = false;
#endif // PLATFORM_SDK_VERSION
#if PLATFORM_SDK_VERSION >= 15
    bool hw_vid_enc_read = usage & GRALLOC_USAGE_HW_VIDEO_ENCODER;
#else // PLATFORM_SDK_VERSION
    bool hw_vid_enc_read = false;
#endif // PLATFORM_SDK_VERSION

    bool yuv_format = false;
    bool raw_format = false;
    int ashmem_size = 0;
    int stride = w;

    GLenum glFormat = 0;
    GLenum glType = 0;
    EmulatorFrameworkFormat selectedEmuFrameworkFormat = FRAMEWORK_FORMAT_GL_COMPATIBLE;

    int bpp = 0;
    int align = 1;
    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bpp = 4;
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            if (rgb888_unsupported_usage) {
                return -EINVAL;  // we dont support RGB_888 for HW usage
            } else {
                bpp = 3;
                glFormat = GL_RGB;
                glType = GL_UNSIGNED_BYTE;
                break;
            }
        case HAL_PIXEL_FORMAT_RGB_565:
            bpp = 2;
            // Workaround: distinguish vs the RGB8/RGBA8
            // by changing |glFormat| to GL_RGB565
            // (previously, it was still GL_RGB)
            glFormat = GL_RGB565;
            glType = GL_UNSIGNED_SHORT_5_6_5;
            break;
#if PLATFORM_SDK_VERSION >= 26
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
#endif // PLATFORM_SDK_VERSION >= 26
#if PLATFORM_SDK_VERSION >= 21
        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_Y16:
#elif PLATFORM_SDK_VERSION >= 16
        case HAL_PIXEL_FORMAT_RAW_SENSOR:
#endif
            bpp = 2;
            align = 16*bpp;
            if (! ((sw_read || hw_cam_read) && (sw_write || hw_cam_write) ) ) {
                // Raw sensor data or Y16 only goes between camera and CPU
                return -EINVAL;
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_SHORT;
            raw_format = true;
            break;
#if PLATFORM_SDK_VERSION >= 17
        case HAL_PIXEL_FORMAT_BLOB:
            bpp = 1;
            if (! (sw_read) ) {
                // Blob data cannot be used by HW other than camera emulator
                // But there is a CTS test trying to have access to it
                // BUG: https://buganizer.corp.google.com/issues/37719518
                return -EINVAL;
            }
            // Not expecting to actually create any GL surfaces for this
            glFormat = GL_LUMINANCE;
            glType = GL_UNSIGNED_BYTE;
            break;
#endif // PLATFORM_SDK_VERSION >= 17
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            align = 1;
            bpp = 1; // per-channel bpp
            yuv_format = true;
            // Not expecting to actually create any GL surfaces for this
            break;
        case HAL_PIXEL_FORMAT_YV12:
            align = 16;
            bpp = 1; // per-channel bpp
            yuv_format = true;
            // We are going to use RGB8888 on the host for Vulkan
            glFormat = GL_RGBA;
            glType = GL_UNSIGNED_BYTE;
            selectedEmuFrameworkFormat = FRAMEWORK_FORMAT_YV12;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            align = 1;
            bpp = 1; // per-channel bpp
            yuv_format = true;
            // We are going to use RGB888 on the host
            glFormat = GL_RGB;
            glType = GL_UNSIGNED_BYTE;
            selectedEmuFrameworkFormat = FRAMEWORK_FORMAT_YUV_420_888;
            break;
        default:
            ALOGE("gralloc_alloc: Unknown format %d", format);
            return -EINVAL;
    }

    //
    // Allocate ColorBuffer handle on the host (only if h/w access is allowed)
    // Only do this for some h/w usages, not all.
    // Also do this if we need to read from the surface, in this case the
    // rendering will still happen on the host but we also need to be able to
    // read back from the color buffer, which requires that there is a buffer
    //
    DEFINE_AND_VALIDATE_HOST_CONNECTION;
#if PLATFORM_SDK_VERSION >= 17
    bool needHostCb = ((!yuv_format && frameworkFormat != HAL_PIXEL_FORMAT_BLOB) ||
                      usage & GOLDFISH_GRALLOC_USAGE_GPU_DATA_BUFFER ||
#else
    bool needHostCb = (!yuv_format ||
#endif // !(PLATFORM_SDK_VERSION >= 17)
                       frameworkFormat == HAL_PIXEL_FORMAT_YV12 ||
                       frameworkFormat == HAL_PIXEL_FORMAT_YCbCr_420_888) &&
                       !raw_format &&
#if PLATFORM_SDK_VERSION >= 15
                      (usage & (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER |
                                GRALLOC_USAGE_HW_2D | GRALLOC_USAGE_HW_COMPOSER |
                                GRALLOC_USAGE_HW_VIDEO_ENCODER |
                                GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_SW_READ_MASK))
#else // PLATFORM_SDK_VERSION
                      (usage & (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER |
                                GRALLOC_USAGE_HW_2D |
                                GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_SW_READ_MASK))
#endif // PLATFORM_SDK_VERSION
                      ;

    if (isHidlGralloc) {
        if (needHostCb || (usage & GRALLOC_USAGE_HW_FB)) {
            // keep space for postCounter
            // AND openCounter for all host cb
            ashmem_size += GOLDFISH_OFFSET_UNIT * 2;
        }
    } else {
        if (usage & GRALLOC_USAGE_HW_FB) {
            // keep space for postCounter
            ashmem_size += GOLDFISH_OFFSET_UNIT * 1;
        }
    }

    // API26 always expect at least one file descriptor is associated with
    // one color buffer
    // BUG: 37719038
    if (PLATFORM_SDK_VERSION >= 26 ||
        sw_read || sw_write || hw_cam_write || hw_vid_enc_read) {
        // keep space for image on guest memory if SW access is needed
        // or if the camera is doing writing
        if (yuv_format) {
            size_t yStride = (w*bpp + (align - 1)) & ~(align-1);
            size_t uvStride = (yStride / 2 + (align - 1)) & ~(align-1);
            size_t uvHeight = h / 2;
            ashmem_size += yStride * h + 2 * (uvHeight * uvStride);
            stride = yStride / bpp;
        } else {
            size_t bpr = (w*bpp + (align-1)) & ~(align-1);
            ashmem_size += (bpr * h);
            stride = bpr / bpp;
        }
    }

    D("gralloc_alloc format=%d, ashmem_size=%d, stride=%d, tid %d\n", format,
      ashmem_size, stride, getCurrentThreadId());

    //
    // Allocate space in ashmem if needed
    //
    int fd = -1;
    if (ashmem_size > 0) {
        // round to page size;
        ashmem_size = (ashmem_size + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

        ALOGD("%s: Creating ashmem region of size %d\n", __FUNCTION__, ashmem_size);
        fd = ashmem_create_region("gralloc-buffer", ashmem_size);
        if (fd < 0) {
            ALOGE("gralloc_alloc failed to create ashmem region: %s\n",
                    strerror(errno));
            return -errno;
        }
    }

    cb_handle_old_t *cb = new cb_handle_old_t(fd, ashmem_size, usage,
                                              w, h, format,
                                              glFormat, glType);

    if (ashmem_size > 0) {
        //
        // map ashmem region if exist
        //
        void *vaddr;
        int err = map_buffer(cb, &vaddr);
        if (err) {
            close(fd);
            delete cb;
            return err;
        }
    }

    const bool hasDMA = has_DMA_support(rcEnc);

    if (needHostCb) {
        if (hostCon && rcEnc) {
            GLenum allocFormat = glFormat;
            // The handling of RGBX_8888 is very subtle. Most of the time
            // we want it to be treated as RGBA_8888, with the exception
            // that alpha is always ignored and treated as 1. The solution
            // is to create 3 channel RGB texture instead and host GL will
            // handle the Alpha channel.
            if (HAL_PIXEL_FORMAT_RGBX_8888 == format) {
                allocFormat = GL_RGB;
            }

            hostCon->lock();
            if (hasDMA) {
                cb->hostHandle = rcEnc->rcCreateColorBufferDMA(rcEnc, w, h, allocFormat, selectedEmuFrameworkFormat);
            } else {
                cb->hostHandle = rcEnc->rcCreateColorBuffer(rcEnc, w, h, allocFormat);
            }
            hostCon->unlock();
        }

        if (!cb->hostHandle) {
            // Could not create colorbuffer on host !!!
            close(fd);
            delete cb;
            ALOGE("%s: failed to create host cb! -EIO", __FUNCTION__);
            return -EIO;
        } else {
            QEMU_PIPE_HANDLE refcountPipeFd = qemu_pipe_open("refcount");
            if(qemu_pipe_valid(refcountPipeFd)) {
                cb->setRefcountPipeFd(refcountPipeFd);
                qemu_pipe_write(refcountPipeFd, &cb->hostHandle, 4);
            }
            D("Created host ColorBuffer 0x%x\n", cb->hostHandle);
        }

        if (isHidlGralloc) { *getOpenCountPtr(cb) = 0; }
    }

    //
    // alloc succeeded - insert the allocated handle to the allocated list
    //
    pthread_mutex_lock(&grdev->lock);
    grdev->allocated.insert(cb);
    pthread_mutex_unlock(&grdev->lock);

    *pHandle = cb;
    D("%s: alloc succeded, new ashmem base and size: %p %d handle: %p",
      __FUNCTION__, cb->ashmemBase, cb->ashmemSize, cb);
    switch (frameworkFormat) {
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
        *pStride = 0;
        break;
    default:
        *pStride = stride;
        break;
    }

    hostCon->lock();
    if (hasDMA) {
        get_gralloc_region(rcEnc);  // map_buffer(cb, ...) refers here
    }
    hostCon->unlock();

    return 0;
}

static int gralloc_free(alloc_device_t* dev,
                        buffer_handle_t handle)
{
    DEFINE_AND_VALIDATE_HOST_CONNECTION;

    const cb_handle_old_t *cb = cb_handle_old_t::from(handle);
    if (!cb) {
        ERR("gralloc_free: invalid handle %p", handle);
        return -EINVAL;
    }

    D("%s: for buf %p ptr %p size %d\n",
      __FUNCTION__, handle, cb->getBufferPtr(), cb->bufferSize);

    if (cb->hostHandle && !cb->hasRefcountPipe()) {
        int32_t openCount = 1;
        int32_t* openCountPtr = &openCount;

        if (isHidlGralloc && cb->getBufferPtr()) {
            openCountPtr = getOpenCountPtr(cb);
        }

        if (*openCountPtr > 0) {
            D("Closing host ColorBuffer 0x%x\n", cb->hostHandle);
            hostCon->lock();
            rcEnc->rcCloseColorBuffer(rcEnc, cb->hostHandle);
            hostCon->unlock();
        } else {
            D("A rcCloseColorBuffer is owed!!! sdk ver: %d", PLATFORM_SDK_VERSION);
            *openCountPtr = -1;
        }
    }

    //
    // detach and unmap ashmem area if present
    //
    if (cb->bufferFd > 0) {
        if (cb->bufferSize > 0 && cb->getBufferPtr()) {
            D("%s: unmapped %p", __FUNCTION__, cb->getBufferPtr());
            munmap(cb->getBufferPtr(), cb->bufferSize);
            put_gralloc_region(rcEnc, cb->bufferSize);
        }
        close(cb->bufferFd);
    }

    if(qemu_pipe_valid(cb->hostHandleRefCountFd)) {
        qemu_pipe_close(cb->hostHandleRefCountFd);
    }
    D("%s: done", __FUNCTION__);
    // remove it from the allocated list
    gralloc_device_t *grdev = (gralloc_device_t *)dev;

    pthread_mutex_lock(&grdev->lock);
    grdev->allocated.erase(cb);
    pthread_mutex_unlock(&grdev->lock);

    delete cb;

    D("%s: exit", __FUNCTION__);
    return 0;
}

static int gralloc_device_close(struct hw_device_t *dev)
{
    gralloc_device_t* d = reinterpret_cast<gralloc_device_t*>(dev);
    if (d) {
        for (std::set<buffer_handle_t>::const_iterator i = d->allocated.begin();
             i != d->allocated.end(); ++i) {
            gralloc_free(&d->device, *i);
        }

        delete d;
    }
    return 0;
}

//
// gralloc module functions - refcount + locking interface
//
static int gralloc_register_buffer(gralloc_module_t const* module,
                                   buffer_handle_t handle)
{
    DEFINE_AND_VALIDATE_HOST_CONNECTION;

    D("%s: start", __FUNCTION__);
    pthread_once(&sFallbackOnce, fallback_init);
    if (sFallback != NULL) {
        return sFallback->registerBuffer(sFallback, handle);
    }

    private_module_t *gr = (private_module_t *)module;
    if (!gr) {
        return -EINVAL;
    }

    cb_handle_old_t *cb = cb_handle_old_t::from_unconst(handle);
    if (!cb) {
        ERR("gralloc_register_buffer(%p): invalid buffer", cb);
        return -EINVAL;
    }

    D("gralloc_register_buffer(%p) w %d h %d format 0x%x",
        handle, cb->width, cb->height, cb->format);

    if (cb->hostHandle != 0 && !cb->hasRefcountPipe()) {
        D("Opening host ColorBuffer 0x%x\n", cb->hostHandle);
        hostCon->lock();
        rcEnc->rcOpenColorBuffer2(rcEnc, cb->hostHandle);
        hostCon->unlock();
    }

    //
    // if the color buffer has ashmem region and it is not mapped in this
    // process map it now.
    //
    if (cb->bufferSize > 0 && cb->mappedPid != getpid()) {
        void *vaddr;
        int err = map_buffer(cb, &vaddr);
        if (err) {
            ERR("gralloc_register_buffer(%p): map failed: %s", cb, strerror(-err));
            return -err;
        }
        cb->mappedPid = getpid();

        if (isHidlGralloc) {
            int32_t* openCountPtr = getOpenCountPtr(cb);
            if (!*openCountPtr) *openCountPtr = 1;
        }
    }

    if (cb->bufferSize > 0) {
        get_ashmem_region(rcEnc, cb);
    }

    return 0;
}

static int gralloc_unregister_buffer(gralloc_module_t const* module,
                                     buffer_handle_t handle)
{
    DEFINE_AND_VALIDATE_HOST_CONNECTION;

    if (sFallback != NULL) {
        return sFallback->unregisterBuffer(sFallback, handle);
    }

    private_module_t *gr = (private_module_t *)module;
    if (!gr) {
        return -EINVAL;
    }

    cb_handle_old_t *cb = cb_handle_old_t::from_unconst(handle);
    if (!cb) {
        ERR("gralloc_unregister_buffer(%p): invalid buffer", cb);
        return -EINVAL;
    }


    if (cb->hostHandle && !cb->hasRefcountPipe()) {
        D("Closing host ColorBuffer 0x%x\n", cb->hostHandle);
        hostCon->lock();
        rcEnc->rcCloseColorBuffer(rcEnc, cb->hostHandle);

        if (isHidlGralloc) {
            // Queue up another rcCloseColorBuffer if applicable.
            // invariant: have ashmem.
            if (cb->bufferSize > 0 && cb->mappedPid == getpid()) {
                int32_t* openCountPtr = getOpenCountPtr(cb);
                if (*openCountPtr == -1) {
                    D("%s: revenge of the rcCloseColorBuffer!", __func__);
                    rcEnc->rcCloseColorBuffer(rcEnc, cb->hostHandle);
                    *openCountPtr = -2;
                }
            }
        }
        hostCon->unlock();
    }

    //
    // unmap ashmem region if it was previously mapped in this process
    // (through register_buffer)
    //
    if (cb->bufferSize > 0 && cb->mappedPid == getpid()) {
        const bool should_unmap = put_ashmem_region(rcEnc, cb);
        if (!should_unmap) goto done;

        int err = munmap(cb->getBufferPtr(), cb->bufferSize);
        if (err) {
            ERR("gralloc_unregister_buffer(%p): unmap failed", cb);
            return -EINVAL;
        }
        cb->bufferSize = 0;
        cb->mappedPid = 0;
        D("%s: Unregister buffer previous mapped to pid %d", __FUNCTION__, getpid());
    }

done:
    D("gralloc_unregister_buffer(%p) done\n", cb);
    return 0;
}

static int gralloc_lock(gralloc_module_t const* module,
                        buffer_handle_t handle, int usage,
                        int l, int t, int w, int h,
                        void** vaddr)
{
    if (sFallback != NULL) {
        return sFallback->lock(sFallback, handle, usage, l, t, w, h, vaddr);
    }

    private_module_t *gr = (private_module_t *)module;
    if (!gr) {
        return -EINVAL;
    }

    cb_handle_old_t *cb = cb_handle_old_t::from_unconst(handle);
    if (!cb) {
        ALOGE("gralloc_lock bad handle\n");
        return -EINVAL;
    }

    // Validate usage,
    //   1. cannot be locked for hw access
    //   2. lock for either sw read or write.
    //   3. locked sw access must match usage during alloc time.
    bool sw_read = (0 != (usage & GRALLOC_USAGE_SW_READ_MASK));
    bool sw_write = (0 != (usage & GRALLOC_USAGE_SW_WRITE_MASK));
    bool hw_read = (usage & GRALLOC_USAGE_HW_TEXTURE);
    bool hw_write = (usage & GRALLOC_USAGE_HW_RENDER);
#if PLATFORM_SDK_VERSION >= 17
    bool hw_cam_write = (usage & GRALLOC_USAGE_HW_CAMERA_WRITE);
    bool hw_cam_read = (usage & GRALLOC_USAGE_HW_CAMERA_READ);
#else // PLATFORM_SDK_VERSION
    bool hw_cam_write = false;
    bool hw_cam_read = false;
#endif // PLATFORM_SDK_VERSION

#if PLATFORM_SDK_VERSION >= 15
    bool hw_vid_enc_read = (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER);
#else // PLATFORM_SDK_VERSION
    bool hw_vid_enc_read = false;
#endif // PLATFORM_SDK_VERSION

    bool sw_read_allowed = (0 != (cb->usage & GRALLOC_USAGE_SW_READ_MASK));

#if PLATFORM_SDK_VERSION >= 15
    // bug: 30088791
    // a buffer was created for GRALLOC_USAGE_HW_VIDEO_ENCODER usage but
    // later a software encoder is reading this buffer: this is actually
    // legit usage.
    sw_read_allowed = sw_read_allowed || (cb->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER);
#endif // PLATFORM_SDK_VERSION >= 15

    bool sw_write_allowed = (0 != (cb->usage & GRALLOC_USAGE_SW_WRITE_MASK));

    if ( (hw_read || hw_write) ||
         (!sw_read && !sw_write &&
                 !hw_cam_write && !hw_cam_read &&
                 !hw_vid_enc_read) ||
         (sw_read && !sw_read_allowed) ||
         (sw_write && !sw_write_allowed) ) {
        ALOGE("gralloc_lock usage mismatch usage=0x%x cb->usage=0x%x\n", usage,
                cb->usage);
        //This is not exactly an error and loose it up.
        //bug: 30784436
        //return -EINVAL;
    }

    void *cpu_addr = NULL;

    //
    // make sure ashmem area is mapped if needed
    //
    if (cb->canBePosted() || sw_read || sw_write ||
            hw_cam_write || hw_cam_read ||
            hw_vid_enc_read) {
        if (cb->ashmemBasePid != getpid() || !cb->getBufferPtr()) {
            return -EACCES;
        }

        cpu_addr = (void *)((char*)cb->getBufferPtr() + getAshmemColorOffset(cb));
    }

    if (cb->hostHandle) {
        // Make sure we have host connection
        DEFINE_AND_VALIDATE_HOST_CONNECTION;
        hostCon->lock();

        //
        // flush color buffer write cache on host and get its sync status.
        //
        int hostSyncStatus = rcEnc->rcColorBufferCacheFlush(rcEnc, cb->hostHandle,
                                                            0,
                                                            sw_read);
        if (hostSyncStatus < 0) {
            // host failed the color buffer sync - probably since it was already
            // locked for write access. fail the lock.
            ALOGE("gralloc_lock cacheFlush failed sw_read=%d\n", sw_read);
            return -EBUSY;
        }

        // camera delivers bits to the buffer directly and does not require
        // an explicit read.
        if (sw_read & !(usage & GRALLOC_USAGE_HW_CAMERA_MASK)) {
            D("gralloc_lock read back color buffer %d %d ashmem base %p sz %d\n",
              cb->width, cb->height, cb->ashmemBase, cb->ashmemSize);
            void* rgb_addr = cpu_addr;
            char* tmpBuf = 0;
            if (cb->format == HAL_PIXEL_FORMAT_YV12 ||
                cb->format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
                if (rcEnc->hasYUVCache()) {
                    uint32_t buffer_size;
                    if (cb->format == HAL_PIXEL_FORMAT_YV12) {
                       get_yv12_offsets(cb->width, cb->height, NULL, NULL,
                                        &buffer_size);
                    } else {
                       get_yuv420p_offsets(cb->width, cb->height, NULL, NULL,
                                           &buffer_size);
                    }
                    D("read YUV copy from host");
                    rcEnc->rcReadColorBufferYUV(rcEnc, cb->hostHandle,
                                            0, 0, cb->width, cb->height,
                                            rgb_addr, buffer_size);
                } else {
                    // We are using RGB888
                    tmpBuf = new char[cb->width * cb->height * 3];
                    rcEnc->rcReadColorBuffer(rcEnc, cb->hostHandle,
                                              0, 0, cb->width, cb->height, cb->glFormat, cb->glType, tmpBuf);
                    if (cb->format == HAL_PIXEL_FORMAT_YV12) {
                        D("convert rgb888 to yv12 here");
                        rgb888_to_yv12((char*)cpu_addr, tmpBuf, cb->width, cb->height, l, t, l+w-1, t+h-1);
                    } else if (cb->format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
                        D("convert rgb888 to yuv420p here");
                        rgb888_to_yuv420p((char*)cpu_addr, tmpBuf, cb->width, cb->height, l, t, l+w-1, t+h-1);
                    }
                    delete [] tmpBuf;
                }
            } else {
                rcEnc->rcReadColorBuffer(rcEnc, cb->hostHandle,
                        0, 0, cb->width, cb->height, cb->glFormat, cb->glType, rgb_addr);
            }
        }

        if (has_DMA_support(rcEnc)) {
            gralloc_dmaregion_register_ashmem(rcEnc, cb->bufferSize);
        }
        hostCon->unlock();
    }

    //
    // is virtual address required ?
    //
    if (sw_read || sw_write || hw_cam_write || hw_cam_read || hw_vid_enc_read) {
        *vaddr = cpu_addr;
    }

    if (sw_write || hw_cam_write) {
        //
        // Keep locked region if locked for s/w write access.
        //
        cb->lockedLeft = l;
        cb->lockedTop = t;
        cb->lockedWidth = w;
        cb->lockedHeight = h;
    }

    DD("gralloc_lock success. vaddr: %p, *vaddr: %p, usage: %x, cpu_addr: %p, base: %p",
            vaddr, vaddr ? *vaddr : 0, usage, cpu_addr, cb->ashmemBase);

    return 0;
}

static int gralloc_unlock(gralloc_module_t const* module,
                          buffer_handle_t handle)
{
    if (sFallback != NULL) {
        return sFallback->unlock(sFallback, handle);
    }

    private_module_t *gr = (private_module_t *)module;
    if (!gr) {
        return -EINVAL;
    }

    cb_handle_old_t *cb = cb_handle_old_t::from_unconst(handle);
    if (!cb) {
        ALOGD("%s: invalid cb handle. -EINVAL", __FUNCTION__);
        return -EINVAL;
    }

    //
    // if buffer was locked for s/w write, we need to update the host with
    // the updated data
    //
    if (cb->hostHandle) {

        // Make sure we have host connection
        DEFINE_AND_VALIDATE_HOST_CONNECTION;
        hostCon->lock();

        char *cpu_addr = (char*)cb->getBufferPtr() + getAshmemColorOffset(cb);

        if (cb->lockedWidth < cb->width || cb->lockedHeight < cb->height) {
            updateHostColorBuffer(cb, true, cpu_addr);
        }
        else {
            updateHostColorBuffer(cb, false, cpu_addr);
        }

        hostCon->unlock();
        DD("gralloc_unlock success. cpu_addr: %p", cpu_addr);
    }

    cb->lockedWidth = cb->lockedHeight = 0;
    return 0;
}

#if PLATFORM_SDK_VERSION >= 18
static int gralloc_lock_ycbcr(gralloc_module_t const* module,
                              buffer_handle_t handle, int usage,
                              int l, int t, int w, int h,
                              android_ycbcr *ycbcr)
{
    // Not supporting fallback module for YCbCr
    if (sFallback != NULL) {
        ALOGD("%s: has fallback, return -EINVAL", __FUNCTION__);
        return -EINVAL;
    }

    if (!ycbcr) {
        ALOGE("%s: got NULL ycbcr struct! -EINVAL", __FUNCTION__);
        return -EINVAL;
    }

    private_module_t *gr = (private_module_t *)module;
    if (!gr) {
        return -EINVAL;
    }

    cb_handle_old_t *cb = cb_handle_old_t::from_unconst(handle);
    if (!cb) {
        ALOGE("%s: bad colorbuffer handle. -EINVAL", __FUNCTION__);
        return -EINVAL;
    }

    if (cb->format != HAL_PIXEL_FORMAT_YV12 &&
        cb->format != HAL_PIXEL_FORMAT_YCbCr_420_888) {
        ALOGE("gralloc_lock_ycbcr can only be used with "
                "HAL_PIXEL_FORMAT_YCbCr_420_888 or HAL_PIXEL_FORMAT_YV12, got %x instead. "
                "-EINVAL",
                cb->format);
        return -EINVAL;
    }

    usage |= (cb->usage & GRALLOC_USAGE_HW_CAMERA_MASK);

    void *vaddr;
    int ret = gralloc_lock(module, handle, usage, l, t, w, h, &vaddr);
    if (ret) {
        return ret;
    }

    uint8_t* cpu_addr = static_cast<uint8_t*>(vaddr);

    // Calculate offsets to underlying YUV data
    size_t yStride;
    size_t cStride;
    size_t cSize;
    size_t yOffset;
    size_t uOffset;
    size_t vOffset;
    size_t cStep;
    size_t align;
    switch (cb->format) {
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            yStride = cb->width;
            cStride = cb->width;
            yOffset = 0;
            vOffset = yStride * cb->height;
            uOffset = vOffset + 1;
            cStep = 2;
            break;
        case HAL_PIXEL_FORMAT_YV12:
            // https://developer.android.com/reference/android/graphics/ImageFormat.html#YV12
            align = 16;
            yStride = (cb->width + (align -1)) & ~(align-1);
            cStride = (yStride / 2 + (align - 1)) & ~(align-1);
            yOffset = 0;
            cSize = cStride * cb->height/2;
            vOffset = yStride * cb->height;
            uOffset = vOffset + cSize;
            cStep = 1;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            yStride = cb->width;
            cStride = yStride / 2;
            yOffset = 0;
            uOffset = cb->height * yStride;
            vOffset = uOffset + cStride * cb->height / 2;
            cStep = 1;
            break;
        default:
            ALOGE("gralloc_lock_ycbcr unexpected internal format %x",
                    cb->format);
            return -EINVAL;
    }

    ycbcr->y = cpu_addr + yOffset;
    ycbcr->cb = cpu_addr + uOffset;
    ycbcr->cr = cpu_addr + vOffset;
    ycbcr->ystride = yStride;
    ycbcr->cstride = cStride;
    ycbcr->chroma_step = cStep;

    // Zero out reserved fields
    memset(ycbcr->reserved, 0, sizeof(ycbcr->reserved));

    DD("gralloc_lock_ycbcr success. usage: %x, ycbcr.y: %p, .cb: %p, .cr: %p, "
           ".ystride: %d , .cstride: %d, .chroma_step: %d, base: %p", usage,
           ycbcr->y, ycbcr->cb, ycbcr->cr, ycbcr->ystride, ycbcr->cstride,
           ycbcr->chroma_step, cb->ashmemBase);

    return 0;
}
#endif // PLATFORM_SDK_VERSION >= 18

static int gralloc_device_open(const hw_module_t* module,
                               const char* name,
                               hw_device_t** device)
{
    int status = -EINVAL;

    D("gralloc_device_open %s\n", name);

    pthread_once( &sFallbackOnce, fallback_init );
    if (sFallback != NULL) {
        return sFallback->common.methods->open(&sFallback->common, name, device);
    }

    if (!strcmp(name, GRALLOC_HARDWARE_GPU0)) {

        // Create host connection and keep it in the TLS.
        // return error if connection with host can not be established
        HostConnection *hostConn = createOrGetHostConnection();
        if (!hostConn) {
            ALOGE("gralloc: failed to get host connection while opening %s\n", name);
            return -EIO;
        }

        //
        // Allocate memory for the gralloc device (alloc interface)
        //
        gralloc_device_t *dev = new gralloc_device_t;
        if (NULL == dev) {
            return -ENOMEM;
        }

        // Initialize our device structure
        //
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = gralloc_device_close;

        dev->device.alloc   = gralloc_alloc;
        dev->device.free    = gralloc_free;
        dev->device.dump = gralloc_dump;
        pthread_mutex_init(&dev->lock, NULL);

        *device = &dev->device.common;
        status = 0;
    }

    return status;
}

//
// define the HMI symbol - our module interface
//
static struct hw_module_methods_t gralloc_module_methods = {
    .open = gralloc_device_open,
};

struct private_module_t HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
#if PLATFORM_SDK_VERSION >= 18
            module_api_version: GRALLOC_MODULE_API_VERSION_0_2,
            hal_api_version: 0,
#elif PLATFORM_SDK_VERSION >= 16
            module_api_version: 1,
            hal_api_version: 0,
#else // PLATFORM_SDK_VERSION
            version_major: 1,
            version_minor: 0,
#endif // PLATFORM_SDK_VERSION
            id: GRALLOC_HARDWARE_MODULE_ID,
            name: "Graphics Memory Allocator Module",
            author: "The Android Open Source Project",
            methods: &gralloc_module_methods,
            dso: NULL,
            reserved: {0, }
        },
        registerBuffer: gralloc_register_buffer,
        unregisterBuffer: gralloc_unregister_buffer,
        lock: gralloc_lock,
        unlock: gralloc_unlock,
        perform: NULL,
#if PLATFORM_SDK_VERSION >= 18
        lock_ycbcr: gralloc_lock_ycbcr,
#endif // PLATFORM_SDK_VERSION >= 18
#if PLATFORM_SDK_VERSION >= 29 // For Q and later
        getTransportSize: NULL,
        validateBufferSize: NULL,
#endif // PLATFORM_SDK_VERSION >= 29
    }
};

/* This function is called once to detect whether the emulator supports
 * GPU emulation (this is done by looking at the qemu.gles kernel
 * parameter, which must be == 1 if this is the case).
 *
 * If not, then load gralloc.default instead as a fallback.
 */

#if __LP64__
static const char kGrallocDefaultSystemPath[] = "/system/lib64/hw/gralloc.goldfish.default.so";
static const char kGrallocDefaultVendorPath[] = "/vendor/lib64/hw/gralloc.goldfish.default.so";
static const char kGrallocDefaultSystemPathPreP[] = "/system/lib64/hw/gralloc.default.so";
static const char kGrallocDefaultVendorPathPreP[] = "/vendor/lib64/hw/gralloc.default.so";
#else
static const char kGrallocDefaultSystemPath[] = "/system/lib/hw/gralloc.goldfish.default.so";
static const char kGrallocDefaultVendorPath[] = "/vendor/lib/hw/gralloc.goldfish.default.so";
static const char kGrallocDefaultSystemPathPreP[] = "/system/lib/hw/gralloc.default.so";
static const char kGrallocDefaultVendorPathPreP[] = "/vendor/lib/hw/gralloc.default.so";
#endif

static void
fallback_init(void)
{
    char  prop[PROPERTY_VALUE_MAX];
    void* module;

    // cuttlefish case: no fallback (if we use sw rendering,
    // we are not using this lib anyway (would use minigbm))
    property_get("ro.boot.hardware", prop, "");

    bool isValid = prop[0] != '\0';

    if (isValid && !strcmp(prop, "cutf_cvm")) {
        return;
    }

    // qemu.gles=0 -> no GLES 2.x support (only 1.x through software).
    // qemu.gles=1 -> host-side GPU emulation through EmuGL
    // qemu.gles=2 -> guest-side GPU emulation.
    property_get("ro.kernel.qemu.gles", prop, "999");

    bool useFallback = false;
    switch (atoi(prop)) {
        case 0:
            useFallback = true;
            break;
        case 1:
            useFallback = false;
            break;
        case 2:
            useFallback = true;
            break;
        default:
            useFallback = false;
            break;
    }

    if (!useFallback) return;

    ALOGD("Emulator without host-side GPU emulation detected. "
          "Loading gralloc.default.so from %s...",
          kGrallocDefaultVendorPath);
    module = dlopen(kGrallocDefaultVendorPath, RTLD_LAZY | RTLD_LOCAL);
    if (!module) {
      module = dlopen(kGrallocDefaultVendorPathPreP, RTLD_LAZY | RTLD_LOCAL);
    }
    if (!module) {
        // vendor folder didn't work. try system
        ALOGD("gralloc.default.so not found in /vendor. Trying %s...",
              kGrallocDefaultSystemPath);
        module = dlopen(kGrallocDefaultSystemPath, RTLD_LAZY | RTLD_LOCAL);
        if (!module) {
          module = dlopen(kGrallocDefaultSystemPathPreP, RTLD_LAZY | RTLD_LOCAL);
        }
    }

    if (module != NULL) {
        sFallback = reinterpret_cast<gralloc_module_t*>(dlsym(module, HAL_MODULE_INFO_SYM_AS_STR));
        if (sFallback == NULL) {
            dlclose(module);
        }
    }
    if (sFallback == NULL) {
        ALOGE("FATAL: Could not find gralloc.default.so!");
    }
}
