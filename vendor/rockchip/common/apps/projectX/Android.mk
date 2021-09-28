###############################################################################
# projectX
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := projectX
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE := true
LOCAL_CERTIFICATE := PRESIGNED
#LOCAL_OVERRIDES_PACKAGES := 
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
LOCAL_PREBUILT_JNI_LIBS := lib/libasm.so \
                           lib/libasmlibrary.so \
                           lib/libdcnn.so \
                           lib/libdistance.so \
                           lib/libface_detection_native.so \
                           lib/libgifmerge.so \
                           lib/libopencv_java.so \
                           lib/libscreenshot.so
                           
include $(BUILD_PREBUILT)
