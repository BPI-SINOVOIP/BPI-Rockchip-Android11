LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests
LOCAL_CTS_TEST_PACKAGE := android.host.adb
LOCAL_MODULE := CtsAdbHostTestCases
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_MODULE_TAGS := optional
LOCAL_JAVA_LIBRARIES := cts-tradefed tradefed compatibility-host-util
LOCAL_JAVA_RESOURCE_FILES := $(HOST_OUT_EXECUTABLES)/check_ms_os_desc
LOCAL_SRC_FILES := $(call all-java-files-under, src)
include $(BUILD_CTS_HOST_JAVA_LIBRARY)
