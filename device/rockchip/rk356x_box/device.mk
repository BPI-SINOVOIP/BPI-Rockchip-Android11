#
# Copyright (c) 2020 Rockchip Electronics Co., Ltd
#
PRODUCT_PACKAGES += \
    displayd \
    libion

# Disable partial updates
PRODUCT_PROPERTY_OVERRIDES += \
    debug.hwui.use_partial_updates=false

BOARD_SEPOLICY_DIRS += vendor/rockchip/hardware/interfaces/neuralnetworks/1.0/default/sepolicy
PRODUCT_PACKAGES += \
    public.libraries-rockchip \
    librknnhal_bridge.rockchip \
    librknn_api_android \
    rockchip.hardware.neuralnetworks@1.0-impl \
    rockchip.hardware.neuralnetworks@1.0-service

$(call inherit-product-if-exists, vendor/rockchip/common/npu/npu.mk)

BOARD_SEPOLICY_DIRS += device/rockchip/rk356x_box/sepolicy_vendor

# enable this for support f2fs with data partion
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs

# used for fstab_generator, sdmmc controller address
PRODUCT_BOOT_DEVICE := fe310000.sdhci,fe330000.nandc

# This ensures the needed build tools are available.
# TODO: make non-linux builds happy with external/f2fs-tool; system/extras/f2fs_utils
ifeq ($(HOST_OS),linux)
  TARGET_USERIMAGES_USE_F2FS := true
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.recovery.rk30board.rc:recovery/root/init.recovery.rk30board.rc \
    vendor/rockchip/common/bin/$(TARGET_ARCH)/busybox:recovery/root/sbin/busybox \

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.rk356x.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.rk356x.rc \
    $(LOCAL_PATH)/wake_lock_filter.xml:system/etc/wake_lock_filter.xml \
    $(LOCAL_PATH)/package_performance.xml:$(TARGET_COPY_OUT_ODM)/etc/package_performance.xml \
    $(TARGET_DEVICE_DIR)/media_profiles_default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml \
    $(LOCAL_PATH)/etc/external_camera_config.xml:$(TARGET_COPY_OUT_VENDOR)/etc/external_camera_config.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml

# copy xml files for Vulkan features.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute-0.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-0.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level-1.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_0_3.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version-1_1.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2019-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.vulkan.deqp.level-2019-03-01.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2020-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.vulkan.deqp.level-2020-03-01.xml

# Get the long list of APNs
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/apns-full-conf.xml:system/etc/apns-conf.xml
PRODUCT_COPY_FILES += vendor/rockchip/common/phone/etc/spn-conf.xml:system/etc/spn-conf.xml
PRODUCT_PROPERTY_OVERRIDES += \
			      ro.product.ota.host = 192.168.1.1:8888 \
    ro.vendor.sdkversion = $(CURRENT_SDK_VERSION) \
    vendor.gralloc.disable_afbc = 0

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/public.libraries.txt:vendor/etc/public.libraries.txt

TARGET_BOARD_CONCISE_RESOLUTION := false
# remove some resolution which are not commonly used
ifeq ($(TARGET_BOARD_CONCISE_RESOLUTION),true)
PRODUCT_COPY_FILES += \
    device/rockchip/rk356x_box/etc/resolution_white.xml:/system/usr/share/resolution_white.xml
endif

#add for camera aiq2.0
#
PRODUCT_PACKAGES += \
       rkaiq_tool_server \
       rkaiq_3A_server
PRODUCT_COPY_FILES += \
       external/camera_engine_rkaiq/rkisp2x_tuner/reset_camera.sh:$(TARGET_COPY_OUT_VENDOR)/etc/camera/reset_camera.sh

# default enalbe mipi camera for box, noeed disable please
TARGET_BOARD_USE_MIPI_CAMEAR := true

# for box mipi camera
ifeq ($(TARGET_BOARD_USE_MIPI_CAMEAR),true)
CAMERA_ETC_PATH := $(TOP)/hardware/rockchip/camera/etc
IQ_FILES_PATH := $(TOP)/external/camera_engine_rkaiq/iqfiles
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/etc/camera/hardware/rockchip/camera/etc/camera/camera3_profiles_rk356x.xml:$(TARGET_COPY_OUT_VENDOR)/etc/camera/camera3_profiles.xml \
	$(call find-copy-subdir-files,*,$(CAMERA_ETC_PATH)/firmware,$(TARGET_COPY_OUT_VENDOR)/firmware) \
	$(call find-copy-subdir-files,*,$(CAMERA_ETC_PATH)/camera,$(TARGET_COPY_OUT_VENDOR)/etc/camera) \
	$(call find-copy-subdir-files,*,$(IQ_FILES_PATH)/,$(TARGET_COPY_OUT_VENDOR)/etc/camera/rkisp2/) \
	$(call find-copy-subdir-files,*,$(LOCAL_PATH)/etc/camera/external/camera_engine_rkaiq/iqfiles,$(TARGET_COPY_OUT_VENDOR)/etc/camera/rkisp2/)
endif

#
# add Rockchip properties here
#
PRODUCT_PROPERTY_OVERRIDES += \
	ro.vendor.rk_sdk=1 \
	ro.ril.ecclist=112,911 \
	ro.opengles.version=196610 \
	wifi.interface=wlan0 \
	ro.audio.monitorOrientation=true \
	debug.nfc.fw_download=false \
	debug.nfc.se=false \
	vendor.hwc.compose_policy=1 \
	sys.wallpaper.rgb565=0 \
	sf.power.control=2073600 \
	sys.rkadb.root=0 \
	ro.sf.fakerotation=false \
	ro.tether.denied=false \
	sys.resolution.changed=false \
	ro.default.size=100 \
	ro.product.usbfactory=rockchip_usb \
	wifi.supplicant_scan_interval=15 \
	ro.factory.tool=0 \
	ro.kernel.android.checkjni=0 \
	ro.build.shutdown_timeout=6 \
	persist.enable_task_snapshots=false \
	persist.sys.show_color_option=false \
	persist.bt.power.down=true \
	ro.wifi.sleep.power.down=true \
	persist.wifi.sleep.delay.ms=0 \
	persist.enable_task_snapshots=false \
	ro.vendor.frameratelock=true
