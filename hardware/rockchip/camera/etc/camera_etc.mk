# prebuilt for config xml files in /vendor/etc/camera or /system/etc/camera
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 28)))
CUR_PATH := $(TOP)/hardware/rockchip/camera/etc
else
CUR_PATH := $(TOP)/hardware/rockchip/camera_v3/etc
endif

ifeq ($(filter box atv vr stbvr, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
ifeq (1,$(strip $(shell expr $(PLATFORM_SDK_VERSION) \>= 26)))
PRODUCT_COPY_FILES += \
	$(CUR_PATH)/camera/camera3_profiles_$(TARGET_BOARD_PLATFORM).xml:$(TARGET_COPY_OUT_VENDOR)/etc/camera/camera3_profiles.xml \
	$(call find-copy-subdir-files,*,$(CUR_PATH)/firmware,$(TARGET_COPY_OUT_VENDOR)/firmware) \
	$(call find-copy-subdir-files,*,$(CUR_PATH)/camera,$(TARGET_COPY_OUT_VENDOR)/etc/camera)
else
PRODUCT_COPY_FILES += \
	$(CUR_PATH)/camera/camera3_profiles_$(TARGET_BOARD_PLATFORM).xml:$(TARGET_COPY_OUT_SYSTEM)/etc/camera/camera3_profiles.xml \
	$(call find-copy-subdir-files,*,$(CUR_PATH)/firmware,$(TARGET_COPY_OUT_SYSTEM)/etc/firmware) \
	$(call find-copy-subdir-files,*,$(CUR_PATH)/camera,$(TARGET_COPY_OUT_SYSTEM)/etc/camera) \
	$(call find-copy-subdir-files,*,$(CUR_PATH)/tools,$(TARGET_COPY_OUT_SYSTEM)/bin)
endif

ifneq ($(filter rk1126 rk356x, $(strip $(TARGET_BOARD_PLATFORM))), )
IQ_FILES_PATH := $(TOP)/external/camera_engine_rkaiq/iqfiles/isp21
PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,$(IQ_FILES_PATH)/,$(TARGET_COPY_OUT_VENDOR)/etc/camera/rkisp2/)
endif
endif


