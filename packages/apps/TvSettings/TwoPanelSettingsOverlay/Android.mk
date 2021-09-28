# Adapted from SettingsGoogle.

LOCAL_PATH := $(call my-dir)
SETTINGS_TWO_PANEL_PATH := $(LOCAL_PATH)
SETTINGS_PATH := $(LOCAL_PATH)/../Settings
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional


LOCAL_SRC_FILES := \
        $(call all-java-files-under, src) \
        $(call all-java-files-under, ../Settings/src) \
        $(call all-Iaidl-files-under, ../Settings/src) \

LOCAL_RESOURCE_DIR := \
    $(SETTINGS_TWO_PANEL_PATH)/res \
    $(SETTINGS_PATH)/res

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.recyclerview_recyclerview \
    androidx.preference_preference \
    androidx.appcompat_appcompat \
    androidx.legacy_legacy-preference-v14 \
    androidx.leanback_leanback-preference \
    androidx.leanback_leanback \
    TwoPanelSettingsLib \

LOCAL_STATIC_JAVA_LIBRARIES := \
    androidx.annotation_annotation \
    statslog-tvsettings

LOCAL_USE_AAPT2 := true
LOCAL_PACKAGE_NAME := TwoPanelSettingsOverlay
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_REQUIRED_MODULES := privapp_whitelist_com.android.tv.settings
LOCAL_FULL_LIBS_MANIFEST_FILES := $(SETTINGS_TWO_PANEL_PATH)/AndroidManifest.xml

LOCAL_PROGUARD_FLAG_FILES := ../Settings/proguard.cfg

ifneq ($(INCREMENTAL_BUILDS),)
    LOCAL_PROGUARD_ENABLED := disabled
    LOCAL_JACK_ENABLED := incremental
    LOCAL_JACK_FLAGS := --multi-dex native
endif

include frameworks/base/packages/SettingsLib/common.mk

LOCAL_OVERRIDES_PACKAGES := TvSettings

LOCAL_JACK_COVERAGE_INCLUDE_FILTER := com.android.tv.settings.*,com.android.settingslib.*,com.android.tv.twopanelsettingsoverlay.*

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(SETTINGS_TWO_PANEL_PATH))
