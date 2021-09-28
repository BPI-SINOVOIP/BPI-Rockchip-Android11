#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

function menu-adb() {
    TMP=$(adb devices -l | grep -v "List of device" | awk '{ print $1 }')
    # TODO(optedoblivion): If the device doesn't have a name (offline), it misnames them
    NTMP=$(adb devices -l | grep -v "List of device" | awk '{ print $6 }' | cut -d ':' -f 2)
    SERIALS=($TMP)
    DEVICES=($NTMP)
    LEN=${#SERIALS[@]}
    result=0
    if [ $LEN -lt 1 ]; then
        echo "No devices connected!"
        exit 1
    fi

    if [ "$LEN" == "" ]; then
        LEN=0
    fi

    answer=0

    DEVICE_NAME="$1 device"

    if [ $LEN -gt 1 ]; then
        echo "+-------------------------------------------------+" 1>&2
        echo "| Choose a ${DEVICE_NAME}:                         " 1>&2
        echo "+-------------------------------------------------+" 1>&2
        echo "|                                                 |" 1>&2
        let fixed_len=$LEN-1
        for i in `seq 0 $fixed_len`;
        do
            serial=${SERIALS[i]}
            device=${DEVICES[i]}
            echo "| $i) $serial $device" 1>&2
            ## TODO[MSB]: Find character count, fill with space and ending box wall
        done
        echo "|                                                 |" 1>&2
        echo "+-------------------------------------------------+" 1>&2
        echo 1>&2
        echo -n "Index number: " 1>&2
        read answer
    fi

    if [ $answer -ge $LEN ]; then
        echo
        echo "Please choose a correct index!" 1>&2
        echo
        exit 1
    fi

    SERIAL=${SERIALS[$answer]}
    echo $SERIAL
}

function UpFind {
    while [[ $PWD != / ]] ; do
        rc=$(find "$PWD" -maxdepth 1 "$@")
        if [ -n "$rc" ]; then
            echo $(dirname "$rc")
            return
        fi
        cd ..
    done
}


function get-android-root() {
    android_root=$(UpFind -name out -type d)
    if [[ -z $android_root ]] ; then
        echo
        echo "Needs to be ran in the android tree"
        echo
        exit 1
    fi
    echo "${android_root}"
}

function banner() {
    echo
    echo "GD On Device Cert Test"
    echo
}

## Main
banner

DRY_RUN=""
DO_BUILD=0
if [ $# -gt 0 ]; then
    for var in "$@"
    do
        if [ "$var" == "-h" ]; then
            echo
            echo "Usage: $0 [-h|-d]"
            echo
            echo "Available Options:"
            echo "=================="
            echo " -h | Help(this) Menu"
            echo " -d | Dry run; just prints commands"
            echo
            exit 0
        elif [ "$var" == "-d" ]; then
            DRY_RUN="echo"
        elif [ "$var" == "-b" ]; then
            DO_BUILD=1
        fi
    done
fi

## Verify devices connected and sane
DUT_SERIAL="$(menu-adb DUT)"
DUT_ADB="adb -s ${DUT_SERIAL}"
DUT_NAME="$(adb devices -l | grep -v "List of device" | grep ${DUT_SERIAL} | awk '{ print $6 }' | cut -d ':' -f 2)"

CERT_SERIAL="$(menu-adb CERT)"
CERT_ADB="adb -s ${CERT_SERIAL}"
CERT_NAME="$(adb devices -l | grep -v "List of device" | grep ${CERT_SERIAL} | awk '{ print $6 }' | cut -d ':' -f 2)"

if [ "${CERT_SERIAL}" == "${DUT_SERIAL}" ]; then
    echo
    echo "ERROR: CERT and DUT cannot be the same device, or you only have one device connected!"
    echo
    exit 1
fi

## Start builds
if [ $DO_BUILD == 1 ]; then
    $DRY_RUN cd $(get-android-root)
    $DRY_RUN . build/envsetup.sh
    #DUT
    $DRY_RUN lunch $DUT_NAME
    $DRY_RUN cd $(get-android-root)/system/bt/gd
    $DRY_RUN mma -j `cat /proc/cpuinfo | grep core | wc -l`
    $DRY_RUN cd $(get-android-root)
    # CERT
    $DRY_RUN lunch $CERT_NAME
    $DRY_RUN cd $(get-android-root)/system/bt/gd
    $DRY_RUN mma -j `cat /proc/cpuinfo | grep core | wc -l`
    $DRY_RUN cd $(get-android-root)
fi

## Set android devices in config
pushd .
cd "${DIR}"
# Reset in case user chooses different item in menu
git co android_devices_config.json
popd
$DRY_RUN sed -i "s/\"DUT\"/\"${DUT_SERIAL}\"/g" ${DIR}/android_devices_config.json
$DRY_RUN sed -i "s/\"CERT\"/\"${CERT_SERIAL}\"/g" ${DIR}/android_devices_config.json

## ACTS
#$DRY_RUN source $(get-android-root)/system/bt/gd/cert/set_up_acts.sh

## Start test
$DRY_RUN $(get-android-root)/system/bt/gd/cert/run_device_cert.sh
