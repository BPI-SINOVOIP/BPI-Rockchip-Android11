# This build script corresponds to a library containing many definitions
# common to both the guest and the host. They relate to
#
LOCAL_PATH := $(call my-dir)

commonSources := \
        GLClientState.cpp \
        GLESTextureUtils.cpp \
        ChecksumCalculator.cpp \
        GLSharedGroup.cpp \
        glUtils.cpp \
        IndexRangeCache.cpp \
        SocketStream.cpp \
        TcpStream.cpp \
        auto_goldfish_dma_context.cpp \

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))

commonSources += \
        goldfish_dma_host.cpp \

else

commonSources += \
        goldfish_dma.cpp \

endif

### CodecCommon  guest ##############################################
$(call emugl-begin-shared-library,libOpenglCodecCommon$(GOLDFISH_OPENGL_LIB_SUFFIX))

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
$(call emugl-import,libqemupipe$(GOLDFISH_OPENGL_LIB_SUFFIX))
else
$(call emugl-export,STATIC_LIBRARIES,libqemupipe.ranchu)
endif

LOCAL_SRC_FILES := $(commonSources)

LOCAL_CFLAGS += -DLOG_TAG=\"eglCodecCommon\"
LOCAL_CFLAGS += -Wno-unused-private-field

$(call emugl-export,SHARED_LIBRARIES,libcutils libutils liblog)

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
$(call emugl-export,SHARED_LIBRARIES,android-emu-shared)
endif

$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))
$(call emugl-end-module)
