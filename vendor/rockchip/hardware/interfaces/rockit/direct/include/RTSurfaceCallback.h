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
 */

#ifndef ROCKIT_DIRECT_RTSURFACECALLBACK_H_
#define ROCKIT_DIRECT_RTSURFACECALLBACK_H_

#include <sys/types.h>
#include <inttypes.h>
#include <utils/RefBase.h>
#include <gui/Surface.h>
#include <system/window.h>

#include "RTSurfaceInterface.h"

namespace android {

class RockitPlayer;
class RTSidebandWindow;
class RTSurfaceCallback : public RTSurfaceInterface {
 public:
    RTSurfaceCallback(const sp<IGraphicBufferProducer> &bufferProducer);
    virtual ~RTSurfaceCallback();

    virtual INT32 connect(INT32 mode);
    virtual INT32 disconnect(INT32 mode);

    virtual INT32 allocateBuffer(RTNativeWindowBufferInfo *info);
    virtual INT32 freeBuffer(void *buf, INT32 fence);
    virtual INT32 remainBuffer(void *buf, INT32 fence);
    virtual INT32 queueBuffer(void *buf, INT32 fence);
    virtual INT32 dequeueBuffer(void **buf);
    virtual INT32 dequeueBufferAndWait(RTNativeWindowBufferInfo *info);
    virtual INT32 mmapBuffer(RTNativeWindowBufferInfo *info, void **ptr);
    virtual INT32 munmapBuffer(void **ptr, INT32 size, void *buf);

    virtual INT32 setCrop(INT32 left, INT32 top, INT32 right, INT32 bottom);
    virtual INT32 setUsage(INT32 usage);
    virtual INT32 setScalingMode(INT32 mode);
    virtual INT32 setDataSpace(INT32 dataSpace);
    virtual INT32 setTransform(INT32 transform);
    virtual INT32 setSwapInterval(INT32 interval);
    virtual INT32 setBufferCount(INT32 bufferCount);
    virtual INT32 setBufferGeometry(INT32 width, INT32 height, INT32 format);
    virtual INT32 setSidebandStream(RTSidebandInfo info);

    virtual INT32 query(INT32 cmd, INT32 *param);
    virtual void* getNativeWindow();
    virtual INT32 setNativeWindow(const sp<IGraphicBufferProducer> &bufferProducer);

 private:
    INT32                   mDrmFd;
    INT32                   mTunnel;
    buffer_handle_t         mSidebandHandle;
    sp<RTSidebandWindow>    mSidebandWindow;
    sp<ANativeWindow>       mNativeWindow;
};
}
#endif  // ROCKIT_DIRECT_RTSURFACECALLBACK_H_
