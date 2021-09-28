/**
 * Copyright (C) 2019 The Android Open Source Project
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
#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <binder/IServiceManager.h>
#include <media/IMediaPlayerService.h>
#include <media/IOMX.h>
#include <media/OMXBuffer.h>
#include <ui/GraphicBuffer.h>
#include <android/hardware/media/omx/1.0/IOmx.h>
#include <android/hardware/media/omx/1.0/IOmxNode.h>
#include <android/hardware/media/omx/1.0/IOmxObserver.h>
#include <android/hardware/media/omx/1.0/types.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMapper.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <android/hardware/media/omx/1.0/IGraphicBufferSource.h>
#include <android/IOMXBufferSource.h>
#include <media/omx/1.0/WOmx.h>
#include <binder/MemoryDealer.h>
#include "media/hardware/HardwareAPI.h"
#include "OMX_Component.h"
#include <binder/ProcessState.h>
#include <media/stagefright/foundation/ALooper.h>
#include <utils/List.h>
#include <utils/Vector.h>
#include <utils/threads.h>
#include <inttypes.h>
#include <utils/Log.h>
#include <media/stagefright/OMXClient.h>

#define DEFAULT_TIMEOUT   5000000
#define OMX_UTILS_IP_PORT 0
#define OMX_UTILS_OP_PORT 1

using namespace android;
typedef hidl::allocator::V1_0::IAllocator IAllocator;

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}
struct Buffer {
    IOMX::buffer_id mID;
    sp<IMemory> mMemory;
    hidl_memory mHidlMemory;
    uint32_t mFlags;
};

status_t omxUtilsInit(char *codecName);
status_t omxUtilsGetParameter(int portIndex,
                              OMX_PARAM_PORTDEFINITIONTYPE *params);
status_t omxUtilsSetParameter(int portIndex,
                              OMX_PARAM_PORTDEFINITIONTYPE *params);
status_t omxUtilsSetPortMode(OMX_U32 port_index, IOMX::PortMode mode);
status_t omxUtilsUseBuffer(OMX_U32 portIndex, const OMXBuffer &omxBuf,
                           android::IOMX::buffer_id *buffer);
status_t omxUtilsSendCommand(OMX_COMMANDTYPE cmd, OMX_S32 param);
status_t omxUtilsEmptyBuffer(android::IOMX::buffer_id buffer,
                             const OMXBuffer &omxBuf, OMX_U32 flags,
                             OMX_TICKS timestamp, int fenceFd);
status_t omxUtilsFillBuffer(android::IOMX::buffer_id buffer,
                            const OMXBuffer &omxBuf, int fenceFd);
status_t omxUtilsFreeBuffer(OMX_U32 portIndex,
                            android::IOMX::buffer_id buffer);
status_t omxUtilsFreeNode();
status_t dequeueMessageForNode(omx_message *msg, int64_t timeoutUs);
void omxExitOnError(status_t ret);
