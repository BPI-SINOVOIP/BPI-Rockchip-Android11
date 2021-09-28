# Copyright (C) 2014 The Android Open Source Project
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
# Modified 2011 by InvenSense, Inc

LOCAL_PATH := $(call my-dir)

#ifneq ($(TARGET_SIMULATOR),true)

ifeq (${TARGET_ARCH},arm64)

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libmplmpu
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm := libmplmpu.so
LOCAL_32_BIT_ONLY := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libmplmpu
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm64 := libmplmpu_64.so
include $(BUILD_PREBUILT)

endif

ifeq (${TARGET_ARCH},arm64)

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libmllite
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm := libmllite.so
LOCAL_32_BIT_ONLY := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libmllite
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_SRC_FILES_arm64 := libmllite_64.so
include $(BUILD_PREBUILT)

endif

# InvenSense fragment of the HAL
include $(CLEAR_VARS)

LOCAL_MODULE := libinvensense_hal
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := invensense

LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"

COMPILE_INVENSENSE_COMPASS_CAL := 0

ifeq ($(BOARD_SENSOR_MPU_PAD),true)
LOCAL_CFLAGS += -DSAMPLE_RATE_200HZ
LOCAL_CFLAGS += -DSENSOR_MPU_PAD
endif

ifeq ($(BOARD_GRAVITY_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DGRAVITY_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_COMPASS_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DCOMPASS_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_GYROSCOPE_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DGYROSCOPE_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_PROXIMITY_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DPROXIMITY_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_LIGHT_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DLIGHT_SENSOR_SUPPORT=1
endif

# ANDROID version check
$(info YD>>PLATFORM_VERSION=$(PLATFORM_VERSION))
MAJOR_VERSION :=$(shell echo $(PLATFORM_VERSION) | cut -f1 -d.)
MINOR_VERSION :=$(shell echo $(PLATFORM_VERSION) | cut -f2 -d.)
VERSION_KK :=$(shell test $(MAJOR_VERSION) -eq 4 -a $(MINOR_VERSION) -gt 3 && echo true)
VERSION_L  :=$(shell test $(MAJOR_VERSION) -eq 5 -a $(MINOR_VERSION) -eq 1 && echo true)
$(info YD>>ANDRIOD VERSION=$(MAJOR_VERSION).$(MINOR_VERSION))
$(info YD>>VERSION_L=$(VERSION_L), VERSION_KK=$(VERSION_KK))
#ANDROID version check END

VERSION_L := true

ifeq ($(VERSION_L),true)
LOCAL_CFLAGS += -DANDROID_LOLLIPOP
else
ifeq ($(VERSION_KK),true)
LOCAL_CFLAGS += -DANDROID_KITKAT
else
LOCAL_CFLAGS += -DANDROID_JELLYBEAN
endif
endif

ifneq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug user))
ifneq ($(COMPILE_INVENSENSE_COMPASS_CAL),0)
LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL
endif
ifeq ($(COMPILE_THIRD_PARTY_ACCEL),1)
LOCAL_CFLAGS += -DTHIRD_PARTY_ACCEL
endif
else # release builds, default
LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL
endif

LOCAL_SRC_FILES += SensorBase.cpp
LOCAL_SRC_FILES += MPLSensor.cpp
LOCAL_SRC_FILES += MPLSupport.cpp
LOCAL_SRC_FILES += InputEventReader.cpp

ifneq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug user))
ifeq ($(COMPILE_INVENSENSE_COMPASS_CAL),0)
LOCAL_SRC_FILES += AkmSensor.cpp
LOCAL_SRC_FILES += CompassSensor.AKM.cpp
else ifeq ($(COMPILE_INVENSENSE_SENSOR_ON_PRIMARY_BUS), 1)
LOCAL_SRC_FILES += CompassSensor.IIO.primary.cpp
LOCAL_CFLAGS += -DSENSOR_ON_PRIMARY_BUS
else
LOCAL_SRC_FILES += CompassSensor.IIO.9150.cpp
endif
else # release builds, default
LOCAL_SRC_FILES += AkmSensor.cpp
LOCAL_SRC_FILES += CompassSensor.AKM.cpp
endif # eng, userdebug & user builds

LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite/linux
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include/linux

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libmllite
LOCAL_SHARED_LIBRARIES += libhardware

# Additions for SysPed
LOCAL_SHARED_LIBRARIES += libmplmpu
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mpl
LOCAL_CPPFLAGS += -DLINUX=1 -Wno-error
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES += libmllite
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite
LOCAL_CPPFLAGS += -DLINUX=1
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#endif # !TARGET_SIMULATOR

