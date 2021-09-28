#! /bin/bash
#
# Script to setup environment to execute bluetooth certification stack
#
# for more info, see go/acts

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

function CompileBluetoothPacketsPython3 {
    echo "bluetooth_packets_python3 is not found, compiling"
    croot
    make -j bluetooth_packets_python3
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]] ; then
    UsageSourcedNotExecuted
    exit 1
fi

if [[ -z "$ANDROID_BUILD_TOP" ]] ; then
    SetUpAndroidBuild
fi

## Check python3.8 is installed properly
## Need Python 3.8 because bluetooth_packets_python3 is compiled against
## Python 3.8 headers
dpkg -l python3.8-dev > /dev/null 2>&1
if [[ $? -ne 0 ]] ; then
    SetupPython38
fi

## Check bluetooth_packets_python3 is compiled succssfully
PYTHONPATH=$PYTHONPATH:$ANDROID_BUILD_TOP/out/host/linux-x86/lib64 python3.8 -c "
import bluetooth_packets_python3 as bp3
bp3.BaseStruct
"
if [[ $? -ne 0 ]] ; then
  pushd .
  CompileBluetoothPacketsPython3
  popd
  python3.8 -c "
import bluetooth_packets_python3 as bp3
bp3.BaseStruct
"
  if [[ $? -ne 0 ]] ; then
    echo "Setup failed as bluetooth_packets_python3 cannot be found"
  else
    echo "Found bluetooth_packets_python3 after compilation"
  fi
else
  echo "Found bluetooth_packets_python3"
fi

## All is good now so go ahead with the acts setup
pushd .
cd $ANDROID_BUILD_TOP/tools/test/connectivity/acts/framework/
sudo python3.8 setup.py develop
if [[ $? -eq 0 ]] ; then
    echo "cert setup complete"
else
    echo "cert setup failed"
fi
popd

