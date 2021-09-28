
LOCAL_PATH:= $(call my-dir)

common_cflags := \
    -Werror -Wno-unused-parameter -Wno-unused-const-variable \
    -I$(LOCAL_PATH)/upstream-netbsd/include/ \

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    rktoolbox.c \

LOCAL_CFLAGS += $(common_cflags)
LOCAL_CONLYFLAGS += -std=gnu99

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libselinux \

LOCAL_MODULE := rktoolbox

# Install the symlinks.
LOCAL_POST_INSTALL_CMD := $(hide) $(foreach t,$(ALL_TOOLS),ln -sf rktoolbox $(TARGET_OUT)/bin/$(t);)

# Including this will define $(intermediates).
#
include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
