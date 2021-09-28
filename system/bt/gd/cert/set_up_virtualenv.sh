#! /bin/bash
#
# Script to setup virtual environment to run GD cert tests and devlop using IDE
#
# Usage
#  1. cd system/bt/gd
#  2. source cert/set_up_virtualenv.sh
#  3. source gd_cert_venv/bin/activate
#  4. [run tests, do development, hack]
#  5. deactivate (or just close the terminal window)

## Android build main build setup script relative to top level android source root
BUILD_SETUP=./build/envsetup.sh

function UsageAndroidTree {
    cat<<EOF
Ensure invoked from within the android source tree
EOF
}

function UsageSourcedNotExecuted {
    cat<<EOF
Ensure script is SOURCED and not executed to persist the build setup
e.g.
source $0
EOF
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

function SetUpAndroidBuild {
    pushd .
    android_root=$(UpFind -name out -type d)
    if [[ -z $android_root ]] ; then
        UsageAndroidTree
        return
    fi
    echo "Found android root $android_root"
    cd $android_root && . $BUILD_SETUP
    echo "Sourced build setup rules"
    cd $android_root && lunch
    popd
}

function SetupPython38 {
    echo "Setting up python3.8"
    sudo apt-get install python3.8-dev
}

function SetupPip3 {
    echo "Setting up pip3"
    sudo apt-get install python3-pip
}

function CompileBluetoothPacketsPython3 {
    echo "bluetooth_packets_python3 is not found, compiling"
    croot
    make -j bluetooth_packets_python3
}

# Deactivate existing virtual environment, if any, ignore errors
deactivate > /dev/null 2>&1

if [[ "${BASH_SOURCE[0]}" == "${0}" ]] ; then
    UsageSourcedNotExecuted
    return 1
fi

## Check python3.8 is installed properly
## Need Python 3.8 because bluetooth_packets_python3 is compiled against
## Python 3.8 headers
dpkg -l python3.8-dev > /dev/null 2>&1
if [[ $? -ne 0 ]] ; then
    SetupPython38
fi

## Check pip3 is installed properly
## Need pip3 for Python 3 support
dpkg -l python3-pip > /dev/null 2>&1
if [[ $? -ne 0 ]] ; then
    SetupPip3
fi

# Install and upgrade virtualenv to latest version
pip3 install --user --upgrade virtualenv > /dev/null 2>&1
if [[ $? -ne 0 ]] ; then
    echo "Error install and upgrade virtualenv"
    return 1
fi

# Set-up Android environment variables
if [[ -z "$ANDROID_BUILD_TOP" ]] ; then
    SetUpAndroidBuild
fi

## Check bluetooth_packets_python3 is compiled succssfully
$ANDROID_BUILD_TOP/system/bt/gd/cert/python3.8-gd -c "
import bluetooth_packets_python3 as bp3
bp3.BaseStruct
"
if [[ $? -ne 0 ]] ; then
  pushd .
  CompileBluetoothPacketsPython3
  popd
  $ANDROID_BUILD_TOP/system/bt/gd/cert/python3.8-gd -c "
import bluetooth_packets_python3 as bp3
bp3.BaseStruct
"
  if [[ $? -ne 0 ]] ; then
    echo "Setup failed as bluetooth_packets_python3 cannot be found"
    return 1
  else
    echo "Found bluetooth_packets_python3 after compilation"
  fi
else
  echo "Found bluetooth_packets_python3"
fi

## Compile and unzip test artifacts
if [[ ! -f "$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py.zip" || ! -f "$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test.zip" ]]; then
    echo "bluetooth_cert_generated_py.zip OR bluetooth_cert_test.zip is not found, compiling"
    m -j dist bluetooth_stack_with_facade
    if [[ ! -f "$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py.zip" || ! -f "$ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test.zip" ]]; then
        echo "Failed to compile bluetooth_stack_with_facade"
        return 1
    fi
fi
unzip -u $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py.zip -d $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py
if [[ $? -ne 0 ]] ; then
    echo "Failed to unzip bluetooth_cert_generated_py.zip"
    return 1
fi
unzip -u $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test.zip -d $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test
if [[ $? -ne 0 ]] ; then
    echo "Failed to unzip bluetooth_cert_test.zip"
    return 1
fi

# Set-up virtualenv
pushd .
cd $ANDROID_BUILD_TOP/system/bt/gd
virtualenv -p python3.8 gd_cert_venv
popd
if [[ $? -ne 0 ]] ; then
    echo "Error setting up virtualenv"
    return 1
fi

# Set up artifacts
pushd .
cd $ANDROID_BUILD_TOP/system/bt/gd/gd_cert_venv/lib/python3.8/site-packages
# Python generated code
ln -sfT $ANDROID_BUILD_TOP/tools/test/connectivity/acts/framework/acts acts
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/cert cert
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/facade facade
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/hal hal
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/hci hci
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/l2cap l2cap
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/neighbor neighbor
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_generated_py/security security
# Native libraries
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test/out/host/linux-x86/lib64/bluetooth_packets_python3.so bluetooth_packets_python3.so
# Per systrace, Python only load from python3.8/lib64 directory for plugin imported native libraries
mkdir -p $ANDROID_BUILD_TOP/system/bt/gd/gd_cert_venv/lib/python3.8/lib64
cd $ANDROID_BUILD_TOP/system/bt/gd/gd_cert_venv/lib/python3.8/lib64
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test/out/host/linux-x86/lib64/libc++.so libc++.so
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test/out/host/linux-x86/lib64/libbluetooth_gd.so libbluetooth_gd.so
ln -sfT $ANDROID_BUILD_TOP/out/dist/bluetooth_cert_test/out/host/linux-x86/lib64/libgrpc++_unsecure.so libgrpc++_unsecure.so
# Binaries
cd $ANDROID_BUILD_TOP/system/bt/gd/gd_cert_venv/bin
ln -sfT $ANDROID_BUILD_TOP/out/host/linux-x86/bin/bluetooth_stack_with_facade bluetooth_stack_with_facade
ln -sfT $ANDROID_BUILD_TOP/out/host/linux-x86/nativetest64/root-canal/root-canal root-canal
popd

# Activate virtualenv
pushd .
source $ANDROID_BUILD_TOP/system/bt/gd/gd_cert_venv/bin/activate
popd
if [[ $? -ne 0 ]] ; then
    echo "Failed to activate virtualenv"
    deactivate
    return 1
fi
if [[ -z "$ANDROID_BUILD_TOP" ]] ; then
    echo "Failed to inherit Android build environment"
    deactivate
    return 1
fi

## Set up ACTS
# sudo is no longer needed since we are in a virtual environment
python3.8 $ANDROID_BUILD_TOP/tools/test/connectivity/acts/framework/setup.py develop
if [[ $? -ne 0 ]] ; then
    echo "ACTS setup failed"
    deactivate
    return 1
fi

pip3 install protobuf
if [[ $? -ne 0 ]] ; then
    echo "Failed to install protobuf"
    deactivate
    return 1
fi

deactivate

echo ""
echo "Please mark GD root directory as \"Project Sources and Headers\" in IDE"
echo "If still seeing errors, invalidate cached and restart"
echo "virtualenv setup complete"
