$(call inherit-product-if-exists, $(LOCAL_PATH)/app/app.mk)

MFG_FWS := $(shell ls $(LOCAL_PATH)/mfg)
PRODUCT_COPY_FILES += \
	$(foreach file, $(MFG_FWS), $(LOCAL_PATH)/mfg/$(file):system/etc/firmware/mfg/$(file))

EXE_FILES := $(shell ls $(LOCAL_PATH)/bin)
PRODUCT_COPY_FILES += \
	$(foreach file, $(EXE_FILES), $(LOCAL_PATH)/bin/$(file):system/bin/$(file))
