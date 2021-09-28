

PRODUCT_PACKAGES += \
    RKUpdateService

ifeq ($(strip $(TARGET_BOARD_HARDWARE)), rk30board)
ifeq ($(strip $(TARGET_ARCH)), arm)
PRODUCT_COPY_FILES += \
       vendor/rockchip/common/apps/RKDeviceTest/lib/arm/libstlport.so:system/lib/libstlport.so \
       vendor/rockchip/common/apps/RKUpdateService/lib/arm/librockchip_update_jni.so:system/lib/librockchip_update_jni.so
else ifeq ($(strip $(TARGET_ARCH)), arm64)
PRODUCT_COPY_FILES += \
       vendor/rockchip/common/apps/RKUpdateService/lib/arm/librockchip_update_jni.so:system/lib/librockchip_update_jni.so \
       vendor/rockchip/common/apps/RKUpdateService/lib/arm64/librockchip_update_jni.so:system/lib64/librockchip_update_jni.so \
       vendor/rockchip/common/apps/RKDeviceTest/lib/arm/libstlport.so:system/lib/libstlport.so \
       vendor/rockchip/common/apps/RKDeviceTest/lib/arm64/libstlport.so:system/lib64/libstlport.so
endif
endif

ifneq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
PRODUCT_PACKAGES += \
	       Lightning
endif

ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
PRODUCT_PACKAGES += \
    userExperienceService
ifeq ($(filter tablet box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
PRODUCT_PACKAGES += \
    RkApkinstaller
endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3288)
#PRODUCT_PACKAGES += \
#    WinStartService \
#    projectX
#endif

ifneq ($(filter rk312x rk3126c, $(TARGET_BOARD_PLATFORM)), )
PRODUCT_PACKAGES += \
    RkVideoPlayer
else
ifeq ($(strip $(PRODUCT_BUILD_MODULE)), px5car)
PRODUCT_PACKAGES += \
	Rk3grVideoPlayer
else
ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PACKAGES += \
    Rk4kVideoPlayer
endif
endif
endif
endif

ifeq ($(strip $(PRODUCT_BUILD_MODULE)), px5car)
PRODUCT_PACKAGES += \
    Rk3grExplorer
else
ifneq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PACKAGES += \
    RkExplorer
endif
endif


ifeq ($(strip $(BOARD_HAS_STRESSTEST_APP)), true)
    PRODUCT_PACKAGES += \
    StressTest 

ifneq ($(filter atv box, $(strip $(TARGET_BOARD_PLATFORM_PRODUCT))), )
PRODUCT_PACKAGES += \
    RKDeviceTest

else
PRODUCT_PACKAGES += \
    DeviceTest

endif
endif

##################for vr app#####################
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), vr)
PRODUCT_PACKAGES += \
		RockVRHome	\
		RKVRSettings	\
		RKVRExplorer	\
		RKVRGlobalActions	\
                RKVRPanorama	\
		RKVRPlayer	
endif

###########for box app ################
ifeq ($(strip $(TARGET_BOARD_PLATFORM_PRODUCT)), box)
PRODUCT_PACKAGES += \
    RKTvLauncher \
    MediaCenter \
    PinyinIME \
    WifiDisplay \
    DLNA

    ifeq ($(BOARD_TV_LOW_MEMOPT), false)
	    PRODUCT_PACKAGES += \
            ChangeLedStatus
	endif

  ifeq ($(strip $(BOARD_USE_LOW_MEM256)), true)
#        PRODUCT_PACKAGES += \
#              SimpleLauncher
  endif
endif
