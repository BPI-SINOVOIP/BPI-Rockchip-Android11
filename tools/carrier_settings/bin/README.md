To run this tool in Android 10 code base, the python tools need to be prebuilt
on Android 11 code base and dropped to this directory. This is to workaround
a bug in the older version of protobuf.

How to:

1. From Android 11 code base, build the python tools:
```
croot
m update_apn
m update_carrier_data
```

2. Copy the generated binary to this directory:
```
cp out/host/linux-x86/bin/update_apn <path/to/this/directory>
cp out/host/linux-x86/bin/update_carrier_data <path/to/this/directory>
```

3. Run the main.sh as usual
