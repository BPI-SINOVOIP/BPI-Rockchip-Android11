Instrumented test suite for CarDialerApp using AndroidJUnitRunner.

```
1. Use atest
$ atest CarDialerInstruTests

If the test is new
$ atest CarDialerInstruTests --rebuild-module-info


2. Manually install and run the tests

Run on local machine (hawk)
$ mma && croot
$ adb install -r out/target/product/hawk/testcases/CarDialerInstruTests/arm64/CarDialerInstruTests.apk
$ adb shell am instrument -w com.android.car.dialer.tests.instrumentation/androidx.test.runner.AndroidJUnitRunner

Run on emulator (gcar_hl_emu_x86)
$ mma && croot
$ adb install -r out/target/product/generic_x86/testcases/CarDialerInstruTests/x86/CarDialerInstruTests.apk
$ adb shell am instrument -w com.android.car.dialer.tests.instrumentation/androidx.test.runner.AndroidJUnitRunner

```
