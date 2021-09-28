
# Start bluetooth from console
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/bluetooth/console_start_bt/brcm_patchram_plus:system/bin/brcm_patchram_plus \
	vendor/rockchip/common/bluetooth/console_start_bt/hciconfig:system/xbin/hciconfig \
	vendor/rockchip/common/bluetooth/console_start_bt/hcidump:system/xbin/hcidump \
	vendor/rockchip/common/bluetooth/console_start_bt/hcitool:system/xbin/hcitool \
	vendor/rockchip/common/bluetooth/console_start_bt/libbluedroid.so:system/lib/libbluedroid.so \
	vendor/rockchip/common/bluetooth/console_start_bt/libbluetooth.so:system/lib/libbluetooth.so \
	vendor/rockchip/common/bluetooth/console_start_bt/libbluetoothd.so:system/lib/libbluetoothd.so \
	vendor/rockchip/common/bluetooth/console_start_bt/libbtio.so:system/lib/libbtio.so \
	vendor/rockchip/common/bluetooth/console_start_bt/libglib.so:system/lib/libglib.so

