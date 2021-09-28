# Rotary Playground: Test app for rotary controller

## Building
```
make RotaryPlayground
```

## Installing
```
adb install out/target/product/[hardware]/system/app/RotaryPlayground/RotaryPlayground.apk
```

## Once installed, launch Rotary Playground in the Launcher, or with this adb command:
```
adb shell am start -n com.android.car.rotaryplayground/com.android.car.rotaryplayground.RotaryActivity
```

## Tools

### goRotary.sh
This script helps you to build, install and run the app.

* To build
```
cd $ANDROID_BUILD_TOP
packages/apps/Car/tests/tools/goRotary.sh b
```
The apks and android.car.jar are in /tmp/rotary by default.

* To install
```
packages/apps/Car/tests/tools/goRotary.sh i
```

* To run
```
packages/apps/Car/tests/tools/goRotary.sh r
```
