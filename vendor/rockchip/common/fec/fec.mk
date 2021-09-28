LOCAL_PATH := $(call my-dir)

ifneq ($(filter rk356x, $(TARGET_BOARD_PLATFORM)), )
PRODUCT_PACKAGES += \
	libdistortion_gl
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/fec/meshXY.bin:$(TARGET_COPY_OUT_VENDOR)/etc/meshXY.bin
endif

