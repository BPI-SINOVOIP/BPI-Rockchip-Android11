include device/rockchip/rk3399/BoardConfig.mk

BOARD_SENSOR_ST := true
BOARD_SENSOR_COMPASS_AK8963-64 := true
BOARD_SENSOR_MPU_PAD := false
BOARD_COMPASS_SENSOR_SUPPORT := true
BOARD_GYROSCOPE_SENSOR_SUPPORT := true
CAMERA_SUPPORT_AUTOFOCUS:= false

BOARD_CAMERA_SUPPORT := true
BOARD_CAMERA_SUPPORT_EXT := true
PRODUCT_KERNEL_DTS := rk3399-evb-ind-lpddr4-android-avb

# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_ROCKCHIP_VIRTUAL_AB_ENABLE := false
BOARD_HAS_RK_4G_MODEM := false

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk3399/rk3399_Android11/recovery.fstab_AB
endif
