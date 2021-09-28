#!/system/bin/sh
# This is a sample code to Pull HIGH BT_RST
echo 1 > /sys/class/rfkill/rfkill0/state
