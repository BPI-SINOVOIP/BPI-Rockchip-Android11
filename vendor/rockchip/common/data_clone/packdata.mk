ifeq ($(strip $(TARGET_ARCH)), arm)
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/data_clone/packdata.sh:system/bin/packdata.sh \
    vendor/rockchip/common/data_clone/bin/$(TARGET_ARCH)/packdata:system/bin/packdata \
    vendor/rockchip/common/data_clone/lib/$(TARGET_ARCH)/libext4_utils.so:system/lib/libext4_utils.so

endif
