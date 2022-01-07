

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
