###############################################################################
# RockVRHome
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := RockVRHome
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE :=true
LOCAL_CERTIFICATE := platform
LOCAL_OVERRIDES_PACKAGES := Launcher3 
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
#LOCAL_PREBUILT_JNI_LIBS := \
	lib/arm/libvr-jni.so	\
	lib/arm/libvrtoolkit.so	\
	lib/arm/libvraudio_engine.so
        
include $(BUILD_PREBUILT)
