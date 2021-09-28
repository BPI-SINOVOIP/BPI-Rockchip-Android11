PRODUCT_COPY_FILES += \
      hardware/google/pixel/mm/pixel-mm.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/pixel-mm.rc

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES += \
    mm_logd
endif

# ZRAM writeback
PRODUCT_PROPERTY_OVERRIDES += \
    ro.zram.mark_idle_delay_mins=60 \
    ro.zram.first_wb_delay_mins=1440 \
    ro.zram.periodic_wb_delay_hours=24

BOARD_SEPOLICY_DIRS += hardware/google/pixel-sepolicy/mm
