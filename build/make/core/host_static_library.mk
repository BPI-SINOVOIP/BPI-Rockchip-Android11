$(call record-module-type,HOST_STATIC_LIBRARY)
LOCAL_IS_HOST_MODULE := true
my_prefix := HOST_
LOCAL_HOST_PREFIX :=
include $(BUILD_SYSTEM)/multilib.mk

ifndef LOCAL_MODULE_HOST_ARCH
ifndef my_module_multilib
# libraries default to building for both architecturess
my_module_multilib := both
endif
endif

LOCAL_2ND_ARCH_VAR_PREFIX :=
include $(BUILD_SYSTEM)/module_arch_supported.mk

ifeq ($(my_module_arch_supported),true)
include $(BUILD_SYSTEM)/host_static_library_internal.mk
endif

ifdef HOST_2ND_ARCH
LOCAL_2ND_ARCH_VAR_PREFIX := $(HOST_2ND_ARCH_VAR_PREFIX)
include $(BUILD_SYSTEM)/module_arch_supported.mk
ifeq ($(my_module_arch_supported),true)
# Build for HOST_2ND_ARCH
LOCAL_BUILT_MODULE :=
LOCAL_INSTALLED_MODULE :=
LOCAL_INTERMEDIATE_TARGETS :=

include $(BUILD_SYSTEM)/host_static_library_internal.mk
endif
LOCAL_2ND_ARCH_VAR_PREFIX :=
endif  # HOST_2ND_ARCH

my_module_arch_supported :=

###########################################################
## Copy headers to the install tree
###########################################################
ifdef LOCAL_COPY_HEADERS
$(if $(filter true,$(BUILD_BROKEN_USES_BUILD_COPY_HEADERS)),\
  $(call pretty-warning,LOCAL_COPY_HEADERS is deprecated. See $(CHANGES_URL)#copy_headers),\
  $(call pretty-error,LOCAL_COPY_HEADERS is obsolete. See $(CHANGES_URL)#copy_headers))
include $(BUILD_SYSTEM)/copy_headers.mk
endif
