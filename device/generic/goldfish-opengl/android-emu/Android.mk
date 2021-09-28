ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
LOCAL_PATH := $(call my-dir)

$(call emugl-begin-shared-library,libandroidemu)
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))
$(call emugl-export,SHARED_LIBRARIES,libcutils libutils liblog)

LOCAL_CFLAGS += \
    -DLOG_TAG=\"androidemu\" \
    -Wno-missing-field-initializers \
    -fvisibility=default \
    -fstrict-aliasing \

LOCAL_SRC_FILES := \
    android/base/AlignedBuf.cpp \
    android/base/files/MemStream.cpp \
    android/base/files/Stream.cpp \
    android/base/files/StreamSerializing.cpp \
    android/base/Pool.cpp \
    android/base/ring_buffer.c \
    android/base/StringFormat.cpp \
    android/base/AndroidSubAllocator.cpp \
    android/base/synchronization/AndroidMessageChannel.cpp \
    android/base/threads/AndroidFunctorThread.cpp \
    android/base/threads/AndroidThreadStore.cpp \
    android/base/threads/AndroidThread_pthread.cpp \
    android/base/threads/AndroidWorkPool.cpp \
    android/base/Tracing.cpp \
    android/utils/debug.c \

$(call emugl-end-module)
endif
