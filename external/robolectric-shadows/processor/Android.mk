LOCAL_PATH := $(call my-dir)

##############################################
# Execute Robolectric processor tests
##############################################
include $(CLEAR_VARS)

LOCAL_MODULE := Run_robolectric_processor_tests

test_source_directory := $(LOCAL_PATH)/src/test/java

test_runtime_libraries := \
  Robolectric_processor_tests

include external/robolectric-shadows/run_robolectric_module_tests.mk
