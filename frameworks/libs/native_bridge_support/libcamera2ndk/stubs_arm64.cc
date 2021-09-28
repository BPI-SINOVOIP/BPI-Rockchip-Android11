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

DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_abortCaptures);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_capture);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_close);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_getDevice);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_logicalCamera_capture);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_logicalCamera_setRepeatingRequest);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_setRepeatingRequest);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_stopRepeating);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraCaptureSession_updateSharedOutput);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_close);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_createCaptureRequest);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_createCaptureRequest_withPhysicalIds);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_createCaptureSession);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_createCaptureSessionWithSessionParameters);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_getId);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraDevice_isSessionConfigurationSupported);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_delete);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_deleteCameraIdList);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_getCameraCharacteristics);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_getCameraIdList);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_openCamera);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_registerAvailabilityCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_registerExtendedAvailabilityCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_unregisterAvailabilityCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraManager_unregisterExtendedAvailabilityCallback);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_copy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_fromCameraMetadata);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_getAllTags);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_getConstEntry);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraMetadata_isLogicalMultiCamera);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraOutputTarget_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACameraOutputTarget_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_addTarget);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_copy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_getAllTags);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_getConstEntry);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_getConstEntry_physicalCamera);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_getUserContext);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_removeTarget);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_double);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_float);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_i32);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_i64);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_double);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_float);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_i32);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_i64);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_rational);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_physicalCamera_u8);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_rational);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setEntry_u8);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureRequest_setUserContext);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutputContainer_add);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutputContainer_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutputContainer_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutputContainer_remove);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutput_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionOutput_free);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionPhysicalOutput_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionSharedOutput_add);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionSharedOutput_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ACaptureSessionSharedOutput_remove);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_abortCaptures);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_capture);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_close);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_getDevice);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_logicalCamera_capture);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_logicalCamera_setRepeatingRequest);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_setRepeatingRequest);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_stopRepeating);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraCaptureSession_updateSharedOutput);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_close);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_createCaptureRequest);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_createCaptureRequest_withPhysicalIds);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_createCaptureSession);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_createCaptureSessionWithSessionParameters);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_getId);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraDevice_isSessionConfigurationSupported);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_delete);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_deleteCameraIdList);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_getCameraCharacteristics);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_getCameraIdList);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_openCamera);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_registerAvailabilityCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_registerExtendedAvailabilityCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_unregisterAvailabilityCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraManager_unregisterExtendedAvailabilityCallback);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_copy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_fromCameraMetadata);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_getAllTags);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_getConstEntry);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraMetadata_isLogicalMultiCamera);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraOutputTarget_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACameraOutputTarget_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_addTarget);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_copy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_getAllTags);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_getConstEntry);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_getConstEntry_physicalCamera);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_getUserContext);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_removeTarget);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_double);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_float);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_i32);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_i64);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_double);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_float);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_i32);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_i64);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_rational);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_physicalCamera_u8);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_rational);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setEntry_u8);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureRequest_setUserContext);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutputContainer_add);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutputContainer_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutputContainer_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutputContainer_remove);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutput_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionOutput_free);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionPhysicalOutput_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionSharedOutput_add);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionSharedOutput_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libcamera2ndk.so", ACaptureSessionSharedOutput_remove);
}
// clang-format on
