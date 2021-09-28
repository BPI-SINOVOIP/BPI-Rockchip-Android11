ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali450)

ifeq ($(strip $(TARGET_ARCH)), x86)
PRODUCT_PROPERTY_OVERRIDES += ro.sf.lcdc_composer=0
PRODUCT_PROPERTY_OVERRIDES += debug.hwui.render_dirty_regions=false
ifeq ($(strip $(GRAPHIC_MEMORY_PROVIDER)), dma_buf)
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/gpu/Mali450/lib/$(TARGET_ARCH)/libGLES_mali.so:system/lib/egl/libGLES_mali.so \
    vendor/rockchip/common/gpu/Mali450/lib/$(TARGET_ARCH)/libGLES_mali.so:obj/lib/libGLES_mali.so \
    vendor/rockchip/common/gpu/Mali450/lib/$(TARGET_ARCH)/hwuihas.bin:system/etc/hwuihas.bin \
    vendor/rockchip/common/gpu/Mali450/lib/$(TARGET_ARCH)/hwuishader.bin:system/etc/hwuishader.bin 
PRODUCT_COPY_FILES += \
    vendor/rockchip/common/gpu/gpu_performance/etc/performance_info.xml:system/etc/performance_info.xml \
    vendor/rockchip/common/gpu/gpu_performance/etc/packages-compat.xml:system/etc/packages-compat.xml \
    vendor/rockchip/common/gpu/gpu_performance/etc/packages-composer.xml:system/etc/packages-composer.xml \
#    vendor/rockchip/common/gpu/gpu_performance/bin/$(TARGET_ARCH)/performance:system/bin/performance \
    vendor/rockchip/common/gpu/gpu_performance/lib/$(TARGET_ARCH)/libperformance_runtime.so:system/lib/libperformance_runtime.so \
    vendor/rockchip/common/gpu/gpu_performance/lib/$(TARGET_ARCH)/gpu.$(TARGET_BOARD_HARDWARE).so:system/lib/hw/gpu.$(TARGET_BOARD_HARDWARE).so
endif
endif

ifeq ($(strip $(TARGET_ARCH)), arm64)
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/gpu/Mali450/lib/arm64/libGLES_mali.so:$(TARGET_COPY_OUT_VENDOR)/lib64/egl/libGLES_mali.so \
	vendor/rockchip/common/gpu/Mali450/lib/arm/libGLES_mali.so:$(TARGET_COPY_OUT_VENDOR)/lib/egl/libGLES_mali.so
else
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/gpu/Mali450/lib/arm/libGLES_mali.so:$(TARGET_COPY_OUT_VENDOR)/lib/egl/libGLES_mali.so
endif

endif
