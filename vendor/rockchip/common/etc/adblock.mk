ifeq ($(strip $(TARGET_ARCH)), arm)
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/etc/.allBlock:system/etc/.allBlock \
	vendor/rockchip/common/etc/.videoBlock:system/etc/.videoBlock \
	vendor/rockchip/common/etc/librkbm.so:system/lib/librkbm.so \
	vendor/rockchip/common/etc/libmultiwindow.so:system/lib/libmultiwindow.so \
    vendor/rockchip/common/etc/bmi:system/bin/bmi \
    vendor/rockchip/common/etc/bmd:system/bin/bmd

endif
