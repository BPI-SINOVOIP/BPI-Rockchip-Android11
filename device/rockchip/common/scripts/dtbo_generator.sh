#!/bin/bash
usage()
{
    echo "USAGE: -d target.dts [-o result.img]"
    echo "Example: ./build_dtbo_image.sh -d ./device-tree-overlay.dts -o ./dtbo.img"
    exit 1
}

TARGET_DTS=device-tree-overlay.dts
OUTPUT_IMAGE=./dtbo.img

if [ $# = 4 ] ;then
    while getopts "d:o:" arg
    do
        case $arg in
            d)
                TARGET_DTS=$OPTARG
                ;;
            o)
                OUTPUT_IMAGE=$OPTARG
                ;;
            ?)
                usage ;;
        esac
    done
elif [ $# = 2 ];then
    while getopts "d:" arg
    do
        case $arg in
            d)
                TARGET_DTS=$OPTARG
                ;;
            ?)
                usage ;;
        esac
    done
elif [ $# = 0 ];then
    echo "Use default commond."
else
    usage
fi

dtc -@ -O dtb -o temp.dtbo $TARGET_DTS
mkdtimg create $OUTPUT_IMAGE temp.dtbo
rm -rf temp.dtbo

echo "======================================================"
echo "Target DTS is            $TARGET_DTS"
echo "Result Image is          $OUTPUT_IMAGE"
echo "Done!"
echo "======================================================"
