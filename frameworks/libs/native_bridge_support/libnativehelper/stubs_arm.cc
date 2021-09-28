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

DEFINE_INTERCEPTABLE_STUB_FUNCTION(AFileDescriptor_create);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AFileDescriptor_getFD);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AFileDescriptor_setFD);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(DlCloseLibrary);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(DlGetError);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(DlGetSymbol);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(DlOpenLibrary);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(EnsureInitialized);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ExpandableStringAppend);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ExpandableStringAssign);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ExpandableStringInitialize);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(ExpandableStringRelease);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JNI_CreateJavaVM);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JNI_GetCreatedJavaVMs);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JNI_GetDefaultJavaVMInitArgs);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_FileDescriptorClass);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_FileDescriptor_descriptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_FileDescriptor_init);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NIOAccessClass);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NIOAccess_getBaseArray);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NIOAccess_getBaseArrayOffset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBufferClass);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer__elementSizeShift);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer_address);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer_array);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer_arrayOffset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer_limit);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniConstants_NioBuffer_position);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniInvocationCreate);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniInvocationDestroy);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniInvocationGetLibrary);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniInvocationGetLibraryWith);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(JniInvocationInit);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniCreateFileDescriptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniCreateString);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniCreateStringArray);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetFDFromFileDescriptor);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetNioBufferBaseArray);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetNioBufferBaseArrayOffset);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetNioBufferFields);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetNioBufferPointer);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniGetReferent);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniLogException);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniRegisterNativeMethods);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniSetFileDescriptorOfFD);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniThrowException);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniThrowExceptionFmt);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniThrowIOException);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniThrowNullPointerException);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniThrowRuntimeException);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(jniUninitializeConstants);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", AFileDescriptor_create);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", AFileDescriptor_getFD);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", AFileDescriptor_setFD);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", DlCloseLibrary);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", DlGetError);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", DlGetSymbol);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", DlOpenLibrary);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", EnsureInitialized);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", ExpandableStringAppend);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", ExpandableStringAssign);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", ExpandableStringInitialize);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", ExpandableStringRelease);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JNI_CreateJavaVM);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JNI_GetCreatedJavaVMs);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JNI_GetDefaultJavaVMInitArgs);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_FileDescriptorClass);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_FileDescriptor_descriptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_FileDescriptor_init);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NIOAccessClass);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NIOAccess_getBaseArray);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NIOAccess_getBaseArrayOffset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBufferClass);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer__elementSizeShift);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer_address);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer_array);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer_arrayOffset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer_limit);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniConstants_NioBuffer_position);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniInvocationCreate);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniInvocationDestroy);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniInvocationGetLibrary);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniInvocationGetLibraryWith);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", JniInvocationInit);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniCreateFileDescriptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniCreateString);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniCreateStringArray);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetFDFromFileDescriptor);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetNioBufferBaseArray);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetNioBufferBaseArrayOffset);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetNioBufferFields);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetNioBufferPointer);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniGetReferent);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniLogException);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniRegisterNativeMethods);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniSetFileDescriptorOfFD);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniThrowException);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniThrowExceptionFmt);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniThrowIOException);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniThrowNullPointerException);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniThrowRuntimeException);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libnativehelper.so", jniUninitializeConstants);
}
// clang-format on
