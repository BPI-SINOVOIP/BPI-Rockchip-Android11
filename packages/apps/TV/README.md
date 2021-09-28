# Live TV

__Live TV__ is the Open Source reference application for watching TV on Android
TVs.

Live TV is a system app for Android TV. It should be compiled with Android TV
platform.

How to build:

1.  Enable the feature PackageManager.FEATURE_LIVE_TV.
2.  Put this project under Android platform repository if required.
3.  Include this package inside platform build.
4.  Build the platform. https://source.android.com/source/building.html

NOTE: This is a reference application and should not be used with further
hardening.

## Build just Live Tv

To install LiveTv

```bash
echo "Compiling"
m -j LiveTv
echo  "Installing"
adb install -r ${OUT}/system/priv-app/LiveTv/LiveTv.apk

```

If it is your first time installing LiveTv you will need to do

```bash
adb root
adb remount
adb push ${OUT}/system/priv-app/LiveTv/LiveTv.apk /system/priv-app/LiveTv/LiveTv.apk
adb reboot
```
