#!/vendor/bin/sh
log -t flash_img "dd.sh flash_img"
if=$(getprop vendor.flash_if_path)
of=$(getprop vendor.flash_of_path)
log -t flash_img "if=$if,of=$of"
RESULT=$(dd if=$if of=$of 2>&1)
log -t flash_img "$RESULT"
setprop flash.success 1
