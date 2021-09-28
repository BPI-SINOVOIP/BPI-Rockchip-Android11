###############################################################################
# GPS HAL libraries
LOCAL_PATH := $(call my-dir)
ifeq ($(strip $(BLUETOOTH_USE_BPLUS)),true)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)gps/ap6xxx/gps.default.so:system/lib/hw/gps.default.so \
    $(LOCAL_PATH)gps/ap6xxx/glgps:system/bin/glgps \
    $(LOCAL_PATH)gps/ap6xxx/gpslogd:system/bin/gpslogd \
    $(LOCAL_PATH)gps/ap6xxx/gpsconfig.xml:system/etc/gps/gpsconfig.xml
endif

