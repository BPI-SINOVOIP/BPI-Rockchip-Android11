//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// clang-format off
#include "native_bridge_support/vdso/interceptable_functions.h"

DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_acquire);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_allocate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_createFromHandle);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_describe);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_getNativeHandle);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_isSupported);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_lock);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_lockAndGetInfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_lockPlanes);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_recvHandleFromUnixSocket);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_release);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_sendHandleToUnixSocket);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AHardwareBuffer_unlock);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindowBuffer_getHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_OemStorageGet);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_OemStorageSet);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_acquire);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_cancelBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_dequeueBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getBuffersDataSpace);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getHeight);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getLastDequeueDuration);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getLastDequeueStartTime);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getLastQueueDuration);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_getWidth);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_lock);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_query);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_queryf);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_queueBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_release);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setAutoPrerotation);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setAutoRefresh);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBufferCount);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersDataSpace);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersDimensions);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersGeometry);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersTimestamp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setBuffersTransform);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setCancelBufferInterceptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setDequeueBufferInterceptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setDequeueTimeout);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setFrameRate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setPerformInterceptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setQueueBufferInterceptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setSharedBufferMode);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setSwapInterval);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_setUsage);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_tryAllocateBuffers);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ANativeWindow_unlockAndPost);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android32AHardwareBuffer_to_GraphicBufferEP15AHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android32AHardwareBuffer_to_GraphicBufferEPK15AHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android34AHardwareBuffer_from_GraphicBufferEPNS_13GraphicBufferE);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android34AHardwareBuffer_isValidPixelFormatEj);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android36AHardwareBuffer_convertToPixelFormatEj);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android38AHardwareBuffer_convertFromPixelFormatEj);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android38AHardwareBuffer_to_ANativeWindowBufferEP15AHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android38AHardwareBuffer_to_ANativeWindowBufferEPK15AHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android41AHardwareBuffer_convertToGrallocUsageBitsEy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android43AHardwareBuffer_convertFromGrallocUsageBitsEy);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_acquire);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_allocate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_createFromHandle);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_describe);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_getNativeHandle);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_isSupported);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_lock);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_lockAndGetInfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_lockPlanes);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_recvHandleFromUnixSocket);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_release);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_sendHandleToUnixSocket);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", AHardwareBuffer_unlock);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindowBuffer_getHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_OemStorageGet);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_OemStorageSet);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_acquire);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_cancelBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_dequeueBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getBuffersDataSpace);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getHeight);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getLastDequeueDuration);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getLastDequeueStartTime);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getLastQueueDuration);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_getWidth);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_lock);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_query);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_queryf);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_queueBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_release);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setAutoPrerotation);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setAutoRefresh);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBufferCount);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersDataSpace);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersDimensions);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersGeometry);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersTimestamp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setBuffersTransform);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setCancelBufferInterceptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setDequeueBufferInterceptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setDequeueTimeout);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setFrameRate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setPerformInterceptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setQueueBufferInterceptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setSharedBufferMode);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setSwapInterval);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_setUsage);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_tryAllocateBuffers);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", ANativeWindow_unlockAndPost);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android32AHardwareBuffer_to_GraphicBufferEP15AHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android32AHardwareBuffer_to_GraphicBufferEPK15AHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android34AHardwareBuffer_from_GraphicBufferEPNS_13GraphicBufferE);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android34AHardwareBuffer_isValidPixelFormatEj);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android36AHardwareBuffer_convertToPixelFormatEj);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android38AHardwareBuffer_convertFromPixelFormatEj);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android38AHardwareBuffer_to_ANativeWindowBufferEP15AHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android38AHardwareBuffer_to_ANativeWindowBufferEPK15AHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android41AHardwareBuffer_convertToGrallocUsageBitsEy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativewindow.so", _ZN7android43AHardwareBuffer_convertFromGrallocUsageBitsEy);
}
// clang-format on
