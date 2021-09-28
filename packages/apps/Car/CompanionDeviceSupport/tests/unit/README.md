# Instructions for running unit tests

### Build unit test module

`m CompanionDeviceSupport-unit-tests`

### Install resulting apk on device

`adb install -r -t $OUT/testcases/CompanionDeviceSupport-unit-tests/arm64/CompanionDeviceSupport-unit-tests.apk`

### Run all tests

`adb shell am instrument -w com.android.car.companiondevicesupport.tests.unit`

### Run tests in a class

`adb shell am instrument -w -e class com.android.car.companiondevicesupport.<classPath> com.android.car.companiondevicesupport.tests.unit`

### Run a specific test

`adb shell am instrument -w -e class com.android.car.companiondevicesupport.<classPath>#<testMethod> com.android.car.companiondevicesupport.tests.unit`

More general information can be found at
http://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html