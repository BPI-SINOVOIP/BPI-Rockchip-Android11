LOCAL_PATH := $(call my-dir)
#The modules list follow belong to vndk-sp.
#The module which belong to vndk-sp is defined by google.
ifndef BOARD_VNDK_VERSION
VNDK_SP_LIBRARIES := \
    libRS_internal\
    libRSDriver\
    libRSCpuRef\
    libbcinfo\
    libblas\
    libft2\
    libpng\
    libcompiler_rt\

VNDK_SP_LIBRARIES += \
    android.hardware.renderscript@1.0\
    android.hardware.graphics.allocator@2.0\
    android.hardware.graphics.mapper@2.0\
    android.hardware.graphics.common@1.0\
    libhwbinder\
    libbase\
    libcutils\
    libhardware\
    libutils\
    libc++\
    libbacktrace\
    libunwind\
    liblzma\
    libhidlbase\
    libhidltransport\


define add-vndk-sp-lib
include $$(CLEAR_VARS)
LOCAL_MODULE := $1.vndk-sp
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PREBUILT_MODULE_FILE := $$(TARGET_OUT)/lib/$1.so
LOCAL_MULTILIB := 32
LOCAL_MODULE_TAGS := optional
LOCAL_INSTALLED_MODULE_STEM := $1.so
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_RELATIVE_PATH := vndk-sp
LOCAL_VENDOR_MODULE := $2
include $$(BUILD_PREBUILT)

include $$(CLEAR_VARS)
LOCAL_MODULE := $1.vndk-sp
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PREBUILT_MODULE_FILE := $$(TARGET_OUT)/lib64/$1.so
LOCAL_MULTILIB := 64
LOCAL_MODULE_TAGS := optional
LOCAL_INSTALLED_MODULE_STEM := $1.so
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_RELATIVE_PATH := vndk-sp
LOCAL_VENDOR_MODULE := $2
include $$(BUILD_PREBUILT)
endef

$(foreach lib,$(VNDK_SP_LIBRARIES),\
    $(eval $(call add-vndk-sp-lib,$(lib),)))
$(foreach lib,$(VNDK_SP_EXT_LIBRARIES),\
    $(eval $(call add-vndk-sp-lib,$(lib),true)))
endif
