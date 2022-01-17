ifdef DTBO_DEVICETREE

$(info rebuilding dtbo image for multi dtbo used....)
intermediates := $(call intermediates-dir-for,FAKE,rockchip_dtbo)

AOSP_DTC_TOOL := $(SOONG_HOST_OUT_EXECUTABLES)/dtc
AOSP_MKDTIMG_TOOL := $(SOONG_HOST_OUT_EXECUTABLES)/mkdtimg
ROCKCHIP_FSTAB_TOOLS := $(SOONG_HOST_OUT_EXECUTABLES)/fstab_tools

rebuild_dts := $(intermediates)
rebuild_dtbo_img := $(intermediates)/rebuild-dtbo.img

dtbo_flags := wait
ifeq ($(strip $(BOARD_USES_AB_IMAGE)), true)
    dtbo_flags := $(dtbo_flags),slotselect
endif # BOARD_USES_AB_IMAGE

ifeq ($(strip $(BOARD_AVB_ENABLE)), true)
    dtbo_flags := $(dtbo_flags),avb
endif # BOARD_AVB_ENABLE

dtbo_boot_device := none
ifdef PRODUCT_BOOT_DEVICE
    dtbo_boot_device := $(PRODUCT_BOOT_DEVICE)
endif

$(rebuild_dts) : $(ROCKCHIP_FSTAB_TOOLS) $(AOSP_DTC_TOOL)
	mkdir -p $(rebuild_dts)
	$(foreach file, $(DTBO_DEVICETREE), \
		$(ROCKCHIP_FSTAB_TOOLS) -I dts \
		-i $(PRODUCT_KERNEL_DIR)/arch/arm64/boot/dts/rockchip/overlay/$(TARGET_PRODUCT)/$(file).dts \
		-p $(dtbo_boot_device) \
		-f $(dtbo_flags) \
		-o $(rebuild_dts)/$(file).dts; \
		$(AOSP_DTC_TOOL) -@ -O dtb -o $(rebuild_dts)/$(file).dtbo $(rebuild_dts)/$(file).dts; \
	)

$(rebuild_dtbo_img) : $(rebuild_dts) $(AOSP_MKDTIMG_TOOL)
	@echo "Building dtbo img file $@."
	$(AOSP_MKDTIMG_TOOL) create $(rebuild_dtbo_img) \
    	$(foreach file, $(DTBO_DEVICETREE), \
		$(rebuild_dts)/$(file).dtbo \
    	)

INSTALLED_RK_DTBO_IMAGE := $(PRODUCT_OUT)/$(notdir $(rebuild_dtbo_img))
$(INSTALLED_RK_DTBO_IMAGE) : $(rebuild_dtbo_img)
	$(call copy-file-to-new-target-with-cp)

ALL_DEFAULT_INSTALLED_MODULES += $(INSTALLED_RK_DTBO_IMAGE)
BOARD_PREBUILT_DTBOIMAGE := $(INSTALLED_RK_DTBO_IMAGE)
endif
