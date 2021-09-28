LOCAL_PATH := $(call my-dir)

##############################################
# Execute Robolectric resources tests
##############################################
include $(CLEAR_VARS)

LOCAL_MODULE := Run_robolectric_resources_tests

test_source_directory := $(LOCAL_PATH)/src/test/java

test_runtime_libraries := \
  Robolectric_resources_tests

include external/robolectric-shadows/run_robolectric_module_tests.mk
