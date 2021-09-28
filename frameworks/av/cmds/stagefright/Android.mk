LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=       \
        AudioPlayer.cpp \
        stagefright.cpp \
        jpeg.cpp        \
        SineSource.cpp

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright libmedia libmedia_codeclist libutils libbinder \
        libstagefright_foundation libjpeg libui libgui libcutils liblog \
        libhidlbase libdatasource libaudioclient \
        android.hardware.media.omx@1.0 \

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/av/media/libstagefright/include \
        frameworks/native/include/media/openmax \

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_SYSTEM_EXT_MODULE:= true
LOCAL_MODULE:= stagefright

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=         \
        AudioPlayer.cpp \
        SineSource.cpp    \
        record.cpp

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright libmedia liblog libutils libbinder \
        libstagefright_foundation libdatasource libaudioclient

LOCAL_C_INCLUDES:= \
        frameworks/av/camera/include \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax \
        frameworks/native/include/media/hardware

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= record

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=         \
        AudioPlayer.cpp \
        recordvideo.cpp

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright libmedia liblog libutils libbinder \
        libstagefright_foundation libaudioclient

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax \
        frameworks/native/include/media/hardware

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= recordvideo

include $(BUILD_EXECUTABLE)


################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=         \
        AudioPlayer.cpp \
        SineSource.cpp    \
        audioloop.cpp

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright libmedia liblog libutils libbinder \
        libstagefright_foundation libaudioclient

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= audioloop

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=         \
        stream.cpp    \

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright liblog libutils libbinder libui libgui \
        libstagefright_foundation libmedia libcutils libdatasource

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= stream

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
        codec.cpp               \
        SimplePlayer.cpp        \

LOCAL_HEADER_LIBRARIES := \
        libmediadrm_headers \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright liblog libutils libbinder libstagefright_foundation \
        libmedia libmedia_omx libaudioclient libui libgui libcutils

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= codec

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        filters/argbtorgba.rscript \
        filters/nightvision.rscript \
        filters/saturation.rscript \
        mediafilter.cpp \

LOCAL_HEADER_LIBRARIES := \
        libmediadrm_headers \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright \
        liblog \
        libutils \
        libbinder \
        libstagefright_foundation \
        libmedia_omx \
        libui \
        libgui \
        libRScpp \

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax \
        frameworks/rs/cpp \
        frameworks/rs \

intermediates := $(call intermediates-dir-for,STATIC_LIBRARIES,libRS,TARGET,)
LOCAL_C_INCLUDES += $(intermediates)

LOCAL_STATIC_LIBRARIES:= \
        libstagefright_mediafilter

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= mediafilter

LOCAL_SANITIZE := cfi

include $(BUILD_EXECUTABLE)

################################################################################

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
        muxer.cpp            \

LOCAL_HEADER_LIBRARIES := \
        libmediametrics_headers \

LOCAL_SHARED_LIBRARIES := \
        libstagefright liblog libutils libbinder libstagefright_foundation \
        libcutils libc

LOCAL_C_INCLUDES:= \
        frameworks/av/media/libstagefright \
        frameworks/native/include/media/openmax

LOCAL_CFLAGS += -Wno-multichar -Werror -Wall

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= muxer

include $(BUILD_EXECUTABLE)
