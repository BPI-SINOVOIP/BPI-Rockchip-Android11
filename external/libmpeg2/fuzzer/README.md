# Fuzzer for libmpeg2 decoder

This describes steps to build mpeg2_dec_fuzzer binary.

## Linux x86/x64

###  Requirements
- cmake (3.5 or above)
- make
- clang (6.0 or above)
  needs to support -fsanitize=fuzzer, -fsanitize=fuzzer-no-link

### Steps to build
Clone libmpeg2 repository
```
$ git clone https://android.googlesource.com/platform/external/libmpeg2
```
Create a directory inside libmpeg2 and change directory
```
 $ cd libmpeg2
 $ mkdir build
 $ cd build
```
Build libmpeg2 using cmake
```
 $ CC=clang CXX=clang++ cmake ../ \
   -DSANITIZE=fuzzer-no-link,address,signed-integer-overflow
 $ make
 ```
Build the fuzzer
```
 $ clang++ -std=c++11 -fsanitize=fuzzer,address -I.  -I../  -I../common \
   -I../decoder -Wl,--start-group ../fuzzer/mpeg2_dec_fuzzer.cpp \
   -o ./mpeg2_dec_fuzzer ./libmpeg2dec.a -Wl,--end-group
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary mpeg2 files to that folder
To run the fuzzer
```
$ ./mpeg2_dec_fuzzer CORPUS_DIR
```

## Android

### Steps to build
Build the fuzzer
```
  $ SANITIZE_TARGET=address SANITIZE_HOST=address mmma -j$(nproc) \
    external/libmpeg2/fuzzer
```

### Steps to run
Create a directory CORPUS_DIR and copy some elementary mpeg2 files to that folder
Push this directory to device.

To run on device
```
  $ adb sync data
  $ adb shell /data/fuzz/mpeg2_dec_fuzzer CORPUS_DIR
```
To run on host
```
  $ $ANDROID_HOST_OUT/fuzz/mpeg2_dec_fuzzer CORPUS_DIR
```

## References:
 * http://llvm.org/docs/LibFuzzer.html
 * https://github.com/google/oss-fuzz
