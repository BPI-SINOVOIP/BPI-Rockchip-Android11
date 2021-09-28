#--------------------------------------------------------------------------
#  Copyright (C) 2014 Fuzhou Rockchip Electronics Co. Ltd. All rights reserved.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  #  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#    * Neither the name of The Linux Foundation nor
#      the names of its contributors may be used to endorse or promote
#      products derived from this software without specific prior written
#      permission.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := vpu_mem_pool.c vpu_dma_buf.c
LOCAL_SHARED_LIBRARIES := libutils libion 
LOCAL_MODULE := libvpu_mem_pool
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS:= -DLOG_TAG=\"VPU_MEM_POOL\"
#include $(BUILD_SHARED_LIBRARY)
#include $(BUILD_STATIC_LIBRARY)

#
# vpu_mem_pool_test
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	vpu_mem_observer.c

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog    \

LOCAL_MODULE := vpu_mem_observer
LOCAL_MODULE_TAGS := optional tests
LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS += -DLOG_TAG=\"VPU_MEM_OBSERVER\"

LOCAL_C_INCLUDES := 

include $(BUILD_EXECUTABLE)

