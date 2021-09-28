#!/bin/bash
# Rockchip 2020 makefile generate script
# Update makefile from modules list.

CONFIGS_PATH=configs
KERNEL_DRIVERS_PATH=../../../../kernel

# TODO rockchip license
MK_HEADER="# Rockchip 2020 makefile"

VENDOR_RAMDISK_GKI_MK=$CONFIGS_PATH/vendor_ramdisk_gki.mk
VENDOR_RAMDISK_LOAD_FILE=$CONFIGS_PATH/vendor_ramdisk_modules.load
VENDOR_RAMDISK_MK_PREFIX="BOARD_VENDOR_RAMDISK_KERNEL_MODULES += \\"

VENDOR_GKI_MK=$CONFIGS_PATH/vendor_gki.mk
VENDOR_LOAD_FILE=$CONFIGS_PATH/vendor_modules.load
VENDOR_MK_PREFIX="BOARD_VENDOR_KERNEL_MODULES"

RECOVERY_GKI_MK=$CONFIGS_PATH/recovery_gki.mk
RECOVERY_LOAD_FILE=$CONFIGS_PATH/recovery_modules.load
RECOVERY_MK_PREFIX="BOARD_RECOVERY_KERNEL_MODULES"

# find_modules_from_config $1 $2 $3
# config_file makefile_name prefix_in_makefile
find_modules_from_config() {
    echo "==========================================="
    echo -e "\033[33mRead modules list from $1\033[0m"
    echo "==========================================="
    echo $MK_HEADER > $2
    echo "# Generate from vendor/rockchip/common/modular_kernel/$1" >> $2
    modules_ramdisk_array=($(cat $1))
    num_array=${#modules_ramdisk_array[@]}
    [[ $num_array -eq 0 ]] && return
    echo "$3 += \\" >> $2
    for MODULE in "${modules_ramdisk_array[@]}"
    do
        module_file=($(find $KERNEL_DRIVERS_PATH -name $MODULE))
        if [ "$module_file" = "" ]; then
            echo -e "\033[33mSkip $MODULE due to KO not found!\033[0m"
        else
            echo "$module_file"|cut -b 13-
            echo -n "    " >> $2
            echo "$module_file \\"|cut -b 13- >> $2
        fi
    done
    echo "==========================================="
}

# update vendor-ramdisk/lib/modules
find_modules_from_config $VENDOR_RAMDISK_LOAD_FILE $VENDOR_RAMDISK_GKI_MK $VENDOR_RAMDISK_MK_PREFIX

# update vendor/lib/modules
find_modules_from_config $VENDOR_LOAD_FILE $VENDOR_GKI_MK $VENDOR_MK_PREFIX

# update recovery/lib/modules
find_modules_from_config $RECOVERY_LOAD_FILE $RECOVERY_GKI_MK $RECOVERY_MK_PREFIX
