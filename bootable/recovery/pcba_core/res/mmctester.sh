#!/system/bin/sh

#source send_cmd_pipe.sh
ismount=0
while true; do
    if [ ! -d "/tmp/extsd" ]; then
        mkdir -p /tmp/extsd
    fi

    for nr in 0 1 2 3 4 5 6;do
        mmcblk="/dev/block/mmcblk0p$nr"
        mmcp=$mmcblk
        mount -t vfat $mmcp /tmp/extsd > /data/log.txt
        if [ $? -eq 0 ]; then
	    ismount=1
            break
        fi
    done
	if [ $ismount -eq 1 ]; then
		break
	fi
done

capacity=`df | grep /tmp/extsd |tr -s ' ' | cut -d ' ' -f 2`
echo "$mmcp: $capacity"

umount /tmp/extsd

echo $capacity > /data/sd_capacity

exit 1
