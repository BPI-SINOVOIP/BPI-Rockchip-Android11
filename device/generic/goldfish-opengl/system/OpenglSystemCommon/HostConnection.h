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
#ifndef __COMMON_HOST_CONNECTION_H
#define __COMMON_HOST_CONNECTION_H

#include "EmulatorFeatureInfo.h"
#include "IOStream.h"
#include "renderControl_enc.h"
#include "ChecksumCalculator.h"
#include "goldfish_dma.h"

#include <cutils/native_handle.h>

#ifdef GOLDFISH_VULKAN
#include <mutex>
#else
#include <utils/threads.h>
#endif

#include <string>

class GLEncoder;
struct gl_client_context_t;
class GL2Encoder;
struct gl2_client_context_t;

namespace goldfish_vk {
class VkEncoder;
}

// ExtendedRCEncoderContext is an extended version of renderControl_encoder_context_t
// that will be used to track available emulator features.
class ExtendedRCEncoderContext : public renderControl_encoder_context_t {
public:
    ExtendedRCEncoderContext(IOStream *stream, ChecksumCalculator *checksumCalculator)
        : renderControl_encoder_context_t(stream, checksumCalculator),
          m_dmaCxt(NULL), m_dmaPtr(NULL), m_dmaPhysAddr(0) { }
    void setSyncImpl(SyncImpl syncImpl) { m_featureInfo.syncImpl = syncImpl; }
    void setDmaImpl(DmaImpl dmaImpl) { m_featureInfo.dmaImpl = dmaImpl; }
    void setHostComposition(HostComposition hostComposition) {
        m_featureInfo.hostComposition = hostComposition; }
    bool hasNativeSync() const { return m_featureInfo.syncImpl >= SYNC_IMPL_NATIVE_SYNC_V2; }
    bool hasNativeSyncV3() const { return m_featureInfo.syncImpl >= SYNC_IMPL_NATIVE_SYNC_V3; }
    bool hasNativeSyncV4() const { return m_featureInfo.syncImpl >= SYNC_IMPL_NATIVE_SYNC_V4; }
    bool hasHostCompositionV1() const {
        return m_featureInfo.hostComposition == HOST_COMPOSITION_V1; }
    bool hasHostCompositionV2() const {
        return m_featureInfo.hostComposition == HOST_COMPOSITION_V2; }
    bool hasYUVCache() const {
        return m_featureInfo.hasYUVCache; }
    bool hasAsyncUnmapBuffer() const {
        return m_featureInfo.hasAsyncUnmapBuffer; }
    DmaImpl getDmaVersion() const { return m_featureInfo.dmaImpl; }
    void bindDmaContext(struct goldfish_dma_context* cxt) { m_dmaCxt = cxt; }
    void bindDmaDirectly(void* dmaPtr, uint64_t dmaPhysAddr) {
        m_dmaPtr = dmaPtr;
        m_dmaPhysAddr = dmaPhysAddr;
    }
    virtual uint64_t lockAndWriteDma(void* data, uint32_t size) {
        if (m_dmaPtr && m_dmaPhysAddr) {
            memcpy(m_dmaPtr, data, size);
            return m_dmaPhysAddr;
        } else if (m_dmaCxt) {
            return writeGoldfishDma(data, size, m_dmaCxt);
        } else {
            ALOGE("%s: ERROR: No DMA context bound!", __func__);
            return 0;
        }
    }
    void setGLESMaxVersion(GLESMaxVersion ver) { m_featureInfo.glesMaxVersion = ver; }
    GLESMaxVersion getGLESMaxVersion() const { return m_featureInfo.glesMaxVersion; }
    bool hasDirectMem() const {
#ifdef HOST_BUILD
        // unit tests do not support restoring "guest" ram because there is no VM
        return false;
#else
        return m_featureInfo.hasDirectMem;
#endif
    }

    const EmulatorFeatureInfo* featureInfo_const() const { return &m_featureInfo; }
    EmulatorFeatureInfo* featureInfo() { return &m_featureInfo; }
private:
    static uint64_t writeGoldfishDma(void* data, uint32_t size,
                                     struct goldfish_dma_context* dmaCxt) {
        ALOGV("%s(data=%p, size=%u): call", __func__, data, size);

