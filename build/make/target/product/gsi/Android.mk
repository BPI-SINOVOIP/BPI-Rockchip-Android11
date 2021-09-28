LOCAL_PATH:= $(call my-dir)

#####################################################################
# list of vndk libraries from the source code.
INTERNAL_VNDK_LIB_LIST := $(SOONG_VNDK_LIBRARIES_FILE)

#####################################################################
# This is the up-to-date list of vndk libs.
# TODO(b/62012285): the lib list should be stored somewhere under
# /prebuilts/vndk
ifeq (REL,$(PLATFORM_VERSION_CODENAME))
LATEST_VNDK_LIB_LIST := $(LOCAL_PATH)/$(PLATFORM_VNDK_VERSION).txt
ifeq ($(wildcard $(LATEST_VNDK_LIB_LIST)),)
$(error $(LATEST_VNDK_LIB_LIST) file not found. Please copy "$(LOCAL_PATH)/current.txt" to "$(LATEST_VNDK_LIB_LIST)" and commit a CL for release branch)
endif
else
LATEST_VNDK_LIB_LIST := $(LOCAL_PATH)/current.txt
endif

#####################################################################
# Check the generate list against the latest list stored in the
# source tree
.PHONY: check-vndk-list

# Check if vndk list is changed
droidcore: check-vndk-list

check-vndk-list-timestamp := $(call intermediates-dir-for,PACKAGING,vndk)/check-list-timestamp
check-vndk-abi-dump-list-timestamp := $(call intermediates-dir-for,PACKAGING,vndk)/check-abi-dump-list-timestamp

ifeq ($(TARGET_IS_64_BIT)|$(TARGET_2ND_ARCH),true|)
# TODO(b/110429754) remove this condition when we support 64-bit-only device
check-vndk-list: ;
else ifeq ($(TARGET_BUILD_PDK),true)
# b/118634643: don't check VNDK lib list when building PDK. Some libs (libandroid_net.so
# and some render-script related ones) can't be built in PDK due to missing frameworks/base.
check-vndk-list: ;
else ifeq ($(TARGET_SKIP_CURRENT_VNDK),true)
check-vndk-list: ;
else ifeq ($(BOARD_VNDK_VERSION),)
# b/143233626 do not check vndk-list when vndk libs are not built
check-vndk-list: ;
else
check-vndk-list: $(check-vndk-list-timestamp)
ifneq ($(SKIP_ABI_CHECKS),true)
check-vndk-list: $(check-vndk-abi-dump-list-timestamp)
endif
endif

_vndk_check_failure_message := " error: VNDK library list has been changed.\n"
ifeq (REL,$(PLATFORM_VERSION_CODENAME))
_vndk_check_failure_message += "       Changing the VNDK library list is not allowed in API locked branches."
else
_vndk_check_failure_message += "       Run \`update-vndk-list.sh\` to update $(LATEST_VNDK_LIB_LIST)"
endif

$(check-vndk-list-timestamp): $(INTERNAL_VNDK_LIB_LIST) $(LATEST_VNDK_LIB_LIST) $(HOST_OUT_EXECUTABLES)/update-vndk-list.sh
	$(hide) ( diff --old-line-format="Removed %L" \
	  --new-line-format="Added %L" \
	  --unchanged-line-format="" \
	  $(LATEST_VNDK_LIB_LIST) $(INTERNAL_VNDK_LIB_LIST) \
	  || ( echo -e $(_vndk_check_failure_message); exit 1 ))
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

#####################################################################
# Script to update the latest VNDK lib list
include $(CLEAR_VARS)
LOCAL_MODULE := update-vndk-list.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_STEM := $(LOCAL_MODULE)
LOCAL_IS_HOST_MODULE := true
include $(BUILD_SYSTEM)/base_rules.mk
$(LOCAL_BUILT_MODULE): PRIVATE_INTERNAL_VNDK_LIB_LIST := $(INTERNAL_VNDK_LIB_LIST)
$(LOCAL_BUILT_MODULE): PRIVATE_LATEST_VNDK_LIB_LIST := $(LATEST_VNDK_LIB_LIST)
$(LOCAL_BUILT_MODULE):
	@echo "Generate: $@"
	@mkdir -p $(dir $@)
	@rm -f $@
	$(hide) echo "#!/bin/bash" > $@
