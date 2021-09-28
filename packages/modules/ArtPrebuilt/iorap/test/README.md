# Introduction

This directory contains a toy app for iorap test. The app shows a version number
when started and reads a text file.

 * iorap_test_app_v1.apk: test app version 1
 * iorap_test_app_v2.apk: test app version 2 (same with version 1 except for the
   version)
 * iorap_test_app_v3.apk: test app version 3 (same with version 1 except for the
   version)
 * src: The src of the app.

# Build
1. Set ANDROID_SDK_ROOT, For example:

  export ANDROID_SDK_ROOT=.../Android/Sdk

2. The command to build the apk is:

  bash build.sh

# Version Change
To change the version of the app,
 1. open file src/IorapTestApp/app/build.gradle
 2. change the "versionCode" field as the following

   android {
      ...
      defaultConfig {
          ...
          versionCode = 1.0
          ...
      }
      ...
   }




