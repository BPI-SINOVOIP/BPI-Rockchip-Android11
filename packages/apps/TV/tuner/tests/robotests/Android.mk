#############################################################
# Tv Robolectric test target.                               #
#############################################################
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := TvTunerRoboTests
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

LOCAL_SRC_FILES := $(call all-java-files-under, javatests)

LOCAL_JAVA_LIBRARIES := \
    Robolectric_all-target \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \

LOCAL_STATIC_JAVA_LIBRARIES := \
    tv-lib-dagger

LOCAL_STATIC_ANDROID_LIBRARIES := \
    androidx.test.core \
    tv-lib-dagger-android \
    tv-test-common \
    tv-test-common-robo \
    tv-tuner-testing \

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
LOCAL_MODULE := RunTvTunerRoboTests

BASE_DIR = com/android/tv/tuner
EXCLUDE_FILES := \
    $(BASE_DIR)/dvb/DvbTunerHalTest.java \
    $(BASE_DIR)/exoplayer/tests/SampleSourceExtractorTest.java \

LOCAL_ROBOTEST_FILES := $(call find-files-in-subdirs,$(LOCAL_PATH)/javatests,*Test.java,.)
LOCAL_ROBOTEST_FILES := $(filter-out $(EXCLUDE_FILES),$(LOCAL_ROBOTEST_FILES))

LOCAL_JAVA_LIBRARIES := \
    Robolectric_all-target \
    TvTunerRoboTests \
    mockito-robolectric-prebuilt \
    robolectric_android-all-stub \
    tv-lib-truth \
    tv-test-common \
    tv-test-common-robo \
    tv-tuner-testing \

LOCAL_TEST_PACKAGE := LiveTv

LOCAL_ROBOTEST_TIMEOUT := 36000

include external/robolectric-shadows/run_robotests.mk
