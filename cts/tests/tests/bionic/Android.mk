LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := CtsBionicTestCases
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/nativetest
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64

LOCAL_CFLAGS := -Wall -Werror

LOCAL_LDFLAGS := -Wl,--rpath,\$${ORIGIN}/lib/bionic-loader-test-libs -Wl,--enable-new-dtags -Wl,--export-dynamic

LOCAL_SHARED_LIBRARIES += \
    ld-android \
    libdl \
    libdl_android \
    libdl_preempt_test_1 \
    libdl_preempt_test_2 \
    libdl_test_df_1_global \
    libtest_elftls_shared_var \
    libtest_elftls_tprel \

LOCAL_WHOLE_STATIC_LIBRARIES += \
    libBionicTests \
    libBionicLoaderTests \
    libBionicElfTlsLoaderTests \
    libBionicCtsGtestMain \

LOCAL_STATIC_LIBRARIES += \
    libbase \
    libmeminfo \
    libziparchive \
    libtinyxml2 \
    liblog \
    libz \
    libutils \
    libgtest \
    libLLVMObject \
    libLLVMBitReader \
    libLLVMMC \
    libLLVMMCParser \
    libLLVMCore \
    libLLVMSupport \

LOCAL_SYSTEM_SHARED_LIBRARIES := libc.bootstrap libm.bootstrap libdl.bootstrap

LOCAL_CXX_STL := libc++_static

# Tag this module as a cts test artifact
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests mts

LOCAL_CTS_TEST_PACKAGE := android.bionic

cts_bionic_tests_2nd_arch_prefix :=
include $(LOCAL_PATH)/Android.build.copy.libs.mk
ifneq ($(TARGET_TRANSLATE_2ND_ARCH),true)
  ifneq ($(TARGET_2ND_ARCH),)
    cts_bionic_tests_2nd_arch_prefix := $(TARGET_2ND_ARCH_VAR_PREFIX)
    include $(LOCAL_PATH)/Android.build.copy.libs.mk
  endif
endif

include $(BUILD_CTS_EXECUTABLE)
