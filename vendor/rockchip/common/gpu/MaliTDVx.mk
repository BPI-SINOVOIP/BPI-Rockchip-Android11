ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-tDVx)
ifeq ($(strip $(TARGET_ARCH)), arm64)
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/gpu/MaliTDVx/lib/arm64/libGLES_mali.so:$(TARGET_COPY_OUT_VENDOR)/lib64/egl/libGLES_mali.so
endif
DRIVER_PATH := kernel/drivers/gpu/arm/bifrost/bifrost_kbase.ko
HAS_BUILD_KERNEL := $(shell test -e $(DRIVER_PATH) && echo true)

PRODUCT_COPY_FILES += \
	vendor/rockchip/common/gpu/MaliTDVx/lib/arm/libGLES_mali.so:$(TARGET_COPY_OUT_VENDOR)/lib/egl/libGLES_mali.so

ifneq ($(strip $(HAS_BUILD_KERNEL)), true)
BOARD_VENDOR_KERNEL_MODULES += \
	vendor/rockchip/common/gpu/MaliTDVx/lib/modules/mali_kbase.ko
else
BOARD_VENDOR_KERNEL_MODULES += \
	$(DRIVER_PATH)
endif
endif
