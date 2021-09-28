ifneq ($(filter rk3399 rk3399pro, $(strip $(TARGET_BOARD_PLATFORM))), )
PRODUCT_PACKAGES += \
	RFTestTool

$(call inherit-product-if-exists, $(LOCAL_PATH)/broadcom/broadcom.mk)
endif
