LOCAL_PATH := $(call my-dir)

include $(call first-makefiles-under, $(LOCAL_PATH))

##############################################
# Run all Robolectric tests
##############################################
include $(CLEAR_VARS)

.PHONY: Run_robolectric_test_suite

Run_robolectric_test_suite: \
  Run_robolectric_utils_tests \
  Run_robolectric_sandbox_tests \
  Run_robolectric_processor_tests \
  Run_robolectric_resources_tests \
  Run_robolectric_shadowapi_tests \
  Run_robolectric_robolectric_tests \
  Run_robolectric_shadows_supportv4_tests \
  Run_robolectric_shadows_httpclient_tests \
  Run_robolectric_shadows_androidx_fragment_tests
