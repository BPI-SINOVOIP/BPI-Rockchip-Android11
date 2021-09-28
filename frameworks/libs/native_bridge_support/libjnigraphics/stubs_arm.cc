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

DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getAlphaFlags);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getAndroidBitmapFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getDataSpace);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getHeight);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getMimeType);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoderHeaderInfo_getWidth);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_computeSampledSize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_createFromAAsset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_createFromBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_createFromFd);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_decodeImage);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_delete);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_getHeaderInfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_getMinimumStride);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_setAndroidBitmapFormat);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_setCrop);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_setDataSpace);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_setTargetSize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AImageDecoder_setUnpremultipliedRequired);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_compress);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_getDataSpace);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_getHardwareBuffer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_getInfo);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_lockPixels);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AndroidBitmap_unlockPixels);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getAlphaFlags);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getAndroidBitmapFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getDataSpace);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getHeight);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getMimeType);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoderHeaderInfo_getWidth);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_computeSampledSize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_createFromAAsset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_createFromBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_createFromFd);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_decodeImage);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_delete);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_getHeaderInfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_getMinimumStride);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_setAndroidBitmapFormat);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_setCrop);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_setDataSpace);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_setTargetSize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AImageDecoder_setUnpremultipliedRequired);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_compress);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_getDataSpace);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_getHardwareBuffer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_getInfo);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_lockPixels);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libjnigraphics.so", AndroidBitmap_unlockPixels);
}
// clang-format on
