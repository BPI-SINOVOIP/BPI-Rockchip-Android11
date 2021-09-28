#############################################################
# Tv Common Robolectric test target.                        #
#############################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := TvCommonRoboTests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_JAVA_LIBRARIES := \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \
    diffutils-prebuilt-jar \

LOCAL_STATIC_JAVA_LIBRARIES := \
    tv-lib-truth \

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.test.ext.truth \
    tv-test-common \
    tv-test-common-robo \

LOCAL_INSTRUMENTATION_FOR := LiveTv

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_JAVA_LIBRARY)

#############################################################
# Tv common runner target to run the previous target.       #
#############################################################
include $(CLEAR_VARS)
LOCAL_MODULE := RunTvCommonRoboTests

LOCAL_ROBOTEST_FILES := $(call find-files-in-subdirs,$(LOCAL_PATH)/src,*Test.java,.)

LOCAL_JAVA_LIBRARIES := \
    TvCommonRoboTests \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \

LOCAL_TEST_PACKAGE := LiveTv

LOCAL_ROBOTEST_TIMEOUT := 36000

include external/robolectric-shadows/run_robotests.mk
