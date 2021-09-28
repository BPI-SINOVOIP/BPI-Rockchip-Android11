# Enable dynamic partitions, Android Q must set this by default.
# Use the non-open-source parts, if they're present
# Android Q -> api_level 29, Pie or earlier should not include this makefile

PRODUCT_USE_DYNAMIC_PARTITIONS := true

# Add standalone odm partition configrations
PRODUCT_BUILD_PRODUCT_IMAGE := true
TARGET_COPY_OUT_PRODUCT := product
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE ?= ext4

PRODUCT_BUILD_SYSTEM_EXT_IMAGE := true
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE ?= ext4

BOARD_BUILD_SUPER_IMAGE_BY_DEFAULT := true
BOARD_SUPER_PARTITION_GROUPS := rockchip_dynamic_partitions
BOARD_ROCKCHIP_DYNAMIC_PARTITIONS_PARTITION_LIST := system system_ext vendor product odm
