LOCAL_PATH := $(call my-dir)

##############################################
# Execute Robolectric shadows httpclient tests
##############################################
include $(CLEAR_VARS)

LOCAL_MODULE := Run_robolectric_shadows_httpclient_tests

test_source_directory := $(LOCAL_PATH)/src/test/java

test_resources_directory := $(LOCAL_PATH)/src/test/resources

test_runtime_libraries := \
  Robolectric_shadows_httpclient_tests \
  robolectric-host-android_all

include external/robolectric-shadows/run_robolectric_module_tests.mk
