#set TARGET_USES_HARDWARE_QCOM_GPS to false to disable this project.

ifeq ($(TARGET_USES_HARDWARE_QCOM_GPS),)
  ifneq ($(filter sdm845 sdm710, $(TARGET_BOARD_PLATFORM)),)
    TARGET_USES_HARDWARE_QCOM_GPS := false
  else ifeq ($(BOARD_IS_AUTOMOTIVE),true)
    TARGET_USES_HARDWARE_QCOM_GPS := false
  else
    TARGET_USES_HARDWARE_QCOM_GPS := true
  endif
endif

ifeq ($(TARGET_USES_HARDWARE_QCOM_GPS),true)
  ifneq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE),)
    LOCAL_PATH := $(call my-dir)
    ifeq ($(BOARD_VENDOR_QCOM_LOC_PDK_FEATURE_SET),true)

      ifneq ($(filter apq8064,$(TARGET_BOARD_PLATFORM)),)
        #For apq8064 use msm8960
        include $(call all-named-subdir-makefiles,msm8960)
      else ifneq ($(filter msm8992,$(TARGET_BOARD_PLATFORM)),)
        #For msm8992 use msm8994
        include $(call all-named-subdir-makefiles,msm8994)
      else ifneq ($(filter msm8960 msm8084 msm8994 msm8996 msm8998,$(TARGET_BOARD_PLATFORM)),)
        include $(call all-named-subdir-makefiles,$(TARGET_BOARD_PLATFORM))
      else ifeq ($(filter msm8916,$(TARGET_BOARD_PLATFORM)),)
        #For all other targets besides msm8916
        GPS_DIRS=core utils loc_api platform_lib_abstractions etc
        include $(call all-named-subdir-makefiles,$(GPS_DIRS))
      endif #TARGET_BOARD_PLATFORM

    else
      ifneq ($(filter msm8909 msm8226 ,$(TARGET_BOARD_PLATFORM)),)
        ifeq ($(TARGET_SUPPORTS_QCOM_3100),true)
          # For SD3100.
          include $(call all-named-subdir-makefiles,msm8909w_3100)
        else
          #For msm8909 target
          GPS_DIRS=msm8909/core msm8909/utils msm8909/loc_api msm8909/etc
          include $(call all-named-subdir-makefiles,$(GPS_DIRS))
        endif
      else ifeq ($(filter msm8916 ,$(TARGET_BOARD_PLATFORM)),)
        GPS_DIRS=core utils loc_api platform_lib_abstractions etc
        include $(call all-named-subdir-makefiles,$(GPS_DIRS))
      endif
    endif #BOARD_VENDOR_QCOM_LOC_PDK_FEATURE_SET

  endif #BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE
endif
