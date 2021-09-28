#
# Copyright 2019 The Android Open-Source Project
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
#

ifneq ($(filter generic_% generic, $(TARGET_DEVICE)),)
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
EMU_EXTRA_FILES := \
        $(PRODUCT_OUT)/system-qemu-config.txt \
        $(PRODUCT_OUT)/ramdisk-qemu.img \
        $(PRODUCT_OUT)/misc_info.txt \
        $(PRODUCT_OUT)/vbmeta.img \
        $(PRODUCT_OUT)/VerifiedBootParams.textproto \
        $(foreach p,$(BOARD_SUPER_PARTITION_PARTITION_LIST),$(PRODUCT_OUT)/$(p).img)

ifeq ($(filter sdk_gphone_%, $(TARGET_PRODUCT)),)
ifeq ($(TARGET_BUILD_VARIANT),user)
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/user/advancedFeatures.ini
else
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/advancedFeatures.ini
endif
else
ifeq ($(TARGET_BUILD_VARIANT),user)
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/google/user/advancedFeatures.ini
else
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/google/userdebug/advancedFeatures.ini
endif
endif

EMU_EXTRA_FILES += device/generic/goldfish/data/etc/config.ini
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/encryptionkey.img
EMU_EXTRA_FILES += device/generic/goldfish/data/etc/userdata.img

name := emu-extra-linux-system-images-$(FILE_NAME_TAG)

EMU_EXTRA_TARGET := $(PRODUCT_OUT)/$(name).zip

EMULATOR_KERNEL_ARCH := $(TARGET_ARCH)
EMULATOR_KERNEL_DIST_NAME := kernel-ranchu
# Below should be the same as PRODUCT_KERNEL_VERSION set in
# device/generic/goldfish/(arm|x86)*-vendor.mk
EMULATOR_KERNEL_VERSION := 5.4

# Use 64-bit kernel even for 32-bit Android
ifeq ($(TARGET_ARCH), x86)
EMULATOR_KERNEL_ARCH := x86_64
EMULATOR_KERNEL_DIST_NAME := kernel-ranchu-64
endif
ifeq ($(TARGET_ARCH), arm)
EMULATOR_KERNEL_ARCH := arm64
EMULATOR_KERNEL_DIST_NAME := kernel-ranchu-64
endif

EMULATOR_KERNEL_FILE := prebuilts/qemu-kernel/$(EMULATOR_KERNEL_ARCH)/$(EMULATOR_KERNEL_VERSION)/kernel-qemu2

$(EMU_EXTRA_TARGET): PRIVATE_PACKAGE_SRC := \
        $(call intermediates-dir-for, PACKAGING, emu_extra_target)

$(EMU_EXTRA_TARGET): $(EMU_EXTRA_FILES) $(EMULATOR_KERNEL_FILE) $(SOONG_ZIP)
	@echo "Package: $@"
	rm -rf $@ $(PRIVATE_PACKAGE_SRC)
	mkdir -p $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/prebuilts/qemu-kernel/$(TARGET_ARCH)
	touch $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/prebuilts/qemu-kernel/$(TARGET_ARCH)/kernel-qemu
	$(foreach f,$(EMU_EXTRA_FILES), cp $(f) $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/$(notdir $(f)) &&) true
	cp $(EMULATOR_KERNEL_FILE) $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/${EMULATOR_KERNEL_DIST_NAME}
	cp -r $(PRODUCT_OUT)/data $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)
	mkdir -p $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/system
	cp $(PRODUCT_OUT)/system/build.prop $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)/system
	$(SOONG_ZIP) -o $@ -C $(PRIVATE_PACKAGE_SRC) -D $(PRIVATE_PACKAGE_SRC)/$(TARGET_ARCH)

.PHONY: emu_extra_imgs
emu_extra_imgs: $(EMU_EXTRA_TARGET)

$(call dist-for-goals, emu_extra_imgs, $(EMU_EXTRA_TARGET))

include $(call all-makefiles-under,$(LOCAL_PATH))
endif
