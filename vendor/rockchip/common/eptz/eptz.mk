LOCAL_PATH := $(call my-dir)

ifneq ($(filter rk356x, $(TARGET_BOARD_PLATFORM)), )
PRODUCT_PACKAGES += \
	libeptz         \
	librockx
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/eptz/model/face_detection_v2_horizontal.data:$(TARGET_COPY_OUT_VENDOR)/etc/model/face_detection_v2_horizontal.data \
    vendor/rockchip/common/eptz/eptz_zoom.conf:$(TARGET_COPY_OUT_VENDOR)/etc/eptz_zoom.conf
endif

