
CUR_PATH := vendor/rockchip/common

#########################################################
#   3G Dongle SUPPORT
#########################################################
PRODUCT_COPY_FILES += \
    $(CUR_PATH)/phone/etc/ppp/ip-down:system/etc/ppp/ip-down \
    $(CUR_PATH)/phone/etc/ppp/ip-up:system/etc/ppp/ip-up \
    $(CUR_PATH)/phone/etc/ppp/call-pppd:system/etc/ppp/call-pppd \
    $(CUR_PATH)/phone/etc/operator_table:system/etc/operator_table

ifeq ($(strip $(PRODUCT_MODEM)), DTS4108C)
PRODUCT_COPY_FILES += \
    $(CUR_PATH)/phone/bin/rild_dts4108c:system/bin/rild \
    $(CUR_PATH)/phone/lib/libreference-ril-dts4108c.so:system/lib/libreference-ril.so \
    $(CUR_PATH)/phone/lib/libril-dts4108c.so:system/lib/libril.so
endif

ifeq ($(strip $(BOARD_HAVE_DONGLE)),true)
PRODUCT_PACKAGES += \
    rild \
    libril-rk29-dataonly \
    usb_dongle \
    usb_modeswitch \
    chat

PRODUCT_PROPERTY_OVERRIDES +=ro.boot.noril=false
else
PRODUCT_PROPERTY_OVERRIDES +=ro.boot.noril=true
endif


PRODUCT_PROPERTY_OVERRIDES += \
    keyguard.no_require_sim=true \
    ro.com.android.dataroaming=true \
	ril.function.dataonly=1
