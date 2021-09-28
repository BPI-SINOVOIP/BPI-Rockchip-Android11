LOCAL_PATH := $(call my-dir)

simpleperf_src_path := system/extras/simpleperf

LLVM_ROOT_PATH := external/llvm
include $(LLVM_ROOT_PATH)/llvm.mk

include $(CLEAR_VARS)
LOCAL_MODULE := CtsSimpleperfTestCases
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

LOCAL_WHOLE_STATIC_LIBRARIES = \
  libsimpleperf_cts_test \

LOCAL_STATIC_LIBRARIES += \
  libsimpleperf_etm_decoder \
  libbacktrace \
  libunwindstack \
  libdexfile_support_static \
  libziparchive \
  libz \
  libgtest \
  libbase \
  libcutils \
  liblog \
  libprocinfo \
  libutils \
  liblzma \
  libLLVMObject \
  libLLVMBitReader \
  libLLVMMC \
  libLLVMMCParser \
  libLLVMCore \
  libLLVMSupport \
  libprotobuf-cpp-lite \
  libevent \
  libopencsd_decoder \

simpleperf_testdata_files := $(shell cd $(simpleperf_src_path); find testdata -type f)

LOCAL_COMPATIBILITY_SUPPORT_FILES := \
  $(foreach file, $(simpleperf_testdata_files), $(simpleperf_src_path)/$(file):CtsSimpleperfTestCases_$(file))

LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

LOCAL_CTS_TEST_PACKAGE := android.simpleperf
include $(LLVM_DEVICE_BUILD_MK)
include $(BUILD_CTS_EXECUTABLE)

simpleperf_src_path :=
simpleperf_testdata_files :=

# Build the test APKs using their own makefiles
include $(call all-makefiles-under,$(LOCAL_PATH))