# Build a temporary HAL that links the InvenSense .so
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.$(TARGET_BOARD_HARDWARE)
LOCAL_PROPRIETARY_MODULE := true
$(info YD>>LOCAL_MODULE=$(LOCAL_MODULE))

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SHARED_LIBRARIES += libmplmpu
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite/linux
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mpl
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include/linux
LOCAL_C_INCLUDES += bionic/libc

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"

ifeq ($(BOARD_GRAVITY_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DGRAVITY_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_COMPASS_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DCOMPASS_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_GYROSCOPE_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DGYROSCOPE_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_PROXIMITY_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DPROXIMITY_SENSOR_SUPPORT=1
endif

ifeq ($(BOARD_LIGHT_SENSOR_SUPPORT), true)
LOCAL_CFLAGS += -DLIGHT_SENSOR_SUPPORT=1
endif

ifeq ($(VERSION_L),true)
LOCAL_CFLAGS += -DANDROID_LOLLIPOP
else
ifeq ($(VERSION_KK),true)
LOCAL_CFLAGS += -DANDROID_KITKAT
else
LOCAL_CFLAGS += -DANDROID_JELLYBEAN
endif
endif

ifeq ($(BOARD_SENSOR_MPU_PAD),true)
LOCAL_CFLAGS += -DSAMPLE_RATE_200HZ
LOCAL_CFLAGS += -DSENSOR_MPU_PAD
endif

ifneq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug user))
ifneq ($(COMPILE_INVENSENSE_COMPASS_CAL),0)
LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL
endif
ifeq ($(COMPILE_THIRD_PARTY_ACCEL),1)
LOCAL_CFLAGS += -DTHIRD_PARTY_ACCEL
endif
ifeq ($(COMPILE_INVENSENSE_SENSOR_ON_PRIMARY_BUS), 1)
LOCAL_SRC_FILES += CompassSensor.IIO.primary.cpp
LOCAL_CFLAGS += -DSENSOR_ON_PRIMARY_BUS
else
LOCAL_SRC_FILES += CompassSensor.IIO.9150.cpp
endif
else # release builds, default
LOCAL_SRC_FILES += CompassSensor.IIO.9150.cpp
endif # eng, userdebug & user

ifeq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug user))
ifneq ($(filter manta grouper tilapia, $(TARGET_DEVICE)),)
# it's already defined in some other Makefile for production builds
#LOCAL_SRC_FILES := sensors_mpl.cpp
else
LOCAL_SRC_FILES := sensors_mpl.cpp
endif
else    # eng, userdebug & user builds
LOCAL_SRC_FILES := sensors_mpl.cpp
endif   # eng, userdebug & user builds
LOCAL_SRC_FILES += SamsungSensorBase.cpp LightSensor.cpp ProximitySensor.cpp
LOCAL_SHARED_LIBRARIES := libinvensense_hal
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libmllite
LOCAL_SHARED_LIBRARIES += libhardware

LOCAL_CFLAGS += -Wno-error
$(info YD>>LOCAL_MODULE=$(LOCAL_MODULE), LOCAL_SRC_FILES=$(LOCAL_SRC_FILES), LOCAL_SHARED_LIBRARIES=$(LOCAL_SHARED_LIBRARIES))
include $(BUILD_SHARED_LIBRARY)

#
# Build sensor_test
sensor_src_files := $(LOCAL_SRC_FILES)
sensor_shared_lib := $(LOCAL_SHARED_LIBRARIES)
sensor_cflags := $(LOCAL_CFLAGS)
sensor_c_include := $(LOCAL_C_INCLUDES)
include $(CLEAR_VARS)
LOCAL_MODULE := sensor_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := $(sensor_src_files)
LOCAL_SHARED_LIBRARIES := $(sensor_shared_lib)
LOCAL_CFLAGS := $(sensor_cflags)
LOCAL_C_INCLUDES := $(sensor_c_include)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

ifneq (${TARGET_ARCH},arm64)

include $(CLEAR_VARS)
LOCAL_MODULE := libmplmpu
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := libmplmpu.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := invensense
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libmllite
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := libmllite.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := invensense
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)
include $(BUILD_PREBUILT)

endif

# Build self_test bin
#ifeq (${TARGET_ARCH},arm64)

include $(CLEAR_VARS)

LOCAL_MODULE := inv_self_test
LOCAL_PROPRIETARY_MODULE := true
#LOCAL_32_BIT_ONLY := true
LOCAL_CFLAGS += -DLINUX

LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mllite/linux
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/mpl
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/software/core/driver/include/linux

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := software/simple_apps/self_test/inv_self_test.c

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libc
LOCAL_SHARED_LIBRARIES += libm
LOCAL_SHARED_LIBRARIES += libz
LOCAL_SHARED_LIBRARIES += libstdc++
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libmplmpu
LOCAL_SHARED_LIBRARIES += libmllite
include $(BUILD_EXECUTABLE)

#endif