        goldfish_dma_write(dmaCxt, data, size);
        uint64_t paddr = goldfish_dma_guest_paddr(dmaCxt);

        ALOGV("%s: paddr=0x%llx", __func__, (unsigned long long)paddr);
        return paddr;
    }

    EmulatorFeatureInfo m_featureInfo;
    struct goldfish_dma_context* m_dmaCxt;
    void* m_dmaPtr;
    uint64_t m_dmaPhysAddr;
};

// Abstraction for gralloc handle conversion
class Gralloc {
public:
    virtual uint32_t createColorBuffer(
        ExtendedRCEncoderContext* rcEnc, int width, int height, uint32_t glformat);
    virtual uint32_t getHostHandle(native_handle_t const* handle) = 0;
    virtual int getFormat(native_handle_t const* handle) = 0;
    virtual size_t getAllocatedSize(native_handle_t const* handle) = 0;
    virtual ~Gralloc() {}
};

// Abstraction for process pipe helper
class ProcessPipe {
public:
    virtual bool processPipeInit(HostConnectionType connType, renderControl_encoder_context_t *rcEnc) = 0;
    virtual ~ProcessPipe() {}
};

struct EGLThreadInfo;


class HostConnection
{
public:
    static HostConnection *get();
    static HostConnection *getWithThreadInfo(EGLThreadInfo* tInfo);
    static void exit();

    static HostConnection *createUnique();
    static void teardownUnique(HostConnection* con);

    ~HostConnection();

    HostConnectionType connectionType() const {
        return m_connectionType;
    }

    GLEncoder *glEncoder();
    GL2Encoder *gl2Encoder();
    goldfish_vk::VkEncoder *vkEncoder();
    ExtendedRCEncoderContext *rcEncoder();
    ChecksumCalculator *checksumHelper() { return &m_checksumHelper; }
    Gralloc *grallocHelper() { return m_grallocHelper; }

    void flush() {
        if (m_stream) {
            m_stream->flush();
        }
    }

    void setGrallocOnly(bool gralloc_only) {
        m_grallocOnly = gralloc_only;
    }

    bool isGrallocOnly() const { return m_grallocOnly; }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif
    void lock() const { m_lock.lock(); }
    void unlock() const { m_lock.unlock(); }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

private:
    // If the connection failed, |conn| is deleted.
    // Returns NULL if connection failed.
    static HostConnection* connect(HostConnection* con);

    HostConnection();
    static gl_client_context_t  *s_getGLContext();
    static gl2_client_context_t *s_getGL2Context();

    const std::string& queryGLExtensions(ExtendedRCEncoderContext *rcEnc);
    // setProtocol initilizes GL communication protocol for checksums
    // should be called when m_rcEnc is created
    void setChecksumHelper(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetSyncImpl(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetDmaImpl(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetGLESMaxVersion(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetNoErrorState(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetHostCompositionImpl(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetDirectMemSupport(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVulkanSupport(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetDeferredVulkanCommandsSupport(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVulkanNullOptionalStringsSupport(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVulkanCreateResourcesWithRequirementsSupport(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVulkanIgnoredHandles(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetYUVCache(ExtendedRCEncoderContext *mrcEnc);
    void queryAndSetAsyncUnmapBuffer(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVirtioGpuNext(ExtendedRCEncoderContext *rcEnc);
    void queryHasSharedSlotsHostMemoryAllocator(ExtendedRCEncoderContext *rcEnc);
    void queryAndSetVulkanFreeMemorySync(ExtendedRCEncoderContext *rcEnc);

private:
    HostConnectionType m_connectionType;
    GrallocType m_grallocType;
    IOStream *m_stream;
    GLEncoder   *m_glEnc;
    GL2Encoder  *m_gl2Enc;
    goldfish_vk::VkEncoder  *m_vkEnc;
    ExtendedRCEncoderContext *m_rcEnc;
    ChecksumCalculator m_checksumHelper;
    Gralloc *m_grallocHelper;
    ProcessPipe *m_processPipe;
    std::string m_glExtensions;
    bool m_grallocOnly;
    bool m_noHostError;
#ifdef GOLDFISH_VULKAN
    mutable std::mutex m_lock;
#else
    mutable android::Mutex m_lock;
#endif
};

#endif
