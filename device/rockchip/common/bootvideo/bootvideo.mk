CUR_PATH := device/rockchip/common/bootvideo

HAVE_BOOT_VIDEO := $(shell test -f $(CUR_PATH)/bootanimation.ts && echo yes)

ifeq ($(HAVE_BOOT_VIDEO), yes)
PRODUCT_COPY_FILES += $(CUR_PATH)/bootanimation.ts:product/media/bootanimation.ts
endif

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.bootvideo.enable=true \
        persist.sys.bootvideo.showtime=-2
