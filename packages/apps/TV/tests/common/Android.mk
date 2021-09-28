LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := tv-test-common-robo

LOCAL_SRC_FILES := \
    $(call all-java-files-under, src/com/android/tv/testing/robo) \
    $(call all-java-files-under, src/com/android/tv/testing/shadows)

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res

LOCAL_STATIC_JAVA_LIBRARIES := \
    robolectric_android-all-stub \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    tv-test-common \

LOCAL_INSTRUMENTATION_FOR := LiveTv

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_JAVA_LIBRARY)
