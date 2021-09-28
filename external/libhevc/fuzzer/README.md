# Fuzzer for libhevc decoder

This describes steps to build hevc_dec_fuzzer binary.

## Linux x86/x64

###  Requirements
- cmake (3.5 or above)
- make
- clang (6.0 or above)
  needs to support -fsanitize=fuzzer, -fsanitize=fuzzer-no-link

### Steps to build
Clone libhevc repository
```
$ git clone https://android.googlesource.com/platform/external/libhevc
```
Create a directory inside libhevc and change directory
```
 $ cd libhevc
 $ mkdir build
 $ cd build
```
Build libhevc using cmake
```
 $ CC=clang CXX=clang++ cmake ../ \
   -DSANITIZE=fuzzer-no-link,address,signed-integer-overflow
 $ make
 ```
Build the fuzzer
```
 $ clang++ -std=c++11 -fsanitize=fuzzer,address -I.  -I../  -I../common \
   -I../decoder -Wl,--start-group ../fuzzer/hevc_dec_fuzzer.cpp \
   -o ./hevc_dec_fuzzer ./libhevcdec.a -Wl,--end-group
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary hevc files to that folder
To run the fuzzer
```
$ ./hevc_dec_fuzzer CORPUS_DIR
```

## Android

### Steps to build
Build the fuzzer
```
  $ SANITIZE_TARGET=address SANITIZE_HOST=address mmma -j$(nproc) \
    external/libhevc/fuzzer
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary hevc files to that folder
Push this directory to device.

To run on device
```
  $ adb sync data
  $ adb shell /data/fuzz/hevc_dec_fuzzer CORPUS_DIR
```
To run on host
```
  $ $ANDROID_HOST_OUT/fuzz/hevc_dec_fuzzer CORPUS_DIR
```

## References:
 * http://llvm.org/docs/LibFuzzer.html
 * https://github.com/google/oss-fuzz
