LOCAL_PATH := $(call my-dir)

################################################################
# Androidx Shadows Shell app just for Robolectric test target. #
################################################################
include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := Robolectric_shadows_androidx_fragment_shell_app

LOCAL_MANIFEST_FILE := src/test/AndroidManifest.xml

LOCAL_RESOURCE_DIR := $(LOCAL_PATH)/src/test/resources/res

# This exists to coerce make into generating and compiling R.java.
LOCAL_STATIC_JAVA_LIBRARIES := androidx.fragment_fragment

LOCAL_MIN_SDK_VERSION := 16

LOCAL_SDK_VERSION := current

LOCAL_USE_AAPT2 := true

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_PACKAGE)

################################################################
# Androidx Shadows test target.                                #
################################################################
include $(CLEAR_VARS)

LOCAL_MODULE := Robolectric_shadows_androidx_fragment_tests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

LOCAL_SRC_FILES := $(call all-java-files-under, src/test/java)

LOCAL_DONT_DELETE_JAR_META_INF := true

LOCAL_STATIC_JAVA_LIBRARIES := \
    Robolectric_all-target \
    truth-prebuilt

LOCAL_JAVA_LIBRARIES := \
    robolectric_android-all-stub

LOCAL_INSTRUMENTATION_FOR := Robolectric_shadows_androidx_fragment_shell_app

LOCAL_MODULE_TAGS := optional

# Generate test_config.properties
include external/robolectric-shadows/gen_test_config.mk

include $(BUILD_STATIC_JAVA_LIBRARY)

################################################################
# Androidx Shadows runner target to run the previous target.   #
################################################################
include $(CLEAR_VARS)

LOCAL_MODULE := Run_robolectric_shadows_androidx_fragment_tests

LOCAL_JAVA_LIBRARIES := \
    Robolectric_shadows_androidx_fragment_tests \
    robolectric_android-all-stub

LOCAL_TEST_PACKAGE := Robolectric_shadows_androidx_fragment_shell_app

LOCAL_ROBOTEST_FILES := $(call find-files-in-subdirs,$(LOCAL_PATH)/src/test/java,*Test.java,.)

include external/robolectric-shadows/run_robotests.mk
