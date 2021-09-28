LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# We only want this apk build for tests.
LOCAL_MODULE_TAGS := tests

LOCAL_JAVA_LIBRARIES := android.test.runner

LOCAL_STATIC_JAVA_LIBRARIES := \
    androidx.test.rules \
    mockito-target \
    androidx.test.espresso.core \
   androidx.test.espresso.contrib-nodeps \
   androidx.test.espresso.intents-nodeps \
    truth-prebuilt

# Include all test java files.
LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := StorageManagerAppTests
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_INSTRUMENTATION_FOR := StorageManager

include $(BUILD_PACKAGE)
