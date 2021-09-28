#!/system/bin/sh
# This is a sample code to Pull LOW BT_RST
echo 0 > /sys/class/rfkill/rfkill0/state
