include device/rockchip/rk3399/BoardConfig.mk

BOARD_SENSOR_ST := true
BOARD_SENSOR_COMPASS_AK8963-64 := true
BOARD_SENSOR_MPU_PAD := false
BOARD_COMPASS_SENSOR_SUPPORT := true
BOARD_GYROSCOPE_SENSOR_SUPPORT := true
BUILD_WITH_GOOGLE_GMS_EXPRESS := false
CAMERA_SUPPORT_AUTOFOCUS:= false

# AB image definition
BOARD_USES_AB_IMAGE := false
BOARD_USES_VIRTUAL_AB_RETROFIT := false

BOARD_HAS_RK_4G_MODEM := true
ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    include device/rockchip/common/BoardConfig_AB.mk
    TARGET_RECOVERY_FSTAB := device/rockchip/rk3399/rk3399_Android10/recovery.fstab_AB
endif
