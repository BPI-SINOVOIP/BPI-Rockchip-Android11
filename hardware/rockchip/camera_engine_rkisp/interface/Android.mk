LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES +=\
	rkisp_control_loop_impl.cpp \
	rkcamera_vendor_tags.cpp \
	settings_processor.cpp \
	CameraWindow.cpp \
	rkisp_dev_manager.cpp \
	mediactl.c \

#LOCAL_CFLAGS += -Wno-error=unused-function -Wno-array-bounds -Wno-error
#LOCAL_CFLAGS += -DLINUX  -D_FILE_OFFSET_BITS=64 -DHAS_STDINT_H -DENABLE_ASSERT
LOCAL_CPPFLAGS += -std=c++11 -Wno-error -frtti
LOCAL_CPPFLAGS += -DLINUX
LOCAL_CPPFLAGS += $(PRJ_CPPFLAGS)
# LOCAL_CPPFLAGS += -v

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../ \
	$(LOCAL_PATH)/../xcore \
	$(LOCAL_PATH)/../xcore/base \
	$(LOCAL_PATH)/../xcore/ia \
	$(LOCAL_PATH)/../ext/rkisp \
	$(LOCAL_PATH)/../plugins/3a/rkiq \
	$(LOCAL_PATH)/../modules/isp \
	$(LOCAL_PATH)/../rkisp/ia-engine \
	$(LOCAL_PATH)/../rkisp/ia-engine/include \
    $(LOCAL_PATH)/../rkisp/ia-engine/include/linux \
    $(LOCAL_PATH)/../rkisp/ia-engine/include/linux/media \
	$(LOCAL_PATH)/../rkisp/isp-engine

LOCAL_STATIC_LIBRARIES := \
	librkisp_analyzer \
	librkisp_isp_engine \
	libisp_ia_engine \
	librkisp_ctrlloop

LOCAL_STATIC_LIBRARIES += \
	libisp_log \
	libisp_aaa_adpf \
	libisp_aaa_awdr \
	libisp_cam_calibdb \
	libisp_calibdb \
	libtinyxml2 \
	libisp_oslayer \
	libisp_ebase

ifeq ($(IS_HAVE_DRM),true)
LOCAL_SHARED_LIBRARIES += \
	libdrm
endif

ifeq ($(IS_ANDROID_OS),true)
LOCAL_SHARED_LIBRARIES += libutils libcutils liblog
LOCAL_SHARED_LIBRARIES += \
	libcamera_metadata
LOCAL_C_INCLUDES += \
    system/media/camera/include \
    system/media/private/camera/include \
    frameworks/av/include
LOCAL_CFLAGS += -DANDROID_PLATEFORM
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
LOCAL_PROPRIETARY_MODULE := true
LOCAL_C_INCLUDES += \
system/core/libutils/include \
system/core/include \
frameworks/native/libs/binder/include
LOCAL_STATIC_LIBRARIES += android.hardware.camera.common@1.0-helper
LOCAL_CFLAGS += -DANDROID_VERSION_ABOVE_8_X
LOCAL_HEADER_LIBRARIES += \
	libutils_headers
else
LOCAL_SHARED_LIBRARIES += \
	libcamera_metadata \
	libcamera_client
endif
else
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../metadata/libcamera_client/include \
	$(LOCAL_PATH)/../metadata/libcamera_metadata/include \
	$(LOCAL_PATH)/../metadata/header_files/include/system/core/include
LOCAL_STATIC_LIBRARIES += \
	librkisp_metadata
endif

LOCAL_MODULE:= librkisp

include $(BUILD_SHARED_LIBRARY)

ifeq ($(IS_ANDROID_OS),true)
include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/ae
LOCAL_MODULE := librkisp_aec
AEC_LIB_NAME := librkisp_aec.so
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
     LOCAL_CHECK_ELF_FILES := false
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
     LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := ../plugins/3a/rkiq/aec/lib32/$(AEC_LIB_NAME)
LOCAL_SRC_FILES_$(TARGET_ARCH) := ../plugins/3a/rkiq/aec/lib64/$(AEC_LIB_NAME)
else
LOCAL_SRC_FILES := ../plugins/3a/rkiq/aec/lib32/$(AEC_LIB_NAME)
endif
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/awb
LOCAL_MODULE := librkisp_awb
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
     LOCAL_CHECK_ELF_FILES := false
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
    LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := ../plugins/3a/rkiq/awb/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_ARCH) := ../plugins/3a/rkiq/awb/lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := ../plugins/3a/rkiq/awb/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := rkisp/af
LOCAL_MODULE := librkisp_af
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
     LOCAL_CHECK_ELF_FILES := false
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
    LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := ../plugins/3a/rkiq/af/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_ARCH) := ../plugins/3a/rkiq/af/lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := ../plugins/3a/rkiq/af/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE := libuvcapp
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
	LOCAL_PROPRIETARY_MODULE := true
endif
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 29)))
     LOCAL_CHECK_ELF_FILES := false
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_MODULE_SUFFIX := .so
ifneq ($(strip $(TARGET_2ND_ARCH)), )
	LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := ../plugins/uvcApp/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
LOCAL_SRC_FILES_$(TARGET_ARCH) := ../plugins/uvcApp/lib64/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
	LOCAL_SRC_FILES := ../plugins/uvcApp/lib32/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)

endif
