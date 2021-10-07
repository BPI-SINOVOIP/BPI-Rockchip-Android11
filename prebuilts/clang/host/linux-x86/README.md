Android Clang/LLVM Prebuilts
============================

For the latest version of this doc, please make sure to visit:
[Android Clang/LLVM Prebuilts Readme Doc](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/README.md)

LLVM Users
----------

* [**Android Platform**](https://android.googlesource.com/platform/)
  * Currently clang-r377782c
  * clang-r353983c1 for Android Q-QPR2 release
  * clang-r353983c for Android Q release
  * clang-4691093 for Android P release
  * Look for "ClangDefaultVersion" and/or "clang-" in [build/soong/cc/config/global.go](https://android.googlesource.com/platform/build/soong/+/master/cc/config/global.go/).
    * [Internal cs/ link](https://cs.corp.google.com/android/build/soong/cc/config/global.go?q=ClangDefaultVersion)

* [**RenderScript**](https://developer.android.com/guide/topics/renderscript/index.html)
  * Currently clang-3289846
  * Look for "RSClangVersion" and/or "clang-" in [build/soong/cc/config/global.go](https://android.googlesource.com/platform/build/soong/+/master/cc/config/global.go/).
    * [Internal cs/ link](https://cs.corp.google.com/android/build/soong/cc/config/global.go?q=RSClangVersion)

* [**Android Linux Kernel**](http://go/android-kernel)
  * Currently clang-r370808
  * Look for "clang-" in [4.19 build configs](https://android.googlesource.com/kernel/common/+/android-4.19/build.config.common).
  * Look for "clang-" in [4.14 build configs](https://android.googlesource.com/kernel/common/+/android-4.14/build.config.common).
  * Look for "clang-" in [4.9 build configs](https://android.googlesource.com/kernel/common/+/android-4.9-q/build.config.common).
  * Internal LLVM developers should look in the partner gerrit for more kernel configurations.

* [**NDK**](https://developer.android.com/ndk)
  * Currently clang-r365631c
  * Look for "clang-" in [ndk/toolchains.py](https://android.googlesource.com/platform/ndk/+/refs/heads/master/ndk/toolchains.py)

* [**Trusty**](https://source.android.com/security/trusty/)
  * Currently clang-r377782c
  * Look for "clang-" in [vendor/google/aosp/scripts/envsetup.sh](https://android.googlesource.com/trusty/vendor/google/aosp/+/master/scripts/envsetup.sh).

* [**Android Emulator**](https://developer.android.com/studio/run/emulator.html)
  * Currently clang-r370808
  * Look for "clang-" in [external/qemu/android/build/cmake/toolchain.cmake](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/build/cmake/toolchain.cmake#25).
    * Note that they work out of the emu-master-dev branch.
    * [Internal cs/ link](https://cs.corp.google.com/android/external/qemu/android/build/cmake/toolchain.cmake?q=clang-)

* [**Context Hub Runtime Environment (CHRE)**](https://android.googlesource.com/platform/system/chre/)
  * Currently clang-r370808
  * Look in [system/chre/build/arch/x86.mk](https://android.googlesource.com/platform/system/chre/+/master/build/arch/x86.mk#12).

* [**Keymaster (system/keymaster) tests**](https://android.googlesource.com/platform/system/keymaster)
  * Currently clang-r370808
  * Look for "clang-" in system/keymaster/Makefile
    * [Outdated AOSP sources](https://android.googlesource.com/platform/system/keymaster/+/master/Makefile)
    * [Internal sources](https://googleplex-android.googlesource.com/platform/system/keymaster/+/master/Makefile)
    * [Internal cs/ link](https://cs.corp.google.com/android/system/keymaster/Makefile?q=clang-)

* [**OpenJDK (jdk/build)**](https://android.googlesource.com/toolchain/jdk/build/)
  * Currently clang-r353983c
  * Look for "clang-" in [build-openjdk-darwin.sh](https://android.googlesource.com/toolchain/jdk/build/+/refs/heads/master/build-openjdk-darwin.sh)

* [**Clang Tools**](https://android.googlesource.com/platform/prebuilts/clang-tools/)
  * Currently clang-r377782c
  * Look for "clang-r" in [envsetup.sh](https://android.googlesource.com/platform/development/+/refs/heads/master/vndk/tools/header-checker/android/envsetup.sh)

* **Android Rust**
  * Currently clang-r377782c
  * Look for "CLANG_REVISION" in [paths.py](https://android.googlesource.com/toolchain/android_rust/+/refs/heads/master/paths.py)

* **Stage 1 compiler**
  * Currently clang-r377782c
  * Look for "clang-r" in [toolchain/llvm_android/build.py](https://android.googlesource.com/toolchain/llvm_android/+/refs/heads/master/build.py)
  * Note the chicken & egg paradox of a self hosting bootstrapping compiler; this can only be updated AFTER a new prebuilt is checked in.

* **Beagle-x15 HOSTCC**
  * Currently clang-r370808
  * Look for "clang-r" in symlink of cc in [device/ti/beagle-x15/hostcc/cc](https://android.googlesource.com/device/ti/beagle-x15/+/refs/heads/master/hostcc/)


Prebuilt Versions
-----------------

* [clang-3289846](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-3289846/) - September 2016
* [clang-r328903](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r328903/) - May 2018
* [clang-r339409b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r339409b/) - October 2018
* [clang-r344140b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r344140b/) - November 2018
* [clang-r346389b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r346389b/) - December 2018
* [clang-r346389c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r346389c/) - January 2019
* [clang-r349610](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r349610/) - February 2019
* [clang-r349610b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r349610b/) - February 2019
* [clang-r353983b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983b/) - March 2019
* [clang-r353983c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983c/) - April 2019
* [clang-r353983d](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983d/) - June 2019
* [clang-r365631b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r365631b/) - September 2019
* [clang-r365631c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r365631c/) - September 2019

More Information
----------------

We have a public mailing list that you can subscribe to:
[android-llvm@googlegroups.com](https://groups.google.com/forum/#!forum/android-llvm)

See also our [release notes](RELEASE_NOTES.md).
