#this only builds the vendor.img

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.zygote=zygote32

PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.logd.size.stats=64K \
    log.tag.stats_log=I

# Additional settings used in all AOSP builds
PRODUCT_PROPERTY_OVERRIDES := \
    ro.config.ringtone=Ring_Synth_04.ogg \
    ro.config.notification_sound=pixiedust.ogg

# Emulator for vendor
$(call inherit-product-if-exists, device/generic/goldfish/x86-vendor.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulator_vendor.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/board/generic_x86/device.mk)

# Overrides
PRODUCT_BRAND := google
PRODUCT_MANUFACTURER := Google
PRODUCT_NAME := sdk_phone_x86_vendor
PRODUCT_DEVICE := generic_x86
PRODUCT_MODEL := Android SDK built for x86
