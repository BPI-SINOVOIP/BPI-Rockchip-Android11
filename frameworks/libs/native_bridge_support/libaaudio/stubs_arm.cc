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

DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_delete);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_openStream);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setAllowedCapturePolicy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setBufferCapacityInFrames);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setChannelCount);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setContentType);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setDataCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setDeviceId);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setDirection);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setErrorCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setFramesPerDataCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setInputPreset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setPerformanceMode);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setPrivacySensitive);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setSampleRate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setSamplesPerFrame);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setSessionId);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setSharingMode);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStreamBuilder_setUsage);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_close);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getAllowedCapturePolicy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getBufferCapacityInFrames);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getBufferSizeInFrames);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getChannelCount);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getContentType);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getDeviceId);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getDirection);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getFramesPerBurst);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getFramesPerDataCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getFramesRead);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getFramesWritten);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getInputPreset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getPerformanceMode);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getSampleRate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getSamplesPerFrame);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getSessionId);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getSharingMode);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getState);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getTimestamp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getUsage);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_getXRunCount);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_isMMapUsed);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_isPrivacySensitive);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_read);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_release);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_requestFlush);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_requestPause);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_requestStart);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_requestStop);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_setBufferSizeInFrames);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_waitForStateChange);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudioStream_write);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudio_convertResultToText);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudio_convertStreamStateToText);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudio_createStreamBuilder);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudio_getMMapPolicy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AAudio_setMMapPolicy);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_delete);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_openStream);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setAllowedCapturePolicy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setBufferCapacityInFrames);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setChannelCount);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setContentType);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setDataCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setDeviceId);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setDirection);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setErrorCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setFramesPerDataCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setInputPreset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setPerformanceMode);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setPrivacySensitive);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setSampleRate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setSamplesPerFrame);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setSessionId);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setSharingMode);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStreamBuilder_setUsage);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_close);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getAllowedCapturePolicy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getBufferCapacityInFrames);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getBufferSizeInFrames);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getChannelCount);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getContentType);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getDeviceId);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getDirection);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getFramesPerBurst);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getFramesPerDataCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getFramesRead);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getFramesWritten);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getInputPreset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getPerformanceMode);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getSampleRate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getSamplesPerFrame);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getSessionId);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getSharingMode);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getState);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getTimestamp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getUsage);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_getXRunCount);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_isMMapUsed);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_isPrivacySensitive);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_read);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_release);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_requestFlush);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_requestPause);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_requestStart);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_requestStop);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_setBufferSizeInFrames);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_waitForStateChange);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudioStream_write);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudio_convertResultToText);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudio_convertStreamStateToText);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudio_createStreamBuilder);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudio_getMMapPolicy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libaaudio.so", AAudio_setMMapPolicy);
}
// clang-format on
