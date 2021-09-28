#! /bin/bash

unzip -u $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py.zip -d $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py

# For bluetooth_packets_python3
PYTHONPATH=$PYTHONPATH:$ANDROID_BUILD_TOP/out/host/linux-x86/lib64:$ANDROID_BUILD_TOP/system/bt/gd:$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py python3.8 `which act.py` -c $ANDROID_BUILD_TOP/system/bt/gd/cert/pts.json -tf $ANDROID_BUILD_TOP/system/bt/gd/cert/pts_l2cap_testcase -tp $ANDROID_BUILD_TOP/system/bt/gd
