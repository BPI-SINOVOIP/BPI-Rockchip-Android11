/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_WELL_KNOWN_CLASSES_H_
#define ART_RUNTIME_WELL_KNOWN_CLASSES_H_

#include "base/locks.h"
#include "jni.h"
#include "obj_ptr.h"

namespace art {

class ArtMethod;

namespace mirror {
class Class;
}  // namespace mirror

// Various classes used in JNI. We cache them so we don't have to keep looking them up.

struct WellKnownClasses {
 public:
  // Run before native methods are registered.
  static void Init(JNIEnv* env);
  // Run after native methods are registered.
  static void LateInit(JNIEnv* env);

  static void Clear();

  static void HandleJniIdTypeChange(JNIEnv* env);

  static void InitStringInit(ObjPtr<mirror::Class> string_class,
                             ObjPtr<mirror::Class> string_builder_class)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static ArtMethod* StringInitToStringFactory(ArtMethod* method);
  static uint32_t StringInitToEntryPoint(ArtMethod* method);

  static ObjPtr<mirror::Class> ToClass(jclass global_jclass) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static void InitFieldsAndMethodsOnly(JNIEnv* env);

 public:
  static jclass dalvik_annotation_optimization_CriticalNative;
  static jclass dalvik_annotation_optimization_FastNative;
  static jclass dalvik_system_BaseDexClassLoader;
  static jclass dalvik_system_DelegateLastClassLoader;
  static jclass dalvik_system_DexClassLoader;
  static jclass dalvik_system_DexFile;
  static jclass dalvik_system_DexPathList;
  static jclass dalvik_system_DexPathList__Element;
  static jclass dalvik_system_EmulatedStackFrame;
  static jclass dalvik_system_InMemoryDexClassLoader;
  static jclass dalvik_system_PathClassLoader;
  static jclass dalvik_system_VMRuntime;
  static jclass java_lang_annotation_Annotation__array;
  static jclass java_lang_BootClassLoader;
  static jclass java_lang_ClassLoader;
  static jclass java_lang_ClassNotFoundException;
  static jclass java_lang_Daemons;
  static jclass java_lang_Error;
  static jclass java_lang_IllegalAccessError;
  static jclass java_lang_NoClassDefFoundError;
  static jclass java_lang_Object;
  static jclass java_lang_OutOfMemoryError;
  static jclass java_lang_reflect_InvocationTargetException;
  static jclass java_lang_reflect_Parameter;
  static jclass java_lang_reflect_Parameter__array;
  static jclass java_lang_reflect_Proxy;
  static jclass java_lang_RuntimeException;
  static jclass java_lang_StackOverflowError;
  static jclass java_lang_String;
  static jclass java_lang_StringFactory;
  static jclass java_lang_System;
  static jclass java_lang_Thread;
  static jclass java_lang_ThreadGroup;
  static jclass java_lang_Throwable;
  static jclass java_nio_ByteBuffer;
  static jclass java_nio_DirectByteBuffer;
  static jclass java_util_Collections;
  static jclass java_util_function_Consumer;
  static jclass libcore_reflect_AnnotationFactory;
  static jclass libcore_reflect_AnnotationMember;
  static jclass libcore_util_EmptyArray;
  static jclass org_apache_harmony_dalvik_ddmc_Chunk;
  static jclass org_apache_harmony_dalvik_ddmc_DdmServer;

