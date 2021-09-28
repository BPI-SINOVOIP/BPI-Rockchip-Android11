LOCAL_PATH := $(call my-dir)

$(call emugl-begin-shared-library,libvulkan_enc)
$(call emugl-export,C_INCLUDES,$(LOCAL_PATH))
$(call emugl-import,libOpenglCodecCommon$(GOLDFISH_OPENGL_LIB_SUFFIX) lib_renderControl_enc)
ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
$(call emugl-import,libandroidemu)
$(call emugl-import,libGoldfishAddressSpace$(GOLDFISH_OPENGL_LIB_SUFFIX))
else
$(call emugl-export,SHARED_LIBRARIES,libandroidemu)
$(call emugl-export,STATIC_LIBRARIES,libGoldfishAddressSpace)
endif

# Vulkan include dir
ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH) \
    $(HOST_EMUGL_PATH)/host/include \
    $(HOST_EMUGL_PATH)/host/include/vulkan
endif

ifneq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../vulkan_enc \
    external/libdrm \
    external/minigbm/cros_gralloc \

LOCAL_HEADER_LIBRARIES += \
    hwvulkan_headers \
    vulkan_headers \

LOCAL_SHARED_LIBRARIES += libdrm

endif

LOCAL_CFLAGS += \
    -DLOG_TAG=\"goldfish_vulkan\" \
    -DVK_ANDROID_native_buffer \
    -DVK_GOOGLE_address_space \
    -Wno-missing-field-initializers \
    -Werror \
    -fstrict-aliasing \
    -DVK_USE_PLATFORM_ANDROID_KHR \
    -DVK_NO_PROTOTYPES \

LOCAL_SRC_FILES := AndroidHardwareBuffer.cpp \
    HostVisibleMemoryVirtualization.cpp \
    Resources.cpp \
    Validation.cpp \
    VulkanStreamGuest.cpp \
    VulkanHandleMapping.cpp \
    ResourceTracker.cpp \
    VkEncoder.cpp \
    goldfish_vk_extension_structs_guest.cpp \
    goldfish_vk_marshaling_guest.cpp \
    goldfish_vk_deepcopy_guest.cpp \
    goldfish_vk_handlemap_guest.cpp \
    goldfish_vk_transform_guest.cpp \

ifeq (true,$(GOLDFISH_OPENGL_BUILD_FOR_HOST))
LOCAL_CFLAGS += -D__ANDROID_API__=28
$(call emugl-export,SHARED_LIBRARIES,libgui)
else
$(call emugl-export,SHARED_LIBRARIES,libsync libnativewindow)
LOCAL_STATIC_LIBRARIES += libarect
endif

$(call emugl-end-module)