ifeq (REL,$(PLATFORM_VERSION_CODENAME))
	$(hide) echo "echo Updating VNDK library list is NOT allowed in API locked branches." >> $@; \
	        echo "exit 1" >> $@
else
	$(hide) echo "if [ -z \"\$${ANDROID_BUILD_TOP}\" ]; then" >> $@; \
	        echo "  echo Run lunch or choosecombo first" >> $@; \
	        echo "  exit 1" >> $@; \
	        echo "fi" >> $@; \
	        echo "cd \$${ANDROID_BUILD_TOP}" >> $@; \
	        echo "cp $(PRIVATE_INTERNAL_VNDK_LIB_LIST) $(PRIVATE_LATEST_VNDK_LIB_LIST)" >> $@; \
	        echo "echo $(PRIVATE_LATEST_VNDK_LIB_LIST) updated." >> $@
endif
	@chmod a+x $@

#####################################################################
# Check that all ABI reference dumps have corresponding
# NDK/VNDK/PLATFORM libraries.

# $(1): The directory containing ABI dumps.
# Return a list of ABI dump paths ending with .so.lsdump.
define find-abi-dump-paths
$(if $(wildcard $(1)), \
  $(addprefix $(1)/, \
    $(call find-files-in-subdirs,$(1),"*.so.lsdump" -and -type f,.)))
endef

# $(1): A list of tags.
# $(2): A list of tag:path.
# Return the file names of the ABI dumps that match the tags.
define filter-abi-dump-paths
$(eval tag_patterns := $(foreach tag,$(1),$(tag):%))
$(notdir $(patsubst $(tag_patterns),%,$(filter $(tag_patterns),$(2))))
endef

VNDK_ABI_DUMP_DIR := prebuilts/abi-dumps/vndk/$(PLATFORM_VNDK_VERSION)
NDK_ABI_DUMP_DIR := prebuilts/abi-dumps/ndk/$(PLATFORM_VNDK_VERSION)
PLATFORM_ABI_DUMP_DIR := prebuilts/abi-dumps/platform/$(PLATFORM_VNDK_VERSION)
VNDK_ABI_DUMPS := $(call find-abi-dump-paths,$(VNDK_ABI_DUMP_DIR))
NDK_ABI_DUMPS := $(call find-abi-dump-paths,$(NDK_ABI_DUMP_DIR))
PLATFORM_ABI_DUMPS := $(call find-abi-dump-paths,$(PLATFORM_ABI_DUMP_DIR))

