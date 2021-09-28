ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t760)
ifeq ($(strip $(TARGET_ARCH)), arm)
# PRODUCT_COPY_FILES += \
    vendor/rockchip/common/gpu/MaliT760/lib/$(TARGET_ARCH)/rk3288w/libGLES_mali.so:vendor/lib/egl/libGLES_mali.so \

    # vendor/rockchip/common/gpu/MaliT760/lib/$(TARGET_ARCH)/rk3288/libGLES_mali.so:vendor/lib/egl/rk3288/libGLES_mali.so \

endif
# change to copy libGLES_mali.so in ./Android.mk and the below is required.
PRODUCT_PACKAGES += \
	libGLES_mali
endif


#
#ifeq ($(strip $(ENABLE_STEREO_DEFORM)), true)
#PRODUCT_COPY_FILES += \
#	vendor/rockchip/common/gpu/libs/libGLES.so:system/lib/egl/libGLES.so
#endif
###################for vr lib#####################
#ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
#PRODUCT_COPY_FILES += \
#	vendor/rockchip/common/gpu/libs/libvsync.so:system/lib/egl/libvsync.so
#endif
