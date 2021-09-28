CUR_PATH := device/rockchip/common/bootshutdown

HAVE_BOOT_ANIMATION := $(shell test -f $(CUR_PATH)/bootanimation.zip && echo yes)
HAVE_SHUTDOWN_ANIMATION := $(shell test -f $(CUR_PATH)/shutdownanimation.zip && echo yes)

ifeq ($(HAVE_BOOT_ANIMATION), yes)
PRODUCT_COPY_FILES += $(CUR_PATH)/bootanimation.zip:$(TARGET_COPY_OUT_ODM)/media/bootanimation.zip
endif
ifeq ($(HAVE_SHUTDOWN_ANIMATION), yes)
PRODUCT_COPY_FILES += $(CUR_PATH)/shutdownanimation.zip:$(TARGET_COPY_OUT_ODM)/media/shutdownanimation.zip
endif
