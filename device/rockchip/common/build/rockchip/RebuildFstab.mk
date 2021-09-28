ifdef PRODUCT_FSTAB_TEMPLATE

$(info build fstab file with $(PRODUCT_FSTAB_TEMPLATE)....)
fstab_flags := wait
fstab_prefix := /dev/block/by-name/
ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    fstab_flags := $(fstab_flags),slotselect
endif # BOARD_USES_AB_IMAGE

ifeq ($(strip $(BOARD_AVB_ENABLE)), true)
    fstab_flags := $(fstab_flags),avb
endif # BOARD_AVB_ENABLE

# GSI does not has dirsync support.
ifeq ($(strip $(BUILD_WITH_GOOGLE_MARKET)), true)
    sync_flags := none
else
    sync_flags := ,dirsync
endif

ifeq ($(strip $(BOARD_SUPER_PARTITION_GROUPS)),rockchip_dynamic_partitions)
    fstab_prefix := none
    fstab_flags := $(fstab_flags),logical
endif # BOARD_USE_DYNAMIC_PARTITIONS

fstab_sdmmc_device := 00000000.dwmmc
ifdef PRODUCT_SDMMC_DEVICE
    fstab_sdmmc_device := $(PRODUCT_SDMMC_DEVICE)
endif

intermediates := $(call intermediates-dir-for,FAKE,rockchip_fstab)
rebuild_fstab := $(intermediates)/fstab.rk30board

ROCKCHIP_FSTAB_TOOLS := $(SOONG_HOST_OUT_EXECUTABLES)/fstab_tools

$(rebuild_fstab) : $(PRODUCT_FSTAB_TEMPLATE) $(ROCKCHIP_FSTAB_TOOLS)
	@echo "Building fstab file $@."
	$(ROCKCHIP_FSTAB_TOOLS) -I fstab \
	-i $(PRODUCT_FSTAB_TEMPLATE) \
	-a $(sync_flags) \
	-p $(fstab_prefix) \
	-f $(fstab_flags) \
	-s $(fstab_sdmmc_device) \
	-o $(rebuild_fstab)

INSTALLED_RK_VENDOR_FSTAB := $(PRODUCT_OUT)/$(TARGET_COPY_OUT_VENDOR)/etc/$(notdir $(rebuild_fstab))
$(INSTALLED_RK_VENDOR_FSTAB) : $(rebuild_fstab)
	$(call copy-file-to-new-target-with-cp)

# Header V3, add vendor_boot
ifeq (1,$(strip $(shell expr $(BOARD_BOOT_HEADER_VERSION) \>= 3)))
INSTALLED_RK_VENDOR_RAMDISK_FSTAB := $(PRODUCT_OUT)/$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/$(notdir $(rebuild_fstab))
$(INSTALLED_RK_VENDOR_RAMDISK_FSTAB) : $(rebuild_fstab)
	$(call copy-file-to-new-target-with-cp)
endif

INSTALLED_RK_RAMDISK_FSTAB := $(PRODUCT_OUT)/$(TARGET_COPY_OUT_RAMDISK)/$(notdir $(rebuild_fstab))
$(INSTALLED_RK_RAMDISK_FSTAB) : $(rebuild_fstab)
	$(call copy-file-to-new-target-with-cp)

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
INSTALLED_RK_RECOVERY_FIRST_STAGE_FSTAB := $(PRODUCT_OUT)/$(TARGET_COPY_OUT_RECOVERY)/root/first_stage_ramdisk/$(notdir $(rebuild_fstab))
$(INSTALLED_RK_RECOVERY_FIRST_STAGE_FSTAB) : $(rebuild_fstab)
	$(call copy-file-to-new-target-with-cp)
endif # BOARD_USES_AB_IMAGE

ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
ALL_DEFAULT_INSTALLED_MODULES += $(INSTALLED_RK_VENDOR_FSTAB) $(INSTALLED_RK_RAMDISK_FSTAB) $(INSTALLED_RK_RECOVERY_FIRST_STAGE_FSTAB)
else
ALL_DEFAULT_INSTALLED_MODULES += $(INSTALLED_RK_VENDOR_FSTAB) $(INSTALLED_RK_RAMDISK_FSTAB)
endif

# Header V3, add vendor_boot
ifeq (1,$(strip $(shell expr $(BOARD_BOOT_HEADER_VERSION) \>= 3)))
ALL_DEFAULT_INSTALLED_MODULES += $(INSTALLED_RK_VENDOR_RAMDISK_FSTAB)
endif

endif
