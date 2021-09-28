LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# We only want this apk build for tests.
LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := $(call all-java-files-under, src) \
    $(call all-java-files-under, ../src) \
    $(call all-Iaidl-files-under, ../src)

LOCAL_PACKAGE_NAME := CarServicesTest
LOCAL_PRIVATE_PLATFORM_APIS := true

LOCAL_CERTIFICATE := platform

LOCAL_MODULE_TAGS := tests

# When built explicitly put it in the data partition
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA_APPS)

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_JAVA_LIBRARIES += \
    android.test.runner \
    android.test.base \
    android.hardware.automotive.vehicle-V2.0-java

LOCAL_STATIC_JAVA_LIBRARIES := \
    android.car.internal.event-log-tags \
    android.car.test.utils \
    android.car.userlib \
    android.car.watchdoglib \
    androidx.test.ext.junit \
    androidx.test.rules \
    mockito-target-extended-minus-junit4 \
    services.core \
    truth-prebuilt

# mockito-target-extended dependencies
LOCAL_JNI_SHARED_LIBRARIES := \
    libdexmakerjvmtiagent \
    libstaticjvmtiagent \

include $(BUILD_PACKAGE)
