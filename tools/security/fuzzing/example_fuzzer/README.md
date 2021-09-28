# example_fuzzer

This is just a simple fuzzer that will run for a few iterations
and then crash. It can be used as a smoke test to confirm that
ASAN+coverage builds and libFuzzer are working correctly.

Fuzz targets (like this one) generally live adjacent to the code that they
exercise. If you wish to write a new target that exercises the library
`/external/example`, the fuzz target should generally be in
`/external/example/test/fuzzers/`.

--------------------------------------------------------------------------------

To build the fuzzer, run:
```
  $ SANITIZE_TARGET=address SANITIZE_HOST=address mmma -j$(nproc) \
    tools/security/example_fuzzer
```
To run on device:
```
  $ adb sync data
  $ adb shell /data/fuzz/example_fuzzer
```
To run on host:
```
  $ $ANDROID_HOST_OUT/fuzz/example_fuzzer
```

--------------------------------------------------------------------------------

For more information, see the libFuzzer documentation at
https://llvm.org/docs/LibFuzzer.html.

The output should look like the output below. You should notice that:
- cov: values are increasing
- NEW units are discovered
- a stack-buffer-overflow is caught by AddressSanitizer
- the overflow is a WRITE
- the artifact generated starts with 'Hi!'

--------------------------------------------------------------------------------

```
INFO: Seed: 1154663995
INFO: Loaded 1 modules   (10 inline 8-bit counters): 10 [0x5bde606000, 0x5bde60600a),
INFO: Loaded 1 PC tables (10 PCs): 10 [0x5bde606010,0x5bde6060b0),
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A corpus is not provided, starting from an empty corpus
#2  INITED cov: 5 ft: 5 corp: 1/1b lim: 4 exec/s: 0 rss: 23Mb
#2133 NEW    cov: 8 ft: 8 corp: 2/26b lim: 25 exec/s: 0 rss: 23Mb L: 25/25 MS: 1 CrossOver-
#2162 REDUCE cov: 8 ft: 8 corp: 2/24b lim: 25 exec/s: 0 rss: 23Mb L: 23/23 MS: 4 CMP-EraseBytes-InsertRepeatedBytes-InsertByte- DE: "\x18\x00\x00\x00\x00\x00\x00\x00"-
=================================================================
==32069==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x007fe3caf8c3 at pc 0x0078919740f4 bp 0x007fe3caf890 sp 0x007fe3caf020
WRITE of size 4 at 0x007fe3caf8c3 thread T0
    #0 0x78919740f0  (/system/lib64/libclang_rt.asan-aarch64-android.so+0xb30f0)
    #1 0x5bde5e0354  (/data/fuzz/example_fuzzer+0xf354)
    #2 0x5bde5f1574  (/data/fuzz/example_fuzzer+0x20574)
    #3 0x5bde5f1118  (/data/fuzz/example_fuzzer+0x20118)
    #4 0x5bde5f2314  (/data/fuzz/example_fuzzer+0x21314)
    #5 0x5bde5f2fc0  (/data/fuzz/example_fuzzer+0x21fc0)
    #6 0x5bde5e4c10  (/data/fuzz/example_fuzzer+0x13c10)
    #7 0x5bde5e0568  (/data/fuzz/example_fuzzer+0xf568)
    #8 0x7891304254  (/apex/com.android.runtime/lib64/bionic/libc.so+0x7c254)

Address 0x007fe3caf8c3 is located in stack of thread T0 at offset 35 in frame
    #0 0x5bde5e008c  (/data/fuzz/example_fuzzer+0xf08c)

  This frame has 2 object(s):
    [32, 35) 'buffer.i' (line 23) <== Memory access at offset 35 overflows this variable
    [48, 72) 'null_terminated_string' (line 31)
HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork
      (longjmp and C++ exceptions *are* supported)
SUMMARY: AddressSanitizer: stack-buffer-overflow (/system/lib64/libclang_rt.asan-aarch64-android.so+0xb30f0)
Shadow bytes around the buggy address:
  0x001ffc795ec0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795ed0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795ee0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795ef0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795f00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x001ffc795f10: 00 00 00 00 f1 f1 f1 f1[03]f2 00 00 00 f3 f3 f3
  0x001ffc795f20: f3 f3 f3 f3 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795f30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795f40: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795f50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x001ffc795f60: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
  Shadow gap:              cc
==32069==ABORTING
MS: 4 CopyPart-InsertByte-PersAutoDict-CMP- DE: "\x18\x00\x00\x00\x00\x00\x00\x00"-"Hi!"-; base unit: adc83b19e793491b1c6ea0fd8b46cd9f32e592fc
0x48,0x69,0x21,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xa,
Hi!\x00\x00\x00\x00\x00\x00\x00\x0a
artifact_prefix='./'; Test unit written to ./crash-8a4daff3931e139b7dfff19e7e47dc75c29c3a5e
Base64: SGkhAAAAAAAAAAo=
```
