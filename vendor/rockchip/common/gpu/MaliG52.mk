ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-G52)

# libs of libGLES_mali.so are installed in ./Android.mk
PRODUCT_PACKAGES += \
        libGLES_mali \
        vulkan.$(TARGET_BOARD_PLATFORM)

DRIVER_PATH := kernel/drivers/gpu/arm/bifrost/bifrost_kbase.ko
HAS_BUILD_KERNEL := $(shell test -e $(DRIVER_PATH) && echo true)

ifneq ($(strip $(HAS_BUILD_KERNEL)), true)
BOARD_VENDOR_KERNEL_MODULES += \
	vendor/rockchip/common/gpu/MaliG52/lib/modules/mali_kbase.ko
else
BOARD_VENDOR_KERNEL_MODULES += \
	$(DRIVER_PATH)
endif

endif
