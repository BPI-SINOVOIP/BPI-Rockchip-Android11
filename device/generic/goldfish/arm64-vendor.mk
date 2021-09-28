PRODUCT_KERNEL_VERSION := 5.4
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += \
    $(wildcard prebuilts/qemu-kernel/arm64/$(PRODUCT_KERNEL_VERSION)/ko/*.ko)

PRODUCT_PROPERTY_OVERRIDES += \
       vendor.rild.libpath=/vendor/lib64/libgoldfish-ril.so

# Note: the following lines need to stay at the beginning so that it can
# take priority  and override the rules it inherit from other mk files
# see copy file rules in core/Makefile
ifeq ($(QEMU_USE_SYSTEM_EXT_PARTITIONS),true)
  PRODUCT_COPY_FILES += \
    device/generic/goldfish/fstab.ranchu.initrd.arm.ex:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/fstab.ranchu \
    device/generic/goldfish/fstab.ranchu.initrd.arm.ex:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/fstab.ranchu \
    device/generic/goldfish/fstab.ranchu.arm.ex:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.ranchu
else
  PRODUCT_COPY_FILES += \
    device/generic/goldfish/fstab.ranchu.initrd.arm:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk/fstab.ranchu \
    device/generic/goldfish/fstab.ranchu.initrd.arm:$(TARGET_COPY_OUT_VENDOR_RAMDISK)/fstab.ranchu \
    device/generic/goldfish/fstab.ranchu.arm:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.ranchu
endif


PRODUCT_COPY_FILES += \
    prebuilts/qemu-kernel/arm64/$(PRODUCT_KERNEL_VERSION)/kernel-qemu2:kernel-ranchu \
    device/generic/goldfish/data/etc/advancedFeatures.ini.arm:advancedFeatures.ini \

EMULATOR_VENDOR_NO_GNSS := true

ifeq ($(QEMU_DISABLE_AVB),true)
    PRODUCT_COPY_FILES += \
      device/generic/goldfish/data/etc/dummy.vbmeta.img:$(PRODUCT_OUT)/vbmeta.img \

endif
