# Instructions for running unit tests

### Build unit test module

`m car-messenger-common-lib-unit-tests`

### Install resulting apk on device

`adb install -r -t $OUT/testcases/car-messenger-common-lib-unit-tests/arm64/car-messenger-common-lib-unit-tests.apk`

### Run all tests

`adb shell am instrument -w com.android.car.messenger.common.tests.unit`

### Run tests in a class

`adb shell am instrument -w -e class com.android.car.messenger.common.<classPath> com.android.car.messenger.common.tests.unit`

### Run a specific test

`adb shell am instrument -w -e class com.android.car.messenger.common.<classPath>#<testMethod> com.android.car.messenger.common.tests.unit`

More general information can be found at
http://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html