$(check-vndk-abi-dump-list-timestamp): PRIVATE_LSDUMP_PATHS := $(LSDUMP_PATHS)
$(check-vndk-abi-dump-list-timestamp):
	$(eval added_vndk_abi_dumps := $(strip $(sort $(filter-out \
	  $(call filter-abi-dump-paths,LLNDK VNDK-SP VNDK-core,$(PRIVATE_LSDUMP_PATHS)), \
	  $(notdir $(VNDK_ABI_DUMPS))))))
	$(if $(added_vndk_abi_dumps), \
	  echo -e "Found unexpected ABI reference dump files under $(VNDK_ABI_DUMP_DIR). It is caused by mismatch between Android.bp and the dump files. Run \`find \$${ANDROID_BUILD_TOP}/$(VNDK_ABI_DUMP_DIR) '(' -name $(subst $(space), -or -name ,$(added_vndk_abi_dumps)) ')' -delete\` to delete the dump files.")

	$(eval added_ndk_abi_dumps := $(strip $(sort $(filter-out \
	  $(call filter-abi-dump-paths,NDK,$(PRIVATE_LSDUMP_PATHS)), \
	  $(notdir $(NDK_ABI_DUMPS))))))
	$(if $(added_ndk_abi_dumps), \
	  echo -e "Found unexpected ABI reference dump files under $(NDK_ABI_DUMP_DIR). It is caused by mismatch between Android.bp and the dump files. Run \`find \$${ANDROID_BUILD_TOP}/$(NDK_ABI_DUMP_DIR) '(' -name $(subst $(space), -or -name ,$(added_ndk_abi_dumps)) ')' -delete\` to delete the dump files.")

	$(eval added_platform_abi_dumps := $(strip $(sort $(filter-out \
	  $(call filter-abi-dump-paths,PLATFORM,$(PRIVATE_LSDUMP_PATHS)), \
	  $(notdir $(PLATFORM_ABI_DUMPS))))))
	$(if $(added_platform_abi_dumps), \
	  echo -e "Found unexpected ABI reference dump files under $(PLATFORM_ABI_DUMP_DIR). It is caused by mismatch between Android.bp and the dump files. Run \`find \$${ANDROID_BUILD_TOP}/$(PLATFORM_ABI_DUMP_DIR) '(' -name $(subst $(space), -or -name ,$(added_platform_abi_dumps)) ')' -delete\` to delete the dump files.")

	$(if $(added_vndk_abi_dumps)$(added_ndk_abi_dumps)$(added_platform_abi_dumps),exit 1)
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

#####################################################################
# VNDK package and snapshot.

ifneq ($(BOARD_VNDK_VERSION),)

include $(CLEAR_VARS)
LOCAL_MODULE := vndk_package
# Filter LLNDK libs moved to APEX to avoid pulling them into /system/LIB
LOCAL_REQUIRED_MODULES := \
    $(filter-out $(LLNDK_MOVED_TO_APEX_LIBRARIES),$(LLNDK_LIBRARIES))

ifneq ($(TARGET_SKIP_CURRENT_VNDK),true)
LOCAL_REQUIRED_MODULES += \
    vndkcorevariant.libraries.txt \
    $(addsuffix .vendor,$(VNDK_CORE_LIBRARIES)) \
    $(addsuffix .vendor,$(VNDK_SAMEPROCESS_LIBRARIES)) \
    $(VNDK_USING_CORE_VARIANT_LIBRARIES) \
    com.android.vndk.current
endif
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
_vndk_versions := $(PRODUCT_EXTRA_VNDK_VERSIONS)
ifneq ($(BOARD_VNDK_VERSION),current)
	_vndk_versions += $(BOARD_VNDK_VERSION)
endif
LOCAL_MODULE := vndk_apex_snapshot_package
LOCAL_REQUIRED_MODULES := $(foreach vndk_ver,$(_vndk_versions),com.android.vndk.v$(vndk_ver))
include $(BUILD_PHONY_PACKAGE)

_vndk_versions :=

endif # BOARD_VNDK_VERSION is set

#####################################################################
# skip_mount.cfg, read by init to skip mounting some partitions when GSI is used.

include $(CLEAR_VARS)
LOCAL_MODULE := gsi_skip_mount.cfg
LOCAL_MODULE_STEM := skip_mount.cfg
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := ETC
LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := init/config

# Adds a symlink under /system/etc/init/config pointing to /system/system_ext/etc/init/config
# because first-stage init in Android 10.0 will read the skip_mount.cfg from /system/etc/* after
# chroot /system.
# TODO: remove this symlink when no need to support new GSI on Android 10.
# The actual file needs to be under /system/system_ext because it's GSI-specific and does not
# belong to core CSI.
LOCAL_POST_INSTALL_CMD := \
    mkdir -p $(TARGET_OUT)/etc/init; \
    ln -sf /system/system_ext/etc/init/config $(TARGET_OUT)/etc/init/config

include $(BUILD_PREBUILT)

#####################################################################
# init.gsi.rc, GSI-specific init script.

include $(CLEAR_VARS)
LOCAL_MODULE := init.gsi.rc
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS := ETC
LOCAL_SYSTEM_EXT_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := init

include $(BUILD_PREBUILT)
