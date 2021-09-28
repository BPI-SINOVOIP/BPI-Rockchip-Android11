# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

# Control APIs used by clients to communicate with HAL.
# ============================================================
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wno-unused-parameter
LOCAL_CFLAGS += -Wall -Werror
LOCAL_MODULE := libwifi-hal-ctrl
LOCAL_VENDOR_MODULE := true
LOCAL_C_INCLUDES := $(LOCAL_PATH)/wifi_hal_ctrl
LOCAL_SRC_FILES := wifi_hal_ctrl/wifi_hal_ctrl.c
LOCAL_HEADER_LIBRARIES := libcutils_headers
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwifi-hal-ctrl_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/wifi_hal_ctrl
LOCAL_HEADER_LIBRARIES := libcutils_headers
include $(BUILD_HEADER_LIBRARY)

# Make the HAL library
# ============================================================
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wno-unused-parameter
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += "-DLOG_NDEBUG=0"
endif

# gscan.cpp: address of array 'cached_results[i].results' will always evaluate to 'true'
LOCAL_CLANG_CFLAGS := -Wno-pointer-bool-conversion

LOCAL_CFLAGS += -Wall -Werror

# Allow implicit fallthrough in nan_ind.cpp:834 until it is fixed.
LOCAL_CFLAGS += -Wno-implicit-fallthrough

ifdef WIFI_DRIVER_STATE_CTRL_PARAM
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_CTRL_PARAM=\"$(WIFI_DRIVER_STATE_CTRL_PARAM)\"
ifdef WIFI_DRIVER_STATE_ON
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_ON=\"$(WIFI_DRIVER_STATE_ON)\"
endif
endif

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	external/libnl/include \
	$(call include-path-for, libhardware_legacy)/hardware_legacy \
	external/wpa_supplicant_8/src/drivers \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \

LOCAL_SRC_FILES := \
	list.cpp \
	wifi_hal.cpp \
	common.cpp \
	cpp_bindings.cpp \
	llstats.cpp \
	gscan.cpp \
	gscan_event_handler.cpp \
	rtt.cpp \
	ifaceeventhandler.cpp \
	tdls.cpp \
	nan.cpp \
	nan_ind.cpp \
	nan_req.cpp \
	nan_rsp.cpp \
	wificonfig.cpp \
	wifilogger.cpp \
	wifilogger_diag.cpp \
	ring_buffer.cpp \
	rb_wrapper.cpp \
	rssi_monitor.cpp \
	roam.cpp \
	radio_mode.cpp

LOCAL_MODULE := libwifi-hal-qcom
LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_SHARED_LIBRARIES += libnetutils liblog libwpa_client libcld80211

ifneq ($(wildcard external/libnl),)
LOCAL_SHARED_LIBRARIES += libnl
LOCAL_C_INCLUDES += external/libnl/include
else
LOCAL_SHARED_LIBRARIES += libnl_2
LOCAL_C_INCLUDES += external/libnl-headers
endif

LOCAL_HEADER_LIBRARIES := libcutils_headers libutils_headers libwifi-hal-ctrl_headers

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_REQUIRED_MODULES :=

LOCAL_CFLAGS += -Wno-unused-parameter -Wall -Werror
LOCAL_CPPFLAGS += -Wno-conversion-null
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += "-DLOG_NDEBUG=0"
endif

# gscan.cpp: address of array 'cached_results[i].results' will always evaluate to 'true'
LOCAL_CLANG_CFLAGS := -Wno-pointer-bool-conversion

# Allow implicit fallthrough in nan_ind.cpp:834 until it is fixed.
LOCAL_CFLAGS += -Wno-implicit-fallthrough

ifdef WIFI_DRIVER_STATE_CTRL_PARAM
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_CTRL_PARAM=\"$(WIFI_DRIVER_STATE_CTRL_PARAM)\"
ifdef WIFI_DRIVER_STATE_ON
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_ON=\"$(WIFI_DRIVER_STATE_ON)\"
endif
endif

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	external/libnl/include \
	$(call include-path-for, libhardware_legacy)/hardware_legacy \
	external/wpa_supplicant_8/src/drivers \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include \

LOCAL_SRC_FILES := \
	list.cpp \
	wifi_hal.cpp \
	common.cpp \
	cpp_bindings.cpp \
	llstats.cpp \
	gscan.cpp \
	gscan_event_handler.cpp \
	rtt.cpp \
	ifaceeventhandler.cpp \
	tdls.cpp \
	nan.cpp \
	nan_ind.cpp \
	nan_req.cpp \
	nan_rsp.cpp \
	wificonfig.cpp \
	wifilogger.cpp \
	wifilogger_diag.cpp \
	ring_buffer.cpp \
	rb_wrapper.cpp \
	rssi_monitor.cpp \
	roam.cpp \
	radio_mode.cpp

LOCAL_CFLAGS += -Wall -Werror
LOCAL_MODULE := libwifi-hal-qcom
LOCAL_VENDOR_MODULE := true
LOCAL_CLANG := true
LOCAL_SHARED_LIBRARIES += libnetutils liblog
LOCAL_SHARED_LIBRARIES += libdl libwpa_client libcld80211
LOCAL_SHARED_LIBRARIES += libwifi-hal-ctrl

ifneq ($(wildcard external/libnl),)
LOCAL_SHARED_LIBRARIES += libnl
LOCAL_C_INCLUDES += external/libnl/include
else
LOCAL_SHARED_LIBRARIES += libnl_2
LOCAL_C_INCLUDES += external/libnl-headers
endif

LOCAL_HEADER_LIBRARIES := libcutils_headers libutils_headers libwifi-hal-ctrl_headers
include $(BUILD_SHARED_LIBRARY)
