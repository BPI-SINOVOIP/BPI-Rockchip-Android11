LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libdrm \
    libutils \
    libbaseparameter \

LOCAL_SRC_FILES:= \
        baseparameter_utilv1.cpp \
        baseparameter_utilv2.cpp \
        main.cpp \

LOCAL_C_INCLUDES += external/libdrm/include/drm
LOCAL_C_INCLUDES += external/libdrm/
LOCAL_C_INCLUDES += hardware/rockchip/libbaseparameter

LOCAL_MODULE:= saveBaseParameter
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CFLAGS += -Wno-unused-parameter
include $(BUILD_EXECUTABLE)
