/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __QCAMERA3_STREAMMEM_H__
#define __QCAMERA3_STREAMMEM_H__

// System dependencies
#include <utils/Mutex.h>
#include <hardware/gralloc1.h>
// Camera dependencies
#include "QCamera3Mem.h"

extern "C" {
#include "mm_camera_interface.h"
}

using namespace android;

namespace qcamera {

class QCamera3StreamMem {
public:
    QCamera3StreamMem(uint32_t maxHeapBuffer);
    virtual ~QCamera3StreamMem();

    uint32_t getCnt();
    int getRegFlags(uint8_t *regFlags);

    // Helper function to access individual QCamera3Buffer object
    int getFd(uint32_t index);
    ssize_t getSize(uint32_t index);
    int invalidateCache(uint32_t index);
    int cleanInvalidateCache(uint32_t index);
    int cleanCache(uint32_t index);
    int32_t getBufDef(const cam_frame_len_offset_t &offset,
            mm_camera_buf_def_t &bufDef, uint32_t index,
            bool virtualAddr);
    void *getPtr(uint32_t index);

    bool valid(uint32_t index);

    // Gralloc buffer related functions
    int registerBuffer(buffer_handle_t *buffer, cam_stream_type_t type);
    int unregisterBuffer(uint32_t index);
    int getMatchBufIndex(void *object);
    void *getBufferHandle(uint32_t index);
    void unregisterBuffers(); //TODO: relace with unififed clear() function?

    // Heap buffer related functions
    int allocateAll(size_t size);
    int allocateOne(size_t size, bool isCached = true);
    void deallocate(); //TODO: replace with unified clear() function?

    // Clear function: unregister for gralloc buffer, and deallocate for heap buffer
    void clear() {unregisterBuffers(); deallocate(); }

    // Frame number getter and setter
    int32_t markFrameNumber(uint32_t index, uint32_t frameNumber);
    int32_t getFrameNumber(uint32_t index);
    int32_t getOldestFrameNumber(uint32_t &index);
    int32_t getGrallocBufferIndex(uint32_t frameNumber);
    int32_t getHeapBufferIndex(uint32_t frameNumber);
    int32_t getBufferIndex(uint32_t frameNumber);

private:
    //variables
    QCamera3HeapMemory mHeapMem;
    QCamera3GrallocMemory mGrallocMem;
    uint32_t mMaxHeapBuffers;
    Mutex mLock;
};

/// @brief Gralloc1 interface functions
struct Gralloc1Interface
{
    int32_t (*CreateDescriptor)(
        gralloc1_device_t*             pGralloc1Device,
        gralloc1_buffer_descriptor_t*  pCreatedDescriptor);
    int32_t (*DestroyDescriptor)(
        gralloc1_device_t*            pGralloc1Device,
        gralloc1_buffer_descriptor_t  descriptor);
    int32_t (*SetDimensions)(
        gralloc1_device_t*           pGralloc1Device,
        gralloc1_buffer_descriptor_t descriptor,
        uint32_t                       width,
        uint32_t                       height);
    int32_t (*SetFormat)(
        gralloc1_device_t*           pGralloc1Device,
        gralloc1_buffer_descriptor_t descriptor,
        int32_t                        format);
    int32_t (*SetLayerCount)(
        gralloc1_device_t*           pGralloc1Device,
        gralloc1_buffer_descriptor_t descriptor,
        uint32_t                     layerCount);
    int32_t (*SetProducerUsage)(
        gralloc1_device_t*           pGralloc1Device,
        gralloc1_buffer_descriptor_t descriptor,
        uint64_t                     usage);
    int32_t (*SetConsumerUsage)(
        gralloc1_device_t*           pGralloc1Device,
        gralloc1_buffer_descriptor_t descriptor,
        uint64_t                     usage);
    int32_t (*Allocate)(
        gralloc1_device_t*                  pGralloc1Device,
        uint32_t                              numDescriptors,
        const gralloc1_buffer_descriptor_t* pDescriptors,
        buffer_handle_t*                    pAllocatedBuffers);
    int32_t (*GetStride)(
        gralloc1_device_t* pGralloc1Device,
        buffer_handle_t    buffer,
        uint32_t*            pStride);
    int32_t (*Release)(
        gralloc1_device_t* pGralloc1Device,
        buffer_handle_t    buffer);
    int32_t (*Lock)(
            gralloc1_device_t*      device,
            buffer_handle_t         buffer,
            uint64_t                producerUsage,
            uint64_t                consumerUsage,
            const gralloc1_rect_t*  accessRegion,
            void**                  outData,
            int32_t                 acquireFence);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief General native buffer implementation
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NativeBufferInterface final
{
public:
    static NativeBufferInterface* GetInstance() {
        static NativeBufferInterface mInstance;
        return &mInstance;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// GetGrallocBufferStride
    ///
    /// @brief Get buffer stride from gralloc interface
    ///
    /// @param handle Native HAL buffer handle
    ///
    /// @return Return the stride size
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    uint32_t GetGrallocBufferStride(uint32_t width, uint32_t height, uint32_t fmt);
private:
    NativeBufferInterface();
    ~NativeBufferInterface();

    gralloc1_device_t* m_pGralloc1Device;
    Gralloc1Interface  m_grallocInterface;
};

};
#endif // __QCAMERA3_STREAMMEM_H__
