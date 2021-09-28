include device/rockchip/rk3399/BoardConfig.mk

BOARD_SENSOR_ST := true
BOARD_SENSOR_MPU_PAD := false
BUILD_WITH_GOOGLE_GMS_EXPRESS := false
CAMERA_SUPPORT_AUTOFOCUS:= false

BOARD_BOOT_HEADER_VERSION := 1
# AB image definition
BOARD_USES_AB_IMAGE := false

ifneq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    BOARD_RECOVERY_MKBOOTIMG_ARGS := --second $(TARGET_PREBUILT_RESOURCE) --header_version 1
    # Android Q use odm instead of oem, but for upgrading to Q, partation list cant be changed, odm will mount at /dev/block/by-name/oem
    BOARD_ODMIMAGE_PARTITION_SIZE := $(shell python device/rockchip/common/get_partition_size.py device/rockchip/rk3399/rk3399_mid/parameter.txt oem)
endif

# No need to place dtb into boot.img for the device upgrading to Q.
BOARD_INCLUDE_DTB_IN_BOOTIMG :=
BOARD_PREBUILT_DTBIMAGE_DIR :=

ifneq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
    #Need to build system as root for the device upgrading to Q.
    BOARD_BUILD_SYSTEM_ROOT_IMAGE := true
endif

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    ifeq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
        PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk3399/rk3399_mid/fstab_ab_retrofit.in
        PRODUCT_DTBO_TEMPLATE := device/rockchip/rk3399/rk3399_mid/dt-overlay_ab_retrofit.in
    else
        PRODUCT_FSTAB_TEMPLATE := device/rockchip/rk3399/rk3399_mid/fstab_ab.in
        PRODUCT_DTBO_TEMPLATE := device/rockchip/rk3399/rk3399_mid/dt-overlay_ab.in
    endif
    include device/rockchip/common/BoardConfig_AB.mk
    ifeq ($(strip $(BOARD_USES_AB_LEGACY_RETROFIT)), true)
        TARGET_RECOVERY_FSTAB := device/rockchip/rk3399/rk3399_mid/recovery.fstab_AB_retrofit
    else
        TARGET_RECOVERY_FSTAB := device/rockchip/rk3399/rk3399_mid/recovery.fstab_AB
    endif
endif
