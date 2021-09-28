#! /bin/bash

if [[ -z "${ANDROID_BUILD_TOP}" ]]; then
  echo "ANDROID_BUILD_TOP is not set"
fi

if [[ -z "${ANDROID_HOST_OUT}" ]]; then
  echo "ANDROID_HOST_OUT is not set for host run"
fi

unzip -o -q $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py.zip -d $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py

PYTHONPATH=$PYTHONPATH:$ANDROID_BUILD_TOP/out/host/linux-x86/lib64:$ANDROID_BUILD_TOP/system/bt/gd:$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py python3.8 `which act.py` -c $ANDROID_BUILD_TOP/system/bt/gd/cert/host_only_config_facade_only.json -tf $ANDROID_BUILD_TOP/system/bt/gd/cert/cert_testcases_facade_only -tp $ANDROID_BUILD_TOP/system/bt/gd
