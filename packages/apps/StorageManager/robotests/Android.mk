#############################################################
# StorageManager Robolectric test target.                   #
#############################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := StorageManagerRoboTests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_JAVA_RESOURCE_DIRS := config

LOCAL_JAVA_LIBRARIES := \
    robolectric_android-all-stub \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    truth-prebuilt

LOCAL_INSTRUMENTATION_FOR := StorageManager

LOCAL_MODULE_TAGS := optional

# Generate test_config.properties
include external/robolectric-shadows/gen_test_config.mk

include $(BUILD_STATIC_JAVA_LIBRARY)

#############################################################
# StorageManager runner target to run the previous target.  #
#############################################################
include $(CLEAR_VARS)

LOCAL_MODULE := RunStorageManagerRoboTests

LOCAL_JAVA_LIBRARIES := \
    StorageManagerRoboTests \
    robolectric_android-all-stub \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    truth-prebuilt

LOCAL_TEST_PACKAGE := StorageManager

LOCAL_ROBOTEST_TIMEOUT := 36000

include external/robolectric-shadows/run_robotests.mk
