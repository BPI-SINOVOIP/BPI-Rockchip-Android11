LOCAL_PATH := vendor/rockchip/common/samba
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/bin/rksmbd:/root/sbin/rksmbd \
    $(LOCAL_PATH)/etc/smb.conf:/system/etc/smb.conf \
    $(LOCAL_PATH)/etc/smbusers:/system/etc/smbusers \
    $(LOCAL_PATH)/etc/smbpasswd:/system/etc/smbpasswd
