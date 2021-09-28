# VHAL Host Emulator
This is a collection of python modules as tools for VHAL development.

[TOC]

## vhal_emulator.py
This allow you to create script to interate with the Vehicle HAL in an AAOS
device from a host.

*   It sends and receives messages to/from the Vehicle HAL via port forwarding
over ADB to communicate with the AAOS device.
*   On the device side, VHAL module VehicleService creates VehicleEmulator to
setup SocketComm to serve the requests.
    *   hardware/interfaces/automotive/vehicle/2.0/default/VehicleService.cpp
    *   hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/VehicleEmulator.cpp
    *   hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/SocketComm.cpp
*   vhal_emulator_test.py tests the Vehicle HAL via adb socket.
    * Note: This may outdated becuase there is no dedicated resrouce. Contribution is welcome.

## vhal_const_generate.py
This generates vhal_consts_2_0.py to update definitions for property ID, value
type, zone, etc. from the types.hal. Run this script whenever types.hal is
changed.

*   Must re-generate when the types.hal file changes.

```
packages/services/Car/tools/emulator/vhal_const_generate.py
```

*   hardware/interfaces/automotive/vehicle/2.0/types.hal

## VehicleHalProto_pb2.py
This defines message interface to VHAL Emulator from VehicleHalProto.proto.

*   Must re-generate whenever the proto file changes.
*   Generated from hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/proto/VehicleHalProto.proto

```
protoDir=$ANDROID_BUILD_TOP/hardware/interfaces/automotive/vehicle/2.0/default/impl/vhal_v2_0/proto
outDir=$ANDROID_BUILD_TOP/packages/services/Car/tools/emulator
# or protoc if perferred
aprotoc -I=$protoDir --python_out=$outDir $protoDir/VehicleHalProto.proto

```

*    It requires Protocol Buffers. You may build one from Android, e.g.

```
. build/envsetup.sh
lunch sdk_gcar_x86-userdebug
m aprotoc -j16
```


## OBD2 Diagnostic Injector
These scripts are useful for testing the Diagnostics API

### diagnostic_builder.py

*   Helper class used by diagnostic_injector.py
*   Stores diagnostic sensor values and bitmasks
*   VehiclePropValue-compatible

### diagnostic_injector.py

*   Deserializes JSON into diagnostic events
*   Sends over HAL Emulator Interface
*   Diagnostic JSON Format example: diagjson.example

```
./diagnostic_injector.py ./diagjson.example
```

## Python GUI Example
gui.py is an example to create an GUI to set the property.

*   packages/services/Car/tools/emulator/gui.py
*   GUI runs on host machine (PyQt4-based widgets)
*   Drives VHAL on target
    *   Works only with default VHAL
    *   Interactions generate SET messages
*   Supports bench testing of apps
*   Easy to add support for more properties
