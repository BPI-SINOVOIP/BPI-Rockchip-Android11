#!/system/bin/sh
# Program: 
# Program packs data partition to a sparse image
# History: 
# 2012-11-23  First release by cw
echo data_partition_size:$1
IMG_FILE="/mnt/sdcard/databk.img"
if [ -f "$IMG_FILE" ]; then
    rm "$IMG_FILE"
fi
packdata -s -l $1 /mnt/sdcard/databk.img /data  && echo "PACK_OK" || echo "PACK_ERROR"
