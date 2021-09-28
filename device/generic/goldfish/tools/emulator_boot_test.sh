#!/bin/bash

#disable for now, as it is still not well tested
#set -e

echo "starting boot test at "
date
uname -a

echo $TARGET_PRODUCT

time_out="600"
op_no_accel=""
unamestr=`uname`
if [[ "$unamestr" == 'Linux' ]]; then
    export PATH=prebuilts/android-emulator/linux-x86_64:$PATH
    op_no_accel="-no-accel"
    if [[ -e '/dev/kvm' ]]; then
        echo "Has /dev/kvm"
        if [[ -r '/dev/kvm' ]]; then
            echo "KVM readable"
            if [[ -w '/dev/kvm' ]]; then
                echo "KVM writable, enable acceleration"
                op_no_accel=""
            fi
        else
            echo "KVM not readable"
        fi
    else
        echo "does not have KVM"
    fi
elif [[ "$unamestr" == 'Darwin' ]]; then
    export PATH=prebuilts/android-emulator/darwin-x86_64:$PATH
else
    echo "Cannot determine OS type, quit"
    exit 1
fi

if [[ $op_no_accel != "" ]]; then
echo "disable smp since there is no acceleration"
echo hw.cpu.ncore=1 >> $ANDROID_PRODUCT_OUT/config.ini
fi

echo $ANDROID_PRODUCT_OUT

which emulator
emulator -version
emulator -accel-check

{
    sleep 600
    echo "kill emulator after 10 minutes"
    pkill -9 qemu-system
} &

emulator -gpu swiftshader_indirect -no-window -show-kernel -verbose -quit-after-boot $time_out \
             -wipe-data -no-snapshot $op_no_accel -skin 480x800x32 -logcat *:I -no-boot-anim

echo "ending boot test at "
date
echo "done"
exit 0
