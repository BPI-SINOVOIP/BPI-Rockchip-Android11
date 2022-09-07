#!/sbin/sh

#source send_cmd_pipe.sh

nr="1"
mmcblk="/dev/block/mmcblk$nr"
mmcp=$mmcblk

#while true; do
    while true; do
        while true; do
            if [ -b "$mmcblk" ]; then
              sleep 1
                if [ -b "$mmcblk" ]; then
                    echo "card$nr insert"
                    break
                fi
            else
               sleep 1
            fi
        done
        
        if [ ! -d "/tmp/extsd" ]; then
            mkdir -p /tmp/extsd
        fi
        
        mmcp=$mmcblk
		echo $mmcp
        mount -t vfat $mmcp /tmp/extsd
        if [ $? -ne 0 ]; then
            mmcp=$mmcblk"p1"
            mount -t vfat $mmcp /tmp/extsd
            if [ $? -ne 0 ]; then
                exit 0
                busybox sleep 3
                continue 2
            fi
        fi

        break
    done
    
    capacity=`busybox df | busybox grep "/tmp/extsd"`
    echo "$mmcp: $capacity"
    
    umount /tmp/extsd
    
    echo $capacity > /data/sd_capacity

	exit 1
#    while true; do
 #       if [ -b "$mmcblk" ]; then
#            echo "please remove card$nr"
#            busybox sleep 1
 #       else
  #          echo "card$nr remove"
  #          break
   #     fi
    #done
#done

