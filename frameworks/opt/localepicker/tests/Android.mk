#############################################################
# Build test package for locale picker lib.                 #
#############################################################

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := LocalePickerTest

LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_PROGUARD_ENABLED := disabled

LOCAL_STATIC_ANDROID_LIBRARIES += localepicker

LOCAL_USE_AAPT2 := true

LOCAL_MODULE_TAGS := optional

include $(BUILD_PACKAGE)

#############################################################
# LocalePicker Robolectric test target.                     #
#############################################################
include $(CLEAR_VARS)

LOCAL_MODULE := LocalePickerRoboTests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/res
LOCAL_JAVA_RESOURCE_DIRS := config

LOCAL_JAVA_LIBRARIES := \
    robolectric_android-all-stub \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    truth-prebuilt

LOCAL_INSTRUMENTATION_FOR := LocalePickerTest

LOCAL_MODULE_TAGS := optional

# Generate test_config.properties
include external/robolectric-shadows/gen_test_config.mk

include $(BUILD_STATIC_JAVA_LIBRARY)

#############################################################
# LocalePicker runner target to run the previous target.    #
#############################################################
include $(CLEAR_VARS)

LOCAL_MODULE := RunLocalePickerRoboTests

LOCAL_JAVA_LIBRARIES := \
    LocalePickerRoboTests \
    robolectric_android-all-stub \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    truth-prebuilt

LOCAL_TEST_PACKAGE := LocalePickerTest

LOCAL_INSTRUMENT_SOURCE_DIRS := $(LOCAL_PATH)/../src

include external/robolectric-shadows/run_robotests.mk
