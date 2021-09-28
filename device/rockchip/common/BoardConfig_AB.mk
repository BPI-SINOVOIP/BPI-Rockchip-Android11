#
# Copyright 2014 The Android Open-Source Project
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

AB_OTA_UPDATER := true
TARGET_NO_RECOVERY := true
BOARD_USES_RECOVERY_AS_BOOT := true
USE_AB_PARAMETER := $(shell test -f $(TARGET_DEVICE_DIR)/parameter_ab.txt && echo true)
ifeq ($(strip $(USE_AB_PARAMETER)), true)
    ifeq ($(PRODUCT_USE_DYNAMIC_PARTITIONS), true)
        ifeq ($(PRODUCT_RETROFIT_DYNAMIC_PARTITIONS), true)
            BOARD_SUPER_PARTITION_METADATA_DEVICE := system
            BOARD_SUPER_PARTITION_BLOCK_DEVICES := system vendor odm
            BOARD_SUPER_PARTITION_SYSTEM_DEVICE_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt system_a)
            BOARD_SUPER_PARTITION_VENDOR_DEVICE_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt vendor_a)
            BOARD_SUPER_PARTITION_ODM_DEVICE_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt odm_a)

            BOARD_SUPER_PARTITION_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SYSTEM_DEVICE_SIZE) + $(BOARD_SUPER_PARTITION_VENDOR_DEVICE_SIZE) + $(BOARD_SUPER_PARTITION_ODM_DEVICE_SIZE))
            BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SIZE) - 4194304)
        else
            BOARD_SUPER_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt super)
            ifeq ($(BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE), true)
                BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SIZE) - 4194304)
            else
                BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SIZE) / 2 - 4194304)
            endif
        endif
    else
        BOARD_SYSTEMIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt system_a)
        BOARD_VENDORIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt vendor_a)
        BOARD_ODMIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt odm_a)
    endif
    BOARD_CACHEIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt cache)
    BOARD_BOOTIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt boot_a)
    BOARD_DTBOIMG_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt dtbo_a)
    # Header V3, add vendor_boot
    ifeq (1,$(strip $(shell expr $(BOARD_BOOT_HEADER_VERSION) \>= 3)))
        BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py $(TARGET_DEVICE_DIR)/parameter_ab.txt vendor_boot_a)
    endif
    #$(info Calculated BOARD_SYSTEMIMAGE_PARTITION_SIZE=$(BOARD_SYSTEMIMAGE_PARTITION_SIZE) use $(TARGET_DEVICE_DIR)/parameter_ab.txt)
else
    ifeq ($(PRODUCT_USE_DYNAMIC_PARTITIONS), true)
        ifneq ($(PRODUCT_RETROFIT_DYNAMIC_PARTITIONS), true)
            ifeq ($(BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE), true)
                ifeq ($(BUILD_WITH_GO_OPT), true)
                    ifeq ($(strip $(TARGET_ARCH)), arm64) # arm64 go
                        BOARD_SUPER_PARTITION_SIZE := 2390753280
                    else # arm go
                        BOARD_SUPER_PARTITION_SIZE := 1971322880
                    endif
                else # non-go
                    BOARD_SUPER_PARTITION_SIZE := 3263168512
                endif
                BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SIZE) - 4194304)
            else
                BOARD_SUPER_PARTITION_SIZE := 5372903424
                BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_SIZE := $(shell expr $(BOARD_SUPER_PARTITION_SIZE) / 2 - 4194304)
            endif
            BOARD_BOOTIMAGE_PARTITION_SIZE := 100663296
        endif
    endif
endif
ifeq ($(strip $(BOARD_AVB_ENABLE)), true)
    BOARD_KERNEL_CMDLINE := androidboot.wificountrycode=CN androidboot.hardware=rk30board androidboot.console=ttyFIQ0 firmware_class.path=/vendor/etc/firmware init=/init rootwait ro init=/init
else
    BOARD_KERNEL_CMDLINE := console=ttyFIQ0 androidboot.baseband=N/A androidboot.wificountrycode=CN androidboot.veritymode=enforcing androidboot.hardware=rk30board androidboot.console=ttyFIQ0 androidboot.verifiedbootstate=orange firmware_class.path=/vendor/etc/firmware init=/init rootwait ro init=/init
endif
ifneq ($(strip $(BOARD_SELINUX_ENFORCING)), true)
    BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
endif
ROCKCHIP_RECOVERYIMAGE_CMDLINE_ARGS := console=ttyFIQ0 androidboot.baseband=N/A androidboot.selinux=permissive androidboot.wificountrycode=CN androidboot.veritymode=enforcing androidboot.hardware=rk30board androidboot.console=ttyFIQ0 firmware_class.path=/vendor/etc/firmware init=/init
TARGET_RECOVERY_FSTAB := $(TARGET_DEVICE_DIR)/recovery.fstab_AB
