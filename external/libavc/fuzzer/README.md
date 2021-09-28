# Fuzzer for libavc decoder

This describes steps to build avc_dec_fuzzer binary.

## Linux x86/x64

###  Requirements
- cmake (3.5 or above)
- make
- clang (6.0 or above)
  needs to support -fsanitize=fuzzer, -fsanitize=fuzzer-no-link

### Steps to build
Clone libavc repository
```
$ git clone https://android.googlesource.com/platform/external/libavc
```
Create a directory inside libavc and change directory
```
 $ cd libavc
 $ mkdir build
 $ cd build
```
Build libavc using cmake
```
 $ CC=clang CXX=clang++ cmake ../ \
   -DSANITIZE=fuzzer-no-link,address,signed-integer-overflow
 $ make
 ```
Build the fuzzer
```
 $ clang++ -std=c++11 -fsanitize=fuzzer,address -I.  -I../  -I../common \
   -I../decoder -Wl,--start-group ../fuzzer/avc_dec_fuzzer.cpp \
   -o ./avc_dec_fuzzer ./libavcdec.a -Wl,--end-group
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary h264 files to that folder
To run the fuzzer
```
$ ./avc_dec_fuzzer CORPUS_DIR
```

## Android

### Steps to build
Build the fuzzer
```
  $ SANITIZE_TARGET=address SANITIZE_HOST=address mmma -j$(nproc) \
    external/libavc/fuzzer
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary h264 files to that folder
Push this directory to device.

To run on device
```
  $ adb sync data
  $ adb shell /data/fuzz/avc_dec_fuzzer CORPUS_DIR
```
To run on host
```
  $ $ANDROID_HOST_OUT/fuzz/avc_dec_fuzzer CORPUS_DIR
```

## References:
 * http://llvm.org/docs/LibFuzzer.html
 * https://github.com/google/oss-fuzz