  static jmethodID dalvik_system_BaseDexClassLoader_getLdLibraryPath;
  static jmethodID dalvik_system_VMRuntime_runFinalization;
  static jmethodID dalvik_system_VMRuntime_hiddenApiUsed;
  static jmethodID java_lang_Boolean_valueOf;
  static jmethodID java_lang_Byte_valueOf;
  static jmethodID java_lang_Character_valueOf;
  static jmethodID java_lang_ClassLoader_loadClass;
  static jmethodID java_lang_ClassNotFoundException_init;
  static jmethodID java_lang_Daemons_start;
  static jmethodID java_lang_Daemons_stop;
  static jmethodID java_lang_Daemons_waitForDaemonStart;
  static jmethodID java_lang_Double_valueOf;
  static jmethodID java_lang_Float_valueOf;
  static jmethodID java_lang_Integer_valueOf;
  static jmethodID java_lang_invoke_MethodHandles_lookup;
  static jmethodID java_lang_invoke_MethodHandles_Lookup_findConstructor;
  static jmethodID java_lang_Long_valueOf;
  static jmethodID java_lang_ref_FinalizerReference_add;
  static jmethodID java_lang_ref_ReferenceQueue_add;
  static jmethodID java_lang_reflect_InvocationTargetException_init;
  static jmethodID java_lang_reflect_Parameter_init;
  static jmethodID java_lang_reflect_Proxy_init;
  static jmethodID java_lang_reflect_Proxy_invoke;
  static jmethodID java_lang_Runtime_nativeLoad;
  static jmethodID java_lang_Short_valueOf;
  static jmethodID java_lang_String_charAt;
  static jmethodID java_lang_Thread_dispatchUncaughtException;
  static jmethodID java_lang_Thread_init;
  static jmethodID java_lang_Thread_run;
  static jmethodID java_lang_ThreadGroup_add;
  static jmethodID java_lang_ThreadGroup_removeThread;
  static jmethodID java_nio_DirectByteBuffer_init;
  static jmethodID java_util_function_Consumer_accept;
  static jmethodID libcore_reflect_AnnotationFactory_createAnnotation;
  static jmethodID libcore_reflect_AnnotationMember_init;
  static jmethodID org_apache_harmony_dalvik_ddmc_DdmServer_broadcast;
  static jmethodID org_apache_harmony_dalvik_ddmc_DdmServer_dispatch;

  static jfieldID dalvik_system_BaseDexClassLoader_pathList;
  static jfieldID dalvik_system_BaseDexClassLoader_sharedLibraryLoaders;
  static jfieldID dalvik_system_DexFile_cookie;
  static jfieldID dalvik_system_DexFile_fileName;
  static jfieldID dalvik_system_DexPathList_dexElements;
  static jfieldID dalvik_system_DexPathList__Element_dexFile;
  static jfieldID dalvik_system_VMRuntime_nonSdkApiUsageConsumer;
  static jfieldID java_io_FileDescriptor_descriptor;
  static jfieldID java_io_FileDescriptor_ownerId;
  static jfieldID java_lang_Thread_parkBlocker;
  static jfieldID java_lang_Thread_daemon;
  static jfieldID java_lang_Thread_group;
  static jfieldID java_lang_Thread_lock;
  static jfieldID java_lang_Thread_name;
  static jfieldID java_lang_Thread_priority;
  static jfieldID java_lang_Thread_nativePeer;
  static jfieldID java_lang_Thread_systemDaemon;
  static jfieldID java_lang_Thread_unparkedBeforeStart;
  static jfieldID java_lang_ThreadGroup_groups;
  static jfieldID java_lang_ThreadGroup_ngroups;
  static jfieldID java_lang_ThreadGroup_mainThreadGroup;
  static jfieldID java_lang_ThreadGroup_name;
  static jfieldID java_lang_ThreadGroup_parent;
  static jfieldID java_lang_ThreadGroup_systemThreadGroup;
  static jfieldID java_lang_Throwable_cause;
  static jfieldID java_lang_Throwable_detailMessage;
  static jfieldID java_lang_Throwable_stackTrace;
  static jfieldID java_lang_Throwable_stackState;
  static jfieldID java_lang_Throwable_suppressedExceptions;
  static jfieldID java_nio_Buffer_address;
  static jfieldID java_nio_Buffer_elementSizeShift;
  static jfieldID java_nio_Buffer_limit;
  static jfieldID java_nio_Buffer_position;
  static jfieldID java_nio_ByteBuffer_address;
  static jfieldID java_nio_ByteBuffer_hb;
  static jfieldID java_nio_ByteBuffer_isReadOnly;
  static jfieldID java_nio_ByteBuffer_limit;
  static jfieldID java_nio_ByteBuffer_offset;
  static jfieldID java_nio_DirectByteBuffer_capacity;
  static jfieldID java_nio_DirectByteBuffer_effectiveDirectAddress;

  static jfieldID java_util_Collections_EMPTY_LIST;
  static jfieldID libcore_util_EmptyArray_STACK_TRACE_ELEMENT;
  static jfieldID org_apache_harmony_dalvik_ddmc_Chunk_data;
  static jfieldID org_apache_harmony_dalvik_ddmc_Chunk_length;
  static jfieldID org_apache_harmony_dalvik_ddmc_Chunk_offset;
  static jfieldID org_apache_harmony_dalvik_ddmc_Chunk_type;
};

}  // namespace art

#endif  // ART_RUNTIME_WELL_KNOWN_CLASSES_H_
