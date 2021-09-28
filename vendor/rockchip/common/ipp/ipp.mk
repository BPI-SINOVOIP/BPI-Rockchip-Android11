ifneq ($(filter %car, $(PRODUCT_BUILD_MODULE)), )

else
PRODUCT_COPY_FILES += \
	vendor/rockchip/common/ipp/lib/rk29-ipp.ko.3.0.101+:system/lib/modules/rk29-ipp.ko.3.0.101+ \
	vendor/rockchip/common/ipp/lib/rk29-ipp.ko.3.0.36+:system/lib/modules/rk29-ipp.ko.3.0.36+ \
	vendor/rockchip/common/ipp/lib/rk29-ipp.ko:system/lib/modules/rk29-ipp.ko
endif
