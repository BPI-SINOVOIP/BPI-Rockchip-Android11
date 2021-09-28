
LOCAL_PATH := $(call my-dir)

###########################################################
# Variant: Privileged app upgrade

include $(CLEAR_VARS)

LOCAL_MODULE := CtsShimPrivUpgradePrebuilt
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_CLASS := APPS
LOCAL_BUILT_MODULE_STEM := package.apk
# Make sure the build system doesn't try to resign the APK
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

# The 'arm' apk has both arm and arm64 so's. Same for x86/x86_64.
my_apk_dir := $(subst arm64,arm,$(TARGET_ARCH))
my_apk_dir := $(subst x86_64,x86,$(my_apk_dir))
LOCAL_REPLACE_PREBUILT_APK_INSTALLED := $(LOCAL_PATH)/apk/$(my_apk_dir)/CtsShimPrivUpgrade.apk

include $(BUILD_PREBUILT)

###########################################################
# Variant: Privileged app upgrade (wrong SHA)

include $(CLEAR_VARS)

LOCAL_MODULE := CtsShimPrivUpgradeWrongSHAPrebuilt
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE_CLASS := APPS
LOCAL_BUILT_MODULE_STEM := package.apk
# Make sure the build system doesn't try to resign the APK
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_COMPATIBILITY_SUITE := cts vts10 general-tests

LOCAL_REPLACE_PREBUILT_APK_INSTALLED := $(LOCAL_PATH)/apk/$(my_apk_dir)/CtsShimPrivUpgradeWrongSHA.apk

include $(BUILD_PREBUILT)

my_apk_dir :=
