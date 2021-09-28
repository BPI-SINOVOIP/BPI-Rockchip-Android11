CUR_PATH := device/rockchip/common/samba
PRODUCT_COPY_FILES += \
    $(CUR_PATH)/bin/rksmbd:/system/bin/rksmbd \
    $(CUR_PATH)/bin/rksmbpasswd:/system/bin/rksmbpasswd \
    $(CUR_PATH)/bin/stopsamba.sh:/system/bin/stopsamba.sh \
    $(CUR_PATH)/etc/smb.conf:/system/etc/smb.conf \
    $(CUR_PATH)/etc/smbusers:/system/etc/smbusers \
    $(CUR_PATH)/etc/smbpasswd:/system/etc/smbpasswd
