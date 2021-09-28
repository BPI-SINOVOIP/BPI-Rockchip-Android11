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

DEFINE_INTERCEPTABLE_STUB_FUNCTION(JNI_OnLoad);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl11UnmapStaticEl);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl15GetStrideStaticEl);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl21GetNativeBufferStaticEl);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl3MapE9AwMapModePPv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl5UnmapEv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl6CreateEii);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl7ReleaseEl);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImpl9MapStaticEl9AwMapModePPv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImplC2Ejj);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android17GraphicBufferImplD2Ev);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android19RegisterDrawFunctorEP7_JNIEnv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android20RaiseFileNumberLimitEv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android21RegisterDrawGLFunctorEP7_JNIEnv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZN7android21RegisterGraphicsUtilsEP7_JNIEnv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZNK7android17GraphicBufferImpl15GetNativeBufferEv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZNK7android17GraphicBufferImpl9GetStrideEv);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(_ZNK7android17GraphicBufferImpl9InitCheckEv);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", JNI_OnLoad);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl11UnmapStaticEl);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl15GetStrideStaticEl);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl21GetNativeBufferStaticEl);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl3MapE9AwMapModePPv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl5UnmapEv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl6CreateEii);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl7ReleaseEl);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImpl9MapStaticEl9AwMapModePPv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImplC2Ejj);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android17GraphicBufferImplD2Ev);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android19RegisterDrawFunctorEP7_JNIEnv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android20RaiseFileNumberLimitEv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android21RegisterDrawGLFunctorEP7_JNIEnv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZN7android21RegisterGraphicsUtilsEP7_JNIEnv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZNK7android17GraphicBufferImpl15GetNativeBufferEv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZNK7android17GraphicBufferImpl9GetStrideEv);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libwebviewchromium_plat_support.so", _ZNK7android17GraphicBufferImpl9InitCheckEv);
}
// clang-format on
