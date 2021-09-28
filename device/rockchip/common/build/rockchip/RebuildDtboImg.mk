ifdef PRODUCT_DTBO_TEMPLATE

$(info rebuilding dtbo image with $(PRODUCT_DTBO_TEMPLATE)....)
intermediates := $(call intermediates-dir-for,FAKE,rockchip_dtbo)

AOSP_DTC_TOOL := $(SOONG_HOST_OUT_EXECUTABLES)/dtc
AOSP_MKDTIMG_TOOL := $(SOONG_HOST_OUT_EXECUTABLES)/mkdtimg
ROCKCHIP_FSTAB_TOOLS := $(SOONG_HOST_OUT_EXECUTABLES)/fstab_tools

rebuild_dts := $(intermediates)/device-tree-overlay.dts
rebuild_dtbo_dtb := $(intermediates)/device-dtbo.dtb
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
$(rebuild_dts) : $(ROCKCHIP_FSTAB_TOOLS) $(PRODUCT_DTBO_TEMPLATE)
	@echo "Building dts file $@."
	$(ROCKCHIP_FSTAB_TOOLS) -I dts \
	-i $(PRODUCT_DTBO_TEMPLATE) \
	-p $(dtbo_boot_device) \
	-f $(dtbo_flags) \
	-o $(rebuild_dts)

$(rebuild_dtbo_img) : $(rebuild_dts) $(AOSP_DTC_TOOL) $(AOSP_MKDTIMG_TOOL)
	@echo "Building dtbo img file $@."
	$(AOSP_DTC_TOOL) -@ -O dtb -o $(rebuild_dtbo_dtb) $(rebuild_dts)
	$(AOSP_MKDTIMG_TOOL) create $(rebuild_dtbo_img) $(rebuild_dtbo_dtb)

INSTALLED_RK_DTBO_IMAGE := $(PRODUCT_OUT)/$(notdir $(rebuild_dtbo_img))
$(INSTALLED_RK_DTBO_IMAGE) : $(rebuild_dtbo_img)
	$(call copy-file-to-new-target-with-cp)

ALL_DEFAULT_INSTALLED_MODULES += $(INSTALLED_RK_DTBO_IMAGE)
BOARD_PREBUILT_DTBOIMAGE := $(INSTALLED_RK_DTBO_IMAGE)
endif
