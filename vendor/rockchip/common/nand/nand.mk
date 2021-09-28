
#ifneq ($(filter arm%, $(TARGET_ARCH)), )

#   vendor/rockchip/common/nand/modules/arm64/drmboot.ko:root/drmboot.ko \ 
#    vendor/rockchip/common/nand/modules/arm64/drmboot.ko:root/drmboot.ko \
    vendor/rockchip/common/nand/modules/arm64/drmboot.ko:root/drmboot.ko \
    vendor/rockchip/common/nand/modules/arm/drmboot.ko:root/drmboot.ko \
    vendor/rockchip/common/nand/modules/arm/drmboot.ko:root/drmboot.ko \
    vendor/rockchip/common/nand/modules/arm/drmboot.ko:root/drmboot.ko


#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3399)
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:root/rk30xxnand_ko.ko \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:recovery/root/rk30xxnand_ko.ko
#endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3366)
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:root/rk30xxnand_ko.ko \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:recovery/root/rk30xxnand_ko.ko
#endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3368)
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:root/rk30xxnand_ko.ko \
    vendor/rockchip/common/nand/modules/arm64/rk30xxnand_ko.ko.3.10.0:recovery/root/rk30xxnand_ko.ko
#endif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3036)
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm/rk3036/rk30xxnand_ko.ko.3.10.0:root/rk30xxnand_ko.ko 
#ndif

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)), rk3188)
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm/rk3188/rk30xxnand_ko.ko:root/rk30xxnand_ko.ko 
#else
#PRODUCT_COPY_FILES += \
    vendor/rockchip/common/nand/modules/arm/rk30xxnand_ko.ko.3.10.0:root/rk30xxnand_ko.ko 
#endif
#endif
