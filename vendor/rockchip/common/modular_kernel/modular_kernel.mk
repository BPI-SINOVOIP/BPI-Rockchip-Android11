$(call inherit-product-if-exists, vendor/rockchip/common/modular_kernel/configs/recovery_gki.mk)
$(call inherit-product-if-exists, vendor/rockchip/common/modular_kernel/configs/vendor_gki.mk)
$(call inherit-product-if-exists, vendor/rockchip/common/modular_kernel/configs/vendor_ramdisk_gki.mk)

# Old way to find all of the KOs
#KERNEL_KO_FILES := $(shell find $(TOPDIR)kernel -name "*.ko" -type f)
#BOARD_VENDOR_RAMDISK_KERNEL_MODULES += \
#	$(foreach file, $(KERNEL_KO_FILES), $(file))
