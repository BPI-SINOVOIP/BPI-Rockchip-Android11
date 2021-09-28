#!/bin/bash
# Author: kenjc@rock-chips.com
# 2021-08-13
# Use: ./gen-package-file.sh IMAGE_PATH to genernate package-file
IMAGE_PATH=$1
readonly HEADER="# NAME\tRelative path\n#\n##HWDEF\tHWDEF\npackage-file\tpackage-file\nbootloader\t$IMAGE_PATH/MiniLoaderAll.bin\nparameter\t$IMAGE_PATH/parameter.txt"

readonly FOOTER="# 要写入backup分区的文件就是自身(update.img)\n# SELF 是关键字，表示升级文件(update.img)自身\n# 在生成升级文件时，不加入SELF文件的内容，但在头部信息中有记录\n# 在解包升级文件时，不解包SELF文件的内容。\nbackup      RESERVED\n#update-script  update-script\n#recover-script recover-script"

readonly PARTITION_TABLE_FILE=$IMAGE_PATH/parameter.txt
partitions_array=(`cat ${PARTITION_TABLE_FILE} |awk '/^CMDLINE/' |grep -Eo "[^(]*[)$]" |sed 's/.$//'|grep -v "userdata"`)
find_all_of_partitions() {
    for partition in "${partitions_array[@]}"
    do
        if [ -f "$IMAGE_PATH/$partition.img" ]; then
            echo -e "$partition\t$IMAGE_PATH/$partition.img"
        elif [ -f "$IMAGE_PATH/${partition%%_*}.img" ]; then
            echo -e "$partition\t$IMAGE_PATH/${partition%%_*}.img"
        fi
    done
}

echo -e $HEADER
find_all_of_partitions
echo -e $FOOTER
