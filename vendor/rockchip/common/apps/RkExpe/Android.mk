LOCAL_PATH := $(call my-dir)
ifeq ($(RECOVERY_WITH_RADICAL_UPDATE),true)
###############################################################################
# RkExpe
include $(CLEAR_VARS)
LOCAL_MODULE := RkExpe
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_TAGS := optional
LOCAL_BUILT_MODULE_STEM := package.apk
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_PRIVILEGED_MODULE := true
LOCAL_CERTIFICATE := platform
LOCAL_SRC_FILES := $(LOCAL_MODULE).apk
# LOCAL_REQUIRED_MODULES := userExperienceService.jar
#LOCAL_PREBUILT_JNI_LIBS :=

# 指定要将 enced_platform_keys 添加到 apk 中. 
# .KP : 
#   新增的 local 变量, 要添加到 'LEAR_VARS := build/core/clear_vars.mk' 中, 以免影响其他 模块. 
#   下面的 LOCAL_PUBLIC_KEY_TO_ENC_RANDOM_PROTECTION_KEY 同. 
LOCAL_ADD_ENCED_PLATFORM_KEYS_TO_APK := true
# 指定作为 PuK_to_enc_PrtK 的文件. 
LOCAL_PUBLIC_KEY_TO_ENC_RANDOM_PROTECTION_KEY := $(LOCAL_PATH)/PuK_to_enc_PrtK

include $(BUILD_PREBUILT)
endif
