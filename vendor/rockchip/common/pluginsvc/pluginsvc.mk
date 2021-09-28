
ifeq ($(strip $(TARGET_BOARD_PLATFORM)), sofia3gr)
PRODUCT_PACKAGES += \
	pluginservice \
	libpluginservice \
	librkplugin
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3188)
PRODUCT_PACKAGES += \
	pluginservice \
	libpluginservice
endif
