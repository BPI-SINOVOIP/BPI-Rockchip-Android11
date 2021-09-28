LOCAL_PATH := $(call my-dir)

### GLESv2_enc Encoder ###########################################
$(call emugl-begin-shared-library,libGLESv2_enc)

LOCAL_SRC_FILES := \
    GL2EncoderUtils.cpp \
    GL2Encoder.cpp \
    GLESv2Validation.cpp \
    gl2_client_context.cpp \
    gl2_enc.cpp \
    gl2_entry.cpp \
    IOStream2.cpp \

LOCAL_CFLAGS += -DLOG_TAG=\"emuglGLESv2_enc\"
LOCAL_CFLAGS += -Wno-unused-private-field


$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))
$(call emugl-import,libOpenglCodecCommon$(GOLDFISH_OPENGL_LIB_SUFFIX))

$(call emugl-end-module)


