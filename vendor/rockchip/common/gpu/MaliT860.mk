# $(info 'in MaliT860.mk')
# $(info TARGET_BOARD_PLATFORM_GPU:$(TARGET_BOARD_PLATFORM_GPU) )
# $(info TARGET_ARCH:$(TARGET_ARCH) )

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)
DRIVER_PATH := kernel/drivers/gpu/arm/midgard/midgard_kbase.ko
HAS_BUILD_KERNEL := $(shell test -e $(DRIVER_PATH) && echo true)

ifneq ($(strip $(HAS_BUILD_KERNEL)), true)
# Move to Android.mk
BOARD_VENDOR_KERNEL_MODULES += \
	vendor/rockchip/common/gpu/MaliT860/lib/modules/mali_kbase.ko
else
BOARD_VENDOR_KERNEL_MODULES += \
	$(DRIVER_PATH)
endif
endif

PRODUCT_PACKAGES += \
	libGLES_mali

ifeq ($(strip $(ENABLE_STEREO_DEFORM)), true)
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/gpu/MaliT860/lib/arm/libGLES_mali.so:system/lib/egl/libGLES_mali.so \
	vendor/rockchip/common/gpu/MaliT860/lib/arm64/libGLES_mali.so:system/lib64/egl/libGLES_mali.so
endif
