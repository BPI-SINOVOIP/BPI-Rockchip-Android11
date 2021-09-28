#############################################################
# Tv Robolectric test target.                               #
#############################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := TvRoboTests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

BASE_DIR = src/com/android/tv
EXCLUDE_FILES := \
    $(BASE_DIR)/data/epg/EpgFetcherImplTest.java \
    $(BASE_DIR)/guide/ProgramItemViewTest.java \

LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_SRC_FILES := $(filter-out $(EXCLUDE_FILES),$(LOCAL_SRC_FILES))

LOCAL_JAVA_LIBRARIES := \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \

LOCAL_STATIC_JAVA_LIBRARIES := \
    tv-lib-dagger \
    tv-lib-truth \

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.leanback_leanback-nodeps \
    androidx.test.core \
    androidx.test.ext.truth \
    tv-lib-dagger-android \
    tv-test-common \
    tv-test-common-robo \

LOCAL_ANNOTATION_PROCESSORS := \
    tv-lib-dagger-android-processor \
    tv-lib-dagger-compiler \

LOCAL_ANNOTATION_PROCESSOR_CLASSES := \
  dagger.internal.codegen.ComponentProcessor,dagger.android.processor.AndroidProcessor

LOCAL_INSTRUMENTATION_FOR := LiveTv

LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_JAVA_LIBRARY)

#############################################################
# Tv runner target to run the previous target.              #
#############################################################
include $(CLEAR_VARS)
LOCAL_MODULE := RunTvRoboTests

BASE_DIR = com/android/tv
EXCLUDE_FILES := \
    $(BASE_DIR)/data/epg/EpgFetcherImplTest.java \
    $(BASE_DIR)/guide/ProgramItemViewTest.java \

LOCAL_ROBOTEST_FILES := $(call find-files-in-subdirs,$(LOCAL_PATH)/src,*Test.java,.)
LOCAL_ROBOTEST_FILES := $(filter-out $(EXCLUDE_FILES),$(LOCAL_ROBOTEST_FILES))

LOCAL_JAVA_LIBRARIES := \
    Robolectric_all-target \
    TvRoboTests \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \
    tv-test-common \
    tv-test-common-robo \

LOCAL_TEST_PACKAGE := LiveTv

LOCAL_ROBOTEST_TIMEOUT := 36000

include external/robolectric-shadows/run_robotests.mk
