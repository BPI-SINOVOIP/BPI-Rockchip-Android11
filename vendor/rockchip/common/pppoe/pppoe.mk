SOURCE_PPPOE_CONF_DIR := vendor/rockchip/common/pppoe/configs
TARGET_PPPOE_CONF_DIR := system/etc/ppp
PRODUCT_COPY_FILES += $(SOURCE_PPPOE_CONF_DIR)/pap-secrets:$(TARGET_PPPOE_CONF_DIR)/pppoe-secrets \
                        $(SOURCE_PPPOE_CONF_DIR)/pppoe.conf:$(TARGET_PPPOE_CONF_DIR)/pppoe.conf

SOURCE_PPPOE_SCRIPT_DIR := vendor/rockchip/common/pppoe/scrips
TARGET_PPPOE_script_DIR := system/bin
PRODUCT_COPY_FILES += $(SOURCE_PPPOE_SCRIPT_DIR)/pppoe-stop:$(TARGET_PPPOE_script_DIR)/pppoe-stop \
                        $(SOURCE_PPPOE_SCRIPT_DIR)/pppoe-connect:$(TARGET_PPPOE_script_DIR)/pppoe-connect \
                        $(SOURCE_PPPOE_SCRIPT_DIR)/pppoe-setup:$(TARGET_PPPOE_script_DIR)/pppoe-setup \
                        $(SOURCE_PPPOE_SCRIPT_DIR)/pppoe-start:$(TARGET_PPPOE_script_DIR)/pppoe-start \
                        $(SOURCE_PPPOE_SCRIPT_DIR)/pppoe-status:$(TARGET_PPPOE_script_DIR)/pppoe-status
