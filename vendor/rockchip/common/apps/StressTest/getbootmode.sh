#! /system/bin/sh
su

echo 0 > /sys/module/kernel/parameters/panic

busybox dmesg -s 50000 | busybox grep "Boot mode:" > mnt/internal_sd/boot_mode.txt
