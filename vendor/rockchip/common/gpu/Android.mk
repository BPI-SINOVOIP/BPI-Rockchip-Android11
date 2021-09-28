LOCAL_PATH := $(call my-dir)
# $(info 'in MaliT860.mk')
# $(info TARGET_BOARD_PLATFORM_GPU:$(TARGET_BOARD_PLATFORM_GPU) )
# $(info TARGET_ARCH:$(TARGET_ARCH) )

# ---------------------------- #

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := MaliT860/lib/$(TARGET_ARCH)/libGLES_mali.so
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := MaliT860/lib/$(TARGET_2ND_ARCH)/libGLES_mali.so
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_PATH_32 := $(TARGET_OUT_VENDOR)/lib/egl
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR)/lib64/egl

# Create symlinks.
LOCAL_POST_INSTALL_CMD := \
	if [ -f $(LOCAL_MODULE_PATH_32)/libGLES_mali.so ];then cd $(TARGET_OUT_VENDOR)/lib; ln -sf egl/libGLES_mali.so libGLES_mali.so; cd -; fi; \
	if [ -f $(LOCAL_MODULE_PATH_64)/libGLES_mali.so ];then cd $(TARGET_OUT_VENDOR)/lib64; ln -sf egl/libGLES_mali.so libGLES_mali.so; cd -; fi; \
	if [ $(BUILD_WITH_GOOGLE_MARKET) != true ];then mkdir -p $(TARGET_OUT_VENDOR)/lib/hw; cd $(TARGET_OUT_VENDOR)/lib/hw; ln -sf ../libGLES_mali.so vulkan.rk3399.so; cd -; fi; \
	if [ $(BUILD_WITH_GOOGLE_MARKET) != true ];then mkdir -p $(TARGET_OUT_VENDOR)/lib64/hw; cd $(TARGET_OUT_VENDOR)/lib64/hw; ln -sf ../libGLES_mali.so vulkan.rk3399.so; cd -; fi; \
	cd $(TARGET_OUT_VENDOR)/lib64; \
	ln -sf egl/libGLES_mali.so libOpenCL.so.1.1; \
	ln -sf libOpenCL.so.1.1 libOpenCL.so.1; \
	ln -sf libOpenCL.so.1 libOpenCL.so; \
	cd -; \
	cd $(TARGET_OUT_VENDOR)/lib; \
	ln -sf egl/libGLES_mali.so libOpenCL.so.1.1; \
	ln -sf libOpenCL.so.1.1 libOpenCL.so.1; \
	ln -sf libOpenCL.so.1 libOpenCL.so; \
	cd -;

include $(BUILD_PREBUILT)
endif # ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t860)

# ---------------------------- #

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-t760)
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_SRC_FILES := MaliT760/lib/arm/rk3288w/libGLES_mali.so
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/egl

# Create symlinks.
LOCAL_POST_INSTALL_CMD := \
	if [ $(BUILD_WITH_GOOGLE_MARKET) != true ];then mkdir -p $(TARGET_OUT_VENDOR)/lib/hw; cd $(TARGET_OUT_VENDOR)/lib/hw; ln -sf ../egl/libGLES_mali.so vulkan.rk3288.so; cd -; fi; \
	cd $(TARGET_OUT_VENDOR)/lib; \
	ln -sf egl/libGLES_mali.so libOpenCL.so.1.1; \
	ln -sf libOpenCL.so.1.1 libOpenCL.so.1; \
	ln -sf libOpenCL.so.1 libOpenCL.so; \
	cd -;

include $(BUILD_PREBUILT)
endif

# ---------------------------- #

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali-G52)
# install libs of mali_so
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := MaliG52/lib/$(TARGET_ARCH)/libGLES_mali.so
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := MaliG52/lib/$(TARGET_2ND_ARCH)/libGLES_mali.so
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_PATH_32 := $(TARGET_OUT_VENDOR)/lib/egl
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR)/lib64/egl
# Create symlinks.
LOCAL_POST_INSTALL_CMD := \
	cd $(TARGET_OUT_VENDOR)/lib64; \
	ln -sf egl/libGLES_mali.so libOpenCL.so; \
	cd -; \
	cd $(TARGET_OUT_VENDOR)/lib; \
	ln -sf egl/libGLES_mali.so libOpenCL.so; \
	cd -;
include $(BUILD_PREBUILT)

# install libs of vulkan_so
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := vulkan.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_$(TARGET_ARCH) := MaliG52/lib/$(TARGET_ARCH)/vulkan.mali.so
LOCAL_SRC_FILES_$(TARGET_2ND_ARCH) := MaliG52/lib/$(TARGET_2ND_ARCH)/vulkan.mali.so
LOCAL_CHECK_ELF_FILES := false
LOCAL_MODULE_PATH_32 := $(TARGET_OUT_VENDOR)/lib/hw
LOCAL_MODULE_PATH_64 := $(TARGET_OUT_VENDOR)/lib64/hw
include $(BUILD_PREBUILT)

endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM_GPU)), mali400)
include $(CLEAR_VARS)
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE := libGLES_mali
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_CHECK_ELF_FILES := false
LOCAL_SRC_FILES := Mali400/lib/arm/libGLES_mali.so
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/egl

# Create symlinks.
LOCAL_POST_INSTALL_CMD := \
	cd $(TARGET_OUT_VENDOR)/lib; \
	ln -sf egl/libGLES_mali.so libOpenCL.so.1.1; \
	ln -sf libOpenCL.so.1.1 libOpenCL.so.1; \
	ln -sf libOpenCL.so.1 libOpenCL.so; \
	cd -;

include $(BUILD_PREBUILT)
endif
