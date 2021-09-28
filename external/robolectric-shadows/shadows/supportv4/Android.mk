LOCAL_PATH := $(call my-dir)

##############################################
# Execute Robolectric shadows supportv4 tests
##############################################
include $(CLEAR_VARS)

LOCAL_MODULE := Run_robolectric_shadows_supportv4_tests

test_source_directory := $(LOCAL_PATH)/src/test/java

test_resources_directory := $(LOCAL_PATH)/src/test/resources

test_runtime_libraries := \
  Robolectric_shadows_supportv4_tests \
  robolectric-host-android_all

include external/robolectric-shadows/run_robolectric_module_tests.mk
