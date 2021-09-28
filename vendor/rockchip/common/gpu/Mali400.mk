ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali400)
ifeq ($(strip $(TARGET_ARCH)), arm)
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcdc_composer=0
PRODUCT_PROPERTY_OVERRIDES += debug.hwui.render_dirty_regions=false
PRODUCT_PACKAGES += \
	libGLES_mali
endif
endif
