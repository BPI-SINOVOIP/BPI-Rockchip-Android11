LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := src/CtUpdateAmbassador.cpp \
                src/HAL.cpp \
                src/IpaEventRelay.cpp \
                src/LocalLogBuffer.cpp \
                src/OffloadStatistics.cpp \
                src/PrefixParser.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc
LOCAL_MODULE := liboffloadhal
LOCAL_CFLAGS := \
        -Wall \
        -Werror \
        -Wno-unused-parameter \
        -Wno-unused-result \

LOCAL_SHARED_LIBRARIES := \
                        libhidlbase \
                        liblog \
                        libcutils \
                        libdl \
                        libbase \
                        libutils \
                        libhardware_legacy \
                        libhardware \
                        android.hardware.tetheroffload.config@1.0 \
                        android.hardware.tetheroffload.control@1.0
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)
