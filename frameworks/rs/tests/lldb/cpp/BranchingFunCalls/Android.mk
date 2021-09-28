LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := CppBranchingFunCalls

LOCAL_SRC_FILES := \
	BranchingFunCalls.cpp \
	scalars.rscript

include frameworks/rs/tests/lldb/cpp/common.mk
include $(BUILD_EXECUTABLE)
